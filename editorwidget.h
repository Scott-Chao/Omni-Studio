#ifndef EDITORWIDGET_H
#define EDITORWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QWebEngineView>
#include <QPdfView>
#include <QPdfDocument>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <QSplitter>
#include <QPageLayout>
#include <functional>

class WikiLinkTextEdit;
class CodeEditor;
class PreviewPage;
class SmdEditor;

class EditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EditorWidget(QWidget *parent = nullptr);
    ~EditorWidget();

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
    void navigateToLine(int lineNumber);
    void clearExtraSelections();
    void navigateEditorToLine(int lineNumber);

    // 预览模式切换
    void setPreviewMode(bool preview);
    bool isPreviewMode() const { return m_editorMode != SmdEdit && m_editorMode == MarkdownEdit && m_previewMode; }
    void refreshPreview(); // 手动刷新预览（如内容改变后调用）
    void updatePreviewContent(std::function<void()> onFinished); // 异步更新预览，完成后回调

    // 分屏预览模式
    void setSplitPreviewMode(bool split);
    bool isSplitPreviewMode() const { return m_splitPreview; }

    // 字体缩放
    void zoomIn();
    void zoomOut();
    void zoomReset();
    qreal zoomFactor() const;
    void setZoomFactor(qreal factor);

    // 编辑器字体设置
    void setEditorFont(const QString &family, int size);
    void setCodeIndentWidth(int width);
    void setSplitPreviewDebounceMs(int ms);
    void applySplitPreviewRatio();

    void reloadEditorColors();

    void setFilePath(const QString &newPath);  // 更新文件路径（用于重命名后）

    // WikiLink 自动补全
    void setFileNames(const QStringList &names);
    void setTagNames(const QStringList &names);
    bool isCodeEdit() const { return m_editorMode == CodeEdit; }
    bool isPdfView() const { return m_editorMode == PdfView; }
    bool isSmdEdit() const { return m_editorMode == SmdEdit; }

    // 自动保存
    void startAutoSave();
    void stopAutoSave();
    void setAutoSaveEnabled(bool enabled);
    QString recoveryTempPath() const { return m_recoveryTempPath; }
    void setRecoveryTempPath(const QString &path) { m_recoveryTempPath = path; }
    QString autoSaveRecoveryDir() const; // 恢复文件目录路径（静态）

    // PDF 导出
    void exportToPdf(const QString &filePath, const QPageLayout &layout);

signals:
    void fileLoaded(const QString &filePath);
    void fileSaved(const QString &filePath);
    void modificationChanged(bool modified);
    void zoomFactorChanged(qreal factor);

    void filePathChanged(const QString &oldPath, const QString &newPath);

    void wikiLinkClicked(const QString &fileName); // 点击 [[文件名]] 时发出
    void runCodeBlockRequested(const QString &language, const QString &code); // 转发代码块运行请求
    void tagClicked(const QString &tag); // 点击 #tag 时发出
    void pdfExportCompleted(const QString &filePath, bool success); // PDF 导出完成

private slots:
    void onTextChanged();
    void updateModificationChanged();
    void onSplitDebounceTimeout();
    void onAutoSaveTimeout();

private:
    enum EditorMode { MarkdownEdit, CodeEdit, PdfView, SmdEdit };
    EditorMode m_editorMode = MarkdownEdit;

    QStackedWidget *m_stackedWidget;
    WikiLinkTextEdit *m_textEdit; // 源码编辑
    QWebEngineView *m_previewView; // 渲染预览
    QWidget *m_previewContainer; // 暗色容器，遮挡 WebEngine 白底
    CodeEditor *m_codeEditor; // 代码编辑
    QString m_filePath;
    bool m_previewMode;
    bool m_previewReady = false; // WebEngine 页面是否已加载模板

    // PDF 视图
    QPdfView *m_pdfView = nullptr;
    QPdfDocument *m_pdfDocument = nullptr;

    // SMD 编辑器
    SmdEditor *m_smdEditor = nullptr;

    // 分屏预览
    bool m_splitPreview = false;
    QSplitter *m_splitSplitter = nullptr;
    QWidget *m_splitTextWrapper = nullptr;
    QWebEngineView *m_splitPreviewView = nullptr;
    PreviewPage *m_splitPreviewPage = nullptr;
    bool m_splitPreviewReady = false;
    QString m_lastSplitPreviewContent;
    QTimer m_splitDebounceTimer;

private:
    void applyZoom();  // 将当前缩放因子应用到编辑器和预览器
    QString processWikiLinks(const QString &markdown); // [[link]] → [link](wikilink:...)
    QString preHighlightCodeBlocks(const QString &markdown); // 对 fenced 代码块进行 C++ 端语法着色
    QString injectHeadingAnchors(const QString &markdown); // 为标题注入 <a id="hl-N"> 锚点，供预览导航用
    QString highlightCodeBlock(const QString &code, const QString &langId); // 单块着色，返回 HTML
    QString preparePreviewContent(const QString &rawMarkdown); // 完整预处理：高亮→保护→wikilink→tag→恢复→</script>转义
    void createSplitPreviewWidgets();
    void updateSplitPreviewContentNow();
    qreal m_zoomFactor = 1.0;
    int m_baseFontSize = 14;

private:
    QString m_originalContent; // 文件的原始纯文本内容
    QTimer m_contentCheckTimer; // 计时器，用于延迟内容比较
    void onContentCheckTimeout(); // 超时后比较内容并更新修改状态

    // 自动保存
    QTimer m_autoSaveTimer;
    QString m_recoveryTempPath; // 恢复文件路径（仅未命名文件使用）
    bool m_autoSaveEnabled = true;
    void autoSaveNow(); // 执行自动保存（不修改 modified 状态）

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // EDITORWIDGET_H
