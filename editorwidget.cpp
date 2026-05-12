#include "editorwidget.h"
#include "wikilinktextedit.h"
#include "codeeditor.h"
#include "tagindex.h"
#include "languageutils.h"
#include "fileutils.h"
#include "configmanager.h"
#include <QFile>
#include <QTextStream>
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
#include <QJsonObject>
#include <QColor>

// Forward declaration for highlightWithRules used in highlightCodeBlock
static QString highlightWithRules(
    const QString &code,
    const QVector<QPair<QRegularExpression, QPair<QColor,bool>>> &rules);

// 自定义 Web 页面：拦截 wikilink 和 runblock 导航
class PreviewPage : public QWebEnginePage {
public:
    std::function<void(const QString &)> onWikiLinkClicked;
    std::function<void(const QString &, const QString &)> onRunCodeBlock;
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
                            onRunCodeBlock(lang, code);
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

    m_previewView = new QWebEngineView(this);
    PreviewPage *previewPage = new PreviewPage(this);
    previewPage->onWikiLinkClicked = [this](const QString &fileName) {
        emit wikiLinkClicked(fileName);
    };
    previewPage->onRunCodeBlock = [this](const QString &language, const QString &code) {
        emit runCodeBlockRequested(language, code);
    };
    previewPage->onTagClicked = [this](const QString &tag) {
        emit tagClicked(tag);
    };
    m_previewView->setPage(previewPage);

    m_textEdit->viewport()->installEventFilter(this);
    m_previewView->installEventFilter(this);
    QTimer::singleShot(0, this, [this]() {
        if (QWidget *fp = m_previewView->focusProxy())
            fp->installEventFilter(this);
    });

    m_codeEditor = new CodeEditor(this);
    m_codeEditor->viewport()->installEventFilter(this);

    // 暗色遮罩容器：在 WebEngine 渲染完成前遮挡白底
    m_previewContainer = new QWidget(this);
    m_previewContainer->setStyleSheet(
        QString("background-color: %1;")
            .arg(ConfigManager::instance().previewContainerBackground().name()));
    QVBoxLayout *containerLayout = new QVBoxLayout(m_previewContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->addWidget(m_previewView);

    // 堆叠布局：索引0=编辑，索引1=预览容器
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->addWidget(m_textEdit);
    m_stackedWidget->addWidget(m_previewContainer);
    m_stackedWidget->addWidget(m_codeEditor);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stackedWidget);
    setLayout(layout);

    // 获取基础字体大小
    QFont baseFont = m_textEdit->font();
    m_baseFontSize = baseFont.pointSize();

    // 同步代码编辑器字体大小
    QFont codeFont = m_codeEditor->font();
    codeFont.setPointSize(m_baseFontSize);
    m_codeEditor->setFont(codeFont);

    // 当编辑区内容改变时，更新修改标志并刷新预览
    connect(m_textEdit, &QTextEdit::textChanged, this, &EditorWidget::onTextChanged);
    connect(m_codeEditor, &QPlainTextEdit::textChanged, this, &EditorWidget::onTextChanged);
    // 当编辑区修改状态变化时发出信号
    connect(m_textEdit, &QTextEdit::textChanged, this, &EditorWidget::updateModificationChanged);
    connect(m_codeEditor, &QPlainTextEdit::textChanged, this, &EditorWidget::updateModificationChanged);

    setPreviewMode(false); // 默认编辑模式

    m_contentCheckTimer.setSingleShot(true);
    m_contentCheckTimer.setInterval(ConfigManager::instance().editorContentCheckTimerMs());
    connect(&m_contentCheckTimer, &QTimer::timeout, this, &EditorWidget::onContentCheckTimeout);

    // 分屏预览 debounce 定时器
    m_splitDebounceTimer.setSingleShot(true);
    m_splitDebounceTimer.setInterval(ConfigManager::instance().previewSplitDebounceMs());
    connect(&m_splitDebounceTimer, &QTimer::timeout, this, &EditorWidget::onSplitDebounceTimeout);

    // 当文本编辑器内容变化时，重置计时器
    connect(m_textEdit, &QTextEdit::textChanged, this, [this]() {
        m_contentCheckTimer.start();
    });
    connect(m_codeEditor, &QPlainTextEdit::textChanged, this, [this]() {
        m_contentCheckTimer.start();
    });
    m_originalContent = toPlainText(); // 记录当前内容，用于内容比较
}

void EditorWidget::setPreviewMode(bool preview)
{
    if (m_editorMode == CodeEdit)
        return;
    if (preview == m_previewMode)
        return;
    // 互斥：进入全屏预览时，退出分屏预览
    if (preview && m_splitPreview) {
        setSplitPreviewMode(false);
    }
    m_previewMode = preview;

    if (m_previewMode) {
        if (!m_previewReady) {
            // 首次预览：加载完整模板（setHtml），延迟到 loadFinished 再切换
            m_previewView->page()->setBackgroundColor(
                ConfigManager::instance().previewWebEngineBackground());

            QFile tmplFile(QStringLiteral(":/preview/template.html"));
            if (!tmplFile.open(QIODevice::ReadOnly | QIODevice::Text))
                return;
            QString tmpl = QString::fromUtf8(tmplFile.readAll());
            tmplFile.close();

            QString safeContent = preparePreviewContent(m_textEdit->toPlainText());
            tmpl.replace(QStringLiteral("{{MARKDOWN_CONTENT}}"), safeContent);
            m_previewView->setHtml(tmpl, QUrl(QStringLiteral("qrc:/preview/")));

            connect(m_previewView->page(), &QWebEnginePage::loadFinished, this,
                [this](bool ok) {
                    disconnect(m_previewView->page(), &QWebEnginePage::loadFinished, this, nullptr);
                    if (!ok) return;
                    m_previewReady = true;
                    m_stackedWidget->setCurrentIndex(1);
                    // WebEngine 内部的 focus proxy 在页面加载后才确定，
                    // 需要在其上安装事件过滤器才能捕获预览区的 Ctrl+滚轮缩放
                    if (QWidget *fp = m_previewView->focusProxy())
                        fp->installEventFilter(this);
                    applyZoom();
                });
        } else {
            updatePreviewContent([this]() {
                m_stackedWidget->setCurrentIndex(1);
                applyZoom();
            });
        }
    } else {
        m_stackedWidget->setCurrentIndex(0);
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

    // --- Helper: wrap a token in a colored span ---
    auto span = [&](const QString &text, const QColor &color, bool bold = false) {
        if (color.isValid() && color != QColor(0, 0, 0)) {
            QString style = QStringLiteral("color:%1;").arg(color.name());
            if (bold) style += QStringLiteral("font-weight:bold;");
            return QStringLiteral("<span style=\"%1\">%2</span>").arg(style, text.toHtmlEscaped());
        }
        return text.toHtmlEscaped();
    };

    struct MultiLineSpan { QString placeholder; QString raw; };

    if (langId == QStringLiteral("cpp")) {
        const QColor preprocColor = cfg.syntaxPreprocessor(); // #C586C0

        // C++ keyword list (from CppSyntaxHighlighter)
        const QStringList keywords = {
            QStringLiteral("alignas"), QStringLiteral("alignof"), QStringLiteral("and"),
            QStringLiteral("and_eq"), QStringLiteral("asm"), QStringLiteral("auto"),
            QStringLiteral("bitand"), QStringLiteral("bitor"), QStringLiteral("break"),
            QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"),
            QStringLiteral("compl"), QStringLiteral("concept"), QStringLiteral("const"),
            QStringLiteral("consteval"), QStringLiteral("constexpr"), QStringLiteral("constinit"),
            QStringLiteral("const_cast"), QStringLiteral("continue"), QStringLiteral("co_await"),
            QStringLiteral("co_return"), QStringLiteral("co_yield"), QStringLiteral("decltype"),
            QStringLiteral("default"), QStringLiteral("delete"), QStringLiteral("do"),
            QStringLiteral("dynamic_cast"), QStringLiteral("else"), QStringLiteral("enum"),
            QStringLiteral("explicit"), QStringLiteral("export"), QStringLiteral("extern"),
            QStringLiteral("false"), QStringLiteral("final"), QStringLiteral("for"),
            QStringLiteral("friend"), QStringLiteral("goto"), QStringLiteral("if"),
            QStringLiteral("inline"), QStringLiteral("mutable"), QStringLiteral("namespace"),
            QStringLiteral("new"), QStringLiteral("noexcept"), QStringLiteral("not"),
            QStringLiteral("not_eq"), QStringLiteral("nullptr"), QStringLiteral("operator"),
            QStringLiteral("or"), QStringLiteral("or_eq"), QStringLiteral("override"),
            QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("public"),
            QStringLiteral("register"), QStringLiteral("reinterpret_cast"), QStringLiteral("requires"),
            QStringLiteral("return"), QStringLiteral("signed"), QStringLiteral("sizeof"),
            QStringLiteral("static"), QStringLiteral("static_assert"), QStringLiteral("static_cast"),
            QStringLiteral("struct"), QStringLiteral("switch"), QStringLiteral("template"),
            QStringLiteral("this"), QStringLiteral("thread_local"), QStringLiteral("throw"),
            QStringLiteral("true"), QStringLiteral("try"), QStringLiteral("typedef"),
            QStringLiteral("typeid"), QStringLiteral("typename"), QStringLiteral("union"),
            QStringLiteral("unsigned"), QStringLiteral("using"), QStringLiteral("virtual"),
            QStringLiteral("void"), QStringLiteral("volatile"), QStringLiteral("while"),
            QStringLiteral("xor"), QStringLiteral("xor_eq")
        };
        // Build keyword regex
        QString kwPattern;
        for (int i = 0; i < keywords.size(); ++i) {
            if (i > 0) kwPattern += QChar(L'|');
            kwPattern += QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(keywords[i]));
        }

        // Type list (from CppSyntaxHighlighter)
        const QStringList types = {
            QStringLiteral("bool"), QStringLiteral("char"), QStringLiteral("char16_t"),
            QStringLiteral("char32_t"), QStringLiteral("char8_t"), QStringLiteral("double"),
            QStringLiteral("float"), QStringLiteral("int"), QStringLiteral("long"),
            QStringLiteral("short"), QStringLiteral("size_t"), QStringLiteral("ssize_t"),
            QStringLiteral("ptrdiff_t"), QStringLiteral("int8_t"), QStringLiteral("int16_t"),
            QStringLiteral("int32_t"), QStringLiteral("int64_t"), QStringLiteral("uint8_t"),
            QStringLiteral("uint16_t"), QStringLiteral("uint32_t"), QStringLiteral("uint64_t"),
            QStringLiteral("wchar_t"), QStringLiteral("std"), QStringLiteral("string"),
            QStringLiteral("wstring"), QStringLiteral("u16string"), QStringLiteral("u32string"),
            QStringLiteral("vector"), QStringLiteral("map"), QStringLiteral("set"),
            QStringLiteral("list"), QStringLiteral("deque"), QStringLiteral("queue"),
            QStringLiteral("stack"), QStringLiteral("array"), QStringLiteral("tuple"),
            QStringLiteral("pair"), QStringLiteral("optional"), QStringLiteral("variant"),
            QStringLiteral("unique_ptr"), QStringLiteral("shared_ptr"), QStringLiteral("weak_ptr"),
            QStringLiteral("function"), QStringLiteral("string_view"), QStringLiteral("span"),
            QStringLiteral("initializer_list"), QStringLiteral("mutex"), QStringLiteral("lock_guard"),
            QStringLiteral("unique_lock"), QStringLiteral("shared_lock"), QStringLiteral("condition_variable"),
            QStringLiteral("promise"), QStringLiteral("future"), QStringLiteral("atomic"),
            QStringLiteral("thread"), QStringLiteral("jthread"), QStringLiteral("filesystem"),
            QStringLiteral("path"), QStringLiteral("error_code"), QStringLiteral("error_category"),
            QStringLiteral("istream"), QStringLiteral("ostream"), QStringLiteral("iostream"),
            QStringLiteral("fstream"), QStringLiteral("sstream"), QStringLiteral("stringstream"),
            QStringLiteral("ifstream"), QStringLiteral("ofstream"), QStringLiteral("QString"),
            QStringLiteral("QWidget"), QStringLiteral("QObject"), QStringLiteral("QVariant"),
            QStringLiteral("QList"), QStringLiteral("QVector"), QStringLiteral("QMap"),
            QStringLiteral("QSet"), QStringLiteral("QHash"), QStringLiteral("QPair"),
            QStringLiteral("QSharedPointer"), QStringLiteral("QScopedPointer")
        };
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

        // Python keyword list (from PythonSyntaxHighlighter)
        const QStringList pyKeywords = {
            QStringLiteral("def"), QStringLiteral("class"), QStringLiteral("if"),
            QStringLiteral("elif"), QStringLiteral("else"), QStringLiteral("for"),
            QStringLiteral("while"), QStringLiteral("import"), QStringLiteral("from"),
            QStringLiteral("as"), QStringLiteral("return"), QStringLiteral("yield"),
            QStringLiteral("try"), QStringLiteral("except"), QStringLiteral("finally"),
            QStringLiteral("raise"), QStringLiteral("with"), QStringLiteral("pass"),
            QStringLiteral("break"), QStringLiteral("continue"), QStringLiteral("lambda"),
            QStringLiteral("del"), QStringLiteral("global"), QStringLiteral("nonlocal"),
            QStringLiteral("assert"), QStringLiteral("async"), QStringLiteral("await"),
            QStringLiteral("match"), QStringLiteral("case"), QStringLiteral("and"),
            QStringLiteral("or"), QStringLiteral("not"), QStringLiteral("is"),
            QStringLiteral("in"), QStringLiteral("True"), QStringLiteral("False"), QStringLiteral("None")
        };
        QString pyKwPattern;
        for (int i = 0; i < pyKeywords.size(); ++i) {
            if (i > 0) pyKwPattern += QChar(L'|');
            pyKwPattern += QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(pyKeywords[i]));
        }

        // Builtins
        const QStringList builtins = {
            QStringLiteral("int"), QStringLiteral("float"), QStringLiteral("str"),
            QStringLiteral("list"), QStringLiteral("dict"), QStringLiteral("tuple"),
            QStringLiteral("set"), QStringLiteral("bool"), QStringLiteral("bytes"),
            QStringLiteral("bytearray"), QStringLiteral("complex"), QStringLiteral("frozenset"),
            QStringLiteral("range"), QStringLiteral("slice"), QStringLiteral("type"),
            QStringLiteral("super"), QStringLiteral("object"), QStringLiteral("property"),
            QStringLiteral("staticmethod"), QStringLiteral("classmethod"),
            QStringLiteral("enumerate"), QStringLiteral("zip"), QStringLiteral("map"),
            QStringLiteral("filter"), QStringLiteral("len"), QStringLiteral("print"),
            QStringLiteral("open"), QStringLiteral("isinstance"), QStringLiteral("hasattr"),
            QStringLiteral("getattr"), QStringLiteral("setattr"), QStringLiteral("sorted"),
            QStringLiteral("reversed"), QStringLiteral("iter"), QStringLiteral("next"),
            QStringLiteral("any"), QStringLiteral("all"), QStringLiteral("sum"),
            QStringLiteral("min"), QStringLiteral("max"), QStringLiteral("abs"),
            QStringLiteral("round"), QStringLiteral("ord"), QStringLiteral("chr"),
            QStringLiteral("repr"), QStringLiteral("input"), QStringLiteral("format"),
            QStringLiteral("id"), QStringLiteral("dir"), QStringLiteral("vars"),
            QStringLiteral("callable"), QStringLiteral("issubclass"), QStringLiteral("eval"),
            QStringLiteral("exec"), QStringLiteral("compile"), QStringLiteral("locals"),
            QStringLiteral("globals"), QStringLiteral("hash"),
            QStringLiteral("ValueError"), QStringLiteral("TypeError"),
            QStringLiteral("KeyError"), QStringLiteral("IndexError"),
            QStringLiteral("AttributeError"), QStringLiteral("ImportError"),
            QStringLiteral("ModuleNotFoundError"), QStringLiteral("NameError"),
            QStringLiteral("FileNotFoundError"), QStringLiteral("ZeroDivisionError"),
            QStringLiteral("StopIteration"), QStringLiteral("RuntimeError"),
            QStringLiteral("OSError"), QStringLiteral("IOError"),
            QStringLiteral("Exception"), QStringLiteral("BaseException"),
            QStringLiteral("Warning"), QStringLiteral("UserWarning"),
            QStringLiteral("DeprecationWarning")
        };
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
            blockHtml += QStringLiteral("<div class=\"code-block-wrapper\">");

            if (showRun && !code.isEmpty()) {
                // HTML-escape code for data-code attribute
                QString escapedCode = code;
                escapedCode.replace(QStringLiteral("&"),  QStringLiteral("&amp;"));
                escapedCode.replace(QStringLiteral("\""), QStringLiteral("&quot;"));
                escapedCode.replace(QStringLiteral("'"),  QStringLiteral("&#39;"));
                escapedCode.replace(QStringLiteral("<"),  QStringLiteral("&lt;"));
                escapedCode.replace(QStringLiteral(">"),  QStringLiteral("&gt;"));

                blockHtml += QStringLiteral("<div class=\"code-block-header\">");
                blockHtml += QStringLiteral("<span class=\"code-lang-label\">%1</span>").arg(lang);
                blockHtml += QStringLiteral("<a class=\"run-code-btn\" href=\"runblock:execute\""
                                            " data-lang=\"%1\" data-code=\"%2\">▶ Run</a>")
                                .arg(langLower, escapedCode);
                blockHtml += QStringLiteral("</div>");
            }

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
        }

        lastPos = match.capturedEnd();
    }

    // Append remaining text after the last code block
    result += QStringView(markdown).mid(lastPos).toString();

    return result;
}

QString EditorWidget::preparePreviewContent(const QString &rawMarkdown)
{
    // Step 1: Pre-highlight code blocks (produces ```highlighted\n<base64>\n``` custom blocks)
    // Step 2: Process wiki links and tags (base64 content won't match [[...]] or #tag)
    // Step 3: Prevent </script> injection for the setHtml() path
    QString content = preHighlightCodeBlocks(rawMarkdown);
    content = processWikiLinks(content);
    content = TagIndex::processTagsForPreview(content);
    content.replace(QStringLiteral("</script>"), QStringLiteral("<\\/script>"));
    return content;
}

void EditorWidget::updatePreviewContent(std::function<void()> onFinished)
{
    QString safeContent = preparePreviewContent(m_textEdit->toPlainText());

    QString base64 = QString::fromLatin1(safeContent.toUtf8().toBase64());
    m_previewView->page()->runJavaScript(
        QStringLiteral("window.renderFromBase64('%1')").arg(base64),
        [this, onFinished](const QVariant &) {
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
    // 将 QTextEdit 的底层文档修改状态向上层发出信号
    emit modificationChanged(isModified());
}

bool EditorWidget::loadFile(const QString &filePath)
{
    QString suffix = QFileInfo(filePath).suffix().toLower();
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
    }

    // Auto-detect code file and switch mode BEFORE setting text,
    // so that setPlainText dispatches to the correct editor.
    QString lang = LanguageUtils::languageForExtension(suffix);
    if (!lang.isEmpty()) {
        m_editorMode = CodeEdit;
        m_codeEditor->setLanguage(lang);
        m_stackedWidget->setCurrentIndex(2);
    } else {
        m_editorMode = MarkdownEdit;
        if (m_stackedWidget->currentIndex() != 0)
            m_stackedWidget->setCurrentIndex(0);
    }

    setPlainText(content);
    m_originalContent = toPlainText();
    setModified(false);
    applyZoom();

    emit fileLoaded(filePath);
    return true;
}

bool EditorWidget::saveFile()
{
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
    m_originalContent = toPlainText();
    setModified(false); // 保存后清除修改标志
    emit fileSaved(m_filePath);
    return true;
}

bool EditorWidget::saveAsFile(const QString &defaultDir)
{
    // 确定对话框起始目录，支持传入路径，否则用主文件夹
    QString startDir = defaultDir.isEmpty() ? QDir::homePath() : defaultDir;
    QFileDialog dialog(this, tr("另存为"), startDir);
    dialog.setNameFilters({tr("Markdown文件 (*.md)"), tr("文本文件 (*.txt)"), tr("所有文件 (*)")});
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix("md");

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
        if (selectedFilter.contains("*.txt"))
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
    return true;
}

QString EditorWidget::toPlainText() const
{
    if (m_editorMode == CodeEdit)
        return m_codeEditor->toPlainText();
    return m_textEdit->toPlainText();
}

void EditorWidget::setPlainText(const QString &text)
{
    if (m_editorMode == CodeEdit)
        m_codeEditor->setPlainText(text);
    else
        m_textEdit->setPlainText(text);
    setModified(false);
}

bool EditorWidget::isModified() const
{
    if (m_editorMode == CodeEdit)
        return m_codeEditor->document()->isModified();
    return m_textEdit->document()->isModified();
}

void EditorWidget::setModified(bool modified)
{
    QTextDocument *doc = (m_editorMode == CodeEdit)
        ? m_codeEditor->document() : m_textEdit->document();
    if (doc->isModified() != modified) {
        doc->setModified(modified);
        emit modificationChanged(modified);
    }
}

void EditorWidget::applyZoom()
{
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

    if (m_previewMode) {
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
    // 检查文件内容是否被修改
    bool contentChanged = (toPlainText() != m_originalContent);
    if (isModified() != contentChanged) {
        setModified(contentChanged);
    }
}

void EditorWidget::setFilePath(const QString &newPath) {
    QString normalized = QFileInfo(newPath).absoluteFilePath();
    if (m_filePath == normalized) return;
    QString oldPath = m_filePath;
    m_filePath = normalized;
    m_originalContent = toPlainText();

    // Re-evaluate language on path change (handles save-as extension changes)
    QString ext = QFileInfo(newPath).suffix().toLower();
    QString lang = LanguageUtils::languageForExtension(ext);
    if (!lang.isEmpty()) {
        m_editorMode = CodeEdit;
        m_codeEditor->setLanguage(lang);
        m_stackedWidget->setCurrentIndex(2);
        applyZoom();
    } else {
        m_editorMode = MarkdownEdit;
        if (m_stackedWidget->currentIndex() != 0)
            m_stackedWidget->setCurrentIndex(0);
    }

    emit filePathChanged(oldPath, normalized);
    emit modificationChanged(isModified());
}

void EditorWidget::setFileNames(const QStringList &names)
{
    if (m_editorMode != CodeEdit)
        m_textEdit->setFileNames(names);
}

void EditorWidget::setTagNames(const QStringList &names)
{
    if (m_editorMode != CodeEdit)
        m_textEdit->setTagNames(names);
}

void EditorWidget::scrollToLine(int lineNumber, const QString &highlightText)
{
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
            sel.format.setBackground(ConfigManager::instance().searchHighlightBackground());
            sel.format.setForeground(ConfigManager::instance().searchHighlightForeground());
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
    // Scroll to line and highlight the entire line in yellow (search-highlight style).
    // Unlike scrollToLine(,highlightText), this does NOT search the full document
    // for matching text — it precisely highlights only the target line.

    if (m_previewMode && !m_splitPreview) {
        setPreviewMode(false);
    }

    QTextBlock block;
    if (m_editorMode == CodeEdit) {
        block = m_codeEditor->document()->findBlockByLineNumber(lineNumber - 1);
    } else {
        block = m_textEdit->document()->findBlockByLineNumber(lineNumber - 1);
    }
    if (!block.isValid())
        return;

    // Position cursor at the target line
    QTextCursor cursor(block);
    if (m_editorMode == CodeEdit) {
        m_codeEditor->setTextCursor(cursor);
        m_codeEditor->ensureCursorVisible();
    } else {
        m_textEdit->setTextCursor(cursor);
        m_textEdit->ensureCursorVisible();
    }

    // Add a full-width yellow highlight on the target line using extra selections
    QTextEdit::ExtraSelection sel;
    sel.format.setBackground(ConfigManager::instance().searchHighlightBackground());
    sel.format.setForeground(ConfigManager::instance().searchHighlightForeground());
    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
    sel.cursor = cursor;
    sel.cursor.clearSelection();

    if (m_editorMode == CodeEdit) {
        // Keep search highlights but add the navigation highlight
        // highlightCurrentLine() will restore this on next cursor move
        QList<QTextEdit::ExtraSelection> selections;
        selections.append(sel);
        m_codeEditor->setExtraSelections(selections);
    } else {
        m_textEdit->setExtraSelections({sel});
    }
}

void EditorWidget::clearExtraSelections()
{
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
    m_splitPreviewPage->onRunCodeBlock = [this](const QString &language, const QString &code) {
        emit runCodeBlockRequested(language, code);
    };
    m_splitPreviewPage->onTagClicked = [this](const QString &tag) {
        emit tagClicked(tag);
    };
    m_splitPreviewView->setPage(m_splitPreviewPage);

    m_splitPreviewView->installEventFilter(this);
    QTimer::singleShot(0, this, [this]() {
        if (QWidget *fp = m_splitPreviewView->focusProxy())
            fp->installEventFilter(this);
    });

    m_splitSplitter->addWidget(m_splitPreviewView);

    // 默认五五分隔
    m_splitSplitter->setSizes({1, 1});

    m_stackedWidget->addWidget(m_splitSplitter);
}

void EditorWidget::setSplitPreviewMode(bool split)
{
    if (m_editorMode == CodeEdit)
        return;
    if (split == m_splitPreview)
        return;

    // 互斥：进入分屏预览时，退出全屏预览
    if (split && m_previewMode) {
        setPreviewMode(false);
    }

    m_splitPreview = split;

    if (m_splitPreview) {
        createSplitPreviewWidgets();

        // 将 m_textEdit 从 page 0 转移到 splitter 左侧
        m_stackedWidget->removeWidget(m_textEdit);
        m_splitTextWrapper->layout()->addWidget(m_textEdit);
        m_textEdit->show();

        if (!m_splitPreviewReady) {
            m_splitPreviewView->page()->setBackgroundColor(
                ConfigManager::instance().previewWebEngineBackground());

            QFile tmplFile(QStringLiteral(":/preview/template.html"));
            if (tmplFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString tmpl = QString::fromUtf8(tmplFile.readAll());
                tmplFile.close();

                QString safeContent = preparePreviewContent(m_textEdit->toPlainText());
                tmpl.replace(QStringLiteral("{{MARKDOWN_CONTENT}}"), safeContent);
                m_splitPreviewView->setHtml(tmpl, QUrl(QStringLiteral("qrc:/preview/")));

                connect(m_splitPreviewView->page(), &QWebEnginePage::loadFinished, this,
                    [this](bool ok) {
                        disconnect(m_splitPreviewView->page(), &QWebEnginePage::loadFinished, this, nullptr);
                        if (!ok) return;
                        m_splitPreviewReady = true;
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
    } else {
        // 退出分屏：将 m_textEdit 从 splitter 放回 page 0
        m_splitTextWrapper->layout()->removeWidget(m_textEdit);
        m_stackedWidget->insertWidget(0, m_textEdit);
        m_stackedWidget->setCurrentWidget(m_textEdit);
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
    QString base64 = QString::fromLatin1(safeContent.toUtf8().toBase64());
    m_splitPreviewView->page()->runJavaScript(
        QStringLiteral("window.renderFromBase64('%1')").arg(base64));
}
