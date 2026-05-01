#include "editorwidget.h"
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QDebug>

EditorWidget::EditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_textEdit(new QTextEdit(this))
{
    // 让 QTextEdit 填满整个 EditorWidget
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_textEdit);
    setLayout(layout);

    // 传递修改状态变化信号
    connect(m_textEdit->document(), &QTextDocument::modificationChanged,
            this, &EditorWidget::modificationChanged);
}

bool EditorWidget::loadFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file for reading:" << file.errorString();
        return false;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString content = in.readAll();
    file.close();

    setPlainText(content);
    m_filePath = filePath;
    setModified(false);
    emit fileLoaded(m_filePath);
    return true;
}

bool EditorWidget::saveFile()
{
    if (m_filePath.isEmpty())
        return false;
    return saveAsFile(m_filePath);
}

bool EditorWidget::saveAsFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qDebug() << "Failed to open file for writing:" << file.errorString();
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << m_textEdit->toPlainText();
    file.close();

    m_filePath = filePath;
    setModified(false);
    emit fileSaved(m_filePath);
    return true;
}

QString EditorWidget::toPlainText() const
{
    return m_textEdit->toPlainText();
}

void EditorWidget::setPlainText(const QString &text)
{
    m_textEdit->setPlainText(text);
}

bool EditorWidget::isModified() const
{
    return m_textEdit->document()->isModified();
}

void EditorWidget::setModified(bool modified)
{
    m_textEdit->document()->setModified(modified);
}
