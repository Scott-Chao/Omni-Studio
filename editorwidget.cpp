#include "editorwidget.h"
#include "wikilinktextedit.h"
#include "codeeditor.h"
#include "smdeditor.h"
#include "tagindex.h"
#include "languageutils.h"
#include "pykeywords.h"
#include <memory>
#include "fileutils.h"
#include "configmanager.h"
#include "thememanager.h"
#include "settingsmanager.h"
#include "cppkeywords.h"
#include "debuglog.h"
#include <QFile>
#include <QTextStream>
#include <QUuid>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QWheelEvent>
#if QT_CONFIG(gestures)
#include <QNativeGestureEvent>
#endif
#include <QFileDialog>
#include <QRegularExpression>
#include <QUrl>
#include <QDesktopServices>
#include <QWebEnginePage>
#include <QTextBlock>
#include <QTextDocument>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonObject>
#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDir>
#include <QPageSize>
#include <QWebEngineView>

// Normalize trailing newlines for robust modified detection.
// Trailing blank lines are a file-format artifact, not a meaningful edit.
static QString normalizeTrailingNewlines(const QString &text)
{
    QString result = text;
    while (result.endsWith(QLatin1Char('\n')) || result.endsWith(QLatin1Char('\r')))
        result.chop(1);
    return result;
}

// Forward declaration for highlightWithRules used in highlightCodeBlock
static QString highlightWithRules(
    const QString &code,
    const QVector<QPair<QRegularExpression, QPair<QColor,bool>>> &rules);

// 自定义 Web 页面：拦截 wikilink 和 runblock 导航
class PreviewPage : public QWebEnginePage {
public:
    std::function<void(const QString &)> onWikiLinkClicked;
    std::function<void(const QString &, const QString &, int)> onRunCodeBlock;
    std::function<void(const QString &)> onTagClicked;

    using QWebEnginePage::QWebEnginePage;

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override
    {
        if (type == NavigationTypeLinkClicked) {
            if (url.scheme() == QStringLiteral("wikilink")) {
                QString linkText = QUrl::fromPercentEncoding(url.path().toUtf8());
                if (onWikiLinkClicked)
                    onWikiLinkClicked(linkText);
                return false;
            }
            if (url.scheme() == QStringLiteral("tag")) {
                QString tag = QUrl::fromPercentEncoding(url.path().toUtf8());
                if (onTagClicked)
                    onTagClicked(tag);
                return false;
            }
            if (url.scheme() == QStringLiteral("runblock")) {
                // Retrieve stored code from JS context
                runJavaScript(QStringLiteral("window._runCodeData"),
                    [this](const QVariant &result) {
                        QString data = result.toString();
                        if (!data.isEmpty() && onRunCodeBlock) {
                            QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
                            QJsonObject obj = doc.object();
                            QString lang = obj.value(QStringLiteral("lang")).toString();
                            QString code = obj.value(QStringLiteral("code")).toString();
                            int blockIndex = obj.value(QStringLiteral("blockIndex")).toInt(0);
                            onRunCodeBlock(lang, code, blockIndex);
                        }
                    });
                return false;
            }
            QDesktopServices::openUrl(url);
            return false;
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
};

EditorWidget::EditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_filePath("")
    , m_previewMode(false)
    , m_zoomFactor(1.0) // 初始化缩放因子
    , m_baseFontSize(0) // 稍后从字体获取
{
    // 创建编辑器和预览控件
    m_textEdit = new WikiLinkTextEdit(this);
    m_textEdit->viewport()->installEventFilter(this);

    m_codeEditor = new CodeEditor(this);
    m_codeEditor->viewport()->installEventFilter(this);

    // 堆叠布局：索引0=编辑，索引1=预览容器（懒创建），索引2=代码编辑...
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->addWidget(m_textEdit);
    m_stackedWidget->addWidget(m_codeEditor);

    // PDF 视图
    m_pdfDocument = new QPdfDocument(this);
    m_pdfView = new QPdfView(this);
    m_pdfView->setDocument(m_pdfDocument);
    m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);
    m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
    m_pdfView->setStyleSheet(
        QStringLiteral("background-color: %1;")
            .arg(ThemeManager::instance().color("editor.background").name()));
    m_pdfView->installEventFilter(this);
    if (auto *vp = m_pdfView->viewport())
        vp->installEventFilter(this);
    m_stackedWidget->addWidget(m_pdfView);

    // SMD 编辑器 (page 5)
    m_smdEditor = new SmdEditor(this);
    m_smdEditor->installEventFilter(this);
    m_stackedWidget->addWidget(m_smdEditor);
    connect(m_smdEditor, &SmdEditor::modificationChanged, this, &EditorWidget::modificationChanged);
    connect(m_smdEditor, &SmdEditor::fileLoaded, this, &EditorWidget::fileLoaded);
    connect(m_smdEditor, &SmdEditor::fileSaved, this, &EditorWidget::fileSaved);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stackedWidget);
    setLayout(layout);

    const auto &cfg = ConfigManager::instance();
    auto &sm = SettingsManager::instance();
    m_baseFontSize = qRound(sm.value("editor.font.size", cfg.editorFontSize()).toDouble());

    {
        QString fontFamily = sm.value("editor.font.family", cfg.editorFontFamily()).toString();
        QFont textFont(fontFamily, m_baseFontSize);
        m_textEdit->setFont(textFont);
        QFont codeFont(fontFamily, m_baseFontSize);
        codeFont.setStyleHint(QFont::Monospace);
        m_codeEditor->setFont(codeFont);

        auto &tm = ThemeManager::instance();
        m_textEdit->setStyleSheet(QString(
            "QTextEdit { background-color: %1; color: %2; selection-background-color: %3; }"
        ).arg(tm.color("editor.background").name(),
              tm.color("editor.foreground").name(),
              tm.color("editor.selectionBackground").name()));
    }

    // 应用缩进宽度设置（代码和 Markdown 独立）
    int codeIndent = sm.value("editor.indent_width", cfg.editorIndentWidth()).toInt();
    int mdIndent = sm.value("editor.markdown_indent_width", cfg.editorMarkdownIndentWidth()).toInt();
    m_codeEditor->setIndentWidth(codeIndent);
    m_textEdit->setIndentWidth(mdIndent);

    // 当编辑区内容改变时，更新修改标志并刷新预览
    connect(m_textEdit, &QTextEdit::textChanged, this, &EditorWidget::onTextChanged);
    connect(m_codeEditor, &QPlainTextEdit::textChanged, this, &EditorWidget::onTextChanged);
    connect(m_codeEditor, &CodeEditor::diagnosticsToggleRequested,
            this, [this]() {
        debugLog("EditorWidget: forwarding diagnosticsToggleRequested");
        emit diagnosticsToggleRequested();
    });
    // 当编辑区修改状态变化时发出信号
    connect(m_textEdit, &QTextEdit::textChanged, this, &EditorWidget::updateModificationChanged);
    connect(m_codeEditor, &QPlainTextEdit::textChanged, this, &EditorWidget::updateModificationChanged);

    setPreviewMode(false); // 默认编辑模式

    m_contentCheckTimer.setSingleShot(true);
    m_contentCheckTimer.setInterval(ConfigManager::instance().editorContentCheckTimerMs());
    connect(&m_contentCheckTimer, &QTimer::timeout, this, &EditorWidget::onContentCheckTimeout);

    // 分屏预览 debounce 定时器
    m_splitDebounceTimer.setSingleShot(true);
    m_splitDebounceTimer.setInterval(
        SettingsManager::instance().value("preview.split_debounce_ms",
                                          ConfigManager::instance().previewSplitDebounceMs()).toInt());
    connect(&m_splitDebounceTimer, &QTimer::timeout, this, &EditorWidget::onSplitDebounceTimeout);

    // 自动保存定时器（周期触发，每 30 秒检查一次）
    m_autoSaveTimer.setInterval(ConfigManager::instance().autoSaveIntervalMs());
    m_autoSaveEnabled = sm.value("editor.auto_save", cfg.autoSaveEnabled()).toBool();
    connect(&m_autoSaveTimer, &QTimer::timeout, this, &EditorWidget::onAutoSaveTimeout);

    // 当文本编辑器内容变化时，重置计时器
    connect(m_textEdit, &QTextEdit::textChanged, this, [this]() {
        m_contentCheckTimer.start();
    });
    connect(m_codeEditor, &QPlainTextEdit::textChanged, this, [this]() {
        m_contentCheckTimer.start();
    });
    connect(m_smdEditor, &SmdEditor::contentChanged, this, [this]() {
        if (m_editorMode == SmdEdit)
            m_contentCheckTimer.start();
    });
    m_originalContent = normalizeTrailingNewlines(toPlainText()); // 记录当前内容，用于内容比较

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &EditorWidget::reloadEditorColors);
}

// ---- 懒创建预览视图 ----

void EditorWidget::ensurePreviewView()
{
    if (m_previewView)
        return;

    m_previewView = new QWebEngineView(this);
    m_previewView->setAttribute(Qt::WA_NoSystemBackground, true);

    PreviewPage *previewPage = new PreviewPage(m_previewView);
    previewPage->onWikiLinkClicked = [this](const QString &fileName) {
        emit wikiLinkClicked(fileName);
    };
    previewPage->onRunCodeBlock = [this](const QString &language, const QString &code, int blockIndex) {
        emit runCodeBlockRequested(language, code, blockIndex);
    };
    previewPage->onTagClicked = [this](const QString &tag) {
        emit tagClicked(tag);
    };
    m_previewView->setPage(previewPage);

    m_previewView->installEventFilter(this);
    QTimer::singleShot(0, this, [this]() {
        if (m_previewView) {
            if (QWidget *fp = m_previewView->focusProxy())
                fp->installEventFilter(this);
        }
    });

    // 暗色遮罩容器：在 WebEngine 渲染完成前遮挡白底
    m_previewContainer = new QWidget(this);
    m_previewContainer->installEventFilter(this);
    m_previewContainer->setStyleSheet(
        QString("background-color: %1;")
            .arg(ThemeManager::instance().color("preview.containerBackground").name()));
    QVBoxLayout *containerLayout = new QVBoxLayout(m_previewContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->addWidget(m_previewView);

    // 插入到 stacked widget 索引 1（m_textEdit 之后，m_codeEditor 之前）
    m_stackedWidget->insertWidget(1, m_previewContainer);
}

void EditorWidget::destroyPreviewView()
{
    if (!m_previewView && !m_previewContainer)
        return;

    m_previewReady = false;
    if (m_previewContainer) {
        m_stackedWidget->removeWidget(m_previewContainer);
        m_previewContainer->deleteLater();
        m_previewContainer = nullptr;
    }
    m_previewView = nullptr;
}

void EditorWidget::destroySplitPreviewWidgets()
{
    if (!m_splitSplitter)
        return;

    m_splitPreviewReady = false;
    m_lastSplitPreviewContent.clear();

    m_stackedWidget->removeWidget(m_splitSplitter);
    m_splitSplitter->deleteLater();
    m_splitSplitter = nullptr;
    m_splitPreviewView = nullptr;
    m_splitPreviewPage = nullptr;
    m_splitTextWrapper = nullptr;
}

void EditorWidget::setPreviewMode(bool preview)
{
    if (m_editorMode == CodeEdit || m_editorMode == PdfView || m_editorMode == SmdEdit) {
        return;
    }
    if (preview == m_previewMode) {
        return;
    }
    // 互斥：进入全屏预览时，退出分屏预览
    if (preview && m_splitPreview) {
        setSplitPreviewMode(false);
    }
    m_previewMode = preview;

    if (m_previewMode) {
        ensurePreviewView();
        if (!m_previewReady) {
            // 首次预览：加载完整模板（setHtml），延迟到 loadFinished 再切换
            m_previewView->page()->setBackgroundColor(
                ThemeManager::instance().color("preview.webEngineBackground"));

            QFile tmplFile(QStringLiteral(":/preview/template.html"));
            if (!tmplFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                return;
            }
            QString tmpl = QString::fromUtf8(tmplFile.readAll());
            tmplFile.close();

            QString safeContent = preparePreviewContent(m_textEdit->toPlainText());
            tmpl.replace(QStringLiteral("{{MARKDOWN_CONTENT}}"), safeContent);
            applyPreviewTheme(tmpl);

            // 关键修复：QStackedWidget 不会更新隐藏页面的子控件尺寸，
            // 导致 QWebEngineView 停留在默认小尺寸 (100x30)，
            // Chromium 以此尺寸初始化 → 后续拉伸 → 右侧/下侧白边
            // 用 setFixedSize 绕过布局，强制 WebEngineView 在 setHtml 时使用正确尺寸
            QSize correctSize = m_stackedWidget->size();
            m_previewView->setFixedSize(correctSize);
            m_previewView->setAttribute(Qt::WA_NativeWindow, true);
            m_previewView->winId();

            m_previewView->setHtml(tmpl, QUrl(QStringLiteral("qrc:/preview/")));

            connect(m_previewView->page(), &QWebEnginePage::loadFinished, this,
                [this](bool ok) {
                    if (!m_previewView) return;
                    auto *page = m_previewView->page();
                    disconnect(page, &QWebEnginePage::loadFinished, this, nullptr);
                    if (!ok) {
                        return;
                    }
                    m_previewReady = true;

                    // 解除 fixedSize，恢复布局管理
                    m_previewView->setMinimumSize(0, 0);
                    m_previewView->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

                    m_stackedWidget->setCurrentWidget(m_previewContainer);

                    // WebEngine 内部的 focus proxy 在页面加载后才确定，
                    // 需要在其上安装事件过滤器才能捕获预览区的 Ctrl+滚轮缩放
                    if (QWidget *fp = m_previewView->focusProxy())
                        fp->installEventFilter(this);
                    applyZoom();
                });
        } else {
            updatePreviewContent([this]() {
                m_stackedWidget->setCurrentWidget(m_previewContainer);
                applyZoom();
            });
        }
    } else {
        m_stackedWidget->setCurrentWidget(m_textEdit);
        destroyPreviewView();
        applyZoom();
    }
}

void EditorWidget::refreshPreview()
{
    updatePreviewContent(nullptr);
    if (m_splitPreview && m_splitPreviewReady) {
        updateSplitPreviewContentNow();
    }
}

QString EditorWidget::processWikiLinks(const QString &markdown)
{
    static const QRegularExpression wikiRegExp(
        QStringLiteral(R"(\[\[((?:[^\[\]]|\[(?1)\])*)\]\])"));

    QRegularExpressionMatchIterator it = wikiRegExp.globalMatch(markdown);
    QString result;
    int lastPos = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        result += QStringView(markdown).mid(lastPos, match.capturedStart() - lastPos).toString();
        QString linkText = match.captured(1);

        QByteArray encoded = QUrl::toPercentEncoding(linkText);
        QString encodedTarget = QString::fromLatin1(encoded);
        result += QStringLiteral("<a href=\"wikilink:%1\">%2</a>")
                      .arg(encodedTarget, linkText.toHtmlEscaped());
        lastPos = match.capturedEnd();
    }
    result += QStringView(markdown).mid(lastPos).toString();
    return result;
}

QString EditorWidget::highlightCodeBlock(const QString &code, const QString &langId)
{
    const auto &cfg = ConfigManager::instance();
    QString html;
    html += QStringLiteral("<pre style=\"background:#1e1e1e;padding:16px;border-radius:0 0 6px 6px;"
                           "overflow-x:auto;margin:0;\">");
    html += QStringLiteral("<code style=\"background:none;padding:0;"
                           "font-family:Consolas,'Courier New',monospace;font-size:0.9em;\">");

    // Colors matching CppSyntaxHighlighter / PythonSyntaxHighlighter
    const QColor kwColor    = cfg.syntaxKeywords();      // #569CD6
    const QColor typeColor  = cfg.syntaxTypes();         // #4EC9B0
    const QColor numColor   = cfg.syntaxNumbers();       // #B5CEA8
    const QColor strColor   = cfg.syntaxStrings();       // #CE9178
    const QColor cmtColor   = cfg.syntaxComments();      // #6A9955

    struct MultiLineSpan { QString placeholder; QString raw; };

    if (langId == QStringLiteral("cpp")) {
        const QColor preprocColor = cfg.syntaxPreprocessor(); // #C586C0

        const QStringList keywords = cppKeywords();
        // Build keyword regex
        QString kwPattern;
        for (int i = 0; i < keywords.size(); ++i) {
            if (i > 0) kwPattern += QChar(L'|');
            kwPattern += QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(keywords[i]));
        }

        // Type list
        const QStringList types = cppCommonTypes();
        QString typePattern;
        for (int i = 0; i < types.size(); ++i) {
            if (i > 0) typePattern += QChar(L'|');
            typePattern += QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(types[i]));
        }

        // Combine patterns — lowest priority first, so later rules override earlier ones
        // This matches CppSyntaxHighlighter's rule application order
        QVector<QPair<QRegularExpression, QPair<QColor,bool>>> cppRules;
        // Keywords (bold) — lowest priority
        cppRules.append({QRegularExpression(kwPattern), {kwColor, true}});
        // Preprocessor
        cppRules.append({QRegularExpression(QStringLiteral("^\\s*#\\s*\\w+")), {preprocColor, false}});
        // Types
        cppRules.append({QRegularExpression(typePattern), {typeColor, false}});
        // Numbers
        cppRules.append({QRegularExpression(
            QStringLiteral("\\b0[xX][0-9a-fA-F]+[']?[0-9a-fA-F]*\\b"
                           "|\\b0[bB][01]+[']?[01]*\\b"
                           "|\\b[0-9]+[']?[0-9]*(?:\\.[0-9]+[']?[0-9]*)?(?:[eE][+-]?[0-9]+)?(?:f|F|l|L|u|U|ll|LL|ull|ULL)?\\b")),
            {numColor, false}});
        // Char literals
        cppRules.append({QRegularExpression(QStringLiteral(R"('(?:[^'\\]|\\.)'|'(?:\\.)')")), {strColor, false}});
        // String literals
        cppRules.append({QRegularExpression(QStringLiteral(R"("(?:[^"\\]|\\.)*")")), {strColor, false}});
        // Single-line comment — highest priority
        cppRules.append({QRegularExpression(QStringLiteral("//[^\n]*")), {cmtColor, false}});

        // Pre-process multi-line comments /*...*/ to protect from line-by-line rules
        QString procCode = code;
        QVector<MultiLineSpan> mlSpans;
        {
            int midx = 0;
            static const QRegularExpression reMLCmt(QStringLiteral(R"(/\*[\s\S]*?\*/)"));
            while (true) {
                QRegularExpressionMatch m = reMLCmt.match(procCode);
                if (!m.hasMatch()) break;
                QString ph = QStringLiteral("\x00MLC%1\x00").arg(midx);
                mlSpans.append({ph, m.captured(0)});
                procCode = procCode.left(m.capturedStart()) + ph + procCode.mid(m.capturedEnd());
                ++midx;
            }
        }
        QString highlighted = highlightWithRules(procCode, cppRules);
        for (const auto &s : mlSpans)
            highlighted.replace(s.placeholder,
                QStringLiteral("<span style=\"color:%1;\">%2</span>")
                    .arg(cmtColor.name(), s.raw.toHtmlEscaped()));
        html += highlighted;
    } else if (langId == QStringLiteral("python")) {
        const QColor decorColor = cfg.syntaxPythonDecorators(); // #C586C0
        const QColor selfColor  = cfg.syntaxPythonSelfCls();    // #DCDCDC

        // Python keyword list
        const QStringList pyKeywords = ::pyKeywords();
        QString pyKwPattern;
        for (int i = 0; i < pyKeywords.size(); ++i) {
            if (i > 0) pyKwPattern += QChar(L'|');
            pyKwPattern += QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(pyKeywords[i]));
        }

        // Builtins
        const QStringList builtins = pyBuiltins();
        QString builtinPattern;
        for (int i = 0; i < builtins.size(); ++i) {
            if (i > 0) builtinPattern += QChar(L'|');
            builtinPattern += QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(builtins[i]));
        }

        // Python rules — lowest priority first
        QVector<QPair<QRegularExpression, QPair<QColor,bool>>> pyRules;
        // Keywords (bold) — lowest priority
        pyRules.append({QRegularExpression(pyKwPattern), {kwColor, true}});
        // Builtins
        pyRules.append({QRegularExpression(builtinPattern), {typeColor, false}});
        // Numbers
        pyRules.append({QRegularExpression(
            QStringLiteral("\\b0[xX][0-9a-fA-F](?:[0-9a-fA-F_]*[0-9a-fA-F])?\\b"
                           "|\\b0[bB][01](?:[01_]*[01])?\\b"
                           "|\\b0[oO][0-7](?:[0-7_]*[0-7])?\\b"
                           "|\\b\\d[\\d_]*(?:\\.\\d[\\d_]*)?(?:[eE][+-]?\\d[\\d_]*(?:\\.\\d[\\d_]*)?)?[jJ]?\\b")),
            {numColor, false}});
        // self / cls
        pyRules.append({QRegularExpression(QStringLiteral("\\bself\\b")), {selfColor, false}});
        pyRules.append({QRegularExpression(QStringLiteral("\\bcls\\b")), {selfColor, false}});
        // Decorator
        pyRules.append({QRegularExpression(QStringLiteral("^\\s*@[\\w.]+")), {decorColor, false}});
        // Strings (double-quoted with optional prefix)
        pyRules.append({QRegularExpression(
            QStringLiteral(R"((?:[furbFURB]{1,2})?"(?:[^"\\]|\\.)*")")),
            {strColor, false}});
        // Strings (single-quoted with optional prefix)
        pyRules.append({QRegularExpression(
            QStringLiteral(R"((?:[furbFURB]{1,2})?'(?:[^'\\]|\\.)*')")),
            {strColor, false}});
        // Comment — highest priority
        pyRules.append({QRegularExpression(QStringLiteral("#[^\n]*")), {cmtColor, false}});

        // Pre-process triple-quoted strings """...""" and '''...''' (with optional prefix)
        // to protect them from single-line string rule fragmentation
        QString procCode = code;
        QVector<MultiLineSpan> tqSpans;
        {
            int tidx = 0;
            static const QRegularExpression reTQD(
                QStringLiteral(R"((?:[furbFURB]{1,2})?(?:"""[\s\S]*?"""|'''[\s\S]*?'''))"));
            while (true) {
                QRegularExpressionMatch m = reTQD.match(procCode);
                if (!m.hasMatch()) break;
                QString ph = QStringLiteral("\x00TQ%1\x00").arg(tidx);
                tqSpans.append({ph, m.captured(0)});
                procCode = procCode.left(m.capturedStart()) + ph + procCode.mid(m.capturedEnd());
                ++tidx;
            }
        }
        QString highlighted = highlightWithRules(procCode, pyRules);
        // Restore triple-quoted strings with comment color (matching PythonSyntaxHighlighter)
        for (const auto &s : tqSpans)
            highlighted.replace(s.placeholder,
                QStringLiteral("<span style=\"color:%1;\">%2</span>")
                    .arg(cmtColor.name(), s.raw.toHtmlEscaped()));
        html += highlighted;
    } else {
        // Unknown language — plain text
        html += code.toHtmlEscaped();
    }

    html += QStringLiteral("</code></pre>");
    return html;
}

// Apply a set of highlighting rules to code text and return HTML
QString highlightWithRules(
    const QString &code,
    const QVector<QPair<QRegularExpression, QPair<QColor,bool>>> &rules)
{
    // Split into lines, remove trailing empty lines caused by newline before closing ```
    QStringList lines = code.split(QChar(L'\n'));
    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    QString result;
    for (int lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
        const QString &line = lines[lineIdx];
        if (line.isEmpty()) {
            result += QChar(L'\n');
            continue;
        }

        // Process rules in REVERSE order (highest priority first).
        // Only fill positions not yet filled, so higher-priority rules (processed first)
        // lock in their colors before lower-priority rules can overwrite them.
        QVector<QColor> colorAt(line.length());
        QVector<bool> boldAt(line.length());
        for (int i = 0; i < line.length(); ++i) {
            colorAt[i] = QColor(); // invalid = not filled
            boldAt[i] = false;
        }
        for (int r = rules.size() - 1; r >= 0; --r) {
            QRegularExpressionMatchIterator it = rules[r].first.globalMatch(line);
            while (it.hasNext()) {
                QRegularExpressionMatch m = it.next();
                int start = m.capturedStart();
                int length = m.capturedLength();
                for (int j = start; j < start + length && j < line.length(); ++j) {
                    if (!colorAt[j].isValid()) {
                        colorAt[j] = rules[r].second.first;
                        boldAt[j] = rules[r].second.second;
                    }
                }
            }
        }

        // Output with spans
        int i = 0;
        while (i < line.length()) {
            if (!colorAt[i].isValid()) {
                // Plain text
                int end = i + 1;
                while (end < line.length() && !colorAt[end].isValid())
                    ++end;
                result += line.mid(i, end - i).toHtmlEscaped();
                i = end;
            } else {
                QColor curColor = colorAt[i];
                bool curBold = boldAt[i];
                int end = i + 1;
                while (end < line.length() && colorAt[end].isValid()
                       && colorAt[end] == curColor && boldAt[end] == curBold)
                    ++end;
                QString style = QStringLiteral("color:%1;").arg(curColor.name());
                if (curBold) style += QStringLiteral("font-weight:bold;");
                result += QStringLiteral("<span style=\"%1\">%2</span>")
                              .arg(style, line.mid(i, end - i).toHtmlEscaped());
                i = end;
            }
        }

        if (lineIdx < lines.size() - 1)
            result += QChar(L'\n');
    }
    return result;
}

QString EditorWidget::preHighlightCodeBlocks(const QString &markdown)
{
    // Match fenced code blocks: ```lang\n...\n```
    // Group 1: opening ``` (captured for closing match via backreference)
    // Group 2: language identifier (optional)
    // Group 3: code content
    static const QRegularExpression fencedRegex(
        QStringLiteral("(```)(\\w*)\\r?\\n([\\s\\S]*?)\\1"));

    // Runnable languages matching the preview-template.js list
    static const QStringList runnableLangs = {
        QStringLiteral("python"), QStringLiteral("py"),
        QStringLiteral("cpp"),   QStringLiteral("c"),
        QStringLiteral("cc"),    QStringLiteral("cxx")
    };

    QString result;
    int lastPos = 0;
    int blockIndex = 0; // 0-based index for all fenced code blocks

    QRegularExpressionMatchIterator it = fencedRegex.globalMatch(markdown);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();

        // Append text before this code block
        result += QStringView(markdown).mid(lastPos, match.capturedStart() - lastPos).toString();

        const QString lang  = match.captured(2);
        const QString code  = match.captured(3);
        const QString langLower = lang.toLower();

        // Check if this is a known code language
        QString langId = LanguageUtils::normalizeCodeFenceLanguage(langLower);

        if (langId.isEmpty()) {
            // Unknown language — keep original fenced block
            result += match.captured(0);
        } else {
            // Known language — apply syntax highlighting
            bool showRun = runnableLangs.contains(langLower);

            // Build the complete HTML wrapper
            QString blockHtml;
            blockHtml += QStringLiteral("<div class=\"code-block-wrapper\" data-block-index=\"%1\">").arg(blockIndex);

            // Always show header when lang is specified
            blockHtml += QStringLiteral("<div class=\"code-block-header\">");
            blockHtml += QStringLiteral("<span class=\"code-lang-label\">%1</span>").arg(lang);

            if (showRun && !code.isEmpty()) {
                // HTML-escape code for data-code attribute
                QString escapedCode = code;
                escapedCode.replace(QStringLiteral("&"),  QStringLiteral("&amp;"));
                escapedCode.replace(QStringLiteral("\""), QStringLiteral("&quot;"));
                escapedCode.replace(QStringLiteral("'"),  QStringLiteral("&#39;"));
                escapedCode.replace(QStringLiteral("<"),  QStringLiteral("&lt;"));
                escapedCode.replace(QStringLiteral(">"),  QStringLiteral("&gt;"));

                blockHtml += QStringLiteral("<a class=\"run-code-btn\" href=\"runblock:execute\""
                                            " data-lang=\"%1\" data-code=\"%2\" data-block-index=\"%3\">▶ Run</a>")
                                .arg(langLower, escapedCode).arg(blockIndex);
            }

            blockHtml += QStringLiteral("</div>");

            // Highlighted code
            QString highlighted = highlightCodeBlock(code, langId);
            if (highlighted.isEmpty()) {
                // Fallback: use original fenced block
                result += match.captured(0);
                lastPos = match.capturedEnd();
                continue;
            }

            blockHtml += highlighted;
            blockHtml += QStringLiteral("</div>");

            // Encode the complete HTML as base64 and wrap in a custom fenced block
            // so marked.js's renderer.code can decode and inject it directly
            QString b64 = QString::fromLatin1(blockHtml.toUtf8().toBase64());
            result += QStringLiteral("```highlighted\n") + b64 + QStringLiteral("\n```");
            ++blockIndex;
        }

        lastPos = match.capturedEnd();
    }

    // Append remaining text after the last code block
    result += QStringView(markdown).mid(lastPos).toString();

    return result;
}

QString EditorWidget::preparePreviewContent(const QString &rawMarkdown)
{
    // Step 0: Inject heading anchors for preview navigation
    // Step 1: Pre-highlight code blocks (produces ```highlighted\n<base64>\n``` custom blocks)
    // Step 2: Process wiki links and tags (base64 content won't match [[...]] or #tag)
    // Step 3: Prevent </script> injection for the setHtml() path
    QString content = injectHeadingAnchors(rawMarkdown);
    content = preHighlightCodeBlocks(content);
    content = processWikiLinks(content);
    content = TagIndex::processTagsForPreview(content);
    content.replace(QStringLiteral("</script>"), QStringLiteral("<\\/script>"));
    return content;
}

void EditorWidget::applyPreviewTheme(QString &tmpl)
{
    auto &tm = ThemeManager::instance();
    auto hex = [&](const QString &token) {
        QColor c = tm.color(token);
        return c.isValid() ? c.name() : QStringLiteral("#000000");
    };
    tmpl.replace(QStringLiteral("{{PREVIEW_BG}}"),        hex("preview.containerBackground"));
    tmpl.replace(QStringLiteral("{{PREVIEW_FG}}"),        hex("editor.foreground"));
    tmpl.replace(QStringLiteral("{{PREVIEW_CODE_BG}}"),   hex("preview.codeBackground"));
    tmpl.replace(QStringLiteral("{{PREVIEW_BORDER}}"),    hex("panel.border"));
    tmpl.replace(QStringLiteral("{{PREVIEW_LINK}}"),      hex("textLink.foreground"));
    tmpl.replace(QStringLiteral("{{PREVIEW_HEADING}}"),   hex("editor.foreground"));
    tmpl.replace(QStringLiteral("{{PREVIEW_BLOCKQUOTE}}"),hex("tab.inactiveForeground"));
    tmpl.replace(QStringLiteral("{{PREVIEW_TH_BG}}"),     hex("list.hoverBackground"));
    tmpl.replace(QStringLiteral("{{PREVIEW_HR}}"),        hex("panel.border"));
}

QString EditorWidget::previewThemeJs()
{
    auto &tm = ThemeManager::instance();
    auto hex = [&](const QString &token) {
        QColor c = tm.color(token);
        return c.isValid() ? c.name() : QStringLiteral("#000000");
    };
    return QStringLiteral(
        "window.updateTheme({"
        "  bg: '%1', fg: '%2', codeBg: '%3', border: '%4',"
        "  link: '%5', heading: '%6', blockquote: '%7', thBg: '%8', hr: '%9'"
        "});"
    ).arg(hex("preview.containerBackground"),
          hex("editor.foreground"),
          hex("preview.codeBackground"),
          hex("panel.border"),
          hex("textLink.foreground"),
          hex("editor.foreground"),
          hex("tab.inactiveForeground"),
          hex("list.hoverBackground"),
          hex("panel.border"));
}

void EditorWidget::refreshPreviewTheme()
{
    auto &tm = ThemeManager::instance();
    if (m_previewView) {
        m_previewView->page()->setBackgroundColor(
            tm.color("preview.webEngineBackground"));
    }
    if (m_previewReady && m_previewView && m_previewView->page()) {
        m_previewView->page()->runJavaScript(previewThemeJs());
    }
    if (m_splitPreviewView) {
        m_splitPreviewView->page()->setBackgroundColor(
            tm.color("preview.webEngineBackground"));
    }
    if (m_splitPreviewReady && m_splitPreviewView) {
        m_splitPreviewView->page()->runJavaScript(previewThemeJs());
    }
}

QMap<int, QString> EditorWidget::extractCodeBlockContents(const QString &markdown) const
{
    QMap<int, QString> blocks;
    static const QRegularExpression fencedRegex(
        QStringLiteral("(```)(\\w*)\\r?\\n([\\s\\S]*?)\\1"));
    int idx = 0;
    auto it = fencedRegex.globalMatch(markdown);
    while (it.hasNext()) {
        auto match = it.next();
        QString langLower = match.captured(2).toLower();
        if (!LanguageUtils::normalizeCodeFenceLanguage(langLower).isEmpty()) {
            blocks[idx++] = match.captured(3);
        }
    }
    return blocks;
}

void EditorWidget::exportToPdf(const QString &filePath, const QPageLayout &layout)
{
    // 1. Prepare the HTML from current markdown content
    QString mdContent = toPlainText();
    QString processedContent = preparePreviewContent(mdContent);

    // 2. Read the preview template
    QFile tmplFile(QStringLiteral(":/preview/template.html"));
    if (!tmplFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit pdfExportCompleted(filePath, false);
        return;
    }
    QString tmpl = QString::fromUtf8(tmplFile.readAll());
    tmplFile.close();

    // 3. Inject content into template
    tmpl.replace(QStringLiteral("{{MARKDOWN_CONTENT}}"), processedContent);

    // 4. Override CSS variables to light/print-friendly theme (PDF needs white bg, dark text)
    tmpl.replace(QStringLiteral("{{PREVIEW_BG}}"),        QStringLiteral("#ffffff"));
    tmpl.replace(QStringLiteral("{{PREVIEW_FG}}"),        QStringLiteral("#1e1e1e"));
    tmpl.replace(QStringLiteral("{{PREVIEW_CODE_BG}}"),   QStringLiteral("#f5f5f5"));
    tmpl.replace(QStringLiteral("{{PREVIEW_BORDER}}"),    QStringLiteral("#dddddd"));
    tmpl.replace(QStringLiteral("{{PREVIEW_LINK}}"),      QStringLiteral("#1a73e8"));
    tmpl.replace(QStringLiteral("{{PREVIEW_HEADING}}"),   QStringLiteral("#000000"));
    tmpl.replace(QStringLiteral("{{PREVIEW_BLOCKQUOTE}}"),QStringLiteral("#666666"));
    tmpl.replace(QStringLiteral("{{PREVIEW_TH_BG}}"),     QStringLiteral("#f0f0f0"));
    tmpl.replace(QStringLiteral("{{PREVIEW_HR}}"),        QStringLiteral("#cccccc"));

    // 5. Create a hidden, off-screen WebEngine view for rendering
    QWebEngineView *pdfView = new QWebEngineView();
    pdfView->resize(1024, 768);
    QWebEnginePage *pdfPage = new QWebEnginePage(pdfView);
    pdfView->setPage(pdfPage);

    // 6. White background for print-friendly PDF
    pdfPage->setBackgroundColor(Qt::white);

    // 7. Load the page. When done, poll for Mermaid async completion, then print to PDF.
    connect(pdfPage, &QWebEnginePage::loadFinished, this,
        [this, pdfView, pdfPage, filePath, layout](bool ok) {
            if (!ok) {
                pdfView->deleteLater();
                emit pdfExportCompleted(filePath, false);
                return;
            }

            // 8. Wait for Mermaid async rendering (if any mermaid blocks exist)
            pdfPage->runJavaScript(
                QStringLiteral(
                    "new Promise(function(resolve) {"
                    "  var check = function() {"
                    "    var mermaidEls = document.querySelectorAll('.mermaid');"
                    "    var svgEls = document.querySelectorAll('.mermaid svg');"
                    "    if (mermaidEls.length === 0 || mermaidEls.length === svgEls.length) {"
                    "      resolve(true);"
                    "    } else {"
                    "      setTimeout(check, 100);"
                    "    }"
                    "  };"
                    "  check();"
                    "});"
                ),
                [this, pdfView, filePath, layout](const QVariant &) {
                    // 9. Mermaid done (or no mermaid blocks). Print to PDF.
                    pdfView->page()->printToPdf(
                        [this, pdfView, filePath](const QByteArray &pdfData) {
                            // 10. Write PDF to disk
                            QFile outFile(filePath);
                            bool ok = outFile.open(QIODevice::WriteOnly)
                                      && outFile.write(pdfData) == pdfData.size();
                            outFile.close();
                            emit pdfExportCompleted(filePath, ok);
                            // 11. Clean up
                            pdfView->deleteLater();
                        },
                        layout);
                });
        });

    // 12. Start loading the page
    pdfView->setHtml(tmpl, QUrl(QStringLiteral("qrc:/preview/")));
}

QString EditorWidget::injectHeadingAnchors(const QString &markdown)
{
    // Insert <a id="hl-LINENUM"></a> before each heading line so that
    // preview-mode navigation can scroll to it via JavaScript.
    QStringList lines = markdown.split(QLatin1Char('\n'));
    QStringList result;
    bool inCodeBlock = false;

    static const QRegularExpression headingRe(QStringLiteral("^(#{1,6})\\s+(.+)$"));

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];
        QString trimmed = line.trimmed();

        if (trimmed.startsWith(QStringLiteral("```"))) {
            inCodeBlock = !inCodeBlock;
        }

        if (!inCodeBlock) {
            QRegularExpressionMatch match = headingRe.match(line);
            if (match.hasMatch()) {
                result.append(QStringLiteral("<a id=\"hl-%1\"></a>").arg(i + 1));
            }
        }

        result.append(line);
    }

    return result.join(QLatin1Char('\n'));
}

void EditorWidget::updatePreviewContent(std::function<void()> onFinished)
{
    if (!m_previewView) {
        if (onFinished) onFinished();
        return;
    }
    QString safeContent = preparePreviewContent(m_textEdit->toPlainText());
    QString rawMarkdown = m_textEdit->toPlainText();

    QString base64 = QString::fromLatin1(safeContent.toUtf8().toBase64());
    // Capture diagnostics and code snapshots for stale-detection on refresh
    QMap<int, QList<SmdDiagnostic>> savedDiags = m_blockDiagnostics;
    QMap<int, QString> savedCode = m_blockDiagnosticCode;
    m_previewView->page()->runJavaScript(
        QStringLiteral("window.renderFromBase64('%1')").arg(base64),
        [this, onFinished, savedDiags, savedCode, rawMarkdown](const QVariant &) {
            if (!savedDiags.isEmpty()) {
                // Filter: only re-apply diagnostics for blocks whose code hasn't changed
                QMap<int, QString> currentCode = extractCodeBlockContents(rawMarkdown);
                QMap<int, QList<SmdDiagnostic>> validDiags;
                for (auto it = savedDiags.cbegin(); it != savedDiags.cend(); ++it) {
                    int idx = it.key();
                    if (savedCode.contains(idx) && currentCode.contains(idx)
                        && savedCode[idx] == currentCode[idx]) {
                        validDiags[idx] = it.value();
                    }
                }
                // Update stored diagnostics to remove stale entries
                m_blockDiagnostics = validDiags;
                // Keep only valid code snapshots
                QMap<int, QString> validCode;
                for (auto it = savedCode.cbegin(); it != savedCode.cend(); ++it) {
                    if (validDiags.contains(it.key()))
                        validCode[it.key()] = it.value();
                }
                m_blockDiagnosticCode = validCode;

                if (!validDiags.isEmpty()) {
                    QJsonObject root;
                    for (auto it = validDiags.cbegin(); it != validDiags.cend(); ++it) {
                        QJsonArray arr;
                        for (const auto &d : it.value()) {
                            QJsonObject dObj;
                            dObj[QStringLiteral("startLine")] = d.startLine;
                            dObj[QStringLiteral("endLine")]   = d.endLine;
                            dObj[QStringLiteral("message")]   = d.message;
                            dObj[QStringLiteral("severity")]  = d.severity;
                            arr.append(dObj);
                        }
                        root[QString::number(it.key())] = arr;
                    }
                    QString diagJson = QString::fromUtf8(
                        QJsonDocument(root).toJson(QJsonDocument::Compact));
                    m_previewView->page()->runJavaScript(
                        QStringLiteral("window.applyBlockDiagnostics(%1)").arg(diagJson));
                }
            }
            if (onFinished)
                onFinished();
        });
}

void EditorWidget::onTextChanged()
{
    // 分屏预览：启动 debounce 定时器，延迟刷新
    if (m_splitPreview) {
        m_splitDebounceTimer.start();
    }
}

void EditorWidget::updateModificationChanged()
{
    // Suppress intermediate modification signals during file loading.
    // QTextEdit::setPlainText() triggers textChanged before we can clear the
    // modified flag, which would briefly flash the unsaved indicator.
    if (m_loading)
        return;
    emit modificationChanged(isModified());
}

bool EditorWidget::loadFile(const QString &filePath)
{
    QString suffix = QFileInfo(filePath).suffix().toLower();

    // PDF 特殊处理：绕过 textExtension 检查，使用 WebEngine 渲染
    if (suffix == QStringLiteral("pdf")) {
        m_editorMode = PdfView;
                m_filePath = QFileInfo(filePath).absoluteFilePath();
        // 退出分屏和全屏预览
        if (m_splitPreview) {
            m_splitTextWrapper->layout()->removeWidget(m_textEdit);
            m_stackedWidget->insertWidget(0, m_textEdit);
            m_splitPreview = false;
            m_lastSplitPreviewContent.clear();
            destroySplitPreviewWidgets();
        }
        if (m_previewMode) {
            m_previewMode = false;
            destroyPreviewView();
        }
        m_stackedWidget->setCurrentIndex(m_stackedWidget->indexOf(m_pdfView));
        applyZoom();
        setModified(false);
        m_pdfDocument->load(filePath);
        emit fileLoaded(filePath);
        return true;
    }

    // SMD 文件特殊处理
    if (suffix == QStringLiteral("smd")) {
        m_editorMode = SmdEdit;
                m_filePath = QFileInfo(filePath).absoluteFilePath();
        if (m_splitPreview) {
            m_splitTextWrapper->layout()->removeWidget(m_textEdit);
            m_stackedWidget->insertWidget(0, m_textEdit);
            m_splitPreview = false;
            m_lastSplitPreviewContent.clear();
            destroySplitPreviewWidgets();
        }
        if (m_previewMode) {
            m_previewMode = false;
            destroyPreviewView();
        }
        m_stackedWidget->setCurrentWidget(m_smdEditor);
        m_smdEditor->loadFile(filePath);
        applyZoom();
        setModified(false);
        emit fileLoaded(filePath);
        return true;
    }

    if (!suffix.isEmpty() && !TextFileUtils::isTextExtension(suffix)) {
        QMessageBox::warning(this, tr("无法打开文件"),
                             tr("不支持的文件类型：.%1\n该文件可能是二进制格式。").arg(suffix));
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    QString content = stream.readAll();
    m_filePath = QFileInfo(filePath).absoluteFilePath();

    // 退出分屏预览（如果有），恢复 m_textEdit 到 page 0
    if (m_splitPreview) {
        m_splitTextWrapper->layout()->removeWidget(m_textEdit);
        m_stackedWidget->insertWidget(0, m_textEdit);
        m_splitPreview = false;
        m_lastSplitPreviewContent.clear();
    }
    // 退出全屏预览
    if (m_previewMode) {
        m_previewMode = false;
        destroyPreviewView();
    }

    // Auto-detect code file and switch mode BEFORE setting text,
    // so that setPlainText dispatches to the correct editor.
    QString lang = LanguageUtils::languageForExtension(suffix);
    if (!lang.isEmpty()) {
        m_editorMode = CodeEdit;
        m_codeEditor->setDocumentUri(QStringLiteral("file:///") + filePath);
        debugLog(QString("EditorWidget: CodeEdit mode, uri=file:///%1").arg(filePath));
        m_codeEditor->setLanguage(lang);
        m_stackedWidget->setCurrentWidget(m_codeEditor);
            } else {
        m_editorMode = MarkdownEdit;
        if (m_stackedWidget->currentIndex() != 0)
            m_stackedWidget->setCurrentWidget(m_textEdit);
            }

    m_loading = true;  // suppress modificationChanged during setPlainText
    setPlainText(content);
    m_originalContent = normalizeTrailingNewlines(toPlainText());
    setModified(false);
    m_loading = false;

    applyZoom();

    emit fileLoaded(filePath);

    // 文件加载后启动自动保存定时器
    startAutoSave();

    return true;
}

bool EditorWidget::saveFile()
{
    if (m_editorMode == PdfView)
        return false;
    if (m_editorMode == SmdEdit) {
        if (m_filePath.isEmpty())
            return false;
        return m_smdEditor->saveFile();
    }
    // 保存
    if (m_filePath.isEmpty())
        return false;
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "保存失败", "无法写入文件：" + m_filePath);
        return false;
    }
    QTextStream stream(&file);
    stream << toPlainText();
    file.close();
    m_originalContent = normalizeTrailingNewlines(toPlainText());
    setModified(false); // 保存后清除修改标志
    emit fileSaved(m_filePath);

    // 手动保存后清理恢复文件并重置 auto-save 定时器
    if (!m_recoveryTempPath.isEmpty()) {
        QFile::remove(m_recoveryTempPath);
        m_recoveryTempPath.clear();
    }
    startAutoSave();

    return true;
}

bool EditorWidget::saveAsFile(const QString &defaultDir)
{
    // 确定对话框起始目录，支持传入路径，否则用主文件夹
    QString startDir = defaultDir.isEmpty() ? QDir::homePath() : defaultDir;
    QFileDialog dialog(this, tr("另存为"), startDir);

    QStringList filters;
    filters << tr("Markdown文件 (*.md)")
            << tr("Smart Markdown文件 (*.smd)")
            << tr("文本文件 (*.txt)")
            << tr("所有文件 (*)");
    if (m_editorMode == SmdEdit)
        dialog.setDefaultSuffix("smd");
    else
        dialog.setDefaultSuffix("md");
    dialog.setNameFilters(filters);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    const QStringList selectedFiles = dialog.selectedFiles();
    if (selectedFiles.isEmpty())
        return false;
    QString newPath = selectedFiles.first();
    if (newPath.isEmpty())
        return false;

    // 若用户未输入后缀，根据选择的过滤器补充
    QFileInfo info(newPath);
    if (info.suffix().isEmpty()) {
        QString selectedFilter = dialog.selectedNameFilter();
        if (selectedFilter.contains("*.smd"))
            newPath += ".smd";
        else if (selectedFilter.contains("*.txt"))
            newPath += ".txt";
        else if (!selectedFilter.contains("(*)"))
            newPath += ".md";
    }

    newPath = QFileInfo(newPath).absoluteFilePath();
    QString oldPath = m_filePath;
    m_filePath = newPath;

    if (!saveFile()) {
        m_filePath = oldPath;
        return false;
    }

    // 若另存为 .smd 且当前不是 SmdEdit 模式，切换到 SMD 编辑器并显示内容
    if (QFileInfo(newPath).suffix().compare("smd", Qt::CaseInsensitive) == 0
        && m_editorMode != SmdEdit) {
        QString content = toPlainText();
        m_editorMode = SmdEdit;
        m_smdEditor->setFilePath(newPath);
        m_smdEditor->setPlainText(content);
        m_smdEditor->setModified(false);
        m_stackedWidget->setCurrentWidget(m_smdEditor);
        applyZoom();
        setModified(false);
    }

    return true;
}

QString EditorWidget::toPlainText() const
{
    if (m_editorMode == CodeEdit)
        return m_codeEditor->toPlainText();
    if (m_editorMode == PdfView)
        return {};
    if (m_editorMode == SmdEdit)
        return m_smdEditor->toPlainText();
    return m_textEdit->toPlainText();
}

QString EditorWidget::selectedText() const
{
    if (m_editorMode == CodeEdit)
        return m_codeEditor->textCursor().selectedText();
    if (m_editorMode == PdfView)
        return {};
    if (m_editorMode == SmdEdit) {
        int idx = m_smdEditor->activeCellIndex();
        if (idx >= 0) {
            SmdCell *cell = m_smdEditor->cellAt(idx);
            if (auto *edit = qobject_cast<QPlainTextEdit*>(cell->editorWidget()))
                return edit->textCursor().selectedText();
        }
        return {};
    }
    return m_textEdit->textCursor().selectedText();
}

void EditorWidget::setPlainText(const QString &text)
{
    if (m_editorMode == PdfView)
        return;
    if (m_editorMode == CodeEdit)
        m_codeEditor->setPlainText(text);
    else if (m_editorMode == SmdEdit)
        m_smdEditor->setPlainText(text);
    else
        m_textEdit->setPlainText(text);
    setModified(false);
}

bool EditorWidget::isModified() const
{
    if (m_editorMode == PdfView)
        return false;
    if (m_editorMode == SmdEdit)
        return m_smdEditor->isModified();
    if (m_editorMode == CodeEdit)
        return m_codeEditor->document()->isModified();
    return m_textEdit->document()->isModified();
}

void EditorWidget::setModified(bool modified)
{
    if (m_editorMode == PdfView)
        return;
    if (m_editorMode == SmdEdit) {
        m_smdEditor->setModified(modified);
        // SmdEditor's setModified already emits modificationChanged,
        // which is connected to EditorWidget's signal
        return;
    }
    QTextDocument *doc = (m_editorMode == CodeEdit)
        ? m_codeEditor->document() : m_textEdit->document();
    if (doc->isModified() != modified) {
        doc->setModified(modified);
        emit modificationChanged(modified);
    }
}

void EditorWidget::setEditorFont(const QString &family, int size)
{
    m_baseFontSize = size;

    QFont textFont(family, size);
    m_textEdit->setFont(textFont);

    QFont codeFont(family, size);
    codeFont.setStyleHint(QFont::Monospace);
    m_codeEditor->setFont(codeFont);
    m_codeEditor->refreshLineNumberArea();

    if (m_smdEditor)
        m_smdEditor->setEditorFont(family, size);

    applyZoom();
}

void EditorWidget::setCodeIndentWidth(int width)
{
    m_codeEditor->setIndentWidth(width);
}

void EditorWidget::setMarkdownIndentWidth(int width)
{
    m_textEdit->setIndentWidth(width);
}

void EditorWidget::setSplitPreviewDebounceMs(int ms)
{
    m_splitDebounceTimer.setInterval(ms);
}

void EditorWidget::applySplitPreviewRatio()
{
    if (!m_splitSplitter)
        return;
    int ratio = SettingsManager::instance().value("preview.split_preview_ratio",
                   ConfigManager::instance().previewSplitPreviewRatio()).toInt();
    QList<int> sizes = m_splitSplitter->sizes();
    if (sizes.size() == 2) {
        int total = sizes[0] + sizes[1];
        m_splitSplitter->setSizes({total * (100 - ratio) / 100, total * ratio / 100});
    } else {
        m_splitSplitter->setSizes({100 - ratio, ratio});
    }
}

void EditorWidget::applyBlockDiagnostics(const QMap<int, QList<SmdDiagnostic>> &diagByBlock)
{
    m_blockDiagnostics = diagByBlock;

    // Snapshot current code block contents for stale-diag detection on refresh
    QString md = m_textEdit ? m_textEdit->toPlainText() : QString();
    m_blockDiagnosticCode = extractCodeBlockContents(md);

    QJsonObject root;
    for (auto it = diagByBlock.cbegin(); it != diagByBlock.cend(); ++it) {
        QJsonArray arr;
        for (const auto &d : it.value()) {
            QJsonObject dObj;
            dObj[QStringLiteral("startLine")] = d.startLine;
            dObj[QStringLiteral("endLine")]   = d.endLine;
            dObj[QStringLiteral("message")]   = d.message;
            dObj[QStringLiteral("severity")]  = d.severity;
            arr.append(dObj);
        }
        root[QString::number(it.key())] = arr;
    }
    QString diagJson = QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Compact));

    const QString js = QStringLiteral("window.applyBlockDiagnostics(%1)").arg(diagJson);
    if (m_previewView && m_previewMode)
        m_previewView->page()->runJavaScript(js);
    if (m_splitPreviewView && m_splitPreview)
        m_splitPreviewView->page()->runJavaScript(js);
}

void EditorWidget::clearBlockDiagnostics()
{
    m_blockDiagnostics.clear();
    m_blockDiagnosticCode.clear();
    if (m_previewView && m_previewMode)
        m_previewView->page()->runJavaScript(QStringLiteral("window.clearBlockDiagnostics()"));
    if (m_splitPreviewView && m_splitPreview)
        m_splitPreviewView->page()->runJavaScript(QStringLiteral("window.clearBlockDiagnostics()"));
}

void EditorWidget::reloadEditorColors()
{
    m_codeEditor->reloadColors();
    if (m_smdEditor)
        m_smdEditor->reloadColors();

    auto &tm = ThemeManager::instance();
    m_textEdit->setStyleSheet(QString(
        "QTextEdit { background-color: %1; color: %2; selection-background-color: %3; }"
    ).arg(tm.color("editor.background").name(),
          tm.color("editor.foreground").name(),
          tm.color("editor.selectionBackground").name()));

    // Update preview container background
    if (m_previewContainer) {
        m_previewContainer->setStyleSheet(
            QString("background-color: %1;")
                .arg(tm.color("preview.containerBackground").name()));
    }

    // Update web engine background
    if (m_previewView) {
        m_previewView->page()->setBackgroundColor(
            tm.color("preview.webEngineBackground"));
    }

    // Update split preview background if active
    if (m_splitPreviewView) {
        m_splitPreviewView->page()->setBackgroundColor(
            tm.color("preview.webEngineBackground"));
    }

    // Push CSS variable updates to preview HTML via JavaScript
    QString js = previewThemeJs();
    if (m_previewReady && m_previewView->page()) {
        m_previewView->page()->runJavaScript(js);
    }
    if (m_splitPreviewReady && m_splitPreviewView) {
        m_splitPreviewView->page()->runJavaScript(js);
    }
}

void EditorWidget::applyZoom()
{
    if (m_editorMode == PdfView) {
        m_pdfView->setZoomFactor(m_zoomFactor);
        return;
    }
    if (m_editorMode == SmdEdit) {
        m_smdEditor->applyZoom(m_zoomFactor, m_baseFontSize);
        return;
    }

    bool wasModified = isModified();
    QTextDocument *doc = (m_editorMode == CodeEdit)
        ? m_codeEditor->document() : m_textEdit->document();
    QSignalBlocker blocker(doc);

    const auto &cfg = ConfigManager::instance();
    int pointSize = qBound(cfg.fontMinPointSize(),
                           qRound(m_baseFontSize * m_zoomFactor),
                           cfg.fontMaxPointSize());

    if (m_editorMode == CodeEdit) {
        QFont f = m_codeEditor->font();
        f.setPointSize(pointSize);
        m_codeEditor->setFont(f);
        m_codeEditor->refreshLineNumberArea();
    } else {
        QFont f = m_textEdit->font();
        f.setPointSize(pointSize);
        m_textEdit->setFont(f);

        QTextCursor cursor(m_textEdit->document());
        cursor.select(QTextCursor::Document);
        QTextCharFormat fmt;
        fmt.setFontPointSize(pointSize);
        cursor.mergeCharFormat(fmt);
    }

    if (m_previewMode && m_previewView) {
        m_previewView->setZoomFactor(m_zoomFactor);
    }
    if (m_splitPreview && m_splitPreviewView) {
        m_splitPreviewView->setZoomFactor(m_zoomFactor);
    }

    doc->setModified(wasModified);
}

void EditorWidget::zoomIn()
{
    setZoomFactor(m_zoomFactor + ConfigManager::instance().zoomStep());
}

void EditorWidget::zoomOut()
{
    setZoomFactor(m_zoomFactor - ConfigManager::instance().zoomStep());
}

void EditorWidget::zoomReset()
{
    setZoomFactor(ConfigManager::instance().zoomDefault());
}

void EditorWidget::setZoomFactor(qreal factor)
{
    const auto &cfg = ConfigManager::instance();
    factor = qBound(cfg.zoomMin(), factor, cfg.zoomMax());
    if (qFuzzyCompare(m_zoomFactor, factor))
        return;
    m_zoomFactor = factor;
    applyZoom();
    emit zoomFactorChanged(m_zoomFactor);
}

qreal EditorWidget::zoomFactor() const
{
    return m_zoomFactor;
}

int EditorWidget::cursorLine() const
{
    if (m_editorMode == SmdEdit)
        return m_smdEditor->activeCellCursorLine();
    if (m_editorMode == CodeEdit)
        return m_codeEditor->textCursor().blockNumber();
    return m_textEdit->textCursor().blockNumber();
}

int EditorWidget::cursorColumn() const
{
    if (m_editorMode == SmdEdit)
        return m_smdEditor->activeCellCursorColumn();
    if (m_editorMode == CodeEdit)
        return m_codeEditor->textCursor().positionInBlock();
    return m_textEdit->textCursor().positionInBlock();
}

void EditorWidget::setCursorPosition(int line, int column)
{
    if (m_editorMode == SmdEdit) {
        m_smdEditor->setActiveCellCursor(line, column);
        return;
    }
    QTextDocument *doc;
    QWidget *w;
    if (m_editorMode == CodeEdit) {
        doc = m_codeEditor->document();
        w = m_codeEditor;
    } else {
        doc = m_textEdit->document();
        w = m_textEdit;
    }
    QTextBlock block = doc->findBlockByLineNumber(line);
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                        qMin(column, block.length() - 1));
    if (auto *pte = qobject_cast<QPlainTextEdit*>(w)) {
        pte->setTextCursor(cursor);
        pte->ensureCursorVisible();
    } else if (auto *te = qobject_cast<QTextEdit*>(w)) {
        te->setTextCursor(cursor);
        te->ensureCursorVisible();
    }
}

// Normalize trailing newlines for robust modified detection.
// Trailing blank lines are a file-format artifact, not a meaningful edit.
void EditorWidget::setOriginalContent(const QString &diskContent)
{
    if (m_editorMode == SmdEdit) {
        // For SmdEdit, set original content so isModified compares against disk
        m_smdEditor->setModified(false); // syncs m_originalContent to current cells
        return;
    }
    m_originalContent = normalizeTrailingNewlines(diskContent);
    // The content check timer will naturally detect differences
    QTextDocument *doc = (m_editorMode == CodeEdit)
        ? m_codeEditor->document() : m_textEdit->document();
    bool nowModified = (normalizeTrailingNewlines(toPlainText()) != m_originalContent);
    if (doc->isModified() != nowModified) {
        doc->setModified(nowModified);
        emit modificationChanged(nowModified);
    }
}

bool EditorWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        QWheelEvent *wheel = static_cast<QWheelEvent*>(event);
        // 当按下 Ctrl 时，视为缩放操作
        if (wheel->modifiers() & (Qt::ControlModifier)) {
            if (wheel->angleDelta().y() > 0)
                zoomIn();
            else if (wheel->angleDelta().y() < 0)
                zoomOut();
            return true; // 阻止原事件，避免内部缩放
        }
    }
#if QT_CONFIG(gestures)
    else if (event->type() == QEvent::NativeGesture) {
        QNativeGestureEvent *gesture = static_cast<QNativeGestureEvent*>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture) {
            if (gesture->value() > 0)
                zoomIn();
            else if (gesture->value() < 0)
                zoomOut();
            return true;
        }
    }
#endif
    return QWidget::eventFilter(obj, event);
}

void EditorWidget::onContentCheckTimeout()
{
    if (m_editorMode == SmdEdit) {
        setModified(m_smdEditor->isModified());
        return;
    }
    // 检查文件内容是否被修改
    bool contentChanged = (normalizeTrailingNewlines(toPlainText()) != m_originalContent);
    if (isModified() != contentChanged) {
        setModified(contentChanged);
    }
}

void EditorWidget::setFilePath(const QString &newPath) {
    QString normalized = QFileInfo(newPath).absoluteFilePath();
    if (m_filePath == normalized) return;
    QString oldPath = m_filePath;
    m_filePath = normalized;
    m_originalContent = normalizeTrailingNewlines(toPlainText());

    // Re-evaluate language on path change (handles save-as extension changes)
    QString ext = QFileInfo(newPath).suffix().toLower();
    if (ext == QStringLiteral("smd")) {
        m_editorMode = SmdEdit;
                m_smdEditor->setFilePath(normalized);
        m_stackedWidget->setCurrentWidget(m_smdEditor);
        applyZoom();
    } else {
        QString lang = LanguageUtils::languageForExtension(ext);
        if (!lang.isEmpty()) {
            m_editorMode = CodeEdit;
                        m_codeEditor->setDocumentUri(QStringLiteral("file:///") + normalized);
            m_codeEditor->setLanguage(lang);
            m_stackedWidget->setCurrentWidget(m_codeEditor);
            applyZoom();
        } else {
            m_editorMode = MarkdownEdit;
                        if (m_stackedWidget->currentIndex() != 0)
                m_stackedWidget->setCurrentWidget(m_textEdit);
        }
    }

    emit filePathChanged(oldPath, normalized);
    emit modificationChanged(isModified());
}

void EditorWidget::setFileNames(const QStringList &names)
{
    if (m_editorMode == PdfView || m_editorMode == SmdEdit)
        return;
    if (m_editorMode != CodeEdit)
        m_textEdit->setFileNames(names);
}

void EditorWidget::setTagNames(const QStringList &names)
{
    if (m_editorMode == PdfView || m_editorMode == SmdEdit)
        return;
    if (m_editorMode != CodeEdit)
        m_textEdit->setTagNames(names);
}

void EditorWidget::scrollToLine(int lineNumber, const QString &highlightText)
{
    if (m_editorMode == PdfView || m_editorMode == SmdEdit)
        return;

    if (m_previewMode && !m_splitPreview) {
        setPreviewMode(false);
    }

    if (m_editorMode == CodeEdit) {
        QTextBlock block = m_codeEditor->document()->findBlockByLineNumber(lineNumber - 1);
        if (!block.isValid())
            return;
        QTextCursor cursor(block);
        m_codeEditor->setTextCursor(cursor);
        m_codeEditor->ensureCursorVisible();

        if (!highlightText.isEmpty())
            m_codeEditor->setSearchHighlights(highlightText);
        return;
    }

    QTextBlock block = m_textEdit->document()->findBlockByLineNumber(lineNumber - 1);
    if (!block.isValid())
        return;

    QTextCursor cursor(block);
    m_textEdit->setTextCursor(cursor);
    m_textEdit->ensureCursorVisible();

    if (!highlightText.isEmpty()) {
        QList<QTextEdit::ExtraSelection> selections;
        QTextCursor searchCursor(m_textEdit->document());

        while (true) {
            QTextCursor found = m_textEdit->document()->find(
                highlightText, searchCursor);
            if (found.isNull())
                break;

            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(ThemeManager::instance().color("search.highlightBackground"));
            sel.format.setForeground(ThemeManager::instance().color("search.highlightForeground"));
            sel.cursor = found;
            selections.append(sel);

            searchCursor = found;
            searchCursor.movePosition(QTextCursor::EndOfWord);
        }

        m_textEdit->setExtraSelections(selections);
    }
}

void EditorWidget::navigateToLine(int lineNumber)
{
    if (m_editorMode == PdfView || m_editorMode == CodeEdit || m_editorMode == SmdEdit) {
        if (m_editorMode == CodeEdit)
            navigateEditorToLine(lineNumber);
        return;
    }

    // Preview or split-preview: scroll the rendered WebEngine view to the heading anchor
    bool hasPreview = m_previewMode || m_splitPreview;
    if (hasPreview) {
        QWebEngineView *view = m_splitPreview ? m_splitPreviewView : m_previewView;
        if (!view) return;
        QString js = QStringLiteral(
            "var el = document.getElementById('hl-%1');"
            "if (el) {"
            "  el.scrollIntoView({behavior: 'smooth', block: 'center'});"
            "  var oldBg = el.style.backgroundColor;"
            "  el.style.backgroundColor = '#FFD700';"
            "  el.style.transition = 'background-color 1.5s ease';"
            "  setTimeout(function() {"
            "    el.style.backgroundColor = oldBg || 'transparent';"
            "  }, 1500);"
            "}"
        ).arg(lineNumber);
        view->page()->runJavaScript(js);
    }

    // Edit mode or split-preview: also highlight in the editor pane
    if (!m_previewMode) {
        navigateEditorToLine(lineNumber);
    }
}

void EditorWidget::navigateEditorToLine(int lineNumber)
{
    if (m_editorMode == PdfView)
        return;

    // Scroll to line and highlight the entire line in yellow (search-highlight style).
    // Unlike scrollToLine(,highlightText), this does NOT search the full document
    // for matching text — it precisely highlights only the target line.

    QTextBlock block;
    if (m_editorMode == CodeEdit)
        block = m_codeEditor->document()->findBlockByLineNumber(lineNumber - 1);
    else
        block = m_textEdit->document()->findBlockByLineNumber(lineNumber - 1);
    if (!block.isValid())
        return;

    QTextCursor cursor(block);
    if (m_editorMode == CodeEdit) {
        m_codeEditor->setTextCursor(cursor);
        m_codeEditor->ensureCursorVisible();
    } else {
        m_textEdit->setTextCursor(cursor);
        m_textEdit->ensureCursorVisible();
    }

    QTextEdit::ExtraSelection sel;
    sel.format.setBackground(ThemeManager::instance().color("search.highlightBackground"));
    sel.format.setForeground(ThemeManager::instance().color("search.highlightForeground"));
    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
    sel.cursor = cursor;
    sel.cursor.clearSelection();

    if (m_editorMode == CodeEdit)
        m_codeEditor->setExtraSelections({sel});
    else
        m_textEdit->setExtraSelections({sel});
}

void EditorWidget::clearExtraSelections()
{
    if (m_editorMode == PdfView || m_editorMode == SmdEdit)
        return;
    if (m_editorMode == CodeEdit)
        m_codeEditor->clearSearchHighlights();
    else
        m_textEdit->setExtraSelections({});
}

// ---- 分屏预览 ----

void EditorWidget::createSplitPreviewWidgets()
{
    if (m_splitSplitter) return;

    m_splitSplitter = new QSplitter(Qt::Horizontal, this);

    // 左侧：包装 m_textEdit 的容器
    m_splitTextWrapper = new QWidget(this);
    QVBoxLayout *wrapperLayout = new QVBoxLayout(m_splitTextWrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    m_splitSplitter->addWidget(m_splitTextWrapper);

    // 右侧：第二个 QWebEngineView
    m_splitPreviewView = new QWebEngineView(this);
    m_splitPreviewPage = new PreviewPage(this);
    m_splitPreviewPage->onWikiLinkClicked = [this](const QString &fileName) {
        emit wikiLinkClicked(fileName);
    };
    m_splitPreviewPage->onRunCodeBlock = [this](const QString &language, const QString &code, int blockIndex) {
        emit runCodeBlockRequested(language, code, blockIndex);
    };
    m_splitPreviewPage->onTagClicked = [this](const QString &tag) {
        emit tagClicked(tag);
    };
    m_splitPreviewView->setPage(m_splitPreviewPage);

    m_splitPreviewView->installEventFilter(this);
    QTimer::singleShot(0, this, [this]() {
        if (m_splitPreviewView) {
            if (QWidget *fp = m_splitPreviewView->focusProxy())
                fp->installEventFilter(this);
        }
    });

    m_splitSplitter->addWidget(m_splitPreviewView);

    int ratio = SettingsManager::instance().value("preview.split_preview_ratio",
                   ConfigManager::instance().previewSplitPreviewRatio()).toInt();
    m_splitSplitter->setSizes({100 - ratio, ratio});

    m_stackedWidget->addWidget(m_splitSplitter);
}

void EditorWidget::setSplitPreviewMode(bool split)
{
    if (m_editorMode == CodeEdit || m_editorMode == PdfView || m_editorMode == SmdEdit)
        return;
    if (split == m_splitPreview)
        return;

    m_splitPreview = split;

    if (m_splitPreview) {
        // 禁用绘制，避免切换过程中间状态的闪烁
        m_stackedWidget->setUpdatesEnabled(false);

        // 互斥：直接清除标志位，不调用 setPreviewMode(false)
        // 以跳过其内部的 setCurrentIndex(0)，避免中间状态绘制
        if (m_previewMode) {
            m_previewMode = false;
        }

        createSplitPreviewWidgets();

        // 先将当前widget切到m_splitSplitter，再转移m_textEdit。
        // 否则removeWidget(m_textEdit)会触发Qt自动选中m_previewContainer
        // （全屏预览的原生WebEngine窗口），导致闪烁。
        m_stackedWidget->setCurrentWidget(m_splitSplitter);

        // 将 m_textEdit 从 page 0 转移到 splitter 左侧
        m_stackedWidget->removeWidget(m_textEdit);
        m_splitTextWrapper->layout()->addWidget(m_textEdit);
        m_textEdit->show();

        if (!m_splitPreviewReady) {
            // 关键修复：确保分屏预览的 QWebEngineView 在 setHtml 前有正确的尺寸
            // 与全屏预览相同的问题：QStackedWidget 不会为隐藏页面更新子控件尺寸
            QSize stackedSize = m_stackedWidget->size();
            int ratio = SettingsManager::instance().value("preview.split_preview_ratio",
                           ConfigManager::instance().previewSplitPreviewRatio()).toInt();
            int previewWidth = qMax(stackedSize.width() * ratio / 100, 100);
            QSize splitViewSize(previewWidth, stackedSize.height());
            m_splitPreviewView->setFixedSize(splitViewSize);
            m_splitPreviewView->setAttribute(Qt::WA_NoSystemBackground, true);
            m_splitPreviewView->setAttribute(Qt::WA_NativeWindow, true);
            m_splitPreviewView->winId();

            m_splitPreviewView->page()->setBackgroundColor(
                ThemeManager::instance().color("preview.webEngineBackground"));

            QFile tmplFile(QStringLiteral(":/preview/template.html"));
            if (tmplFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString tmpl = QString::fromUtf8(tmplFile.readAll());
                tmplFile.close();

                QString safeContent = preparePreviewContent(m_textEdit->toPlainText());
                tmpl.replace(QStringLiteral("{{MARKDOWN_CONTENT}}"), safeContent);
                applyPreviewTheme(tmpl);
                m_splitPreviewView->setHtml(tmpl, QUrl(QStringLiteral("qrc:/preview/")));

                connect(m_splitPreviewView->page(), &QWebEnginePage::loadFinished, this,
                    [this](bool ok) {
                        if (!m_splitPreviewView) return;
                        auto *page = m_splitPreviewView->page();
                        disconnect(page, &QWebEnginePage::loadFinished, this, nullptr);
                        if (!ok) return;
                        m_splitPreviewReady = true;

                        // 解除 fixedSize，恢复布局管理
                        m_splitPreviewView->setMinimumSize(0, 0);
                        m_splitPreviewView->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
                        if (QWidget *fp = m_splitPreviewView->focusProxy())
                            fp->installEventFilter(this);
                        m_lastSplitPreviewContent = m_textEdit->toPlainText();
                        applyZoom();
                    });
            }
        } else {
            updateSplitPreviewContentNow();
            applyZoom();
        }

        m_stackedWidget->setCurrentWidget(m_splitSplitter);
        m_stackedWidget->setUpdatesEnabled(true);
    } else {
        // 退出分屏：将 m_textEdit 从 splitter 放回 page 0
        m_splitTextWrapper->layout()->removeWidget(m_textEdit);
        m_stackedWidget->insertWidget(0, m_textEdit);
        m_stackedWidget->setCurrentWidget(m_textEdit);
        destroySplitPreviewWidgets();
        applyZoom();
    }
}

void EditorWidget::onSplitDebounceTimeout()
{
    if (!m_splitPreview || !m_splitPreviewReady) return;

    QString currentContent = m_textEdit->toPlainText();
    if (currentContent == m_lastSplitPreviewContent) return;

    m_lastSplitPreviewContent = currentContent;
    updateSplitPreviewContentNow();
}

void EditorWidget::updateSplitPreviewContentNow()
{
    if (!m_splitPreviewView || !m_splitPreviewReady) return;

    QString safeContent = preparePreviewContent(m_textEdit->toPlainText());
    QString rawMarkdown = m_textEdit->toPlainText();
    QString base64 = QString::fromLatin1(safeContent.toUtf8().toBase64());
    QMap<int, QList<SmdDiagnostic>> savedDiags = m_blockDiagnostics;
    QMap<int, QString> savedCode = m_blockDiagnosticCode;
    m_splitPreviewView->page()->runJavaScript(
        QStringLiteral("window.renderFromBase64('%1')").arg(base64),
        [this, savedDiags, savedCode, rawMarkdown](const QVariant &) {
            if (!savedDiags.isEmpty()) {
                // Filter: only re-apply diagnostics for blocks whose code hasn't changed
                QMap<int, QString> currentCode = extractCodeBlockContents(rawMarkdown);
                QMap<int, QList<SmdDiagnostic>> validDiags;
                for (auto it = savedDiags.cbegin(); it != savedDiags.cend(); ++it) {
                    int idx = it.key();
                    if (savedCode.contains(idx) && currentCode.contains(idx)
                        && savedCode[idx] == currentCode[idx]) {
                        validDiags[idx] = it.value();
                    }
                }
                m_blockDiagnostics = validDiags;
                QMap<int, QString> validCode;
                for (auto it = savedCode.cbegin(); it != savedCode.cend(); ++it) {
                    if (validDiags.contains(it.key()))
                        validCode[it.key()] = it.value();
                }
                m_blockDiagnosticCode = validCode;

                if (!validDiags.isEmpty()) {
                    QJsonObject root;
                    for (auto it = validDiags.cbegin(); it != validDiags.cend(); ++it) {
                        QJsonArray arr;
                        for (const auto &d : it.value()) {
                            QJsonObject dObj;
                            dObj[QStringLiteral("startLine")] = d.startLine;
                            dObj[QStringLiteral("endLine")]   = d.endLine;
                            dObj[QStringLiteral("message")]   = d.message;
                            dObj[QStringLiteral("severity")]  = d.severity;
                            arr.append(dObj);
                        }
                        root[QString::number(it.key())] = arr;
                    }
                    QString diagJson = QString::fromUtf8(
                        QJsonDocument(root).toJson(QJsonDocument::Compact));
                    m_splitPreviewView->page()->runJavaScript(
                        QStringLiteral("window.applyBlockDiagnostics(%1)").arg(diagJson));
                }
            }
        });
}

// ==================================================================
// Auto-save
// ==================================================================

EditorWidget::~EditorWidget()
{
    stopAutoSave();
}

void EditorWidget::startAutoSave()
{
    if (m_editorMode == PdfView)
        return;
    if (m_autoSaveEnabled) {
        m_autoSaveTimer.start();
    }
}

void EditorWidget::stopAutoSave()
{
    if (m_editorMode == PdfView)
        return;
    m_autoSaveTimer.stop();
}

void EditorWidget::setAutoSaveEnabled(bool enabled)
{
    m_autoSaveEnabled = enabled;
    if (enabled)
        m_autoSaveTimer.start();
    else
        m_autoSaveTimer.stop();
}

void EditorWidget::onAutoSaveTimeout()
{
    if (!isModified())
        return;
    autoSaveNow();
}

void EditorWidget::autoSaveNow()
{
    if (!isModified())
        return;

    if (!m_filePath.isEmpty()) {
        // 有路径的文件：直接写入原文件，不清除修改标记
        QFile file(m_filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << toPlainText();
            file.close();
        }
    } else {
        // 未命名文件：保存到恢复目录
        if (m_recoveryTempPath.isEmpty()) {
            QString recoveryDir = autoSaveRecoveryDir();
            QDir().mkpath(recoveryDir);
            m_recoveryTempPath = recoveryDir + "/untitled_"
                                 + QUuid::createUuid().toString(QUuid::Id128)
                                 + ".md";
        }
        QFile file(m_recoveryTempPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << toPlainText();
            file.close();
        }
    }
}

QString EditorWidget::autoSaveRecoveryDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + "/SM-Recovery";
}
