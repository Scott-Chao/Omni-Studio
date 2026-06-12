#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QMap>
#include "smd/smddiagnostic.h"
#include "ai/aihistorymanager.h"
#include <QTabWidget>
#include <QSplitter>
#include <QSet>
#include <QFileInfo>
#include <QLabel>
#include <QMap>
#include <QPointer>
#include <functional>

class ActivityBar;
class TabManager;
class FileExplorerWidget;
class EditorWidget;
class SettingsManager;
class CompileRunManager;
class CodeBlockRunner;
class OpenJudgeManager;
class SettingsChangeHandler;
class QDockWidget;
class QPushButton;
class QToolButton;
class QMenu;
class RightPanelContainer;
class SearchPanel;
class BottomPanel;
class OutputPanel;
class TerminalPanel;
class JudgePanel;
class OpenJudgeWidget;
class SubmitResultPanel;
struct SubmissionResult;
class JudgeEngine;
class QStackedWidget;
class SettingsPanel;
class HelpPanel;
class AiPanel;
class AiRequestHandler;
class IndexManager;
class CrashRecoveryManager;
class TitleBarButton;

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
    void onSearchResultDoubleClicked(const QString &filePath, int lineNumber,
                                     const QString &searchText); // 打开搜索结果（双击永久）
    void onSearchTextChangedByUser(); // 搜索文本变化时清除编辑器高亮
    void onWikiLinkClicked(const QString &fileName); // 点击双向链接
    void startAsyncIndexBuild(); // 异步版本，避免大目录卡死 UI
    void buildFileIndexAsync(std::function<void()> onComplete = nullptr); // 轻量异步，仅重建文件索引
    void onFileRenamedInIndex(const QString &oldPath, const QString &newPath); // 增量更新：重命名
    void onFileDeletedInIndex(const QString &path); // 增量更新：删除
    void onFileMovedOrRenamed(const QString &oldPath, const QString &newPath); // 通过文件树进行文件移动
    void updateWikiLinksAfterRename(const QStringList &affectedSources,
                                    const QString &oldLinkText,
                                    const QString &newLinkText); // 重命名时更新所有引用的 wiki 链接文本

protected:
    void closeEvent(QCloseEvent *event) override; // 当用户关闭窗口时自动保存
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
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
    QToolButton *m_zoomOutBtn;
    QToolButton *m_zoomInBtn;
    QToolButton *m_zoomResetBtn;
    QMetaObject::Connection m_editorZoomConnection; // 用来管理当前编辑器的缩放信号连接

    void updateZoomLabel(); // 更新百分比标签
    void refreshZoomButtonStyle(); // 主题切换时刷新缩放按钮样式
    void connectCurrentEditorZoomSignal(); // 连接当前编辑器的缩放信号
    // 预览模式激活状态调整
    QAction *m_previewAction = nullptr;
    QAction *m_splitPreviewAction = nullptr;
    QAction *m_exportPdfAction = nullptr;
    void updatePreviewActionState();
    void updateSplitPreviewActionState();
    // 编译运行按钮状态 — 由 CompileRunManager 管理
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
    // 搜索面板
    SearchPanel *m_searchPanel;
    QAction *toggleSearchAction;

    // 编译运行管理
    CompileRunManager *m_compileRunMgr = nullptr;
    BottomPanel *m_bottomPanel;
    TerminalPanel *m_terminalPanel = nullptr;
    QAction *m_toggleTerminalAction = nullptr;
    QSplitter *m_rightSplitter;

    // 设置变更处理
    SettingsChangeHandler *m_settingsHandler = nullptr;

    // MD 代码块执行与诊断（▶ 按钮）
    CodeBlockRunner *m_codeBlockRunner = nullptr;

    QMetaObject::Connection m_diagnosticsProviderConnection;
    QMetaObject::Connection m_codeBlockConnection;
    QMetaObject::Connection m_fileLoadedConnection;  // current editor fileLoaded → refresh panels
    QMetaObject::Connection m_smdCellChangedConnection; // SmdEditor activeCellChanged → updateAiActionBar

    // 本地评测
    JudgePanel *m_judgePanel;
    QAction *m_toggleJudgeAction;

    // 左侧面板栈（文件夹/搜索/评测 VS Code 风格覆盖）
    QStackedWidget *m_leftStack;
    void showLeftPanel(int index);
    void toggleLeftPanel();
    int m_savedLeftPanelWidth = 220;
    QPointer<QPushButton> m_leftSearchCloseBtn;
    QPointer<QPushButton> m_leftJudgeCloseBtn;
    QAction *m_toggleExplorerAction = nullptr;

    // OpenJudge 提交管理
    OpenJudgeManager *m_openJudgeMgr = nullptr;

    // 索引管理器
    IndexManager *m_indexManager = nullptr;

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
    void updateAiActionBar();
    void filterAiHistoryByCurrentFile();
    AiPanel *m_aiPanel = nullptr;
    QDockWidget *m_dockAi = nullptr;
    QAction *m_toggleAiAction = nullptr;
    AiRequestHandler *m_aiHandler = nullptr;

    void showRightPanel(int panelIndex);

    // .md ↔ .smd 转换
    QAction *m_convertMdSmdAction = nullptr;

    // 快捷键动态映射
    QMap<QString, QAction*> m_shortcutActions;

    // Track which editors have had their scroll areas registered with ScrollbarHider
    QSet<EditorWidget*> m_editorScrollAreasRegistered;

    // 自定义标题栏控件
    TitleBarButton *m_minimizeBtn = nullptr;
    TitleBarButton *m_maximizeBtn = nullptr;
    TitleBarButton *m_closeBtn = nullptr;
    QWidget *m_toolbarSpacer = nullptr;
    QToolBar *m_toolBar = nullptr;
    bool m_toolbarDragPending = false;  // 最大化时单击→拖拽区分标志
    QToolButton *m_fileMenuBtn = nullptr;
    QMenu *m_fileMenu = nullptr;
    QAction *m_toolbarRunAction = nullptr;
    QAction *m_toolbarDropdownAction = nullptr;
    void setupCustomTitleBar();
    void refreshTitleBarStyle();

    void onJudgeRunAll();
    void onOpenJudgeRequested();
    void onConvertMdSmd();
    void toggleSettings();
    void toggleHelp();
    void applyEqualWidthTab(bool enabled);
    void convertMdToSmd(EditorWidget *editor, const QFileInfo &fi);
    void convertSmdToMd(EditorWidget *editor, const QFileInfo &fi);

    void updateCurrentEditorCompletions();
    void updateCurrentEditorDiagnostics();
    void connectSmdActiveCell();

    // 崩溃恢复
    void checkCrashRecovery();
    CrashRecoveryManager *m_crashRecovery = nullptr;

    // Wiki 链接解析
    QString findWikiTarget(const QString &fileName);

};
#endif // MAINWINDOW_H
