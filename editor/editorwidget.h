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
#include <QMap>
#include <QList>
#include <functional>
#include "smd/smddiagnostic.h"

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
    void setLoading(bool loading) { m_loading = loading; }
    QString currentFilePath() const { return m_filePath; }

    // 搜索导航
    void scrollToLine(int lineNumber, const QString &highlightText = QString());
    void navigateToLine(int lineNumber);
    void clearExtraSelections();
    void clearOutlineHighlight();
    void navigateEditorToLine(int lineNumber);

    // 预览模式切换
    void setPreviewMode(bool preview);
    bool isPreviewMode() const { return m_editorMode != SmdEdit && m_editorMode == MarkdownEdit && m_previewMode; }
    void refreshPreview(); // 手动刷新预览（如内容改变后调用）
    void refreshPreviewTheme(); // 刷新预览页面的主题颜色（设置面板关闭时调用）
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
    void setMarkdownIndentWidth(int width);
    void setSplitPreviewDebounceMs(int ms);
    void applySplitPreviewRatio();

    // Block-level diagnostics for MD code blocks (injects JS squiggly lines into preview)
    void applyBlockDiagnostics(const QMap<int, QList<SmdDiagnostic>> &diagByBlock);
    void clearBlockDiagnostics();

    void reloadEditorColors();

    void setFilePath(const QString &newPath);  // 更新文件路径（用于重命名后）

    // WikiLink 自动补全
    void setFileNames(const QStringList &names);
    void setTagNames(const QStringList &names);
    bool isCodeEdit() const { return m_editorMode == CodeEdit; }
    bool isPdfView() const { return m_editorMode == PdfView; }
    bool isSmdEdit() const { return m_editorMode == SmdEdit; }
    SmdEditor* smdEditor() const { return m_smdEditor; }
    CodeEditor* codeEditor() const { return m_codeEditor; }

    // Cursor position
    int cursorLine() const;
    int cursorColumn() const;
    void setCursorPosition(int line, int column);

    // Sync original content baseline to given disk content
    void setOriginalContent(const QString &diskContent);

    // Selected text from the active sub-editor
    QString selectedText() const;

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
    void runCodeBlockRequested(const QString &language, const QString &code, int blockIndex); // 转发代码块运行请求
    void diagnosticsToggleRequested(); // Ctrl+E 切换诊断面板
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
    QWebEngineView *m_previewView = nullptr; // 渲染预览（懒创建）
    QWidget *m_previewContainer = nullptr; // 暗色容器，遮挡 WebEngine 白底
    CodeEditor *m_codeEditor; // 代码编辑
    QString m_filePath;
    QString m_lastSearchHighlightText;
    int m_outlineHighlightLine = -1; // 1-based line from outline navigation
    bool m_previewMode;
    bool m_previewReady = false; // WebEngine 页面是否已加载模板

    // Block diagnostics for MD code blocks (re-applied on preview refresh)
    QMap<int, QList<SmdDiagnostic>> m_blockDiagnostics;
    QMap<int, QString> m_blockDiagnosticCode; // snapshot of code content per block

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
    void ensurePreviewView();    // 懒创建 m_previewView + m_previewContainer
    void destroyPreviewView();   // 销毁并释放预览视图
    void destroySplitPreviewWidgets(); // 销毁分屏预览控件
    void applyZoom();  // 将当前缩放因子应用到编辑器和预览器
    QString processWikiLinks(const QString &markdown); // [[link]] → [link](wikilink:...)
    QString preHighlightCodeBlocks(const QString &markdown); // 对 fenced 代码块进行 C++ 端语法着色
    QString injectHeadingAnchors(const QString &markdown); // 为标题注入 <a id="hl-N"> 锚点，供预览导航用
    QString highlightCodeBlock(const QString &code, const QString &langId); // 单块着色，返回 HTML
    QMap<int, QString> extractCodeBlockContents(const QString &markdown) const; // 提取围栏代码块内容，key=blockIndex
    QString preparePreviewContent(const QString &rawMarkdown); // 完整预处理：高亮→保护→wikilink→tag→恢复→</script>转义
    void applyPreviewTheme(QString &tmpl); // 替换模板中的 {{PREVIEW_*}} 为当前主题颜色
    QString previewThemeJs(); // 返回更新 CSS 变量的 JavaScript 代码
    void createSplitPreviewWidgets();
    void updateSplitPreviewContentNow();
    void runPreviewUpdate(QWebEngineView *view, std::function<void()> onFinished);
    void applyMarkdownExtraSelections();
    qreal m_zoomFactor = 1.0;
    int m_baseFontSize = 14;

private:
    QString m_originalContent; // 文件的原始纯文本内容
    QTimer m_contentCheckTimer; // 计时器，用于延迟内容比较
    void onContentCheckTimeout(); // 超时后比较内容并更新修改状态

    // 自动保存
    QTimer m_autoSaveTimer;
    QString m_recoveryTempPath; // 恢复文件路径（仅未命名文件使用）
    bool m_loading = false;    // 文件加载过程中抑制修改信号
    bool m_autoSaveEnabled = true;
    void autoSaveNow(); // 执行自动保存（不修改 modified 状态）

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // EDITORWIDGET_H
