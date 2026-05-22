#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QMap>
#include "smddiagnostic.h"
#include "ai/aiprovider.h"
#include "ai/prompttemplates.h"
#include <QTabWidget>
#include <QSplitter>
#include <QFileInfo>
#include <QLabel>
#include <QMap>
#include <QPointer>
#include <atomic>
#include <memory>

class ActivityBar;
class TabManager;
class FileExplorerWidget;
class EditorWidget;
class SettingsManager;
class QDockWidget;
class QPushButton;
class QToolButton;
class QMenu;
class RightPanelContainer;
class BacklinkIndex;
class SearchPanel;
class ProcessRunner;
class BottomPanel;
class OutputPanel;
class JudgePanel;
class OpenJudgeWindow;
class SubmitResultPanel;
struct SubmissionResult;
class TagIndex;
class SettingsPanel;
class HelpPanel;
class AiPanel;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    SettingsManager *m_settings; // 配置信息
    ActivityBar *m_activityBar; // 活动栏（左侧竖条按钮）
    FileExplorerWidget *m_explorer; // 文件浏览器控件
    QSplitter *m_splitter; // 分隔条
    TabManager *m_tabManager; // 标签页栏
    QString m_currentFilePath; // 当前打开的文件路径（文件树）

private slots:
    void onFileSelected(const QString &filePath); // 单击文件 → 预览模式打开
    void onFileDoubleClicked(const QString &filePath); // 双击文件 → 永久打开
    void saveFile(); // 保存文件
    void onSaveFileAs(); // 另存为
    void onExportPdf(); // 导出 PDF
    void newFile(); // 新建文件
    void saveSettings(); // 保存设置
    void onOpenFolder(); // 打开文件夹
    void onFolderChanged(const QString &newPath); // 记录用户打开的文件夹，实现记忆功能
    void onZoomIn(); // 放大
    void onZoomOut(); // 缩小
    void onZoomReset(); // 重置大小
    void onRequestDelete(const QString &path, bool isDir); // 删除文件/文件夹
    void onHistoryFileClicked(const QString &filePath); // 打开历史记录
    void onSearchResultClicked(const QString &filePath, int lineNumber,
                               const QString &searchText); // 打开搜索结果
    void onWikiLinkClicked(const QString &fileName); // 点击双向链接
    void buildFileIndex(); // 全量更新索引
    void startAsyncIndexBuild(); // 异步版本，避免大目录卡死 UI
    void onFileRenamedInIndex(const QString &oldPath, const QString &newPath); // 增量更新：重命名
    void onFileDeletedInIndex(const QString &path); // 增量更新：删除
    void onFileMovedOrRenamed(const QString &oldPath, const QString &newPath); // 通过文件树进行文件移动
    void updateWikiLinksAfterRename(const QStringList &affectedSources,
                                    const QString &oldLinkText,
                                    const QString &newLinkText); // 重命名时更新所有引用的 wiki 链接文本

protected:
    void closeEvent(QCloseEvent *event) override; // 当用户关闭窗口时自动保存
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool event(QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void closeTabsUnderPath(const QString &dirPath);

    EditorWidget* currentEditor() const;
    void updateTabTitle(EditorWidget *editor, bool modified);
    void syncFileTreeSelection(); // 将文件树选中同步到当前标签页
    void loadSettings(); // 程序启动时读取配置

    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
    QAction *m_zoomResetAction;
    QLabel *m_zoomLabel; // 显示当前缩放百分比
    QMetaObject::Connection m_editorZoomConnection; // 用来管理当前编辑器的缩放信号连接

    void updateZoomLabel(); // 更新百分比标签
    void connectCurrentEditorZoomSignal(); // 连接当前编辑器的缩放信号
    // 预览模式激活状态调整
    QAction *m_previewAction = nullptr;
    QAction *m_splitPreviewAction = nullptr;
    QAction *m_exportPdfAction = nullptr;
    void updatePreviewActionState();
    void updateSplitPreviewActionState();
    // 右侧统一面板（历史/大纲/标签/反链）
    RightPanelContainer *m_rightPanel;
    QDockWidget *m_dockRightPanel;
    QAction *toggleRightPanelAction;
    QAction *m_toggleHistoryAction = nullptr;
    QAction *m_toggleOutlineAction = nullptr;
    QAction *m_toggleTagsAction = nullptr;
    QAction *m_toggleBacklinksAction = nullptr;
    void addToRecentFiles(const QString &filePath);
    void refreshBacklinks();
    void refreshTags();
    void onTagClicked(const QString &tag);
    BacklinkIndex *m_backlinkIndex;
    // 搜索面板
    SearchPanel *m_searchPanel;
    QDockWidget *m_dockSearch;
    QAction *toggleSearchAction;

    // 编译运行
    ProcessRunner *m_processRunner;
    QAction *m_runToolAction = nullptr;
    QMenu *m_runMenu = nullptr;
    BottomPanel *m_bottomPanel;
    QSplitter *m_rightSplitter;
    QAction *m_compileAction;
    QAction *m_runAction;
    QAction *m_compileRunAction;
    QAction *m_stopAction;

    QMetaObject::Connection m_diagnosticsProviderConnection;
    QMetaObject::Connection m_codeBlockConnection;
    int m_codeBlockCounter = 0;

    // MD code block diagnostics
    QMap<QString, QMap<int, QList<SmdDiagnostic>>> m_mdDiagnostics;
    QMap<QString, int> m_lastRunBlockIndexMd;
    bool m_isRunningCodeBlock = false;
    bool m_processManuallyStopped = false;
    int m_currentBlockIndexMd = -1;
    QString m_currentMdFilePath;
    QString m_currentBlockLanguage;
    QString m_mdStderrBuffer;
    QMetaObject::Connection m_stderrBufferConnection;

    // 本地评测
    JudgePanel *m_judgePanel;
    QDockWidget *m_dockJudge;
    QAction *m_toggleJudgeAction;

    // OpenJudge 窗口（单例）
    QPointer<OpenJudgeWindow> m_openJudgeWindow;
    // 提交结果面板
    SubmitResultPanel *m_submitResultPanel = nullptr;

    // 标签系统
    TagIndex *m_tagIndex;

    // 大纲面板 —— 移至 RightPanelContainer
    void refreshOutline();

    // 设置面板
    SettingsPanel *m_settingsPanel = nullptr;
    QWidget *m_settingsOverlay = nullptr;
    QAction *m_settingsAction = nullptr;

    // 帮助面板
    HelpPanel *m_helpPanel = nullptr;
    QWidget *m_helpOverlay = nullptr;
    QAction *m_helpAction = nullptr;

    // AI 助手
    AiPanel *m_aiPanel = nullptr;
    QDockWidget *m_dockAi = nullptr;
    QAction *m_toggleAiAction = nullptr;
    AiProvider *m_aiProvider = nullptr;
    QList<Message> m_aiHistory;
    bool m_aiStreaming = false;

    void startAiRequest(AiAction action, const QString &freeQuery = QString());
    void abortAiRequest();
    void showRightPanel(int panelIndex);

    // .md ↔ .smd 转换
    QAction *m_convertMdSmdAction = nullptr;

    // 快捷键动态映射
    QMap<QString, QAction*> m_shortcutActions;

    // 自定义标题栏控件
    QPushButton *m_minimizeBtn = nullptr;
    QPushButton *m_maximizeBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;
    QWidget *m_toolbarSpacer = nullptr;
    void setupCustomTitleBar();

    void onCompile();
    void onRun();
    void onCompileAndRun();
    void onStopProcess();
    void onCompileFinished(bool success);
    void onRunFinished(int exitCode);
    void onJudgeRunAll();
    void onOpenJudgeRequested();
    void onOpenJudgeSampleSelected(const QString &folderPath);
    void onCodeBlockRequested(const QString &language, const QString &code, int blockIndex);
    void onSubmitToOpenJudge();
    void onConvertMdSmd();
    void onSubmissionResultReady(const SubmissionResult &result);
    void onOpenJudgeLoginStateChanged(bool loggedIn, const QString &username);
    void toggleSettings();
    void toggleHelp();
    void parseAndShowBlockDiagnostics();
    void loadMdDiagnosticsForCurrentTab();
    void onDefaultZoomChanged(qreal zoom);
    void onEditorSettingChanged(const QString &key, const QVariant &value);
    void onAppearanceSettingChanged(const QString &key, const QVariant &value);
    void onOutputPanelSettingChanged(const QString &key, const QVariant &value);
    void onPreviewSettingChanged(const QString &key, const QVariant &value);
    void onSearchSettingChanged(const QString &key, const QVariant &value);
    void onAiSettingChanged(const QString &key, const QVariant &value);
    void updateAiActionBar();
    void onAiPartialResponse(const QString &text);
    void onAiFinished();
    void onAiError(const QString &message);
    void onResetToDefaults();
    void onShortcutChanged(const QString &actionKey, const QString &keySequenceText);
    QString saveCodeToTempFile(EditorWidget *editor);
    QString saveCodeBlockToTempFile(const QString &language, const QString &code);
    void showOutputPanel();
    void toggleDiagnosticsInCodeEditor();
    void convertMdToSmd(EditorWidget *editor, const QFileInfo &fi);
    void convertSmdToMd(EditorWidget *editor, const QFileInfo &fi);

    // 崩溃恢复
    void checkCrashRecovery();
    void cleanStaleRecoveryFiles();
    void clearRecoveryDirectory();

    // 键：文件名（不带路径，不带后缀，如 "笔记"）
    // 值：该文件名对应的所有绝对路径列表（处理同名文件）
    QMap<QString, QStringList> m_fileIndex;
    std::shared_ptr<std::atomic<bool>> m_scanCancelled;
    std::atomic<uint64_t> m_scanId{0};
    QString findWikiTarget(const QString &fileName); // 向上递归搜索目标文件
    void updateCurrentEditorCompletions(); // 更新当前编辑器的 WikiLink 补全列表

};
#endif // MAINWINDOW_H
