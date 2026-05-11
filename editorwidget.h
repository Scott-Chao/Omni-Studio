#ifndef EDITORWIDGET_H
#define EDITORWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QWebEngineView>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <functional>

class WikiLinkTextEdit;
class CodeEditor;

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

    // 搜索导航
    void scrollToLine(int lineNumber, const QString &highlightText = QString());
    void clearExtraSelections();

    // 预览模式切换
    void setPreviewMode(bool preview);
    bool isPreviewMode() const { return m_editorMode == MarkdownEdit && m_previewMode; }
    void refreshPreview(); // 手动刷新预览（如内容改变后调用）
    void updatePreviewContent(std::function<void()> onFinished); // 异步更新预览，完成后回调

    // 字体缩放
    void zoomIn();
    void zoomOut();
    void zoomReset();
    qreal zoomFactor() const;
    void setZoomFactor(qreal factor);

    void setFilePath(const QString &newPath);  // 更新文件路径（用于重命名后）

    // WikiLink 自动补全
    void setFileNames(const QStringList &names);
    void setTagNames(const QStringList &names);
    bool isCodeEdit() const { return m_editorMode == CodeEdit; }

signals:
    void fileLoaded(const QString &filePath);
    void fileSaved(const QString &filePath);
    void modificationChanged(bool modified);
    void zoomFactorChanged(qreal factor);

    void filePathChanged(const QString &oldPath, const QString &newPath);

    void wikiLinkClicked(const QString &fileName); // 点击 [[文件名]] 时发出
    void runCodeBlockRequested(const QString &language, const QString &code); // 转发代码块运行请求
    void tagClicked(const QString &tag); // 点击 #tag 时发出

private slots:
    void onTextChanged();
    void updateModificationChanged();

private:
    enum EditorMode { MarkdownEdit, CodeEdit };
    EditorMode m_editorMode = MarkdownEdit;

    QStackedWidget *m_stackedWidget;
    WikiLinkTextEdit *m_textEdit; // 源码编辑
    QWebEngineView *m_previewView; // 渲染预览
    QWidget *m_previewContainer; // 暗色容器，遮挡 WebEngine 白底
    CodeEditor *m_codeEditor; // 代码编辑
    QString m_filePath;
    bool m_previewMode;
    bool m_previewReady = false; // WebEngine 页面是否已加载模板

private:
    void applyZoom();  // 将当前缩放因子应用到编辑器和预览器
    QString processWikiLinks(const QString &markdown); // [[link]] → [link](wikilink:...)
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
