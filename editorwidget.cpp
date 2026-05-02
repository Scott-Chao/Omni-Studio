#include "editorwidget.h"
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>

EditorWidget::EditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_filePath("")
    , m_previewMode(false)
{
    // 创建编辑器和预览控件
    m_textEdit = new QTextEdit(this);
    m_previewBrowser = new QTextBrowser(this);
    m_previewBrowser->setOpenExternalLinks(true); // 允许点击链接

    // 堆叠布局：索引0=编辑，索引1=预览
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->addWidget(m_textEdit);
    m_stackedWidget->addWidget(m_previewBrowser);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stackedWidget);
    setLayout(layout);

    // 当编辑区内容改变时，更新修改标志并刷新预览
    connect(m_textEdit, &QTextEdit::textChanged, this, &EditorWidget::onTextChanged);
    // 当编辑区修改状态变化时发出信号
    connect(m_textEdit, &QTextEdit::textChanged, this, &EditorWidget::updateModificationChanged);

    setPreviewMode(false); // 默认编辑模式
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
}

void EditorWidget::refreshPreview()
{
    // 获取当前 Markdown 源码
    QString markdown = m_textEdit->toPlainText();
    // 使用 Qt 的 setMarkdown 进行渲染
    QTextDocument doc;
    doc.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);
    m_previewBrowser->setHtml(doc.toHtml());
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
    m_filePath = filePath;
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
    setModified(false); // 保存后清除修改标志
    emit fileSaved(m_filePath);
    return true;
}

bool EditorWidget::saveAsFile()
{
    // 另存为
    QString newPath = QFileDialog::getSaveFileName(this, "另存为", "", "Markdown文件 (*.md);;文本文件 (*.txt)");
    if (newPath.isEmpty())
        return false;
    m_filePath = newPath;
    return saveFile();
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
    m_textEdit->document()->setModified(modified);
}
