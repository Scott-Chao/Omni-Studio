#ifndef EDITORWIDGET_H
#define EDITORWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QTextBrowser>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QTimer>

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

    // 字体缩放
    void zoomIn();
    void zoomOut();
    void zoomReset();
    qreal zoomFactor() const;
    void setZoomFactor(qreal factor);

    void setFilePath(const QString &newPath);  // 更新文件路径（用于重命名后）

signals:
    void fileLoaded(const QString &filePath);
    void fileSaved(const QString &filePath);
    void modificationChanged(bool modified);
    void zoomFactorChanged(qreal factor);

    void filePathChanged(const QString &oldPath, const QString &newPath);

private slots:
    void onTextChanged();
    void updateModificationChanged();

private:
    QStackedWidget *m_stackedWidget;
    QTextEdit *m_textEdit; // 源码编辑
    QTextBrowser *m_previewBrowser; // 渲染预览
    QString m_filePath;
    bool m_previewMode;

private:
    void applyZoom();  // 将当前缩放因子应用到编辑器和预览器
    qreal m_zoomFactor = 1.0;
    int m_baseFontSize = 14;

private:
    QString m_originalContent; // 文件的原始纯文本内容
    QTimer m_contentCheckTimer; // 计时器，用于延迟内容比较
    void onContentCheckTimeout(); // 超时后比较内容并更新修改状态

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // EDITORWIDGET_H
