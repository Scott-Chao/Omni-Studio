#include "editorwidget.h"
#include "fileutils.h"
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

EditorWidget::EditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_filePath("")
    , m_previewMode(false)
    , m_zoomFactor(1.0) // 初始化缩放因子
    , m_baseFontSize(0) // 稍后从字体获取
{
    // 创建编辑器和预览控件
    m_textEdit = new QTextEdit(this);
    m_previewBrowser = new QTextBrowser(this);
    m_previewBrowser->setOpenExternalLinks(false); // 禁止自动调用浏览器
    m_previewBrowser->setOpenLinks(false); // 禁用自动导航

    // 连接预览器的链接点击信号
    connect(m_previewBrowser, &QTextBrowser::anchorClicked, this, [this](const QUrl &url){
        if (url.scheme() == "wikilink") {
            emit wikiLinkClicked(url.path()); // 发出信号，通知主窗口查找文件
        } else {
            // 如果是普通网页链接，通过浏览器打开
            QDesktopServices::openUrl(url);
        }
    });

    m_textEdit->viewport()->installEventFilter(this);
    m_previewBrowser->viewport()->installEventFilter(this);

    // 堆叠布局：索引0=编辑，索引1=预览
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->addWidget(m_textEdit);
    m_stackedWidget->addWidget(m_previewBrowser);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stackedWidget);
    setLayout(layout);

    // 获取基础字体大小
    QFont baseFont = m_textEdit->font();
    m_baseFontSize = baseFont.pointSize();

    // 当编辑区内容改变时，更新修改标志并刷新预览
    connect(m_textEdit, &QTextEdit::textChanged, this, &EditorWidget::onTextChanged);
    // 当编辑区修改状态变化时发出信号
    connect(m_textEdit, &QTextEdit::textChanged, this, &EditorWidget::updateModificationChanged);

    setPreviewMode(false); // 默认编辑模式

    m_contentCheckTimer.setSingleShot(true);
    m_contentCheckTimer.setInterval(300); // 设置300ms无文本变化后进行一次内容比较
    connect(&m_contentCheckTimer, &QTimer::timeout, this, &EditorWidget::onContentCheckTimeout);

    // 当文本编辑器内容变化时，重置计时器
    connect(m_textEdit, &QTextEdit::textChanged, this, [this]() {
        m_contentCheckTimer.start();
    });
    m_originalContent = toPlainText(); // 记录当前内容，用于内容比较
}

void EditorWidget::setPreviewMode(bool preview)
{
    m_previewMode = preview;
    if (m_previewMode) {
        refreshPreview(); // 刷新预览内容
        m_stackedWidget->setCurrentIndex(1);
    } else {
        m_stackedWidget->setCurrentIndex(0);
    }
    applyZoom(); // 切换模式后立即应用字体缩放
}

void EditorWidget::refreshPreview()
{
    QString markdown = m_textEdit->toPlainText();

    // 递归正则：支持任意层配对 []
    static const QRegularExpression wikiRegExp(
        QStringLiteral(R"(\[\[((?:[^\[\]]|\[(?1)\])*)\]\])"));

    QRegularExpressionMatchIterator it = wikiRegExp.globalMatch(markdown);
    QString result;
    int lastPos = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        result += QStringView(markdown).mid(lastPos, match.capturedStart() - lastPos).toString();
        QString linkText = match.captured(1);  // 完整的链接文本（含内部方括号）

        // 转义显示文本中的 [ 和 ]，避免破坏 Markdown 链接语法
        QString escapedText = linkText;
        escapedText.replace(QLatin1Char('['), QStringLiteral("\\["));
        escapedText.replace(QLatin1Char(']'), QStringLiteral("\\]"));

        // 对链接目标进行 URL 编码，避免空格/特殊字符破坏 Markdown 链接语法
        QByteArray encoded = QUrl::toPercentEncoding(linkText);
        QString encodedTarget = QString::fromLatin1(encoded);
        result += QStringLiteral("[%1](wikilink:%2)")
                      .arg(escapedText, encodedTarget);
        lastPos = match.capturedEnd();
    }
    result += QStringView(markdown).mid(lastPos).toString();

    QTextDocument *doc = m_previewBrowser->document();
    doc->setMarkdown(result, QTextDocument::MarkdownDialectGitHub);
    doc->setDefaultFont(m_textEdit->font());

    int pointSize = qRound(m_baseFontSize * m_zoomFactor);
    doc->setDefaultStyleSheet(
        QStringLiteral("body { font-size: %1pt; } * { font-size: inherit; }")
            .arg(pointSize));
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
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    setPlainText(stream.readAll());
    m_filePath = QFileInfo(filePath).absoluteFilePath();
    m_originalContent = toPlainText();
    setModified(false);
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
    // 获取文字
    return m_textEdit->toPlainText();
}

void EditorWidget::setPlainText(const QString &text)
{
    m_textEdit->setPlainText(text);
    setModified(false);
}

bool EditorWidget::isModified() const
{
    // 获取修改标识
    return m_textEdit->document()->isModified();
}

void EditorWidget::setModified(bool modified)
{
    if (m_textEdit->document()->isModified() != modified) {
        m_textEdit->document()->setModified(modified);
        emit modificationChanged(modified); // 触发标题更新
    }
}

void EditorWidget::applyZoom()
{
    bool wasModified = m_textEdit->document()->isModified();
    QSignalBlocker blocker(m_textEdit->document());

    int pointSize = qBound(1, qRound(m_baseFontSize * m_zoomFactor), 72);

    // 编辑区：视图字体 + 全文字符格式
    QFont f = m_textEdit->font();
    f.setPointSize(pointSize);
    m_textEdit->setFont(f);

    QTextCursor cursor(m_textEdit->document());
    cursor.select(QTextCursor::Document);
    QTextCharFormat fmt;
    fmt.setFontPointSize(pointSize);
    cursor.mergeCharFormat(fmt); // 只改变字号，保留加粗等

    // 预览区
    if (m_previewMode) {
        refreshPreview();
    }

    m_textEdit->document()->setModified(wasModified);
}

void EditorWidget::zoomIn()
{
    setZoomFactor(m_zoomFactor + 0.1);
}

void EditorWidget::zoomOut()
{
    setZoomFactor(m_zoomFactor - 0.1);
}

void EditorWidget::zoomReset()
{
    setZoomFactor(1.0);
}

void EditorWidget::setZoomFactor(qreal factor)
{
    // 设置缩放比例
    factor = qBound(0.5, factor, 3.0); // 允许 50%-300%
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
    // 更新原始内容副本为当前内容（磁盘文件已改名且内容不变）
    m_originalContent = toPlainText();
    emit filePathChanged(oldPath, normalized);
    // 修改状态不变，但路径改变可能需要重新检查（例如高亮等）
    emit modificationChanged(isModified());
}
