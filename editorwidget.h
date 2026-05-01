#ifndef EDITORWIDGET_H
#define EDITORWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QString>

class EditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EditorWidget(QWidget *parent = nullptr);

    // 文件操作
    bool loadFile(const QString &filePath);      // 加载指定文件
    bool saveFile();                              // 保存当前文件
    bool saveAsFile(const QString &filePath);     // 另存为

    // 获取状态
    QString currentFilePath() const { return m_filePath; }
    QString toPlainText() const;
    bool isModified() const;

    // 设置内容
    void setPlainText(const QString &text);
    void setModified(bool modified);

signals:
    void fileLoaded(const QString &filePath);
    void fileSaved(const QString &filePath);
    void modificationChanged(bool modified);

private:
    QTextEdit *m_textEdit;
    QString m_filePath;
};

#endif // EDITORWIDGET_H
