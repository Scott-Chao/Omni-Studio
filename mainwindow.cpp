#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "fileexplorerwidget.h"
#include "editorwidget.h"
#include "settingsmanager.h"
#include "configmanager.h"
#include "tabmanager.h"
#include "historypanel.h"
#include "backlinkindex.h"
#include "backlinkspanel.h"
#include "searchpanel.h"
#include "fileutils.h"
#include "processrunner.h"
#include "outputpanel.h"
#include "judgepanel.h"
#include "openjudgewindow.h"
#include "submissionpanel.h"
#include "compilerutils.h"
#include "languageutils.h"
#include "tagindex.h"
#include "tagpanel.h"
#include "outlinepanel.h"
#include "outlineutils.h"
#include "settingspanel.h"
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QPainter>
#include <QTextStream>
#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QStatusBar>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

// 预览调试日志 — 输出到 release 文件夹下的 preview_debug.log
static void previewLogMw(const QString &msg)
{
    static QFile logFile;
    static QTextStream stream;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        QString logPath = QCoreApplication::applicationDirPath() + QStringLiteral("/preview_debug_mw.log");
        logFile.setFileName(logPath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            stream.setDevice(&logFile);
            stream << "=== Preview Debug Log (MainWindow) ===" << Qt::endl;
            stream.flush();
        }
    }
    if (stream.device()) {
        stream << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << " " << msg << Qt::endl;
        stream.flush();
    }
}

#define PREVIEW_LOG_MW(msg) do { \
    QString _logMsg; QDebug _dbg(&_logMsg); _dbg << msg; \
    previewLogMw(_logMsg); \
} while(0)

#include <QSplitter>
#include <QVBoxLayout>
#include <QDir>
#include <QShortcut>
#include <QKeySequence>
#include <QToolBar>
#include <QAction>
#include <QStyle>
#include <QFileDialog>
#include <QHeaderView>
#include <QCloseEvent>
#include <QMessageBox>
#include <QFileInfo>
#include <QToolButton>
#include <QPushButton>
#include <QInputDialog>
#include <QWindow>
#include <utility>
#include <QDockWidget>
#include <QDirIterator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDateTime>
#include <QCoreApplication>
#include <QThread>
#include <QTimer>

namespace {
QString replaceWikiLinkText(const QString &content, const QString &oldText, const QString &newText)
{
    static const QRegularExpression wikiRegExp(
        QStringLiteral(R"(\[\[((?:[^\[\]]|\[(?1)\])*)\]\])"));

    QString result;
    int lastPos = 0;
    QRegularExpressionMatchIterator it = wikiRegExp.globalMatch(content);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        result += QStringView(content).mid(lastPos, match.capturedStart() - lastPos).toString();

        if (match.captured(1) == oldText) {
            result += QStringLiteral("[[%1]]").arg(newText);
        } else {
            result += match.captured(0);
        }
        lastPos = match.capturedEnd();
    }
    result += QStringView(content).mid(lastPos).toString();
    return result;
}
} // anonymous namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_settings(new SettingsManager("config.ini"))
    , m_explorer(new FileExplorerWidget(this))
    , m_splitter(new QSplitter(Qt::Horizontal, this))
    , m_tabManager(new TabManager(this))
    , m_zoomLabel(nullptr)
{
    ui->setupUi(this);

    // 设置窗口标题与无边框
    setWindowTitle(tr("Smart Markdown"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                   Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

    // 强制创建原生窗口句柄，添加 WS_THICKFRAME 以启用边缘缩放和 Aero Snap
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowLongPtr(hwnd, GWL_STYLE,
                     GetWindowLongPtr(hwnd, GWL_STYLE) | WS_THICKFRAME);
#endif

    // 创建历史记录面板
    m_historyPanel = new HistoryPanel(m_settings, this);
    m_historyPanel->loadHistory();
    connect(m_historyPanel, &HistoryPanel::fileClicked, this, &MainWindow::onHistoryFileClicked);

    // 将面板放入 QDockWidget，放在右侧
    m_dockHistory = new QDockWidget(tr("历史记录"), this);
    m_dockHistory->setWidget(m_historyPanel);
    m_dockHistory->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_dockHistory);

    m_dockHistory->hide(); // 默认隐藏历史记录

    // 左右侧边栏互斥：当右侧面板显示时隐藏左侧搜索面板
    connect(m_dockHistory, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible) m_dockSearch->hide();
    });

    // 工具栏最左侧插入显示/隐藏面板的按钮
    toggleHistoryAction = m_dockHistory->toggleViewAction();
    toggleHistoryAction->setToolTip(tr("显示/隐藏历史记录"));
    toggleHistoryAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_history", "Ctrl+H")));

    // 创建反向链接索引与面板
    m_backlinkIndex = new BacklinkIndex;

    m_backlinksPanel = new BacklinksPanel(this);
    connect(m_backlinksPanel, &BacklinksPanel::fileClicked, this, &MainWindow::onHistoryFileClicked);

    m_dockBacklinks = new QDockWidget(tr("反向链接"), this);
    m_dockBacklinks->setWidget(m_backlinksPanel);
    m_dockBacklinks->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_dockBacklinks);
    m_dockBacklinks->hide();

    connect(m_dockBacklinks, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible) m_dockSearch->hide();
    });

    toggleBacklinksAction = m_dockBacklinks->toggleViewAction();
    toggleBacklinksAction->setToolTip(tr("显示/隐藏反向链接"));
    toggleBacklinksAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_backlinks", "Ctrl+Shift+B")));

    // 创建搜索面板
    m_searchPanel = new SearchPanel(this);
    connect(m_searchPanel, &SearchPanel::resultClicked,
            this, &MainWindow::onSearchResultClicked);

    m_dockSearch = new QDockWidget(tr("搜索"), this);
    m_dockSearch->setWidget(m_searchPanel);
    m_dockSearch->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::LeftDockWidgetArea, m_dockSearch);
    m_dockSearch->hide();

    toggleSearchAction = m_dockSearch->toggleViewAction();
    toggleSearchAction->setToolTip(tr("显示/隐藏搜索"));
    toggleSearchAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_search", "Ctrl+Shift+F")));

    connect(m_dockSearch, &QDockWidget::visibilityChanged,
            this, [this](bool visible) {
        if (visible) {
            m_searchPanel->focusSearchInput();
            // 隐藏所有右侧面板
            m_dockHistory->hide();
            m_dockBacklinks->hide();
            m_dockTag->hide();
            m_dockOutline->hide();
            m_dockJudge->hide();
        }
    });

    // ----- 输出面板 -----
    m_outputPanel = new OutputPanel(this);
    m_outputPanel->setMinimumHeight(ConfigManager::instance().outputPanelMinHeight());
    m_outputPanel->hide();

    connect(m_outputPanel, &OutputPanel::stopRequested, this, &MainWindow::onStopProcess);
    connect(m_outputPanel, &OutputPanel::hideRequested, this, [this]() {
        if (m_processRunner->isRunning())
            onStopProcess();
        m_outputPanel->hide();
    });

    // ----- 编译运行管理器 -----
    m_processRunner = new ProcessRunner(this);
    connect(m_outputPanel, &OutputPanel::sendInput, m_processRunner, &ProcessRunner::writeInput);
    connect(m_outputPanel, &OutputPanel::sendRawInput, m_processRunner, &ProcessRunner::writeRaw);
    connect(m_processRunner, &ProcessRunner::outputReceived, m_outputPanel, &OutputPanel::appendOutput);
    connect(m_processRunner, &ProcessRunner::compileFinished, this, &MainWindow::onCompileFinished);
    connect(m_processRunner, &ProcessRunner::runFinished, this, &MainWindow::onRunFinished);
    connect(m_processRunner, &ProcessRunner::processStarted, this, [this]() {
        m_stopAction->setEnabled(true);
        m_compileAction->setEnabled(false);
        m_runAction->setEnabled(false);
        m_compileRunAction->setEnabled(false);
        // 编译阶段禁止交互（无光标、不可选），运行阶段延迟启用输入
        if (m_processRunner->isAcceptingInput()) {
            QTimer::singleShot(50, this, [this]() {
                if (m_processRunner->isRunning())
                    m_outputPanel->setRunning(true);
            });
        } else {
            m_outputPanel->enableTextSelection(false);
        }
    });
    connect(m_processRunner, &ProcessRunner::processStopped, this, [this]() {
        m_stopAction->setEnabled(false);
        m_outputPanel->setRunning(false);
        // 恢复文本选择，将焦点移至编辑器，下次运行需手动点击终端
        m_outputPanel->enableTextSelection(true);
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor)
            editor->setFocus();
        // Re-enable buttons based on current tab
        bool isCode = editor && editor->isCodeEdit();
        m_compileAction->setVisible(isCode);
        m_compileAction->setEnabled(isCode);
        m_runAction->setVisible(isCode);
        m_runAction->setEnabled(isCode);
        m_compileRunAction->setVisible(isCode);
        m_compileRunAction->setEnabled(isCode);
    });

    // ----- 本地评测面板 -----
    m_judgePanel = new JudgePanel(this);
    m_dockJudge = new QDockWidget(tr("代码评测"), this);
    m_dockJudge->setWidget(m_judgePanel);
    m_dockJudge->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_dockJudge);
    m_dockJudge->hide();

    connect(m_dockJudge, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible) m_dockSearch->hide();
    });

    m_toggleJudgeAction = m_dockJudge->toggleViewAction();
    m_toggleJudgeAction->setToolTip(tr("显示/隐藏代码评测"));
    m_toggleJudgeAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_judge", "Ctrl+Shift+J")));

    connect(m_judgePanel, &JudgePanel::runAllRequested,
            this, &MainWindow::onJudgeRunAll);
    connect(m_judgePanel, &JudgePanel::openJudgeRequested,
            this, &MainWindow::onOpenJudgeRequested);
    connect(m_judgePanel, &JudgePanel::submitToOpenJudgeRequested,
            this, &MainWindow::onSubmitToOpenJudge);

    // ----- 标签面板 -----
    m_tagIndex = new TagIndex;

    m_tagPanel = new TagPanel(this);
    connect(m_tagPanel, &TagPanel::fileClicked, this, &MainWindow::onHistoryFileClicked);
    connect(m_tagPanel, &TagPanel::tagClicked, this, &MainWindow::onTagClicked);

    m_dockTag = new QDockWidget(tr("标签"), this);
    m_dockTag->setWidget(m_tagPanel);
    m_dockTag->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_dockTag);
    m_dockTag->hide();

    connect(m_dockTag, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible) m_dockSearch->hide();
    });

    toggleTagAction = m_dockTag->toggleViewAction();
    toggleTagAction->setToolTip(tr("显示/隐藏标签"));
    toggleTagAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_tags", "Ctrl+Shift+T")));

    // ----- 大纲面板 -----
    m_outlinePanel = new OutlinePanel(this);
    connect(m_outlinePanel, &OutlinePanel::headingClicked, this, [this](int line, const QString &) {
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor)
            editor->navigateToLine(line);
    });

    m_dockOutline = new QDockWidget(tr("大纲"), this);
    m_dockOutline->setWidget(m_outlinePanel);
    m_dockOutline->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_dockOutline);
    m_dockOutline->hide();

    connect(m_dockOutline, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible) m_dockSearch->hide();
    });

    toggleOutlineAction = m_dockOutline->toggleViewAction();
    toggleOutlineAction->setToolTip(tr("显示/隐藏大纲"));
    toggleOutlineAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_outline", "Ctrl+Shift+O")));

    // ----- 工具栏 -----
    QToolBar *toolBar = addToolBar("文件工具栏");
    toolBar->setMovable(false);
    toolBar->setFloatable(false);

    // 设置
    m_settingsAction = new QAction(tr("设置"), this);
    m_settingsAction->setShortcut(
        QKeySequence(ConfigManager::instance().shortcut("toggle_settings", "Ctrl+,")));
    m_settingsAction->setToolTip(tr("打开/关闭设置面板 (Ctrl+,)"));
    addAction(m_settingsAction);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::toggleSettings);
    toolBar->insertAction(nullptr, m_settingsAction);

    // 历史记录
    toolBar->insertAction(nullptr, toggleHistoryAction);
    // 反向链接
    toolBar->insertAction(nullptr, toggleBacklinksAction);
    // 搜索
    toolBar->insertAction(nullptr, toggleSearchAction);
    // 标签
    toolBar->insertAction(nullptr, toggleTagAction);
    // 大纲
    toolBar->insertAction(nullptr, toggleOutlineAction);
    // 代码评测
    toolBar->insertAction(nullptr, m_toggleJudgeAction);
    toolBar->insertSeparator(toggleHistoryAction);

    // 打开目录
    QAction *openDirAction = new QAction("打开目录", this);
    toolBar->addAction(openDirAction);
    connect(openDirAction, &QAction::triggered, this, &MainWindow::onOpenFolder); // 点击按钮
    connect(m_explorer, &FileExplorerWidget::folderChanged, this, &MainWindow::onFolderChanged); // 响应打开路径改变

    toolBar->addSeparator();

    // 新建文件（快捷键 Ctrl+N）
    QAction *newAction = new QAction("新建", this);
    newAction->setShortcut(QKeySequence::New);
    toolBar->addAction(newAction);
    connect(newAction, &QAction::triggered, this, &MainWindow::newFile);

    toolBar->addSeparator();

    // 保存（快捷键 Ctrl+S）
    QAction *saveAction = new QAction("保存", this);
    saveAction->setShortcut(QKeySequence::Save);
    addAction(saveAction);
    toolBar->addAction(saveAction);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);

    toolBar->addSeparator();

    // 另存为（快捷键Ctrl+Shift+S）
    QAction *saveAsAction = new QAction("另存为", this);
    saveAsAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("save_as", "Ctrl+Shift+S")));
    addAction(saveAsAction);
    toolBar->addAction(saveAsAction);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::onSaveFileAs);

    // 导出PDF（快捷键Ctrl+E）
    m_exportPdfAction = new QAction("导出PDF", this);
    m_exportPdfAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("export_pdf", "Ctrl+E")));
    addAction(m_exportPdfAction);
    toolBar->addAction(m_exportPdfAction);
    connect(m_exportPdfAction, &QAction::triggered, this, &MainWindow::onExportPdf);

    toolBar->addSeparator();

    // 预览（快捷键Ctrl+Shift+P）
    m_previewAction = new QAction("预览模式", this);
    m_previewAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_preview", "Ctrl+Shift+P")));
    m_previewAction->setCheckable(true);
    toolBar->addAction(m_previewAction);
    connect(m_previewAction, &QAction::toggled, this, [this](bool checked) {
        PREVIEW_LOG_MW("previewAction toggled, checked=" << checked);
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor) {
            if (checked && !editor->currentFilePath().toLower().endsWith(".md")) {
                PREVIEW_LOG_MW("previewAction — 跳过(非.md文件)");
                return;
            }
            PREVIEW_LOG_MW("previewAction — 调用 editor->setPreviewMode(" << checked << ")");
            editor->setPreviewMode(checked);
            if (checked && m_splitPreviewAction && m_splitPreviewAction->isChecked()) {
                m_splitPreviewAction->blockSignals(true);
                m_splitPreviewAction->setChecked(false);
                m_splitPreviewAction->blockSignals(false);
            }
        }
    });

    // 分屏预览（快捷键Ctrl+P）
    m_splitPreviewAction = new QAction(tr("分屏预览"), this);
    m_splitPreviewAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_split_preview", "Ctrl+P")));
    m_splitPreviewAction->setCheckable(true);
    addAction(m_splitPreviewAction);
    toolBar->addAction(m_splitPreviewAction);
    connect(m_splitPreviewAction, &QAction::toggled, this, [this](bool checked) {
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor) {
            if (checked && !editor->currentFilePath().toLower().endsWith(".md"))
                return;
            editor->setSplitPreviewMode(checked);
            if (checked && m_previewAction && m_previewAction->isChecked()) {
                m_previewAction->blockSignals(true);
                m_previewAction->setChecked(false);
                m_previewAction->blockSignals(false);
            }
        }
    });

    toolBar->addSeparator();

    // 编译 (F6)
    m_compileAction = new QAction(tr("编译"), this);
    m_compileAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("compile_only", "F6")));
    addAction(m_compileAction);
    m_compileAction->setVisible(false);
    toolBar->addAction(m_compileAction);
    connect(m_compileAction, &QAction::triggered, this, &MainWindow::onCompile);

    // 运行 (F7)
    m_runAction = new QAction(tr("运行"), this);
    m_runAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("run_only", "F7")));
    addAction(m_runAction);
    m_runAction->setVisible(false);
    toolBar->addAction(m_runAction);
    connect(m_runAction, &QAction::triggered, this, &MainWindow::onRun);

    // 编译运行 (F5)
    m_compileRunAction = new QAction(tr("编译运行"), this);
    m_compileRunAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("compile_and_run", "F5")));
    addAction(m_compileRunAction);
    m_compileRunAction->setVisible(false);
    toolBar->addAction(m_compileRunAction);
    connect(m_compileRunAction, &QAction::triggered, this, &MainWindow::onCompileAndRun);

    // 终止 (Ctrl+Break) — 仅快捷键，不放在工具栏
    m_stopAction = new QAction(tr("终止"), this);
    m_stopAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("stop_process", "Ctrl+Break")));
    addAction(m_stopAction);
    m_stopAction->setEnabled(false);
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::onStopProcess);

    // 将工具栏改造为自定义标题栏
    setupCustomTitleBar();

    // 添加缩放项
    QStatusBar *status = statusBar();

    // 创建缩放相关的 QAction
    m_zoomOutAction = new QAction(tr("缩小"), this);
    m_zoomOutAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("zoom_out", "Ctrl+-")));
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::onZoomOut);

    m_zoomInAction = new QAction(tr("放大"), this);
    m_zoomInAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("zoom_in", "Ctrl+=")));
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::onZoomIn);

    m_zoomResetAction = new QAction(tr("重置缩放"), this);
    m_zoomResetAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("zoom_reset", "Ctrl+0")));
    connect(m_zoomResetAction, &QAction::triggered, this, &MainWindow::onZoomReset);

    // 创建缩放百分比标签
    m_zoomLabel = new QLabel("100%");
    m_zoomLabel->setMinimumWidth(50);
    m_zoomLabel->setAlignment(Qt::AlignCenter);

    // 把 QAction 包装成 QToolButton，便于放入布局
    QToolButton *zoomOutBtn = new QToolButton;
    zoomOutBtn->setDefaultAction(m_zoomOutAction);
    zoomOutBtn->setText("-");

    QToolButton *zoomInBtn = new QToolButton;
    zoomInBtn->setDefaultAction(m_zoomInAction);
    zoomInBtn->setText("+");

    QToolButton *zoomResetBtn = new QToolButton;
    zoomResetBtn->setDefaultAction(m_zoomResetAction);
    zoomResetBtn->setText("重置");

    // 将按钮和标签放入一个水平布局的 Widget
    QWidget *zoomWidget = new QWidget();
    QHBoxLayout *zoomLayout = new QHBoxLayout(zoomWidget);
    zoomLayout->setContentsMargins(0, 0, 0, 0);
    zoomLayout->addWidget(zoomOutBtn);
    zoomLayout->addWidget(m_zoomLabel);
    zoomLayout->addWidget(zoomInBtn);
    zoomLayout->addWidget(zoomResetBtn);

    // 添加到状态栏
    status->addPermanentWidget(zoomWidget);

    // 当切换标签页时，更新预览按钮的选中状态
    connect(m_tabManager, &QTabWidget::currentChanged, this, &MainWindow::updatePreviewActionState);
    connect(m_tabManager, &QTabWidget::currentChanged, this, &MainWindow::updateSplitPreviewActionState);

    // ----- 设置面板（悬浮遮罩 + 面板）-----
    m_settingsOverlay = new QWidget(this);
    m_settingsOverlay->setObjectName("settingsOverlay");
    m_settingsOverlay->setStyleSheet(
        "#settingsOverlay { background-color: rgba(0, 0, 0, 128); }"
    );
    m_settingsOverlay->hide();

    m_settingsPanel = new SettingsPanel(m_settingsOverlay);
    connect(m_settingsPanel, &SettingsPanel::closeRequested, this, &MainWindow::toggleSettings);
    connect(m_settingsPanel, &SettingsPanel::defaultZoomChanged, this, &MainWindow::onDefaultZoomChanged);
    connect(m_settingsPanel, &SettingsPanel::editorSettingChanged, this, &MainWindow::onEditorSettingChanged);
    connect(m_settingsPanel, &SettingsPanel::appearanceSettingChanged, this, &MainWindow::onAppearanceSettingChanged);
    connect(m_settingsPanel, &SettingsPanel::outputPanelSettingChanged, this, &MainWindow::onOutputPanelSettingChanged);
    connect(m_settingsPanel, &SettingsPanel::previewSettingChanged, this, &MainWindow::onPreviewSettingChanged);
    connect(m_settingsPanel, &SettingsPanel::searchSettingChanged, this, &MainWindow::onSearchSettingChanged);
    connect(m_settingsPanel, &SettingsPanel::resetToDefaultsRequested, this, &MainWindow::onResetToDefaults);

    // ----- 界面布局 -----
    // 设置 TabManager 的样式（原有样式保留，可进一步调整）
    {
        const auto &cfg = ConfigManager::instance();
        m_tabManager->setStyleSheet(
            QString(
                "QTabBar::tab {"
                "   height: %1px;"
                "   margin-right: %2px;"
                "   padding: %3px %4px;"
                "   border-top-left-radius: %5px;"
                "   border-top-right-radius: %5px;"
                "}"
                "QTabBar::tab:selected {"
                "   background: %6;"
                "   color: %7;"
                "}"
                "QTabBar::tab:hover:!selected {"
                "   background: %8;"
                "}"
            )
            .arg(22) // height
            .arg(2)  // margin-right
            .arg(4)  // padding top/bottom
            .arg(12) // padding left/right
            .arg(4)  // border-radius
            .arg(cfg.previewContainerBackground().name()) // selected bg (#2d2d2d)
            .arg("#ffffff")                               // selected text
            .arg("#4a4a4a")                               // hover bg
        );
    }

    // 右侧垂直分割线：编辑器在上，输出面板在下
    m_rightSplitter = new QSplitter(Qt::Vertical, this);
    m_rightSplitter->addWidget(m_tabManager);
    m_rightSplitter->addWidget(m_outputPanel);
    m_rightSplitter->setStretchFactor(0, ConfigManager::instance().rightSplitterEditorStretch());
    m_rightSplitter->setStretchFactor(1, ConfigManager::instance().rightSplitterOutputStretch());

    m_splitter->addWidget(m_explorer);
    m_splitter->addWidget(m_rightSplitter);
    setCentralWidget(m_splitter);

    // 当标签页切换时，更新缩放标签并重新连接当前编辑器的缩放信号
    connect(m_tabManager, &QTabWidget::currentChanged, this, [this](int) {
        updateZoomLabel();
        connectCurrentEditorZoomSignal();
        syncFileTreeSelection();
        refreshBacklinks();
        refreshTags();
        refreshOutline();
        updateCurrentEditorCompletions();

        // 更新编译运行按钮状态
        EditorWidget *editor = m_tabManager->currentEditor();
        bool isCode = editor && editor->isCodeEdit();
        bool running = m_processRunner->isRunning();
        m_compileAction->setVisible(isCode);
        m_compileAction->setEnabled(isCode && !running);
        m_runAction->setVisible(isCode);
        m_runAction->setEnabled(isCode && !running);
        m_compileRunAction->setVisible(isCode);
        m_compileRunAction->setEnabled(isCode && !running);

        // 导出PDF按钮仅对 .md 文件可见
        bool isMd = editor && editor->currentFilePath().toLower().endsWith(".md");
        m_exportPdfAction->setVisible(isMd);
        m_exportPdfAction->setEnabled(isMd);
    });

    // 初始连接
    connectCurrentEditorZoomSignal();
    updateZoomLabel();

    // 连接信号：文件树点击 -> 打开文件
    connect(m_explorer, &FileExplorerWidget::fileClicked, this, &MainWindow::onFileSelected);

    // 将重命名、删除的请求直接转发给 FileExplorerWidget 的内部槽
    connect(m_explorer, &FileExplorerWidget::requestDelete, this, &MainWindow::onRequestDelete);
    // 监听删除成功信号
    connect(m_explorer, &FileExplorerWidget::itemDeleted, this, &MainWindow::onFileDeletedInIndex);
    // 监听操作失败信号，显示错误提示
    connect(m_explorer, &FileExplorerWidget::operationFailed, this, [this](const QString &errorMsg) {
        QMessageBox::warning(this, tr("错误"), errorMsg);
    });
    connect(m_explorer, &FileExplorerWidget::fileRenamed,
            this, &MainWindow::onFileMovedOrRenamed);
    qApp->installEventFilter(this);

    loadSettings();
    checkCrashRecovery(); // 检测崩溃恢复文件
    m_searchPanel->setRootPath(m_explorer->rootPath());
    startAsyncIndexBuild();
    updatePreviewActionState();
    updateSplitPreviewActionState();
}

MainWindow::~MainWindow()
{
    if (m_scanCancelled)
        m_scanCancelled->store(true);
    delete ui;
}

// ----- 转发给TabManager的槽函数 -----
void MainWindow::onFileSelected(const QString &filePath)
{
    m_tabManager->openFile(filePath);
    if (auto *editor = m_tabManager->currentEditor()) {
        editor->setZoomFactor(m_settings->editorDefaultZoom());
    }
    updateZoomLabel();
    updatePreviewActionState();
    updateSplitPreviewActionState();
    addToRecentFiles(filePath);
}

void MainWindow::newFile()
{
    EditorWidget *newEditor = m_tabManager->newFile();
    if (newEditor) {
        newEditor->setZoomFactor(m_settings->editorDefaultZoom());
        updateZoomLabel();
    }
    updatePreviewActionState();
    updateSplitPreviewActionState();
}

void MainWindow::saveFile()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor)
        return;
    if (editor->currentFilePath().isEmpty()) {
        onSaveFileAs(); // 无路径情况下另存为，更新记忆路径
    } else {
        // 保存已有文件，不修改另存为记忆
        if (editor->saveFile()) {
            m_backlinkIndex->rebuildFile(editor->currentFilePath(),
                                         m_explorer->rootPath(), m_fileIndex);
            m_tagIndex->rebuildFile(editor->currentFilePath());
            updatePreviewActionState(); // 刷新预览按钮状态
            updateSplitPreviewActionState();
            addToRecentFiles(editor->currentFilePath());
            refreshBacklinks(); // 保存后更新反链
            refreshTags();
            refreshOutline();
        }
    }
}


void MainWindow::onSaveFileAs()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor) return;
    QString lastSaveDir = m_settings->lastSaveAsFolderPath(QDir::homePath()); // 读取上次另存为目录

    if (editor->saveAsFile(lastSaveDir)) {
        // 保存成功，获取新目录并立即写回配置
        QString newFilePath = editor->currentFilePath();
        QString newDir = QFileInfo(newFilePath).absolutePath();
        if (!newDir.isEmpty()) {
            m_settings->setLastSaveAsFolderPath(newDir);
        }
        buildFileIndex();
        m_backlinkIndex->rebuildFile(newFilePath, m_explorer->rootPath(), m_fileIndex);
        m_tagIndex->rebuildFile(newFilePath);
        updatePreviewActionState();
        updateSplitPreviewActionState();
        addToRecentFiles(newFilePath);
        refreshBacklinks();
        refreshTags();
    }
}

void MainWindow::onExportPdf()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor) return;

    // 构造默认文件名和路径
    QString defaultName = QStringLiteral("untitled.pdf");
    QString lastDir = m_settings->lastSaveAsFolderPath(QDir::homePath());
    if (!editor->currentFilePath().isEmpty()) {
        QFileInfo info(editor->currentFilePath());
        defaultName = info.completeBaseName() + QStringLiteral(".pdf");
        lastDir = info.absolutePath();
    }

    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("导出为PDF"),
        lastDir + QStringLiteral("/") + defaultName,
        tr("PDF文件 (*.pdf)"));

    if (filePath.isEmpty())
        return;

    // 记忆导出目录
    m_settings->setLastSaveAsFolderPath(QFileInfo(filePath).absolutePath());

    // A4 纵向，20mm 侧边距，25mm 上下边距
    QPageLayout pageLayout(
        QPageSize(QPageSize::A4),
        QPageLayout::Portrait,
        QMarginsF(20, 25, 20, 25),
        QPageLayout::Millimeter);

    // 单次连接导出完成信号
    connect(editor, &EditorWidget::pdfExportCompleted, this,
        [this](const QString &path, bool success) {
            if (success) {
                statusBar()->showMessage(
                    tr("PDF 导出成功: %1").arg(QFileInfo(path).fileName()), 5000);
            } else {
                QMessageBox::warning(this, tr("导出失败"),
                    tr("无法导出 PDF：%1").arg(path));
            }
        },
        Qt::SingleShotConnection);

    editor->exportToPdf(filePath, pageLayout);
}

// ----- 转发给FileExplorerWidget的槽函数 -----
void MainWindow::onOpenFolder()
{
    m_explorer->selectFolder(m_settings->lastFolderPath(QDir::homePath())); // 将记忆目录传给 selectFolder，对话框将从这里开始浏览
}

void MainWindow::onFolderChanged(const QString &newPath)
{
    m_settings->setLastFolderPath(newPath); // 立即持久化
    startAsyncIndexBuild();
    m_searchPanel->setRootPath(newPath);
    syncFileTreeSelection(); // 若当前编辑文件在新根目录内，自动选中
}

// ----- 配置读写 -----
void MainWindow::saveSettings()
{
    m_settings->setLastFolderPath(m_explorer->rootPath());
    m_settings->setWindowGeometry(saveGeometry());
    m_settings->setSplitterState(m_splitter->saveState());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 先尝试关闭所有标签页。如果用户取消了任何一个，则阻止窗口关闭。
    if (!m_tabManager->closeAllTabs()) {
        event->ignore();   // 不关闭窗口
        return;
    }
    // 所有标签都已安全关闭，保存配置并退出
    m_historyPanel->saveHistory();
    m_settings->flushOverrides();
    saveSettings();
    clearRecoveryDirectory(); // 正常关闭，清理恢复目录
    if (m_openJudgeWindow)
        m_openJudgeWindow->close();
    event->accept();
}

void MainWindow::loadSettings()
{
    // 载入配置信息
    QString lastPath = m_settings->lastFolderPath();
    m_explorer->setRootPath(lastPath);

    const auto &cfg = ConfigManager::instance();
    QByteArray geometryData = m_settings->windowGeometry();
    if (!geometryData.isEmpty()) {
        restoreGeometry(geometryData);
    } else {
        resize(cfg.mainWindowDefaultWidth(), cfg.mainWindowDefaultHeight());
        move(cfg.mainWindowDefaultX(), cfg.mainWindowDefaultY());
    }

    QByteArray splitterData = m_settings->splitterState();
    if (!splitterData.isEmpty()) {
        m_splitter->restoreState(splitterData);
    } else {
        m_splitter->setStretchFactor(1, cfg.mainSplitterDefaultRatio());
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_settingsOverlay && m_settingsOverlay->isVisible()) {
        m_settingsOverlay->setGeometry(this->rect());
        // Clamp panel position to stay within new overlay bounds
        QPoint panelPos = m_settingsPanel->pos();
        panelPos.setX(qBound(0, panelPos.x(), m_settingsOverlay->width() - m_settingsPanel->width()));
        panelPos.setY(qBound(0, panelPos.y(), m_settingsOverlay->height() - m_settingsPanel->height()));
        m_settingsPanel->move(panelPos);
    }
}

void MainWindow::toggleSettings()
{
    if (!m_settingsOverlay || !m_settingsPanel)
        return;

    if (m_settingsOverlay->isVisible()) {
        m_settingsOverlay->hide();
    } else {
        m_settingsOverlay->setGeometry(this->rect());
        m_settingsOverlay->raise();

        m_settingsPanel->setDefaultZoom(m_settings->editorDefaultZoom());
        m_settingsPanel->syncFromSettings(*m_settings);

        int panelW = m_settingsPanel->width();
        int panelH = m_settingsPanel->height();
        int x = (m_settingsOverlay->width() - panelW) / 2;
        int y = (m_settingsOverlay->height() - panelH) / 2;
        m_settingsPanel->move(qMax(0, x), qMax(0, y));
        m_settingsPanel->raise();

        m_settingsOverlay->show();
    }
}

void MainWindow::onDefaultZoomChanged(qreal zoom)
{
    m_settings->setEditorDefaultZoom(zoom);

    // 将新的默认缩放应用到所有已打开的编辑器
    for (int i = 0; i < m_tabManager->count(); ++i) {
        if (auto *editor = qobject_cast<EditorWidget*>(m_tabManager->widget(i))) {
            editor->setZoomFactor(zoom);
        }
    }
    updateZoomLabel();
}

void MainWindow::onEditorSettingChanged(const QString &key, const QVariant &value)
{
    m_settings->setSettingOverride(key, value);

    if (key == "editor.indent_width") {
        int width = value.toInt();
        for (int i = 0; i < m_tabManager->count(); ++i) {
            if (auto *editor = qobject_cast<EditorWidget*>(m_tabManager->widget(i))) {
                if (editor->isCodeEdit())
                    editor->setCodeIndentWidth(width);
            }
        }
    } else if (key == "editor.font.family") {
        QString family = value.toString();
        int size = m_settings->settingOverride("editor.font.size",
                     ConfigManager::instance().editorFontSize()).toInt();
        for (int i = 0; i < m_tabManager->count(); ++i) {
            if (auto *editor = qobject_cast<EditorWidget*>(m_tabManager->widget(i))) {
                editor->setEditorFont(family, size);
            }
        }
    } else if (key == "editor.font.size") {
        int size = value.toInt();
        QString family = m_settings->settingOverride("editor.font.family",
                           ConfigManager::instance().editorFontFamily()).toString();
        for (int i = 0; i < m_tabManager->count(); ++i) {
            if (auto *editor = qobject_cast<EditorWidget*>(m_tabManager->widget(i))) {
                editor->setEditorFont(family, size);
            }
        }
    } else if (key == "editor.auto_save") {
        bool enabled = value.toBool();
        for (int i = 0; i < m_tabManager->count(); ++i) {
            if (auto *editor = qobject_cast<EditorWidget*>(m_tabManager->widget(i))) {
                editor->setAutoSaveEnabled(enabled);
            }
        }
    }
}

void MainWindow::onAppearanceSettingChanged(const QString &key, const QVariant &value)
{
    m_settings->setSettingOverride(key, value);

    for (int i = 0; i < m_tabManager->count(); ++i) {
        if (auto *editor = qobject_cast<EditorWidget*>(m_tabManager->widget(i))) {
            editor->reloadEditorColors();
        }
    }
}

void MainWindow::onOutputPanelSettingChanged(const QString &key, const QVariant &value)
{
    m_settings->setSettingOverride(key, value);

    if (key == "output_panel.font.size") {
        QFont font = m_outputPanel->font();
        font.setPointSize(value.toInt());
        m_outputPanel->setOutputFont(font);
    } else if (key == "output_panel.max_blocks") {
        m_outputPanel->setMaxBlocks(value.toInt());
    }
}

void MainWindow::onPreviewSettingChanged(const QString &key, const QVariant &value)
{
    m_settings->setSettingOverride(key, value);

    if (key == "preview.split_debounce_ms") {
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor)
            editor->setSplitPreviewDebounceMs(value.toInt());
    } else if (key == "preview.split_preview_ratio") {
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor)
            editor->applySplitPreviewRatio();
    }
}

void MainWindow::onSearchSettingChanged(const QString &key, const QVariant &value)
{
    m_settings->setSettingOverride(key, value);
    // Search settings take effect on next search operation
}

void MainWindow::onResetToDefaults()
{
    const auto &cfg = ConfigManager::instance();

    // Clear all setting overrides
    for (const QString &key : m_settings->allOverrideKeys())
        m_settings->removeSettingOverride(key);
    m_settings->setEditorDefaultZoom(cfg.zoomDefault());
    m_settings->flushOverrides();

    // Reset settings panel controls to defaults
    m_settingsPanel->syncFromSettings(*m_settings);

    // Apply default zoom + editor font to all editors
    for (int i = 0; i < m_tabManager->count(); ++i) {
        if (auto *editor = qobject_cast<EditorWidget*>(m_tabManager->widget(i))) {
            editor->setZoomFactor(cfg.zoomDefault());
            editor->setEditorFont(cfg.editorFontFamily(), cfg.editorFontSize());
            if (editor->isCodeEdit())
                editor->setCodeIndentWidth(cfg.editorIndentWidth());
            editor->reloadEditorColors();
        }
    }
    updateZoomLabel();

    // Reset output panel
    QFont opFont = m_outputPanel->font();
    opFont.setPointSize(cfg.outputPanelFontSize());
    m_outputPanel->setOutputFont(opFont);
    m_outputPanel->setMaxBlocks(cfg.outputPanelMaxBlocks());

    // Reset preview settings on current editor
    if (auto *editor = m_tabManager->currentEditor()) {
        editor->setSplitPreviewDebounceMs(cfg.previewSplitDebounceMs());
    }
}

// 缩放相关槽函数
void MainWindow::onZoomIn()
{
    if (auto *editor = m_tabManager->currentEditor()) {
        editor->zoomIn();
    }
}

void MainWindow::onZoomOut()
{
    if (auto *editor = m_tabManager->currentEditor()) {
        editor->zoomOut();
    }
}

void MainWindow::onZoomReset()
{
    if (auto *editor = m_tabManager->currentEditor()) {
        editor->setZoomFactor(m_settings->editorDefaultZoom());
    }
}

void MainWindow::updateZoomLabel()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (editor) {
        int percent = qRound(editor->zoomFactor() * 100);
        m_zoomLabel->setText(QStringLiteral("%1%").arg(percent));
    } else {
        m_zoomLabel->setText(QStringLiteral("100%"));
    }
}

void MainWindow::connectCurrentEditorZoomSignal()
{
    disconnect(m_editorZoomConnection); // 断开前一个编辑器的缩放信号连接
    disconnect(m_codeBlockConnection); // 断开前一个编辑器的代码块信号连接
    EditorWidget *editor = m_tabManager->currentEditor();
    if (editor) {
        m_editorZoomConnection = connect(editor, &EditorWidget::zoomFactorChanged, this, &MainWindow::updateZoomLabel); // 监听缩放
        connect(editor, &EditorWidget::filePathChanged, this,
                &MainWindow::updatePreviewActionState, Qt::UniqueConnection); // 监听路径变化
        connect(editor, &EditorWidget::wikiLinkClicked, this,
                &MainWindow::onWikiLinkClicked, Qt::UniqueConnection);
        m_codeBlockConnection = connect(editor, &EditorWidget::runCodeBlockRequested,
                                        this, &MainWindow::onCodeBlockRequested);
        connect(editor, &EditorWidget::tagClicked, this, &MainWindow::onTagClicked,
                Qt::UniqueConnection);
    }
}

void MainWindow::syncFileTreeSelection()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor)
        return;
    QString filePath = editor->currentFilePath();
    if (filePath.isEmpty())
        return;
    m_explorer->selectFile(filePath);
}

void MainWindow::onRequestDelete(const QString &path, bool isDir)
{
    QFileInfo info(path);
    QString type = isDir ? tr("文件夹") : tr("文件");
    QString msg;

    // 构建确认消息
    if (isDir) {
        // 检查是否有未保存的子文件，以便在消息中添加警告
        QStringList openedPaths = m_tabManager->allOpenedFilePaths();
        QString dirPrefix = QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()) + "/";
        bool hasUnsaved = false;
        for (const QString &opened : std::as_const(openedPaths)) {
            QString openedNormalized = QDir::fromNativeSeparators(opened);
            if (openedNormalized.startsWith(dirPrefix, Qt::CaseInsensitive)) {
                EditorWidget *editor = m_tabManager->findEditorByPath(opened);
                if (editor && editor->isModified()) {
                    hasUnsaved = true;
                    break;
                }
            }
        }
        if (hasUnsaved) {
            msg = tr("文件夹 \"%1\" 中包含未保存的修改。\n"
                     "继续删除将丢失这些更改。\n\n"
                     "确定要删除该文件夹及其所有内容吗？")
                      .arg(info.fileName());
        } else {
            msg = tr("确定要删除文件夹 \"%1\" 及其所有内容吗？\n此操作不可撤销。")
                      .arg(info.fileName());
        }
    } else {
        // 对于文件，检查是否已打开且已修改
        EditorWidget *editor = m_tabManager->findEditorByPath(path);
        if (editor && editor->isModified()) {
            msg = tr("文件 \"%1\" 有未保存的修改。\n"
                     "继续删除将丢失这些更改。\n\n"
                     "确定要删除吗？")
                      .arg(info.fileName());
        } else {
            msg = tr("确定要删除文件 \"%1\" 吗？\n此操作不可撤销。")
                      .arg(info.fileName());
        }
    }

    // 弹出确认对话框
    int ret = QMessageBox::question(this, tr("确认删除"), msg,
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    // 标准化路径
    QString normalizedPath = QFileInfo(path).absoluteFilePath();

    // 关闭相关标签页（强制不保存）
    if (isDir) {
        QStringList openedPaths = m_tabManager->allOpenedFilePaths();
        QStringList toClose;
        QString dirPrefix = QDir::fromNativeSeparators(normalizedPath) + "/";
        for (const QString &opened : std::as_const(openedPaths)) {
            QString openedNormalized = QDir::fromNativeSeparators(opened);
            if (openedNormalized.startsWith(dirPrefix, Qt::CaseInsensitive))
                toClose.append(opened);
        }
        // 强制关闭（不保存），不弹出保存对话框
        for (const QString &filePath : std::as_const(toClose)) {
            m_tabManager->closeTabByPath(filePath, false);
        }
    } else {
        // 强制关闭单个文件（不保存）
        m_tabManager->closeTabByPath(normalizedPath, false);
    }

    // 执行删除
    m_explorer->deleteItem(path, isDir);
}

void MainWindow::updatePreviewActionState()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor) {
        m_previewAction->setVisible(false);
        m_previewAction->setEnabled(false);
        return;
    }

    QString filePath = editor->currentFilePath();
    bool isMd = filePath.toLower().endsWith(".md");

    if (editor->isSmdEdit()) {
        m_previewAction->setVisible(false);
        m_previewAction->setEnabled(false);
        return;
    }

    m_previewAction->setVisible(isMd);
    m_previewAction->setEnabled(isMd);

    if (!isMd && editor->isPreviewMode()) {
        editor->setPreviewMode(false);
        m_previewAction->setChecked(false);
    } else if (isMd) {
        m_previewAction->setChecked(editor->isPreviewMode());
    }
}

void MainWindow::updateSplitPreviewActionState()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor) {
        m_splitPreviewAction->setVisible(false);
        m_splitPreviewAction->setEnabled(false);
        return;
    }

    if (editor->isSmdEdit()) {
        m_splitPreviewAction->setVisible(false);
        m_splitPreviewAction->setEnabled(false);
        return;
    }

    QString filePath = editor->currentFilePath();
    bool isMd = filePath.toLower().endsWith(".md");

    m_splitPreviewAction->setVisible(isMd);
    m_splitPreviewAction->setEnabled(isMd);

    if (!isMd && editor->isSplitPreviewMode()) {
        editor->setSplitPreviewMode(false);
        m_splitPreviewAction->setChecked(false);
    } else if (isMd) {
        m_splitPreviewAction->setChecked(editor->isSplitPreviewMode());
    }
}

void MainWindow::addToRecentFiles(const QString &filePath)
{
    QString cleanPath = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
    if (!cleanPath.isEmpty())
        m_historyPanel->addFile(cleanPath);
}

void MainWindow::refreshBacklinks()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor || editor->currentFilePath().isEmpty()) {
        m_backlinksPanel->showBacklinks({});
        return;
    }

    QString filePath = editor->currentFilePath();
    QStringList sources = m_backlinkIndex->backlinksFor(filePath);
    m_backlinksPanel->showBacklinks(sources);
}

void MainWindow::refreshTags()
{
    QStringList tags = m_tagIndex->allTags();
    tags.sort(Qt::CaseInsensitive);
    m_tagPanel->showAllTags(tags);
}

void MainWindow::refreshOutline()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor || !editor->currentFilePath().toLower().endsWith(QStringLiteral(".md"))) {
        m_outlinePanel->clear();
        return;
    }
    QString content = editor->toPlainText();
    auto headings = extractHeadingsFromContent(content);
    m_outlinePanel->showHeadings(headings);
}

void MainWindow::onTagClicked(const QString &tag)
{
    QStringList files = m_tagIndex->filesForTag(tag);
    files.sort();
    m_tagPanel->showFilesForTag(tag, files);
    m_dockTag->show();
    m_dockTag->raise();
}

void MainWindow::onHistoryFileClicked(const QString &filePath)
{
    if (!QFile::exists(filePath)) {
        QMessageBox::warning(this, tr("文件不存在"),
                             tr("无法打开文件，文件可能已被移动或删除：\n%1").arg(filePath));
        m_historyPanel->removeFile(filePath);
        return;
    }

    EditorWidget *editor = m_tabManager->openFile(filePath);
    if (!editor) return;

    // ---- 判断是否需要切换文件树根目录 ----
    QString absolutePath = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
    QDir rootDir(m_explorer->rootPath());
    QString relative = rootDir.relativeFilePath(absolutePath);

    // 如果相对路径以 ".." 开头，说明文件在当前根目录之外
    if (relative.startsWith("..")) {
        QString newRoot = QFileInfo(absolutePath).absolutePath();
        m_explorer->setRootPath(newRoot);
        m_settings->setLastFolderPath(newRoot);
        syncFileTreeSelection();
    }
    addToRecentFiles(absolutePath); // 记录到历史（置顶）
    m_dockHistory->hide(); // 隐藏面板
}

void MainWindow::onSearchResultClicked(const QString &filePath,
                                        int lineNumber,
                                        const QString &searchText)
{
    if (!QFile::exists(filePath)) {
        QMessageBox::warning(this, tr("文件不存在"),
                             tr("无法打开文件，文件可能已被移动或删除：\n%1").arg(filePath));
        return;
    }

    EditorWidget *editor = m_tabManager->openFile(filePath);
    if (!editor) return;

    editor->scrollToLine(lineNumber, searchText);
}

void MainWindow::onWikiLinkClicked(const QString &fileName)
{
    QString targetPath = findWikiTarget(fileName);

    if (!targetPath.isEmpty()) {
        // 找到文件，直接打开
        onFileSelected(targetPath);
    } else {
        // 未找到文件，询问是否创建
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("自动创建"),
                                      tr("未找到文件 \"%1\"。\n是否要创建一个新的 Markdown 文件？").arg(fileName),
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            QString targetDir;
            EditorWidget *current = m_tabManager->currentEditor();

            // 优先在当前打开文件所在的目录下创建，如果没有打开文件，则在根目录创建
            if (current && !current->currentFilePath().isEmpty()) {
                targetDir = QFileInfo(current->currentFilePath()).absolutePath();
            } else {
                targetDir = m_explorer->rootPath();
            }

            // 拼接完整路径
            QString newFileName = fileName;
            QFileInfo fi(newFileName);
            if (fi.suffix().isEmpty() || !TextFileUtils::isTextExtension(fi.suffix())) {
                newFileName += ".md";
            }
            QString newFilePath = targetDir + "/" + newFileName;

            // 执行创建
            QFile file(newFilePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.close();
                onFileSelected(newFilePath);

                // 全量重建索引：所有引用 [[fileName]] 的文件现在都能解析到新建的文件
                startAsyncIndexBuild();
            } else {
                QMessageBox::warning(this, tr("创建失败"),
                                     tr("无法在以下位置创建文件：\n%1").arg(newFilePath));
            }
        }
    }
}

void MainWindow::buildFileIndex()
{
    m_fileIndex.clear();
    QString root = m_explorer->rootPath();
    if (root.isEmpty() || QDir(root).isRoot() || root == QDir::homePath())
        return;

    QDirIterator it(root, TextFileUtils::scanNameFilters(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString fullPath = it.next();
        QFileInfo info(fullPath);
        QString baseName = info.completeBaseName();
        m_fileIndex[baseName].append(fullPath);
    }
    updateCurrentEditorCompletions();
}

void MainWindow::startAsyncIndexBuild()
{
    // Cancel any in-flight scan
    if (m_scanCancelled)
        m_scanCancelled->store(true);
    m_scanCancelled = std::make_shared<std::atomic<bool>>(false);
    uint64_t scanId = ++m_scanId;

    QString root = m_explorer->rootPath();
    if (root.isEmpty() || QDir(root).isRoot() || root == QDir::homePath()) {
        m_fileIndex.clear();
        m_backlinkIndex->setData({});
        updateCurrentEditorCompletions();
        return;
    }

    auto cancelled = m_scanCancelled;
    QThread::create([this, cancelled, scanId, root]() {
        // Phase 1: Build file index
        QMap<QString, QStringList> fileIndex;
        QDirIterator it(root, TextFileUtils::scanNameFilters(), QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (cancelled->load()) return;
            QString fullPath = it.next();
            QFileInfo info(fullPath);
            fileIndex[info.completeBaseName()].append(fullPath);
        }
        if (cancelled->load()) return;

        // Phase 2: Build backlink index
        BacklinkIndex::BacklinkData backlinkData =
            BacklinkIndex::buildFromPath(root, fileIndex);
        if (cancelled->load()) return;

        // Phase 3: Build tag index
        TagIndex::TagData tagData = TagIndex::buildFromPath(root);
        if (cancelled->load()) return;

        // Deliver results to main thread
        QMetaObject::invokeMethod(this, [this, scanId,
                                         fileIndex = std::move(fileIndex),
                                         data = std::move(backlinkData),
                                         tagData = std::move(tagData)]() mutable {
            if (scanId != m_scanId.load()) return; // Stale
            m_fileIndex = std::move(fileIndex);
            m_backlinkIndex->setData(std::move(data));
            m_tagIndex->setData(std::move(tagData));
            updateCurrentEditorCompletions();
            refreshBacklinks();
            refreshTags();
        }, Qt::QueuedConnection);
    })->start();
}

void MainWindow::updateCurrentEditorCompletions()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (editor) {
        editor->setFileNames(m_fileIndex.keys());
        editor->setTagNames(m_tagIndex->allTags());
    }
}

void MainWindow::onFileRenamedInIndex(const QString &oldPath, const QString &newPath)
{
    // 更新标签管理器中的路径
    m_tabManager->updateEditorFilePath(oldPath, newPath);

    // 在索引迁移前捕获受影响源文件列表
    QString oldBaseName = QFileInfo(oldPath).completeBaseName();
    QString newBaseName = QFileInfo(newPath).completeBaseName();
    QStringList affectedSources;
    if (oldBaseName != newBaseName) {
        affectedSources = m_backlinkIndex->backlinksFor(oldPath);
    }

    buildFileIndex();
    m_backlinkIndex->onFileRenamed(oldPath, newPath);
    m_tagIndex->onFileRenamed(oldPath, newPath);

    // 更新所有引用文件中的 [[旧文件名]] 为 [[新文件名]]
    updateWikiLinksAfterRename(affectedSources, oldBaseName, newBaseName);

    refreshBacklinks();
}

void MainWindow::onFileDeletedInIndex(const QString &path)
{
    buildFileIndex();
    m_backlinkIndex->onFileDeleted(path);
    m_tagIndex->onFileDeleted(path);
    m_historyPanel->removeFile(path);
    refreshBacklinks();
}

void MainWindow::updateWikiLinksAfterRename(const QStringList &affectedSources,
                                             const QString &oldLinkText,
                                             const QString &newLinkText)
{
    if (affectedSources.isEmpty() || oldLinkText == newLinkText)
        return;

    QString rootPath = m_explorer->rootPath();
    for (const QString &srcPath : affectedSources) {
        // 获取源文件内容（优先用编辑器中的内容，以保留未保存的更改）
        EditorWidget *editor = m_tabManager->findEditorByPath(srcPath);
        QString content;
        if (editor) {
            content = editor->toPlainText();
        } else {
            QFile file(srcPath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            QTextStream in(&file);
            content = in.readAll();
            file.close();
        }

        // 替换 wiki 链接文本
        QString newContent = replaceWikiLinkText(content, oldLinkText, newLinkText);
        if (newContent == content)
            continue; // 无变化则跳过

        // 写入新内容
        QFile outFile(srcPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
            continue;
        QTextStream out(&outFile);
        out << newContent;
        outFile.close();

        // 若文件在打开的标签中，从磁盘重新加载以正确更新编辑器状态
        if (editor) {
            editor->loadFile(srcPath);
        }

        // 重建此文件的双向链接索引
        m_backlinkIndex->rebuildFile(srcPath, rootPath, m_fileIndex);
    }
}

// ============================================================
// 自定义标题栏 — 自绘按钮，内嵌系统原生图标
// ============================================================
namespace {
class CaptionBtn : public QPushButton
{
public:
    CaptionBtn(QStyle::StandardPixmap iconId, bool closeBtn, QWidget *parent)
        : QPushButton(parent), m_iconId(iconId), m_closeBtn(closeBtn)
    {
        setFixedSize(46, 32);
        setFlat(true);
        setCursor(Qt::ArrowCursor);
        setStyleSheet("QPushButton { border: none; background: transparent; }");
        setIconSize(QSize(18, 18));
        setIcon(style()->standardIcon(m_iconId));
        setMouseTracking(true);
    }

    void setIconType(QStyle::StandardPixmap id) {
        m_iconId = id;
        setIcon(style()->standardIcon(id));
    }

protected:
    void enterEvent(QEnterEvent *) override { m_hovered = true; repaint(); }
    void leaveEvent(QEvent *) override       { m_hovered = false; repaint(); }

    void paintEvent(QPaintEvent *event) override
    {
        QPainter p(this);
        // hover 背景
        if (m_hovered) {
            p.fillRect(rect(), m_closeBtn ? QColor("#c42b1c") : QColor(0x3a, 0x3a, 0x3a));
        }
        p.end();
        // 让 QPushButton 负责图标居中绘制
        QPushButton::paintEvent(event);
    }

private:
    QStyle::StandardPixmap m_iconId;
    bool m_closeBtn;
    bool m_hovered = false;
};
} // anonymous namespace

void MainWindow::setupCustomTitleBar()
{
    QToolBar *tb = findChild<QToolBar*>();
    if (!tb) return;

    tb->installEventFilter(this);

    m_toolbarSpacer = new QWidget;
    m_toolbarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbarSpacer->installEventFilter(this);
    tb->addWidget(m_toolbarSpacer);

    m_minimizeBtn = new CaptionBtn(QStyle::SP_TitleBarMinButton, false, this);
    m_minimizeBtn->setIconSize(QSize(28, 28));  // 横线图标视觉偏小，单独放大
    m_minimizeBtn->setToolTip(tr("最小化"));
    connect(m_minimizeBtn, &QPushButton::clicked, this, &QMainWindow::showMinimized);
    tb->addWidget(m_minimizeBtn);

    m_maximizeBtn = new CaptionBtn(QStyle::SP_TitleBarMaxButton, false, this);
    m_maximizeBtn->setIconSize(QSize(16, 16));
    m_maximizeBtn->setToolTip(tr("最大化"));
    connect(m_maximizeBtn, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) showNormal(); else showMaximized();
    });
    tb->addWidget(m_maximizeBtn);

    m_closeBtn = new CaptionBtn(QStyle::SP_TitleBarCloseButton, true, this);
    m_closeBtn->setIconSize(QSize(16, 16));
    m_closeBtn->setToolTip(tr("关闭"));
    connect(m_closeBtn, &QPushButton::clicked, this, &QMainWindow::close);
    tb->addWidget(m_closeBtn);
}

// ============================================================
// 无边框窗口 — 边缘 resize & 窗口样式
// ============================================================
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCCREATE) {
            // 为无边框窗口添加 WS_THICKFRAME，使系统发送 WM_NCHITTEST
            HWND hwnd = reinterpret_cast<HWND>(winId());
            LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
            style |= WS_THICKFRAME;
            SetWindowLongPtr(hwnd, GWL_STYLE, style);
        }
        if (msg->message == WM_NCHITTEST && !isMaximized()) {
            const int x = GET_X_LPARAM(msg->lParam);
            const int y = GET_Y_LPARAM(msg->lParam);
            RECT r;
            GetWindowRect(reinterpret_cast<HWND>(winId()), &r);
            const int bw = r.right - r.left;
            const int bh = r.bottom - r.top;
            const int m = 10;
            const bool onL = (x >= r.left && x <= r.left + m);
            const bool onR = (x >= r.right - m && x <= r.right);
            const bool onT = (y >= r.top && y <= r.top + m);
            const bool onB = (y >= r.bottom - m && y <= r.bottom);

            if (onT && onL)     { *result = HTTOPLEFT;     return true; }
            if (onT && onR)     { *result = HTTOPRIGHT;    return true; }
            if (onB && onL)     { *result = HTBOTTOMLEFT;  return true; }
            if (onB && onR)     { *result = HTBOTTOMRIGHT; return true; }
            if (onT)            { *result = HTTOP;          return true; }
            if (onB)            { *result = HTBOTTOM;       return true; }
            if (onL)            { *result = HTLEFT;         return true; }
            if (onR)            { *result = HTRIGHT;        return true; }
        }
    }
#else
    Q_UNUSED(result);
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

bool MainWindow::event(QEvent *event)
{
    // 窗口边缘缩放：在子控件之前拦截鼠标事件
    if (event->type() == QEvent::MouseButtonPress && !isMaximized()) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            const int m = 10;
            const int x = me->pos().x();
            const int y = me->pos().y();
            bool onL = (x <= m);
            bool onR = (x >= width() - m);
            bool onT = (y <= m);
            bool onB = (y >= height() - m);

            Qt::Edges edges;
            if (onT) edges |= Qt::TopEdge;
            if (onB) edges |= Qt::BottomEdge;
            if (onL) edges |= Qt::LeftEdge;
            if (onR) edges |= Qt::RightEdge;

            if (edges != Qt::Edges{} && windowHandle()) {
                windowHandle()->startSystemResize(edges);
                return true;
            }
        }
    }
    return QMainWindow::event(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (m_maximizeBtn) {
            auto *cb = static_cast<CaptionBtn*>(m_maximizeBtn);
            if (isMaximized()) {
                cb->setIconType(QStyle::SP_TitleBarNormalButton);
                cb->setToolTip(tr("还原"));
            } else {
                cb->setIconType(QStyle::SP_TitleBarMaxButton);
                cb->setToolTip(tr("最大化"));
            }
        }
    }
    QMainWindow::changeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 工具栏拖拽：点击空白区域或 spacer 移动窗口
    if (m_minimizeBtn && m_maximizeBtn && m_closeBtn) {
        QToolBar *tb = qobject_cast<QToolBar*>(watched);
        bool isToolbarOrSpacer = (tb != nullptr) || (watched == m_toolbarSpacer);
        if (isToolbarOrSpacer && event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            bool shouldMove = (watched == m_toolbarSpacer);
            if (!shouldMove && tb) {
                QWidget *child = tb->childAt(me->pos());
                shouldMove = (!child || child == m_toolbarSpacer);
            }
            if (shouldMove && me->button() == Qt::LeftButton) {
                if (isMaximized()) {
                    // 最大化拖拽：还原 → 处理事件 → 定位 → 系统拖拽
                    QPoint gpos = me->globalPos();
                    QPoint localInWindow = mapFromGlobal(gpos);
                    showNormal();
                    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                    move(gpos.x() - localInWindow.x(), gpos.y() - localInWindow.y());
                }
                if (windowHandle())
                    windowHandle()->startSystemMove();
                return true;
            }
        }
        if (isToolbarOrSpacer && event->type() == QEvent::MouseButtonDblClick) {
            if (isMaximized())
                showNormal();
            else
                showMaximized();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        QWidget *clickedWidget = QApplication::widgetAt(QCursor::pos());
        QToolButton *btn = qobject_cast<QToolButton*>(clickedWidget);

        // Auto-hide history panel when clicking outside
        if (m_dockHistory->isVisible()) {
            if (btn && btn->defaultAction() == toggleHistoryAction)
                return QMainWindow::eventFilter(watched, event);
            if (!m_dockHistory->isAncestorOf(clickedWidget))
                m_dockHistory->hide();
        }

        // Auto-hide backlinks panel when clicking outside
        if (m_dockBacklinks->isVisible()) {
            if (btn && btn->defaultAction() == toggleBacklinksAction)
                return QMainWindow::eventFilter(watched, event);
            if (!m_dockBacklinks->isAncestorOf(clickedWidget))
                m_dockBacklinks->hide();
        }

        // Auto-hide tag panel when clicking outside
        if (m_dockTag->isVisible()) {
            if (btn && btn->defaultAction() == toggleTagAction)
                return QMainWindow::eventFilter(watched, event);
            if (!m_dockTag->isAncestorOf(clickedWidget))
                m_dockTag->hide();
        }

        // Auto-hide outline panel when clicking outside
        if (m_dockOutline->isVisible()) {
            if (btn && btn->defaultAction() == toggleOutlineAction)
                return QMainWindow::eventFilter(watched, event);
            if (!m_dockOutline->isAncestorOf(clickedWidget))
                m_dockOutline->hide();
        }

        // Close settings panel when clicking overlay background
        if (m_settingsOverlay && m_settingsOverlay->isVisible()) {
            if (btn && btn->defaultAction() == m_settingsAction)
                return QMainWindow::eventFilter(watched, event);
            if (m_settingsPanel->isAncestorOf(clickedWidget))
                return QMainWindow::eventFilter(watched, event);
            if (m_settingsOverlay->isAncestorOf(clickedWidget) &&
                !m_settingsPanel->isAncestorOf(clickedWidget)) {
                toggleSettings();
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

QString MainWindow::findWikiTarget(const QString &fileName)
{
    QString root = m_explorer->rootPath();
    if (root.isEmpty()) return QString();

    // 处理显式路径，尝试在根目录下直接拼接
    const QStringList exts = TextFileUtils::textExtensions();
    for (const QString &ext : exts) {
        QString directPath = root + "/" + fileName + "." + ext;
        if (QFile::exists(directPath))
            return QDir::cleanPath(directPath);
    }

    // 获取当前上下文路径
    QString currentDir;
    EditorWidget *currentEditor = m_tabManager->currentEditor();
    if (currentEditor && !currentEditor->currentFilePath().isEmpty()) {
        currentDir = QFileInfo(currentEditor->currentFilePath()).absolutePath();
    } else {
        currentDir = root;
    }

    // 尝试在当前目录下查找
    QString localPath = currentDir + "/" + fileName;
    for (const QString &ext : TextFileUtils::textExtensions()) {
        if (QFile::exists(localPath + "." + ext))
            return QDir::cleanPath(localPath + "." + ext);
    }

    // 使用全局索引查询
    // 提取链接中的文件名部分（例如 A/B 提取出 B）
    QString baseName = QFileInfo(fileName).completeBaseName();
    if (m_fileIndex.contains(baseName)) {
        const QStringList &candidates = m_fileIndex[baseName];
        if (candidates.isEmpty()) return QString();

        if (candidates.size() == 1) return candidates.first();

        // 如果有多个同名文件，计算路径距离，选择最接近当前目录的一个
        QString bestMatch = candidates.first();
        int minDistance = 999;

        for (const QString &path : candidates) {
            // 计算相对路径的复杂度作为距离
            QString rel = QDir(currentDir).relativeFilePath(path);
            int distance = rel.count("/");
            if (distance < minDistance) {
                minDistance = distance;
                bestMatch = path;
            }
        }
        return bestMatch;
    }

    return QString();
}

void MainWindow::onFileMovedOrRenamed(const QString &oldPath, const QString &newPath)
{
    onFileRenamedInIndex(oldPath, newPath); // 更新全局双向链接索引
    m_tabManager->updatePathsAfterMove(oldPath, newPath); // 更新所有已打开标签页的路径
    m_historyPanel->replacePath(oldPath, newPath); // 更新历史记录中的路径
}

// ============================================================
// 编译运行相关槽函数
// ============================================================

void MainWindow::onCompile()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor || !editor->isCodeEdit())
        return;

    QString filePath = editor->currentFilePath();
    if (filePath.isEmpty() || editor->isModified()) {
        filePath = saveCodeToTempFile(editor);
        if (filePath.isEmpty())
            return;
    }

    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == QStringLiteral("py") || ext == QStringLiteral("pyw")) {
        showOutputPanel();

        m_outputPanel->clearOutput();
        m_outputPanel->appendOutput(tr("Python 不需要编译，请使用 运行 (F7) 或 编译运行 (F5)。\n"), false);
        m_outputPanel->setStatus(tr("提示"), false);
        return;
    }

    showOutputPanel();

    m_outputPanel->clearOutput();
    m_outputPanel->setStatus(tr("编译中..."));
    m_processRunner->startCompile(filePath);
}

void MainWindow::onRun()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor)
        return;

    QString filePath = editor->currentFilePath();
    if (!filePath.isEmpty()) {
        QString ext = QFileInfo(filePath).suffix().toLower();
        if (ext == QStringLiteral("py") || ext == QStringLiteral("pyw")) {
            if (filePath.isEmpty() || editor->isModified()) {
                filePath = saveCodeToTempFile(editor);
                if (filePath.isEmpty())
                    return;
            }
            showOutputPanel();
    
            m_outputPanel->clearOutput();
            m_outputPanel->setStatus(tr("运行中..."));
            m_processRunner->startRunPython(filePath);
            return;
        }
    }

    if (m_processRunner->lastExecutable().isEmpty()) {
        // 还没有编译过的可执行文件，转为编译运行
        onCompileAndRun();
        return;
    }

    showOutputPanel();

    m_outputPanel->clearOutput();
    m_outputPanel->setStatus(tr("运行中..."));
    m_processRunner->startRun(m_processRunner->lastExecutable());
}

void MainWindow::onCompileAndRun()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor || !editor->isCodeEdit())
        return;

    QString filePath = editor->currentFilePath();
    if (filePath.isEmpty() || editor->isModified()) {
        filePath = saveCodeToTempFile(editor);
        if (filePath.isEmpty())
            return;
    }

    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == QStringLiteral("py") || ext == QStringLiteral("pyw")) {
        showOutputPanel();

        m_outputPanel->clearOutput();
        m_outputPanel->setStatus(tr("运行中..."));
        m_processRunner->startRunPython(filePath);
        return;
    }

    showOutputPanel();

    m_outputPanel->clearOutput();
    m_outputPanel->setStatus(tr("编译中..."));
    m_processRunner->startCompileAndRun(filePath);
}

void MainWindow::onStopProcess()
{
    m_processRunner->stop();
    m_outputPanel->appendOutput(QStringLiteral("\n--- ") + tr("已终止") + QStringLiteral(" ---\n"), false);
    m_outputPanel->setStatus(tr("已终止"), true);
}

void MainWindow::onCompileFinished(bool success)
{
    if (success) {
        m_outputPanel->setStatus(tr("编译成功"));
    } else {
        m_outputPanel->setStatus(tr("编译失败"), true);
    }
}

void MainWindow::onRunFinished(int exitCode)
{
    m_outputPanel->appendOutput(
        QStringLiteral("\n--- ") + tr("进程退出 (代码: %1)").arg(exitCode) + QStringLiteral(" ---\n"), false);
    m_outputPanel->setStatus(
        tr("完成 (代码: %1)").arg(exitCode), exitCode != 0);
}

void MainWindow::onJudgeRunAll()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor || !editor->isCodeEdit()) {
        QMessageBox::information(this, tr("提示"),
                                  tr("请打开一个代码文件进行评测。"));
        return;
    }

    // Save current code to file (or temp file if unsaved)
    QString filePath = editor->currentFilePath();
    if (filePath.isEmpty() || editor->isModified()) {
        filePath = saveCodeToTempFile(editor);
        if (filePath.isEmpty())
            return;
    }

    // Ensure test folder is set
    if (m_judgePanel->testFolder().isEmpty()) {
        QMessageBox::information(this, tr("提示"),
                                  tr("请先在评测面板中选择测试用例文件夹。"));
        return;
    }

    // Show and raise the judge dock
    m_dockJudge->show();
    m_dockJudge->raise();

    m_judgePanel->runJudge(filePath);
}

void MainWindow::onOpenJudgeRequested()
{
    bool isNew = false;
    if (!m_openJudgeWindow) {
        m_openJudgeWindow = new OpenJudgeWindow(m_settings, nullptr);
        isNew = true;
        connect(m_openJudgeWindow, &OpenJudgeWindow::sampleSelected,
                this, &MainWindow::onOpenJudgeSampleSelected);
        connect(m_openJudgeWindow, &OpenJudgeWindow::loginStateChanged,
                this, &MainWindow::onOpenJudgeLoginStateChanged);
        connect(m_openJudgeWindow, &OpenJudgeWindow::submissionResultReady,
                this, &MainWindow::onSubmissionResultReady);
        connect(m_openJudgeWindow, &OpenJudgeWindow::submissionFailed,
                this, [this](const QString &error) {
            QMessageBox::warning(this, tr("提交失败"), error);
        });
    }
    // Restore from minimized state
    m_openJudgeWindow->setWindowState(m_openJudgeWindow->windowState() & ~Qt::WindowMinimized);
    m_openJudgeWindow->show();
    m_openJudgeWindow->raise();
    m_openJudgeWindow->activateWindow();

    // Show login dialog if not logged in (after window is visible)
    if (isNew || !m_openJudgeWindow->isLoggedIn()) {
        QTimer::singleShot(200, this, [this]() {
            if (m_openJudgeWindow && !m_openJudgeWindow->isLoggedIn())
                m_openJudgeWindow->onReLogin();
        });
    }
}

void MainWindow::onOpenJudgeSampleSelected(const QString &folderPath)
{
    m_judgePanel->setTestFolder(folderPath);
    m_dockJudge->show();
    m_dockJudge->raise();
}

void MainWindow::onSubmitToOpenJudge()
{
    // 1. Check if there's a code file open
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor || !editor->isCodeEdit()) {
        QMessageBox::information(this, tr("提示"),
            tr("请打开一个代码文件进行提交"));
        return;
    }

    // 2. Get code and determine language
    QString code = editor->toPlainText();
    if (code.trimmed().isEmpty()) {
        QMessageBox::information(this, tr("提示"),
            tr("代码内容为空"));
        return;
    }

    // 3. Check if OpenJudge window exists and has a problem selected
    if (!m_openJudgeWindow || !m_openJudgeWindow->hasCurrentProblem()) {
        // Trigger auto-login / show window so user can select a problem
        bool autoLoginInitiated = m_openJudgeWindow && m_openJudgeWindow->tryAutoLogin();
        if (!autoLoginInitiated) {
            onOpenJudgeRequested();
        }
        QMessageBox::information(this, tr("提示"),
            tr("请先在 OpenJudge 中选择一道题目"));
        return;
    }

    // 4. Check login status
    if (!m_openJudgeWindow->isLoggedIn()) {
        bool autoLoginInitiated = m_openJudgeWindow->tryAutoLogin();
        if (!autoLoginInitiated) {
            onOpenJudgeRequested();
        }
        QMessageBox::information(this, tr("提示"),
            autoLoginInitiated ? tr("正在自动登录 OpenJudge，请稍后重试")
                               : tr("请先登录 OpenJudge"));
        return;
    }

    // Save unsaved content first
    QString filePath = editor->currentFilePath();
    if (filePath.isEmpty() || editor->isModified()) {
        filePath = saveCodeToTempFile(editor);
        if (filePath.isEmpty()) {
            QMessageBox::warning(this, tr("错误"),
                tr("无法保存代码文件"));
            return;
        }
    }

    QString ext = QFileInfo(filePath).suffix().toLower();

    // Map to OpenJudge language IDs (from config)
    QMap<QString, int> langMap = ConfigManager::instance().openJudgeSubmissionLanguageMap();
    int langId = langMap.value("." + ext, 1); // default: G++

    // Submit through OpenJudgeWindow
    m_openJudgeWindow->submitCurrentProblem(code, langId);

    // 5. Show a brief status message in the judge panel
    m_dockJudge->show();
    m_dockJudge->raise();
}

void MainWindow::onSubmissionResultReady(const SubmissionResult &result)
{
    // Hide the output panel if it's visible
    m_outputPanel->hide();

    // Create the submit result panel on first use
    if (!m_submitResultPanel) {
        m_submitResultPanel = new SubmitResultPanel(this);
        // Insert it into the right splitter, positioned where output panel is
        int outputIdx = m_rightSplitter->indexOf(m_outputPanel);
        m_rightSplitter->insertWidget(outputIdx, m_submitResultPanel);
        connect(m_submitResultPanel, &SubmitResultPanel::hideRequested, this, [this]() {
            m_submitResultPanel->hide();
        });
    }

    m_submitResultPanel->showResult(result);
    m_submitResultPanel->setVisible(true);

    // Resize splitter to give the result panel configured ratio height
    double ratio = ConfigManager::instance().submissionResultHeightRatio();
    int total = m_rightSplitter->height();
    if (total > 0) {
        int panelH = qRound(total * ratio);
        int editorH = total - panelH;
        QList<int> sizes;
        sizes.reserve(m_rightSplitter->count());
        for (int i = 0; i < m_rightSplitter->count(); ++i) {
            QWidget *w = m_rightSplitter->widget(i);
            if (w == m_submitResultPanel)
                sizes.append(panelH);
            else if (w == m_tabManager)
                sizes.append(editorH);
            else
                sizes.append(0);
        }
        m_rightSplitter->setSizes(sizes);
    }
}

void MainWindow::onOpenJudgeLoginStateChanged(bool loggedIn, const QString &username)
{
    Q_UNUSED(username);
    // Can be used to update UI state when login state changes
    if (loggedIn) {
        // Could show a status bar message
    }
}

QString MainWindow::saveCodeToTempFile(EditorWidget *editor)
{
    if (!editor)
        return {};

    QString rootPath = m_explorer->rootPath();
    if (rootPath.isEmpty())
        rootPath = QDir::tempPath();

    QString content = editor->toPlainText();
    QString filePath = editor->currentFilePath();

    if (filePath.isEmpty()) {
        // 新建的文件：在根目录下创建临时 .cpp 文件
        const auto &cfg = ConfigManager::instance();
        filePath = rootPath + QStringLiteral("/") + cfg.compilerTempFilePrefix()
                   + QString::number(QCoreApplication::applicationPid())
                   + cfg.compilerTempFileSuffix();
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    QTextStream out(&file);
    out << content;
    file.close();

    editor->setFilePath(filePath);
    editor->setModified(false);
    return filePath;
}

void MainWindow::showOutputPanel()
{
    m_outputPanel->setVisible(true);
    double ratio = ConfigManager::instance().outputPanelDefaultHeightRatio();
    int total = m_rightSplitter->height();
    if (total > 0) {
        int outputH = qRound(total * ratio);
        int editorH = total - outputH;
        QList<int> sizes;
        sizes.reserve(m_rightSplitter->count());
        for (int i = 0; i < m_rightSplitter->count(); ++i) {
            QWidget *w = m_rightSplitter->widget(i);
            if (w == m_outputPanel)
                sizes.append(outputH);
            else if (w == m_tabManager)
                sizes.append(editorH);
            else
                sizes.append(0);
        }
        m_rightSplitter->setSizes(sizes);
    }
}

QString MainWindow::saveCodeBlockToTempFile(const QString &language, const QString &code)
{
    QString ext;
    if (language == QStringLiteral("python")) {
        ext = QStringLiteral("py");
    } else if (language == QStringLiteral("cpp")) {
        ext = QStringLiteral("cpp");
    } else {
        return {};
    }

    const QString tempPath = QDir::tempPath()
        + QStringLiteral("/") + ConfigManager::instance().compilerCodeBlockPrefix()
        + QString::number(QCoreApplication::applicationPid())
        + QStringLiteral("_")
        + QString::number(m_codeBlockCounter++)
        + QStringLiteral(".")
        + ext;

    QFile file(tempPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};

    QTextStream out(&file);
    out << code;
    file.close();
    return tempPath;
}

void MainWindow::onCodeBlockRequested(const QString &language, const QString &code)
{
    const QString normalizedLang = LanguageUtils::normalizeCodeFenceLanguage(language);

    if (normalizedLang.isEmpty()) {
        showOutputPanel();
        m_outputPanel->clearOutput();
        m_outputPanel->appendOutput(
            tr("不支持的语言: %1\n当前支持: python, cpp\n").arg(language), true);
        m_outputPanel->setStatus(tr("错误"), true);
        return;
    }

    const QString filePath = saveCodeBlockToTempFile(normalizedLang, code);
    if (filePath.isEmpty()) {
        showOutputPanel();
        m_outputPanel->clearOutput();
        m_outputPanel->appendOutput(
            tr("错误: 无法创建临时文件。\n"), true);
        m_outputPanel->setStatus(tr("错误"), true);
        return;
    }

    showOutputPanel();
    m_outputPanel->clearOutput();

    if (normalizedLang == QStringLiteral("python")) {
        m_outputPanel->appendOutput(
            QStringLiteral("--- ") + tr("运行 Python 代码块 ---\n"), false);
        m_outputPanel->setStatus(tr("运行中..."));
        m_processRunner->startRunPython(filePath);
    } else if (normalizedLang == QStringLiteral("cpp")) {
        m_outputPanel->appendOutput(
            QStringLiteral("--- ") + tr("编译运行 C++ 代码块 ---\n"), false);
        m_outputPanel->setStatus(tr("编译中..."));
        m_processRunner->startCompileAndRun(filePath);
    }
}

// ==================================================================
// Crash recovery
// ==================================================================

void MainWindow::checkCrashRecovery()
{
    cleanStaleRecoveryFiles();

    QString recoveryDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                          + "/SM-Recovery";
    QDir dir(recoveryDir);
    if (!dir.exists())
        return;

    QStringList entries = dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
    if (entries.isEmpty())
        return;

    // 有恢复文件 → 询问用户
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("恢复文件"));
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText(tr("检测到上次异常退出，发现 %1 个未保存的临时文件。").arg(entries.size()));
    msgBox.setInformativeText(tr("是否恢复未保存的内容？\n\n"
                                 "选择“恢复”将打开临时文件供您手动保存；\n"
                                 "选择“丢弃”将删除所有临时文件。"));
    QPushButton *restoreBtn = msgBox.addButton(tr("恢复(&R)"), QMessageBox::AcceptRole);
    QPushButton *discardBtn = msgBox.addButton(tr("丢弃(&D)"), QMessageBox::DestructiveRole);
    msgBox.setDefaultButton(restoreBtn);

    msgBox.exec();

    if (msgBox.clickedButton() == restoreBtn) {
        // 恢复：依次打开每个恢复文件
        for (const QString &entry : entries) {
            QString filePath = dir.absoluteFilePath(entry);
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            QString content = QString::fromUtf8(file.readAll());
            file.close();

            // 创建新标签页，填入恢复内容
            EditorWidget *editor = m_tabManager->newFile();
            editor->setPlainText(content);
            editor->setModified(true);

            // 记录恢复文件路径，后续手动保存时清理
            editor->setRecoveryTempPath(filePath);

            // 更新标签标题
            int idx = m_tabManager->indexOf(editor);
            if (idx >= 0)
                m_tabManager->setTabText(idx, tr("未命名（已恢复）"));
        }
    } else {
        // 丢弃：删除整个恢复目录
        clearRecoveryDirectory();
    }
}

void MainWindow::cleanStaleRecoveryFiles()
{
    QString recoveryDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                          + "/SM-Recovery";
    QDir dir(recoveryDir);
    if (!dir.exists())
        return;

    int maxAgeHours = ConfigManager::instance().autoSaveRecoveryMaxAgeHours();
    qint64 cutoff = QDateTime::currentSecsSinceEpoch() - (maxAgeHours * 3600);

    const QStringList entries = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        QString filePath = dir.absoluteFilePath(entry);
        QFileInfo info(filePath);
        if (info.lastModified().toSecsSinceEpoch() < cutoff) {
            QFile::remove(filePath);
        }
    }

    // 如果目录已空，删除它
    if (dir.entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
        dir.removeRecursively();
    }
}

void MainWindow::clearRecoveryDirectory()
{
    QString recoveryDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                          + "/SM-Recovery";
    QDir dir(recoveryDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
}
