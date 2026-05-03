#ifndef EDITORWIDGET_H
#define EDITORWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QTextBrowser>
#include <QStackedWidget>
#include <QVBoxLayout>

class EditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EditorWidget(QWidget *parent = nullptr);

    // 文件操作
    bool loadFile(const QString &filePath);
    bool saveFile();
    bool saveAsFile(const QString &defaultDir = QString());

    // 内容访问
    QString toPlainText() const;
    void setPlainText(const QString &text);
    bool isModified() const;
    void setModified(bool modified);
    QString currentFilePath() const { return m_filePath; }

    // 预览模式切换
    void setPreviewMode(bool preview);
    bool isPreviewMode() const { return m_previewMode; }
    void refreshPreview(); // 手动刷新预览（如内容改变后调用）

signals:
    void fileLoaded(const QString &filePath);
    void fileSaved(const QString &filePath);
    void modificationChanged(bool modified);

private slots:
    void onTextChanged();
    void updateModificationChanged();

private:
    QStackedWidget *m_stackedWidget;
    QTextEdit      *m_textEdit; // 源码编辑
    QTextBrowser   *m_previewBrowser; // 渲染预览

    QString         m_filePath;
    bool            m_previewMode;
};

#endif // EDITORWIDGET_H
