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
#include <QJsonDocument>
#include <QJsonObject>
#include <QColor>

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

            QString safeContent = processWikiLinks(m_textEdit->toPlainText());
            safeContent = TagIndex::processTagsForPreview(safeContent);
            safeContent.replace(QStringLiteral("</script>"), QStringLiteral("<\\/script>"));
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

void EditorWidget::updatePreviewContent(std::function<void()> onFinished)
{
    QString safeContent = processWikiLinks(m_textEdit->toPlainText());
    safeContent = TagIndex::processTagsForPreview(safeContent);
    safeContent.replace(QStringLiteral("</script>"), QStringLiteral("<\\/script>"));

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
    // 如果处于预览模式，且希望实时更新，可以取消注释下面一行
    // if (m_previewMode) refreshPreview();
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
    if (m_previewMode) {
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

void EditorWidget::clearExtraSelections()
{
    if (m_editorMode == CodeEdit)
        m_codeEditor->clearSearchHighlights();
    else
        m_textEdit->setExtraSelections({});
}
