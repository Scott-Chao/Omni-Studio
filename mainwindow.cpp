#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <dwmapi.h>
#include <QPainter>

namespace {

// 顶层半透明覆盖层，用于设置/帮助面板的背景变暗效果
// 必须是顶层窗口（Qt::Tool），防止 Qt 裁剪下方 QWebEngineView 的原生 HWND → 黑屏
class OverlayWidget : public QWidget
{
public:
    explicit OverlayWidget(QWidget *parent = nullptr)
        : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint)
    {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0, 0, 0, 128));
    }
};

} // namespace
#include "panels/fileexplorerwidget.h"
#include "editor/editorwidget.h"
#include "config/settingsmanager.h"
#include "config/configmanager.h"
#include "thememanager.h"
#include "editor/tabmanager.h"
#include "panels/historypanel.h"
#include "index/backlinkindex.h"
#include "panels/sidebarpanels.h"
#include "panels/searchpanel.h"
#include "utilities.h"
#include "panels/activitybar.h"
#include "panels/rightpanelcontainer.h"
#include "panels/outputpanel.h"
#include "panels/bottompanel.h"
#include "editor/codeeditor.h"
#include "runner/compilerunmanager.h"
#include "runner/codeblockrunner.h"
#include "judge/openjudgemanager.h"
#include "config/settingschangehandler.h"
#include "panels/judgepanel.h"
#include "judge/judgeengine.h"
#include "panels/openjudgewidget.h"
#include "panels/submissionpanel.h"
#include "index/tagindex.h"
#include "panels/outlinepanel.h"
#include "panels/settingspanel.h"
#include "panels/helppanel.h"
#include "ai/aipanel.h"
#include "ai/aicontextmanager.h"
#include "ai/prompttemplates.h"
#include "ai/aiproviders.h"
#include "ai/aihistorylistwidget.h"
#include "ai/errorjournal.h"
#include "ai/airequesthandler.h"
#include "index/indexmanager.h"
#include "crashrecoverymanager.h"
#include "smd/smdformat.h"
#include "smd/smdeditor.h"
#include "editor/codeeditor.h"
#include <QPdfView>
#include "editor/scrollbarhider.h"
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
#include <QIcon>
#include <QWindow>
#include <QMenu>
#include <utility>
#include <QDockWidget>
#include <QStackedWidget>
#include <QDirIterator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDateTime>
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QAbstractScrollArea>
#include <QPainter>

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

// ── Helper: create themed icon from SVG ──────────────────────────
static QIcon coloredSvgIcon(const QString &svgPath, const QColor &color, int size = 24)
{
    QIcon src(svgPath);
    QPixmap srcPm = src.pixmap(size, size);
    if (srcPm.isNull())
        return src;
    QImage img = srcPm.toImage().convertToFormat(QImage::Format_ARGB32);
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(img.rect(), color);
    p.end();
    return QIcon(QPixmap::fromImage(img));
}

// ── Helper: dock widget title bar with themed close button ──────
static QWidget *createDockTitleBar(const QString &title, QDockWidget *dock)
{
    auto *bar = new QWidget;
    bar->setObjectName("dockTitleBar");

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 0, 4, 0);
    layout->setSpacing(0);

    auto *label = new QLabel(title);
    label->setObjectName("dockTitleBarLabel");

    auto *closeBtn = new QPushButton;
    closeBtn->setObjectName("dockTitleCloseBtn");
    closeBtn->setIcon(coloredSvgIcon(":/icons/close",
        ThemeManager::instance().color("titleBar.foreground"), 16));
    closeBtn->setIconSize(QSize(16, 16));
    closeBtn->setFixedSize(22, 22);
    closeBtn->setFlat(true);
    closeBtn->setCursor(Qt::PointingHandCursor);

    QObject::connect(closeBtn, &QPushButton::clicked, dock, &QDockWidget::hide);

    layout->addWidget(label);
    layout->addStretch();
    layout->addWidget(closeBtn);

    return bar;
}

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

    // 左侧面板栈（VS Code 风格覆盖：搜索/评测替换文件浏览器）
    m_leftStack = new QStackedWidget(this);
    m_leftStack->addWidget(m_explorer); // index 0: 文件浏览器

    // Restore saved theme
    ThemeManager::instance().setStyleSheetTarget(this);
    ThemeManager::instance().loadQss(); // apply QSS to MainWindow unconditionally
    QString savedTheme = m_settings->settingOverride("appearance.theme").toString();
    if (!savedTheme.isEmpty())
        ThemeManager::instance().loadTheme(savedTheme);

    // 设置窗口标题与无边框
    setWindowTitle(tr("Smart Markdown"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                   Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

    // 强制创建原生窗口句柄，添加 WS_THICKFRAME 以启用边缘缩放和 Aero Snap
    // 并向 DWM 请求圆角（Windows 11 系统级窗口圆角）
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowLongPtr(hwnd, GWL_STYLE,
                     GetWindowLongPtr(hwnd, GWL_STYLE) | WS_THICKFRAME);
    DWORD corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                           &corner, sizeof(corner));
#endif

    // 创建索引管理器
    m_indexManager = new IndexManager(this);
    m_crashRecovery = new CrashRecoveryManager(this);
    m_indexManager->setTabManager(m_tabManager);
    connect(m_indexManager, &IndexManager::fullIndexReady, this, [this]() {
        updateCurrentEditorCompletions();
        refreshBacklinks();
        refreshTags();
    });
    connect(m_indexManager, &IndexManager::fileIndexReady, this, [this]() {
        updateCurrentEditorCompletions();
    });

    // 统一右侧面板（历史/大纲/标签/反链）
    m_rightPanel = new RightPanelContainer(m_settings, this);
    connect(m_rightPanel, &RightPanelContainer::fileClicked, this, &MainWindow::onHistoryFileClicked);
    connect(m_rightPanel, &RightPanelContainer::tagClicked, this, &MainWindow::onTagClicked);
    connect(m_rightPanel, &RightPanelContainer::headingClicked, this, [this](int line, const QString &) {
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor)
            editor->navigateToLine(line);
    });

    m_dockRightPanel = new QDockWidget(tr("面板"), this);
    m_dockRightPanel->setWidget(m_rightPanel);
    m_dockRightPanel->setFeatures(QDockWidget::DockWidgetMovable);
    m_dockRightPanel->setTitleBarWidget(createDockTitleBar(tr("面板"), m_dockRightPanel));
    addDockWidget(Qt::RightDockWidgetArea, m_dockRightPanel);
    m_dockRightPanel->hide();

    connect(m_dockRightPanel, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        toggleRightPanelAction->setChecked(visible);
        if (!visible) {
            EditorWidget *editor = m_tabManager->currentEditor();
            if (editor)
                editor->clearOutlineHighlight();
        }
    });

    // 用 focusChanged 替代全局 eventFilter 实现点击编辑器时自动隐藏右侧面板
    // 用 singleShot(0) 避免中间态焦点经过编辑器导致误关（如打开左侧面板时）
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget * /*old*/, QWidget *now) {
        if (now && m_tabManager && m_tabManager->isAncestorOf(now)
            && m_dockRightPanel && m_dockRightPanel->isVisible()) {
            QTimer::singleShot(0, this, [this]() {
                if (m_dockRightPanel && m_dockRightPanel->isVisible()) {
                    QWidget *focused = QApplication::focusWidget();
                    if (focused && m_tabManager && m_tabManager->isAncestorOf(focused))
                        m_dockRightPanel->hide();
                }
            });
        }
    });

    // 在 TabManager 上安装事件过滤器，捕获焦点已在内时点击编辑器内任意位置
    // 也能隐藏右侧面板（focusChanged 在焦点不变时不会触发）。
    m_tabManager->installEventFilter(this);

    toggleRightPanelAction = new QAction(tr("面板"), this);
    toggleRightPanelAction->setCheckable(true);
    toggleRightPanelAction->setToolTip(tr("显示/隐藏右侧面板 (历史/大纲/标签/反链)"));
    toggleRightPanelAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_right_panel", "Ctrl+Shift+E")));
    connect(toggleRightPanelAction, &QAction::triggered, this, [this]() {
        if (m_dockRightPanel->isVisible()) {
            m_dockRightPanel->hide();
        } else {
            showRightPanel(m_rightPanel->currentPanel());
        }
    });

    m_rightPanel->historyPanel()->loadHistory();

    // 各面板的独立快捷键（切换标签并显示右侧面板）
    m_toggleHistoryAction = new QAction(tr("历史记录"), this);
    m_toggleHistoryAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_history", "Ctrl+H")));
    m_toggleHistoryAction->setToolTip(tr("显示/隐藏历史记录"));
    addAction(m_toggleHistoryAction);
    connect(m_toggleHistoryAction, &QAction::triggered, this, [this]() {
        if (m_dockRightPanel->isVisible() && m_rightPanel->currentPanel() == 0)
            m_dockRightPanel->hide();
        else
            showRightPanel(0);
    });

    m_toggleOutlineAction = new QAction(tr("大纲"), this);
    m_toggleOutlineAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_outline", "Ctrl+Shift+O")));
    m_toggleOutlineAction->setToolTip(tr("显示/隐藏大纲"));
    addAction(m_toggleOutlineAction);
    connect(m_toggleOutlineAction, &QAction::triggered, this, [this]() {
        if (m_dockRightPanel->isVisible() && m_rightPanel->currentPanel() == 1)
            m_dockRightPanel->hide();
        else
            showRightPanel(1);
    });

    m_toggleTagsAction = new QAction(tr("标签"), this);
    m_toggleTagsAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_tags", "Ctrl+Shift+T")));
    m_toggleTagsAction->setToolTip(tr("显示/隐藏标签"));
    addAction(m_toggleTagsAction);
    connect(m_toggleTagsAction, &QAction::triggered, this, [this]() {
        if (m_dockRightPanel->isVisible() && m_rightPanel->currentPanel() == 2)
            m_dockRightPanel->hide();
        else
            showRightPanel(2);
    });

    m_toggleBacklinksAction = new QAction(tr("反向链接"), this);
    m_toggleBacklinksAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_backlinks", "Ctrl+Shift+B")));
    m_toggleBacklinksAction->setToolTip(tr("显示/隐藏反向链接"));
    addAction(m_toggleBacklinksAction);
    connect(m_toggleBacklinksAction, &QAction::triggered, this, [this]() {
        if (m_dockRightPanel->isVisible() && m_rightPanel->currentPanel() == 3)
            m_dockRightPanel->hide();
        else
            showRightPanel(3);
    });

    // 创建搜索面板
    m_searchPanel = new SearchPanel(this);
    connect(m_searchPanel, &SearchPanel::resultClicked,
            this, &MainWindow::onSearchResultClicked);
    connect(m_searchPanel, &SearchPanel::searchTextChanged,
            this, &MainWindow::onSearchTextChangedByUser);

    toggleSearchAction = new QAction(tr("显示/隐藏搜索"), this);
    toggleSearchAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_search", "Ctrl+Shift+F")));
    connect(toggleSearchAction, &QAction::triggered, this, [this]() {
        if (m_leftStack->currentIndex() == 1)
            showLeftPanel(0);
        else
            showLeftPanel(1);
        if (m_leftStack->currentIndex() == 1)
            m_searchPanel->focusSearchInput();
    });
    addAction(toggleSearchAction);

    // ----- 底部统一面板（输出 + 诊断）-----
    m_bottomPanel = new BottomPanel(this);
    m_bottomPanel->setMinimumHeight(ConfigManager::instance().outputPanelMinHeight());
    m_bottomPanel->hide();

    connect(m_bottomPanel, &BottomPanel::closeRequested, this, [this]() {
        if (m_compileRunMgr && m_compileRunMgr->isRunning())
            m_compileRunMgr->stop();
        m_bottomPanel->hide();
    });
    connect(m_bottomPanel, &BottomPanel::diagnosticsLineClicked, this, [this](int line) {
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor && editor->isCodeEdit())
            editor->navigateEditorToLine(line);
    });

    // 右侧垂直分割线：编辑器在上，输出面板在下
    m_rightSplitter = new QSplitter(Qt::Vertical, this);
    m_rightSplitter->addWidget(m_tabManager);
    m_rightSplitter->addWidget(m_bottomPanel);
    m_rightSplitter->setStretchFactor(0, ConfigManager::instance().rightSplitterEditorStretch());
    m_rightSplitter->setStretchFactor(1, ConfigManager::instance().rightSplitterOutputStretch());

    // ----- 编译运行管理器（ProcessRunner + 编译/运行/终止）-----
    m_compileRunMgr = new CompileRunManager(m_tabManager, m_bottomPanel,
                                             m_settings, m_explorer,
                                             m_rightSplitter, this);

    // ----- MD 代码块执行与诊断 -----
    m_codeBlockRunner = new CodeBlockRunner(m_tabManager, m_bottomPanel,
                                             m_compileRunMgr, this);

    // ----- 设置变更处理 -----
    m_settingsHandler = new SettingsChangeHandler(m_tabManager, m_settings, this);

    // ----- 本地评测面板 -----
    m_judgePanel = new JudgePanel(this);

    // ----- OpenJudge 提交管理 -----
    m_openJudgeMgr = new OpenJudgeManager(m_tabManager, m_judgePanel,
                                           m_rightSplitter, this);

    m_toggleJudgeAction = new QAction(tr("显示/隐藏代码评测"), this);
    m_toggleJudgeAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_judge", "Ctrl+Shift+J")));
    connect(m_toggleJudgeAction, &QAction::triggered, this, [this]() {
        if (m_leftStack->currentIndex() == 2)
            showLeftPanel(0);
        else
            showLeftPanel(2);
    });
    addAction(m_toggleJudgeAction);

    connect(m_judgePanel, &JudgePanel::runAllRequested,
            this, &MainWindow::onJudgeRunAll);
    connect(m_judgePanel, &JudgePanel::openJudgeRequested,
            this, [this]() { onOpenJudgeRequested(); });
    connect(m_judgePanel, &JudgePanel::submitToOpenJudgeRequested,
            this, [this]() { m_openJudgeMgr->submit(m_explorer->rootPath()); });

    // OpenJudgeManager cross-cutting signal routing
    connect(m_openJudgeMgr, &OpenJudgeManager::submissionFailed,
            this, [this](const QString &error) {
        QMessageBox::warning(this, tr("提交失败"), error);
    });
    connect(m_openJudgeMgr, &OpenJudgeManager::ideModeChanged,
            this, [this](bool ideMode) {
        if (m_compileRunMgr) {
            m_compileRunMgr->updateActions();
            if (!ideMode && m_compileRunMgr->isRunning())
                m_compileRunMgr->stop();
        }
        if (!ideMode)
            m_bottomPanel->hide();
    });
    connect(m_openJudgeMgr, &OpenJudgeManager::diagnosticsToggleRequested,
            m_compileRunMgr, &CompileRunManager::toggleDiagnostics);
    connect(m_openJudgeMgr, &OpenJudgeManager::showJudgePanelRequested,
            this, [this]() { showLeftPanel(2); });

    // 搜索包装页（标题栏 + 关闭按钮）
    {
        QWidget *page = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        QWidget *header = new QWidget;
        header->setObjectName("leftPanelHeader");
        QHBoxLayout *hdrLayout = new QHBoxLayout(header);
        hdrLayout->setContentsMargins(8, 4, 4, 4);
        QLabel *title = new QLabel(tr("搜索"));
        QPushButton *closeBtn = new QPushButton;
        closeBtn->setIcon(QIcon(":/icons/close"));
        closeBtn->setIconSize(QSize(16, 16));
        closeBtn->setFixedSize(22, 22);
        closeBtn->setFlat(true);
        closeBtn->setCursor(Qt::PointingHandCursor);
        {
            auto &tm = ThemeManager::instance();
            closeBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: transparent; border: none; }"
                "QPushButton:hover { background: %1; border-radius: 3px; }")
                .arg(tm.color("titleBar.buttonCloseHover").name()));
            m_leftSearchCloseBtn = closeBtn;
        }
        connect(closeBtn, &QPushButton::clicked, this, [this]() { showLeftPanel(0); });
        hdrLayout->addWidget(title);
        hdrLayout->addStretch();
        hdrLayout->addWidget(closeBtn);
        layout->addWidget(header);
        layout->addWidget(m_searchPanel);

        m_leftStack->addWidget(page); // index 1: 搜索
    }

    // 评测包装页
    {
        QWidget *page = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        QWidget *header = new QWidget;
        header->setObjectName("leftPanelHeader");
        QHBoxLayout *hdrLayout = new QHBoxLayout(header);
        hdrLayout->setContentsMargins(8, 4, 4, 4);
        QLabel *title = new QLabel(tr("代码评测"));
        QPushButton *closeBtn = new QPushButton;
        closeBtn->setIcon(QIcon(":/icons/close"));
        closeBtn->setIconSize(QSize(16, 16));
        closeBtn->setFixedSize(22, 22);
        closeBtn->setFlat(true);
        closeBtn->setCursor(Qt::PointingHandCursor);
        {
            auto &tm = ThemeManager::instance();
            closeBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: transparent; border: none; }"
                "QPushButton:hover { background: %1; border-radius: 3px; }")
                .arg(tm.color("titleBar.buttonCloseHover").name()));
            m_leftJudgeCloseBtn = closeBtn;
        }
        connect(closeBtn, &QPushButton::clicked, this, [this]() { showLeftPanel(0); });
        hdrLayout->addWidget(title);
        hdrLayout->addStretch();
        hdrLayout->addWidget(closeBtn);
        layout->addWidget(header);
        layout->addWidget(m_judgePanel);

        m_leftStack->addWidget(page); // index 2: 评测
    }

    // 标签索引已随 m_backlinkIndex 在上方创建
    // 标签/大纲面板已集成到 m_rightPanel 中

    // ----- 工具栏（同时充当标题栏）-----
    m_toolBar = addToolBar("文件工具栏");
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setIconSize(QSize(24, 24));
    m_toolBar->installEventFilter(this);
    m_toolBar->setIconSize(QSize(18, 18));

    // 左侧：[文件 ▼] 下拉菜单
    m_fileMenuBtn = new QToolButton(m_toolBar);
    m_fileMenuBtn->setText(tr("文件"));
    m_fileMenuBtn->setToolTip(tr("文件操作"));
    m_fileMenuBtn->setFixedHeight(32);
    // Replace platform dropdown arrow with themed "v" chevron on the right
    m_fileMenuBtn->setIcon(QIcon(":/icons/chevron-down"));
    m_fileMenuBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_fileMenuBtn->setLayoutDirection(Qt::RightToLeft); // icon → visual right
    m_fileMenuBtn->setIconSize(QSize(7, 7));

    {
        m_fileMenu = new QMenu(m_fileMenuBtn);
        m_fileMenu->setLayoutDirection(Qt::LeftToRight);

        QAction *openDirAct = m_fileMenu->addAction(tr("打开目录"));
        connect(openDirAct, &QAction::triggered, this, &MainWindow::onOpenFolder);

        m_fileMenu->addSeparator();

        QAction *newAct = m_fileMenu->addAction(tr("新建文件"));
        newAct->setShortcut(QKeySequence::New);
        addAction(newAct);
        connect(newAct, &QAction::triggered, this, &MainWindow::newFile);
        m_shortcutActions["new_file"] = newAct;

        QAction *saveAct = m_fileMenu->addAction(tr("保存"));
        saveAct->setShortcut(QKeySequence::Save);
        addAction(saveAct);
        connect(saveAct, &QAction::triggered, this, &MainWindow::saveFile);
        m_shortcutActions["save"] = saveAct;

        QAction *saveAsAct = m_fileMenu->addAction(tr("另存为"));
        saveAsAct->setShortcut(QKeySequence(ConfigManager::instance().shortcut("save_as", "Ctrl+Shift+S")));
        addAction(saveAsAct);
        connect(saveAsAct, &QAction::triggered, this, &MainWindow::onSaveFileAs);
        m_shortcutActions["save_as"] = saveAsAct;

        // Manual popup to control position — left edge must stay on screen
        connect(m_fileMenuBtn, &QToolButton::clicked, this, [this]() {
            QPoint pos = m_fileMenuBtn->mapToGlobal(
                QPoint(0, m_fileMenuBtn->height()));
            QScreen *scr = QGuiApplication::screenAt(pos);
            if (!scr) scr = QGuiApplication::primaryScreen();
            const QRect screen = scr->availableGeometry();
            if (pos.x() < screen.left())
                pos.setX(screen.left());
            m_fileMenu->popup(pos);
        });

    }
    m_toolBar->addWidget(m_fileMenuBtn);

    // 中间可拖拽区域（Expanding spacer — 双击最大化/还原）
    m_toolbarSpacer = new QWidget;
    m_toolbarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolBar->addWidget(m_toolbarSpacer);

    // 帮助按钮（置于侧栏按钮左侧）
    m_helpAction = new QAction(QIcon(":/icons/help"), tr("帮助"), this);
    m_helpAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_help", "F1")));
    addAction(m_helpAction);
    m_toolBar->addAction(m_helpAction);
    connect(m_helpAction, &QAction::triggered, this, &MainWindow::toggleHelp);

    // 右侧面板（历史/大纲/标签/反链）
    toggleRightPanelAction->setIcon(QIcon(":/icons/panel"));
    m_toolBar->addAction(toggleRightPanelAction);

    // 右侧：预览
    m_previewAction = new QAction(QIcon(":/icons/preview"), tr("预览"), this);
    m_previewAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_preview", "Ctrl+Shift+P")));
    m_previewAction->setCheckable(true);
    addAction(m_previewAction);
    m_toolBar->addAction(m_previewAction);
    connect(m_previewAction, &QAction::toggled, this, [this](bool checked) {
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor) {
            if (checked && !editor->currentFilePath().toLower().endsWith(".md")) {
                return;
            }
            editor->setPreviewMode(checked);
            if (checked && m_splitPreviewAction && m_splitPreviewAction->isChecked()) {
                m_splitPreviewAction->blockSignals(true);
                m_splitPreviewAction->setChecked(false);
                m_splitPreviewAction->blockSignals(false);
            }
        }
    });

    // 分屏预览
    m_splitPreviewAction = new QAction(QIcon(":/icons/split"), tr("分屏"), this);
    m_splitPreviewAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_split_preview", "Ctrl+P")));
    m_splitPreviewAction->setCheckable(true);
    addAction(m_splitPreviewAction);
    m_toolBar->addAction(m_splitPreviewAction);
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

    // 运行 ▼ (编译 / 运行 / 编译运行) — actions 由 CompileRunManager 管理
    addAction(m_compileRunMgr->compileAction());
    addAction(m_compileRunMgr->runAction());
    addAction(m_compileRunMgr->compileRunAction());

    // ── 主运行按钮：QAction + addAction()（标准 Qt 工具栏模式）──
    m_toolbarRunAction = new QAction(QIcon(":/icons/run"), tr("运行"), this);
    m_toolbarRunAction->setToolTip(tr("编译运行"));
    m_toolbarRunAction->setVisible(false);
    connect(m_toolbarRunAction, &QAction::triggered, this, [this]() {
        QTimer::singleShot(0, m_compileRunMgr, &CompileRunManager::compileAndRun);
    });
    m_toolBar->addAction(m_toolbarRunAction);

    // ── 下拉 chevron 按钮：QAction + addAction() + setMenu() ──
    m_toolbarDropdownAction = new QAction(QIcon(":/icons/chevron-down"), tr("运行选项"), this);
    m_toolbarDropdownAction->setMenu(m_compileRunMgr->runMenu());
    m_toolbarDropdownAction->setToolTip(tr("编译/运行/编译运行选项"));
    m_toolbarDropdownAction->setVisible(false);
    m_toolBar->addAction(m_toolbarDropdownAction);

    // 同步：m_runToolAction 状态 → 两个工具栏 Action
    auto syncRunBtns = [this]() {
        if (!m_compileRunMgr) return;
        auto *act = m_compileRunMgr->runToolAction();
        m_toolbarRunAction->setVisible(act->isVisible());
        m_toolbarRunAction->setEnabled(act->isEnabled());
        m_toolbarDropdownAction->setVisible(act->isVisible());
        m_toolbarDropdownAction->setEnabled(act->isEnabled());
    };
    syncRunBtns();
    connect(m_compileRunMgr->runToolAction(), &QAction::changed, this, syncRunBtns);

    // 终止 (Ctrl+Break) — 仅快捷键，不放在工具栏
    addAction(m_compileRunMgr->stopAction());

    // 窗口控制按钮（置于最右）
    setupCustomTitleBar();

    // 设置（快捷键 Ctrl+,，不在工具栏中）
    m_settingsAction = new QAction(tr("设置"), this);
    m_settingsAction->setShortcut(
        QKeySequence(ConfigManager::instance().shortcut("toggle_settings", "Ctrl+,")));
    addAction(m_settingsAction);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::toggleSettings);

    // 导出 PDF（快捷键 Ctrl+E）
    m_exportPdfAction = new QAction(tr("导出PDF"), this);
    m_exportPdfAction->setShortcut(
        QKeySequence(ConfigManager::instance().shortcut("export_pdf", "Ctrl+E")));
    addAction(m_exportPdfAction);
    connect(m_exportPdfAction, &QAction::triggered, this, &MainWindow::onExportPdf);

    // .md ↔ .smd 转换（快捷键 Ctrl+T）
    m_convertMdSmdAction = new QAction(tr("转换 .md ↔ .smd"), this);
    m_convertMdSmdAction->setShortcut(
        QKeySequence(ConfigManager::instance().shortcut("convert_md_smd", "Ctrl+T")));
    addAction(m_convertMdSmdAction);
    connect(m_convertMdSmdAction, &QAction::triggered, this, &MainWindow::onConvertMdSmd);

    // 文件目录变更时重建索引
    connect(m_explorer, &FileExplorerWidget::folderChanged, this, &MainWindow::onFolderChanged);

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
    m_zoomOutBtn = new QToolButton;
    m_zoomOutBtn->setDefaultAction(m_zoomOutAction);
    m_zoomOutBtn->setText("-");
    m_zoomOutBtn->setAutoRaise(true);

    m_zoomInBtn = new QToolButton;
    m_zoomInBtn->setDefaultAction(m_zoomInAction);
    m_zoomInBtn->setText("+");
    m_zoomInBtn->setAutoRaise(true);

    m_zoomResetBtn = new QToolButton;
    m_zoomResetBtn->setDefaultAction(m_zoomResetAction);
    m_zoomResetBtn->setText("重置");
    m_zoomResetBtn->setAutoRaise(true);

    refreshZoomButtonStyle();

    // 将按钮和标签放入一个水平布局的 Widget
    QWidget *zoomWidget = new QWidget();
    QHBoxLayout *zoomLayout = new QHBoxLayout(zoomWidget);
    zoomLayout->setContentsMargins(0, 0, 0, 0);
    zoomLayout->addWidget(m_zoomOutBtn);
    zoomLayout->addWidget(m_zoomLabel);
    zoomLayout->addWidget(m_zoomInBtn);
    zoomLayout->addWidget(m_zoomResetBtn);

    // 添加到状态栏
    status->addPermanentWidget(zoomWidget);

    // 当切换标签页时，更新预览按钮的选中状态
    connect(m_tabManager, &QTabWidget::currentChanged, this, &MainWindow::updatePreviewActionState);
    connect(m_tabManager, &QTabWidget::currentChanged, this, &MainWindow::updateSplitPreviewActionState);

    // 当切换标签页时，更新 AI 动作按钮列表
    connect(m_tabManager, &QTabWidget::currentChanged, this, [this](int) {
        updateAiActionBar();
    });

    // 切换文件时自动关闭 OpenJudge 提交结果面板（由 OpenJudgeManager 管理）

    // ----- 设置面板（悬浮遮罩 + 面板）-----
    m_settingsOverlay = new OverlayWidget();
    m_settingsOverlay->installEventFilter(this);
    m_settingsOverlay->hide();
    m_settingsPanel = new SettingsPanel(m_settingsOverlay);
    connect(m_settingsPanel, &SettingsPanel::closeRequested, this, &MainWindow::toggleSettings);
    connect(m_settingsPanel, &SettingsPanel::defaultZoomChanged, m_settingsHandler, &SettingsChangeHandler::handleDefaultZoom);
    connect(m_settingsPanel, &SettingsPanel::editorSettingChanged, m_settingsHandler, &SettingsChangeHandler::handleEditorSetting);
    connect(m_settingsPanel, &SettingsPanel::appearanceSettingChanged, this, [this](const QString &key, const QVariant &value) {
        // Handle MainWindow-specific appearance changes
        if (key == "editor.file_tree_item_height") {
            m_settings->setSettingOverride(key, value);
            m_explorer->setItemHeight(value.toInt());
            return;
        }
        if (key == "editor.equal_width_tab") {
            m_settings->setSettingOverride(key, value);
            applyEqualWidthTab(value.toBool());
            return;
        }
        if (key == "appearance.tab_height") {
            m_settings->setSettingOverride(key, value);
            ThemeManager::instance().setTabHeight(value.toInt());
            return;
        }
        m_settingsHandler->handleAppearanceSetting(key, value);
    });
    connect(m_settingsPanel, &SettingsPanel::outputPanelSettingChanged, this, [this](const QString &key, const QVariant &value) {
        m_settingsHandler->handleOutputPanelSetting(key, value);
        // OutputPanel is a UI widget owned by MainWindow, apply directly
        if (key == "output_panel.font.size") {
            QFont font = m_bottomPanel->outputPanel()->font();
            font.setPointSize(value.toInt());
            m_bottomPanel->outputPanel()->setOutputFont(font);
        } else if (key == "output_panel.max_blocks") {
            m_bottomPanel->outputPanel()->setMaxBlocks(value.toInt());
        }
    });
    connect(m_settingsPanel, &SettingsPanel::previewSettingChanged, m_settingsHandler, &SettingsChangeHandler::handlePreviewSetting);
    connect(m_settingsPanel, &SettingsPanel::searchSettingChanged, m_settingsHandler, &SettingsChangeHandler::handleSearchSetting);
    connect(m_settingsPanel, &SettingsPanel::aiSettingChanged, m_settingsHandler, &SettingsChangeHandler::handleAiSetting);
    connect(m_settingsPanel, &SettingsPanel::toolSettingChanged, m_settingsHandler, &SettingsChangeHandler::handleToolSetting);
    connect(m_settingsPanel, &SettingsPanel::resetToDefaultsRequested, this, [this]() {
        m_settingsHandler->handleResetToDefaults(m_shortcutActions);
        m_settingsPanel->setDefaultZoom(ConfigManager::instance().zoomDefault());
        m_settingsPanel->syncFromSettings(*m_settings);
        applyEqualWidthTab(false);
        // Reset output panel
        auto &cfg = ConfigManager::instance();
        OutputPanel *op = m_bottomPanel->outputPanel();
        QFont opFont = op->font();
        opFont.setPointSize(cfg.outputPanelFontSize());
        op->setOutputFont(opFont);
        op->setMaxBlocks(cfg.outputPanelMaxBlocks());
        op->reloadShortcuts();
        // Reset preview on current editor
        if (auto *editor = m_tabManager->currentEditor())
            editor->setSplitPreviewDebounceMs(cfg.previewSplitDebounceMs());
    });
    connect(m_settingsPanel, &SettingsPanel::shortcutChanged, this, [this](const QString &actionKey, const QString &seq) {
        m_settingsHandler->handleShortcutChanged(actionKey, seq, m_shortcutActions);
        // OutputPanel and Explorer shortcuts need MainWindow-owned widget references
        if (m_bottomPanel)
            m_bottomPanel->outputPanel()->reloadShortcuts();
        if (m_explorer)
            m_explorer->reloadShortcuts();
    });

    // SettingsChangeHandler signal routing
    connect(m_settingsHandler, &SettingsChangeHandler::zoomLabelUpdateRequested, this, &MainWindow::updateZoomLabel);
    connect(m_settingsHandler, &SettingsChangeHandler::equalWidthTabRequested, this, &MainWindow::applyEqualWidthTab);

    // ----- 帮助面板（悬浮遮罩 + 面板）-----
    m_helpOverlay = new OverlayWidget();
    m_helpOverlay->installEventFilter(this);
    m_helpOverlay->hide();
    m_helpPanel = new HelpPanel(m_helpOverlay);
    connect(m_helpPanel, &HelpPanel::closeRequested, this, &MainWindow::toggleHelp);

    // 应用保存的文件树条目高度
    int treeItemHeight = m_settings->value("editor.file_tree_item_height",
                                           ConfigManager::instance().editorFileTreeItemHeight()).toInt();
    m_explorer->setItemHeight(treeItemHeight);

    // 应用保存的等宽标签页设置
    applyEqualWidthTab(m_settings->value("editor.equal_width_tab", false).toBool());

    // ----- AI 助手面板 -----
    m_aiPanel = new AiPanel(this);
    m_dockAi = new QDockWidget(tr("AI 助手"), this);
    m_dockAi->setWidget(m_aiPanel);
    m_dockAi->setFeatures(QDockWidget::DockWidgetMovable);
    m_dockAi->setTitleBarWidget(createDockTitleBar(tr("AI 助手"), m_dockAi));
    addDockWidget(Qt::RightDockWidgetArea, m_dockAi);
    tabifyDockWidget(m_dockRightPanel, m_dockAi);
    m_dockAi->hide();

    m_toggleAiAction = new QAction(tr("AI 助手"), this);
    m_toggleAiAction->setCheckable(true);
    m_toggleAiAction->setToolTip(tr("显示/隐藏 AI 助手"));
    m_toggleAiAction->setShortcut(
        QKeySequence(ConfigManager::instance().shortcut("toggle_ai", "Ctrl+Shift+A")));
    connect(m_toggleAiAction, &QAction::triggered, this, [this]() {
        if (m_dockAi->isVisible()) {
            m_dockAi->hide();
        } else {
            m_dockRightPanel->hide();
            m_dockAi->show();
            m_dockAi->raise();
        }
    });
    addAction(m_toggleAiAction);

    // AI request handler
    m_aiHandler = new AiRequestHandler(this);
    m_aiHandler->setAiPanel(m_aiPanel);
    m_aiHandler->setTabManager(m_tabManager);
    m_aiHandler->setSettingsManager(m_settings);
    connect(m_aiHandler, &AiRequestHandler::streamingStateChanged, this, [this](bool streaming) {
        m_aiPanel->setInputEnabled(!streaming);
    });

    // Build shortcut action map for dynamic rebinding
    m_shortcutActions["toggle_right_panel"] = toggleRightPanelAction;
    m_shortcutActions["toggle_history"] = m_toggleHistoryAction;
    m_shortcutActions["toggle_outline"] = m_toggleOutlineAction;
    m_shortcutActions["toggle_tags"] = m_toggleTagsAction;
    m_shortcutActions["toggle_backlinks"] = m_toggleBacklinksAction;
    m_shortcutActions["toggle_search"] = toggleSearchAction;
    m_shortcutActions["toggle_explorer"] = m_toggleExplorerAction;
    m_shortcutActions["toggle_judge"] = m_toggleJudgeAction;
    m_shortcutActions["toggle_preview"] = m_previewAction;
    m_shortcutActions["toggle_split_preview"] = m_splitPreviewAction;
    m_shortcutActions["compile_only"] = m_compileRunMgr->compileAction();
    m_shortcutActions["run_only"] = m_compileRunMgr->runAction();
    m_shortcutActions["compile_and_run"] = m_compileRunMgr->compileRunAction();
    m_shortcutActions["stop_process"] = m_compileRunMgr->stopAction();
    m_shortcutActions["toggle_settings"] = m_settingsAction;
    m_shortcutActions["toggle_help"] = m_helpAction;
    m_shortcutActions["export_pdf"] = m_exportPdfAction;
    m_shortcutActions["convert_md_smd"] = m_convertMdSmdAction;
    m_shortcutActions["zoom_in"] = m_zoomInAction;
    m_shortcutActions["zoom_out"] = m_zoomOutAction;
    m_shortcutActions["zoom_reset"] = m_zoomResetAction;
    m_shortcutActions["toggle_ai"] = m_toggleAiAction;

    // AI 端到端信号连接
    connect(m_aiPanel, &AiPanel::sendMessage, this, [this](const QString &text) {
        m_aiHandler->startAiRequest(AiAction::FreeChat, text);
    });
    connect(m_aiPanel, &AiPanel::actionTriggered, this, [this](int actionIndex) {
        m_aiHandler->startAiRequest(static_cast<AiAction>(actionIndex));
    });
    connect(m_aiPanel, &AiPanel::clearRequested, this, [this]() {
        m_aiHandler->clearConversation();
    });
    connect(m_aiPanel, &AiPanel::newConversationRequested, this, [this]() {
        m_aiHandler->newConversation();
    });

    // ── History list widget connections ──
    {
        auto *historyWidget = m_aiPanel->historyListWidget();
        connect(historyWidget, &AiHistoryListWidget::conversationSelected, this, [this](const QString &convId) {
            m_aiHandler->loadAiConversation(convId);
            filterAiHistoryByCurrentFile();
        });
        connect(historyWidget, &AiHistoryListWidget::renameRequested, this, [this](const QString &convId) {
            auto &mgr = AiHistoryManager::instance();
            AiConversation conv = mgr.conversationById(convId);
            if (!conv.isValid()) return;
            bool ok;
            QString newTitle = QInputDialog::getText(this, tr("重命名对话"), tr("新名称："),
                                                       QLineEdit::Normal, conv.title, &ok);
            if (ok && !newTitle.trimmed().isEmpty())
                mgr.renameConversation(convId, newTitle.trimmed());
        });
        connect(historyWidget, &AiHistoryListWidget::deleteRequested, this, [this](const QString &convId) {
            auto &mgr = AiHistoryManager::instance();
            AiConversation conv = mgr.conversationById(convId);
            if (!conv.isValid()) return;
            auto result = QMessageBox::question(this, tr("删除对话"),
                tr("确定要删除「%1」吗？").arg(conv.title));
            if (result == QMessageBox::Yes) {
                if (convId == mgr.currentConversationId()) {
                    m_aiPanel->clearChat();
                    m_aiHandler->clearHistory();
                }
                mgr.deleteConversation(convId);
            }
        });
        connect(historyWidget, &AiHistoryListWidget::exportRequested, this, [this](const QString &convId) {
            auto &mgr = AiHistoryManager::instance();
            AiConversation conv = mgr.conversationById(convId);
            if (!conv.isValid()) return;
            QString filePath = QFileDialog::getSaveFileName(this, tr("导出对话"),
                conv.title + QStringLiteral(".md"), tr("Markdown (*.md)"));
            if (filePath.isEmpty()) return;
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(mgr.exportToMarkdown(convId).toUtf8());
            }
        });
    }

    // ── Refresh history list when manager signals changes ──
    connect(&AiHistoryManager::instance(), &AiHistoryManager::conversationListChanged,
            this, &MainWindow::filterAiHistoryByCurrentFile);

    // ── Refresh history list when history tab becomes visible ──
    connect(m_aiPanel, &AiPanel::historyListVisibilityChanged, this, [this](bool) {
        filterAiHistoryByCurrentFile();
        m_aiPanel->historyListWidget()->setActiveConversationId(
            AiHistoryManager::instance().currentConversationId());
    });

    // ----- 界面布局 -----
    // 左侧活动栏（搜索/设置/导出PDF/评测）
    m_activityBar = new ActivityBar(this);

    m_splitter->addWidget(m_activityBar);
    m_splitter->addWidget(m_leftStack);
    m_splitter->addWidget(m_rightSplitter);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setStretchFactor(2, 1);
    setCentralWidget(m_splitter);

    // 当标签页切换时，更新缩放标签并重新连接当前编辑器的缩放信号
    connect(m_tabManager, &QTabWidget::currentChanged, this, [this](int) {
        updateZoomLabel();
        connectCurrentEditorZoomSignal();
        syncFileTreeSelection();

        EditorWidget *editor = m_tabManager->currentEditor();

        // Disable save/save-as when OpenJudge tab is active (not a file)
        bool isOpenJudgeTab = (qobject_cast<OpenJudgeWidget*>(m_tabManager->currentWidget()) != nullptr);
        if (auto *act = m_shortcutActions.value("save"))
            act->setEnabled(!isOpenJudgeTab);
        if (auto *act = m_shortcutActions.value("save_as"))
            act->setEnabled(!isOpenJudgeTab);

        // 连接当前编辑器的诊断面板切换信号
        if (editor) {
            connect(editor, &EditorWidget::diagnosticsToggleRequested,
                    m_compileRunMgr, &CompileRunManager::toggleDiagnostics,
                    Qt::UniqueConnection);
        }
        // 连接 fileLoaded 信号：覆盖所有文件加载场景（含预览标签复用不触发 currentChanged 的情况）
        disconnect(m_fileLoadedConnection);
        if (editor) {
            m_fileLoadedConnection = connect(editor, &EditorWidget::fileLoaded,
                                             this, [this](const QString &) {
                EditorWidget *current = m_tabManager->currentEditor();
                if (current != sender()) return;
                refreshBacklinks();
                refreshTags();
                refreshOutline();
                filterAiHistoryByCurrentFile();
                updateCurrentEditorCompletions();
                if (m_compileRunMgr)
                    m_compileRunMgr->updateActions();

                // 更新导出PDF按钮可见性（预览标签复用时不会触发 currentChanged）
                bool isMd = current->currentFilePath().toLower().endsWith(".md");
                m_exportPdfAction->setEnabled(isMd);
                m_exportPdfAction->setVisible(isMd);
                m_activityBar->setExportPdfVisible(isMd);

                // Auto-close output panel for non-code files
                if (!current->isCodeEdit()) {
                    debugLog(QString("fileLoaded: non-code file=%1, hiding panel")
                        .arg(current->currentFilePath()));
                    disconnect(m_diagnosticsProviderConnection);
                    m_bottomPanel->setCurrentEditor(nullptr);
                    if (m_compileRunMgr && m_compileRunMgr->isRunning())
                        m_compileRunMgr->stop();
                    QPointer<BottomPanel> bp = m_bottomPanel;
                    QTimer::singleShot(0, this, [bp]() {
                        if (bp) bp->hide();
                    });
                }
            });
        }

        refreshBacklinks();
        refreshTags();
        refreshOutline();
        filterAiHistoryByCurrentFile();
        updateCurrentEditorCompletions();

        // 更新编译运行按钮状态（代码文件 或 OpenJudge IDE 模式）
        if (m_compileRunMgr)
            m_compileRunMgr->updateActions();

        // 导出PDF 仅对 .md 文件启用（按钮可见 + 快捷键生效）
        bool isMd = editor && editor->currentFilePath().toLower().endsWith(".md");
        m_exportPdfAction->setEnabled(isMd);
        m_exportPdfAction->setVisible(isMd);
        m_activityBar->setExportPdfVisible(isMd);

        // 代码编辑器诊断连接（交由统一函数处理，与 onFileSelected 共享逻辑）
        debugLog(QString("tabSwitch: editor=%1 isCode=%2 file=%3")
            .arg((quintptr)editor)
            .arg(editor ? (int)editor->isCodeEdit() : -1)
            .arg(editor ? editor->currentFilePath() : "null"));
        updateCurrentEditorDiagnostics();
        if (editor && !editor->isCodeEdit()) {
            // Non-code file: auto-close the run panel
            debugLog("tabSwitch: non-code branch - hiding panel");
            if (m_compileRunMgr && m_compileRunMgr->isRunning())
                m_compileRunMgr->stop();
            QPointer<BottomPanel> bp = m_bottomPanel;
            QTimer::singleShot(0, this, [bp]() {
                if (bp) bp->hide();
            });
        }
    });

    // 文件浏览面板折叠/展开
    m_toggleExplorerAction = new QAction(tr("切换文件浏览"), this);
    m_toggleExplorerAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("toggle_explorer", "Ctrl+B")));
    connect(m_toggleExplorerAction, &QAction::triggered, this, &MainWindow::toggleLeftPanel);
    addAction(m_toggleExplorerAction);

    // ActivityBar 信号连接
    connect(m_activityBar, &ActivityBar::explorerClicked, this, [this]() {
        if (m_leftStack->currentIndex() == 0 && !m_leftStack->isHidden())
            toggleLeftPanel();
        else
            showLeftPanel(0);
    });
    connect(m_activityBar, &ActivityBar::searchClicked, this, [this]() {
        if (m_leftStack->currentIndex() == 1 && !m_leftStack->isHidden()) {
            toggleLeftPanel();
        } else {
            showLeftPanel(1);
            m_searchPanel->focusSearchInput();
        }
    });
    connect(m_activityBar, &ActivityBar::settingsClicked, this, &MainWindow::toggleSettings);
    connect(m_activityBar, &ActivityBar::exportPdfClicked, this, &MainWindow::onExportPdf);
    connect(m_activityBar, &ActivityBar::judgeClicked, this, [this]() {
        if (m_leftStack->currentIndex() == 2 && !m_leftStack->isHidden())
            toggleLeftPanel();
        else
            showLeftPanel(2);
    });
    connect(m_activityBar, &ActivityBar::aiClicked, this, [this]() {
        if (m_dockAi->isVisible()) {
            m_dockAi->hide();
        } else {
            m_dockRightPanel->hide();
            m_dockAi->show();
            m_dockAi->raise();
            m_activityBar->setAiActive(true);
        }
    });

    // 同步 ActivityBar 激活状态与面板可见性
    connect(m_dockAi, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        m_activityBar->setAiActive(visible);
        m_toggleAiAction->setChecked(visible);
    });

    // 初始连接
    connectCurrentEditorZoomSignal();
    updateZoomLabel();

    // 连接信号：文件树单击 → 预览打开，双击 → 永久打开
    connect(m_explorer, &FileExplorerWidget::fileClicked, this, &MainWindow::onFileSelected);
    connect(m_explorer, &FileExplorerWidget::fileDoubleClicked, this, &MainWindow::onFileDoubleClicked);

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
    loadSettings();
    checkCrashRecovery(); // 检测崩溃恢复文件
    m_searchPanel->setRootPath(m_explorer->rootPath());
    startAsyncIndexBuild();
    updatePreviewActionState();
    updateSplitPreviewActionState();
    updateAiActionBar();

    // Scrollbar auto-hide: manage all scrollable areas
    auto *hider = new ScrollbarHider(this);
    {
        const auto areas = findChildren<QAbstractScrollArea*>();
        for (auto *area : areas) {
            hider->manage(area);
            // PDF scrollbar always visible
            if (qobject_cast<QPdfView*>(area)) {
                hider->setAlwaysVisible(area);
            }
        }
    }

    // Watch for dynamically created QAbstractScrollAreas (e.g. PDF view in new tabs)
    // Only scan each editor once — its widget tree is stable after creation.
    connect(m_tabManager, &QTabWidget::currentChanged, this, [this, hider](int) {
        if (auto *editor = m_tabManager->currentEditor()) {
            if (m_editorScrollAreasRegistered.contains(editor))
                return;
            m_editorScrollAreasRegistered.insert(editor);
            connect(editor, &QObject::destroyed, this, [this](QObject *obj) {
                m_editorScrollAreasRegistered.remove(
                    static_cast<EditorWidget*>(obj));
            });
            const auto areas = editor->findChildren<QAbstractScrollArea*>();
            for (auto *area : areas) {
                hider->manage(area);
                // PDF scrollbar always visible
                if (qobject_cast<QPdfView*>(area)) {
                    hider->setAlwaysVisible(area);
                }
            }
        }
    });

    // Themed toolbar icons and spacing — apply initially and on theme switch
    refreshTitleBarStyle();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::refreshTitleBarStyle);

    // Reapply global QSS to trigger a full widget-tree repolish.
    // Without this, QToolBarLayout uses Fusion's default internal margins
    // (e.g. PM_ToolBarItemMargin) that aren't cleared by the toolbar's own
    // stylesheet's padding:0 until a parent-triggered style recalculation —
    // which normally only happens on theme switch via loadQss().
    ThemeManager::instance().loadQss();

}

MainWindow::~MainWindow()
{
    delete ui;
}

// ----- 转发给TabManager的槽函数 -----
void MainWindow::onFileSelected(const QString &filePath)
{
    m_tabManager->openPreview(filePath);
    if (auto *editor = m_tabManager->currentEditor()) {
        editor->setZoomFactor(m_settings->editorDefaultZoom());
    }
    updateZoomLabel();
    updatePreviewActionState();
    updateSplitPreviewActionState();
    addToRecentFiles(filePath);
    // 更新运行按钮显隐（代码文件 或 OpenJudge IDE 模式）
    if (m_compileRunMgr)
        m_compileRunMgr->updateActions();
    // openPreview may reuse the existing preview tab without changing
    // the current index, so currentChanged is NOT emitted. Explicitly
    // refresh side panels whose content depends on the active file.
    refreshBacklinks();
    refreshTags();
    refreshOutline();
    filterAiHistoryByCurrentFile();
    updateCurrentEditorCompletions();
    // Also update diagnostics — preview reuse bypasses the currentChanged
    // handler that normally wires up the provider connection.
    updateCurrentEditorDiagnostics();
}

void MainWindow::updateCurrentEditorDiagnostics()
{
    auto *editor = m_tabManager->currentEditor();
    if (editor && editor->isCodeEdit()) {
        CodeEditor *ce = qobject_cast<CodeEditor*>(
            editor->findChild<CodeEditor*>());
        if (ce && ce->completionProvider()) {
            m_bottomPanel->setCurrentEditor(ce);
            disconnect(m_diagnosticsProviderConnection);
            m_diagnosticsProviderConnection = connect(
                ce->completionProvider(),
                &CompletionProvider::diagnosticsUpdated,
                m_bottomPanel, &BottomPanel::setDiagnostics);
            m_bottomPanel->setDiagnostics(ce->diagnostics());
        }
    } else {
        disconnect(m_diagnosticsProviderConnection);
        m_bottomPanel->setCurrentEditor(nullptr);
        m_bottomPanel->clearDiagnostics();
    }
}

void MainWindow::onFileDoubleClicked(const QString &filePath)
{
    // 如果目标文件就是当前预览标签页的文件，直接提升为永久
    EditorWidget *preview = m_tabManager->previewEditor();
    if (preview) {
        QString previewPath = QDir::cleanPath(preview->currentFilePath());
        QString targetPath  = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
        if (previewPath.compare(targetPath, Qt::CaseInsensitive) == 0) {
            m_tabManager->promotePreviewToPermanent();
            return;
        }
    }
    // 否则：以永久方式打开
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
            m_indexManager->backlinkIndex()->rebuildFile(editor->currentFilePath(),
                                         m_explorer->rootPath(), m_indexManager->fileIndex());
            m_indexManager->tagIndex()->rebuildFile(editor->currentFilePath());
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
        // 异步重建文件索引，完成后更新 backlink/tag
        buildFileIndexAsync([this, newFilePath]() {
            m_indexManager->backlinkIndex()->rebuildFile(newFilePath, m_explorer->rootPath(), m_indexManager->fileIndex());
            m_indexManager->tagIndex()->rebuildFile(newFilePath);
            refreshBacklinks();
            refreshTags();
        });
        updatePreviewActionState();
        updateSplitPreviewActionState();
        addToRecentFiles(newFilePath);
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
    m_rightPanel->historyPanel()->saveHistory();
    m_settings->flushOverrides();
    saveSettings();
    m_crashRecovery->clearRecoveryDirectory(); // 正常关闭，清理恢复目录
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
    // 验证保存的分割条状态是否与当前 widget 数量匹配
    bool validSplitter = false;
    if (!splitterData.isEmpty()) {
        QSplitter testSplitter(Qt::Horizontal);
        // 按相同顺序添加虚拟 widget 进行验证
        testSplitter.addWidget(new QWidget);
        testSplitter.addWidget(new QWidget);
        testSplitter.addWidget(new QWidget);
        validSplitter = testSplitter.restoreState(splitterData);
    }
    if (validSplitter) {
        m_splitter->restoreState(splitterData);
    } else {
        m_splitter->setStretchFactor(2, cfg.mainSplitterDefaultRatio());
    }
}

static void positionOverlay(QWidget *overlay, QWidget *panel, const QMainWindow *mainWindow)
{
    QPoint topLeft = mainWindow->mapToGlobal(QPoint(0, 0));
    overlay->setGeometry(topLeft.x(), topLeft.y(), mainWindow->width(), mainWindow->height());
    int panelW = panel->width();
    int panelH = panel->height();
    int x = (mainWindow->width() - panelW) / 2;
    int y = (mainWindow->height() - panelH) / 2;
    panel->move(qMax(0, x), qMax(0, y));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_settingsOverlay && m_settingsOverlay->isVisible())
        positionOverlay(m_settingsOverlay, m_settingsPanel, this);
    if (m_helpOverlay && m_helpOverlay->isVisible())
        positionOverlay(m_helpOverlay, m_helpPanel, this);
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    if (m_settingsOverlay && m_settingsOverlay->isVisible()) {
        QPoint topLeft = mapToGlobal(QPoint(0, 0));
        m_settingsOverlay->move(topLeft);
    }
    if (m_helpOverlay && m_helpOverlay->isVisible()) {
        QPoint topLeft = mapToGlobal(QPoint(0, 0));
        m_helpOverlay->move(topLeft);
    }
}

void MainWindow::toggleSettings()
{
    if (m_settingsOverlay->isVisible()) {
        m_settingsOverlay->hide();
        if (auto *editor = m_tabManager->currentEditor())
            editor->refreshPreviewTheme();
    } else {
        m_settingsPanel->setDefaultZoom(m_settings->editorDefaultZoom());
        m_settingsPanel->syncFromSettings(*m_settings);
        positionOverlay(m_settingsOverlay, m_settingsPanel, this);
        m_settingsOverlay->show();
    }
}

void MainWindow::toggleHelp()
{
    if (m_helpOverlay->isVisible()) {
        m_helpOverlay->hide();
    } else {
        positionOverlay(m_helpOverlay, m_helpPanel, this);
        m_helpOverlay->show();
    }
}

void MainWindow::showRightPanel(int panelIndex)
{
    m_dockAi->hide();
    m_dockRightPanel->show();
    m_dockRightPanel->raise();
    m_rightPanel->setActivePanel(panelIndex);
}

void MainWindow::showLeftPanel(int index)
{
    // 如果面板已折叠，先展开并恢复宽度
    if (m_leftStack->isHidden()) {
        m_leftStack->show();
        QList<int> sizes = m_splitter->sizes();
        if (sizes.size() == 3) {
            int available = sizes[2] - m_savedLeftPanelWidth;
            if (available < 100) available = 100;
            sizes[1] = m_savedLeftPanelWidth;
            sizes[2] = available;
            m_splitter->setSizes(sizes);
        }
    }

    m_leftStack->setCurrentIndex(index);
    m_activityBar->setExplorerActive(index == 0);
    m_activityBar->setSearchActive(index == 1);
    m_activityBar->setJudgeActive(index == 2);

    if (index > 0 && m_dockRightPanel)
        m_dockRightPanel->hide();
}

void MainWindow::toggleLeftPanel()
{
    if (m_leftStack->isHidden()) {
        showLeftPanel(m_leftStack->currentIndex());
    } else {
        m_savedLeftPanelWidth = m_leftStack->width();
        m_leftStack->hide();
        m_activityBar->setExplorerActive(false);
        m_activityBar->setSearchActive(false);
        m_activityBar->setJudgeActive(false);
    }
}

void MainWindow::updateAiActionBar()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor) {
        m_aiPanel->clearActionList();
        return;
    }

    const AiEditorMode mode = AiContextManager::currentEditorMode(editor);
    m_aiPanel->setActionList(actionsForMode(mode));
}

void MainWindow::filterAiHistoryByCurrentFile()
{
    auto *historyWidget = m_aiPanel->historyListWidget();
    if (!historyWidget)
        return;

    auto &mgr = AiHistoryManager::instance();
    QList<AiConversation> convs = mgr.conversationsByFile(m_currentFilePath);
    historyWidget->setConversations(convs);
    historyWidget->setActiveConversationId(mgr.currentConversationId());
}

void MainWindow::applyEqualWidthTab(bool enabled)
{
    if (auto *bar = qobject_cast<CustomTabBar*>(m_tabManager->tabBar()))
        bar->setEqualWidth(enabled);
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

void MainWindow::refreshZoomButtonStyle()
{
    auto &tm = ThemeManager::instance();
    QString zoomBtnStyle = QStringLiteral(
        "QToolButton { background: transparent; color: %1; border: none; border-radius: 3px;"
        "  padding: 0px 4px; font-size: 18px; font-weight: bold; min-height: 24px; max-height: 24px; }"
        "QToolButton:hover { background: %2; }"
    ).arg(tm.color("workbench.foreground").name(),
          tm.color("button.hoverBackground").name());
    QString zoomResetBtnStyle = QStringLiteral(
        "QToolButton { background: transparent; color: %1; border: none; border-radius: 3px;"
        "  padding: 0px 4px; min-height: 24px; max-height: 24px; }"
        "QToolButton:hover { background: %2; }"
    ).arg(tm.color("workbench.foreground").name(),
          tm.color("button.hoverBackground").name());
    if (m_zoomOutBtn) m_zoomOutBtn->setStyleSheet(zoomBtnStyle);
    if (m_zoomInBtn) m_zoomInBtn->setStyleSheet(zoomBtnStyle);
    if (m_zoomResetBtn) m_zoomResetBtn->setStyleSheet(zoomResetBtnStyle);
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
                                        m_codeBlockRunner, &CodeBlockRunner::runCodeBlock);
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
        m_rightPanel->historyPanel()->addFile(cleanPath);
}

void MainWindow::refreshBacklinks()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor || editor->currentFilePath().isEmpty()) {
        m_rightPanel->backlinksPanel()->showBacklinks({});
        return;
    }

    QString filePath = editor->currentFilePath();
    QStringList sources = m_indexManager->backlinkIndex()->backlinksFor(filePath);
    m_rightPanel->backlinksPanel()->showBacklinks(sources);
}

void MainWindow::refreshTags()
{
    QStringList tags = m_indexManager->tagIndex()->allTags();
    tags.sort(Qt::CaseInsensitive);
    m_rightPanel->tagPanel()->showAllTags(tags);
}

void MainWindow::refreshOutline()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor || !editor->currentFilePath().toLower().endsWith(QStringLiteral(".md"))) {
        m_rightPanel->outlinePanel()->clear();
        return;
    }
    QString content = editor->toPlainText();
    auto headings = extractHeadingsFromContent(content);
    m_rightPanel->outlinePanel()->showHeadings(headings);
}

void MainWindow::onTagClicked(const QString &tag)
{
    QStringList files = m_indexManager->tagIndex()->filesForTag(tag);
    files.sort();
    m_rightPanel->tagPanel()->showFilesForTag(tag, files);
    m_rightPanel->setActivePanel(2); // 切换到标签面板
    m_dockRightPanel->show();
    m_dockRightPanel->raise();
}

void MainWindow::onHistoryFileClicked(const QString &filePath)
{
    if (!QFile::exists(filePath)) {
        QMessageBox::warning(this, tr("文件不存在"),
                             tr("无法打开文件，文件可能已被移动或删除：\n%1").arg(filePath));
        m_rightPanel->historyPanel()->removeFile(filePath);
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
        startAsyncIndexBuild();
        syncFileTreeSelection();
    }
    addToRecentFiles(absolutePath); // 记录到历史（置顶）
    m_dockRightPanel->hide(); // 隐藏面板
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

void MainWindow::onSearchTextChangedByUser()
{
    if (auto *editor = m_tabManager->currentEditor())
        editor->clearExtraSelections();
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

void MainWindow::startAsyncIndexBuild()
{
    m_indexManager->startAsyncIndexBuild(m_explorer->rootPath());
}

void MainWindow::buildFileIndexAsync(std::function<void()> onComplete)
{
    QString root = m_explorer->rootPath();
    m_indexManager->buildFileIndexAsync(root, std::move(onComplete));
}

void MainWindow::updateCurrentEditorCompletions()
{
    m_indexManager->updateCurrentEditorCompletions(m_tabManager->currentEditor());
}

void MainWindow::onFileRenamedInIndex(const QString &oldPath, const QString &newPath)
{
    // Capture affected sources before the index update
    QString oldBaseName = QFileInfo(oldPath).completeBaseName();
    QString newBaseName = QFileInfo(newPath).completeBaseName();
    QStringList affectedSources;
    if (oldBaseName != newBaseName) {
        affectedSources = m_indexManager->backlinkIndex()->backlinksFor(oldPath);
    }

    // Delegate index + tab manager updates
    m_indexManager->onFileRenamedInIndex(oldPath, newPath);

    // Async rebuild + wiki link update
    buildFileIndexAsync([this, affectedSources = std::move(affectedSources),
                         oldBaseName, newBaseName]() {
        updateWikiLinksAfterRename(affectedSources, oldBaseName, newBaseName);
        refreshBacklinks();
    });
}

void MainWindow::onFileDeletedInIndex(const QString &path)
{
    m_indexManager->onFileDeletedInIndex(path);
    buildFileIndexAsync();
    m_rightPanel->historyPanel()->removeFile(path);
    refreshBacklinks();
}

void MainWindow::updateWikiLinksAfterRename(const QStringList &affectedSources,
                                             const QString &oldLinkText,
                                             const QString &newLinkText)
{
    if (affectedSources.isEmpty() || oldLinkText == newLinkText)
        return;

    // Collect editor content on main thread first — GUI access must stay here
    struct SourceInfo {
        QString path;
        QString content;  // non-empty = from editor; empty = read from disk in thread
        bool fromEditor = false;
    };
    QList<SourceInfo> sources;
    sources.reserve(affectedSources.size());
    for (const QString &srcPath : affectedSources) {
        SourceInfo info;
        info.path = srcPath;
        EditorWidget *editor = m_tabManager->findEditorByPath(srcPath);
        if (editor) {
            info.content = editor->toPlainText();
            info.fromEditor = true;
        }
        sources.append(std::move(info));
    }

    QString rootPath = m_explorer->rootPath();
    int updateId = m_indexManager->nextWikiLinkUpdateId();

    QThread::create([this, sources = std::move(sources), oldLinkText, newLinkText,
                     rootPath, updateId]() {
        if (updateId != m_indexManager->currentWikiLinkUpdateId()) return;

        QStringList writtenPaths;
        writtenPaths.reserve(sources.size());
        for (const auto &src : sources) {
            if (updateId != m_indexManager->currentWikiLinkUpdateId()) return;

            QString content = src.content;
            if (content.isEmpty() && !src.fromEditor) {
                QFile file(src.path);
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                    continue;
                QTextStream in(&file);
                content = in.readAll();
                file.close();
            }

            QString newContent = replaceWikiLinkText(content, oldLinkText, newLinkText);
            if (newContent == content)
                continue;

            QFile outFile(src.path);
            if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
                continue;
            QTextStream out(&outFile);
            out << newContent;
            outFile.close();

            writtenPaths.append(src.path);
        }

        if (updateId != m_indexManager->currentWikiLinkUpdateId()) return;

        QMetaObject::invokeMethod(this, [this, updateId,
                                         writtenPaths = std::move(writtenPaths),
                                         rootPath]() {
            if (updateId != m_indexManager->currentWikiLinkUpdateId()) return;

            // Capture the current editor BEFORE any reloads.
            EditorWidget *currentBefore = m_tabManager->currentEditor();

            for (const QString &path : writtenPaths) {
                // rebuildFile FIRST so that fileLoaded→refreshBacklinks sees fresh data
                m_indexManager->backlinkIndex()->rebuildFile(path, rootPath, m_indexManager->fileIndex());
                EditorWidget *editor = m_tabManager->findEditorByPath(path);
                if (editor)
                    editor->loadFile(path);
            }

            // Only refresh if the active tab was NOT changed by the reloads.
            EditorWidget *currentAfter = m_tabManager->currentEditor();
            if (currentAfter == currentBefore) {
                refreshBacklinks();
            }
        }, Qt::QueuedConnection);
    })->start();
}

// ============================================================
// 标题栏样式刷新 — 响应主题切换
// ============================================================

void MainWindow::refreshTitleBarStyle()
{
    auto &tm = ThemeManager::instance();

    // Toolbar background and text color
    m_toolBar->setStyleSheet(QStringLiteral(
        "QToolBar { background: %1; border: none; spacing: 2px; padding: 0; }"
        "QToolBar QToolButton {"
        "  background: transparent; border: none; padding: 4px 8px;"
        "}"
        "QToolBar QToolButton:hover {"
        "  background: %2;"
        "}"
    ).arg(tm.color("titleBar.background").name(),
          tm.color("titleBar.buttonHover").name()));
    m_toolBar->setContentsMargins(0, 0, 0, 0);

    // File menu button — wider hover, themed "v" chevron, no native menu-indicator
    QColor fileFg = tm.color("titleBar.foreground");
    m_fileMenuBtn->setIcon(coloredSvgIcon(":/icons/chevron-down", fileFg, 7));
    m_fileMenuBtn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  color: %1; background: transparent; border: none;"
        "  padding-left: 12px; padding-right: 12px;"
        "  font-size: 12px; spacing: -2px;"
        "}"
        "QToolButton:hover { background: %2; }"
        "QToolButton::menu-indicator { image: none; width: 0px; }"
    ).arg(fileFg.name(),
          tm.color("titleBar.buttonHover").name()));

    // Run buttons: main (green ▶) + dropdown (themed chevron)
    if (m_toolbarRunAction) {
        m_toolbarRunAction->setIcon(coloredSvgIcon(":/icons/run", QColor("#4CAF50")));
    }
    if (m_toolbarDropdownAction) {
        m_toolbarDropdownAction->setIcon(
            coloredSvgIcon(":/icons/chevron-down", tm.color("titleBar.foreground"), 6));
    }
    // Style the QToolButton widgets created by addAction()
    if (auto *btn = qobject_cast<QToolButton *>(
            m_toolBar->widgetForAction(m_toolbarRunAction))) {
        btn->setStyleSheet(QStringLiteral(
            "QToolButton { background: transparent; border: none;"
            "  padding: 4px 4px 4px 6px; }"
            "QToolButton:hover { background: %1; }"
        ).arg(tm.color("titleBar.buttonHover").name()));
    }
    if (auto *btn = qobject_cast<QToolButton *>(
            m_toolBar->widgetForAction(m_toolbarDropdownAction))) {
        btn->setPopupMode(QToolButton::InstantPopup);
        btn->setIconSize(QSize(6, 6));
        btn->setStyleSheet(QStringLiteral(
            "QToolButton { background: transparent; border: none;"
            "  padding: 4px 4px 4px 2px; }"
            "QToolButton:hover { background: %1; }"
            "QToolButton::menu-indicator { image: none; width: 0px; }"
        ).arg(tm.color("titleBar.buttonHover").name()));
    }

    // File menu dropdown
    m_fileMenu->setStyleSheet(QStringLiteral(
        "QMenu { background: %1; border: 1px solid %2; padding: 4px; }"
        "QMenu::item { padding: 6px 24px; color: %3; }"
        "QMenu::item:selected { background: %4; color: %5; }"
        "QMenu::separator { height: 1px; background: %6; margin: 4px 8px; }"
    ).arg(tm.color("menu.background").name(),
          tm.color("panel.border").name(),
          tm.color("menu.foreground").name(),
          tm.color("menu.selectionBackground").name(),
          tm.color("badge.foreground").name(),
          tm.color("menu.separatorColor").name()));

    // Run menus (main button + dropdown chevron button)
    if (m_compileRunMgr) {
        QString runMenuQss = QStringLiteral(
            "QMenu { background: %1; border: 1px solid %2; padding: 4px; }"
            "QMenu::item { padding: 6px 24px; color: %3; }"
            "QMenu::item:selected { background: %4; color: %5; }"
            "QMenu::separator { height: 1px; background: %6; margin: 4px 8px; }"
        ).arg(tm.color("menu.background").name(),
              tm.color("panel.border").name(),
              tm.color("menu.foreground").name(),
              tm.color("menu.selectionBackground").name(),
              tm.color("badge.foreground").name(),
              tm.color("menu.separatorColor").name());
        m_compileRunMgr->runMenu()->setStyleSheet(runMenuQss);
    }

    // Themed toolbar action icons — match activity bar icons
    QColor titleFg = tm.color("activityBar.foreground");
    m_helpAction->setIcon(coloredSvgIcon(":/icons/help", titleFg));
    toggleRightPanelAction->setIcon(coloredSvgIcon(":/icons/panel", titleFg));
    m_previewAction->setIcon(coloredSvgIcon(":/icons/preview", titleFg));
    m_splitPreviewAction->setIcon(coloredSvgIcon(":/icons/split", titleFg));

    // Themed close button icons — adapt to titleBar.foreground for light/dark mode
    QColor closeFg = tm.color("titleBar.foreground");
    auto recolorCloseBtn = [&](QPushButton *btn) {
        if (btn) btn->setIcon(coloredSvgIcon(":/icons/close", closeFg, 16));
    };
    recolorCloseBtn(m_leftSearchCloseBtn);
    recolorCloseBtn(m_leftJudgeCloseBtn);
    for (auto *dock : {m_dockRightPanel, m_dockAi}) {
        if (dock)
            recolorCloseBtn(dock->findChild<QPushButton*>("dockTitleCloseBtn"));
    }
}

// ============================================================
// 自定义标题栏 — 自绘 TitleBarButton
// ============================================================
#include "titlebarbutton.h"

void MainWindow::setupCustomTitleBar()
{
    QToolBar *tb = findChild<QToolBar*>();
    if (!tb) return;

    // Spacer is already created and added before the call
    m_toolbarSpacer->installEventFilter(this);

    m_minimizeBtn = new TitleBarButton(TitleBarButton::Minimize, this);
    connect(m_minimizeBtn, &QPushButton::clicked, this, &QMainWindow::showMinimized);
    tb->addWidget(m_minimizeBtn);

    m_maximizeBtn = new TitleBarButton(TitleBarButton::Maximize, this);
    connect(m_maximizeBtn, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) showNormal(); else showMaximized();
    });
    tb->addWidget(m_maximizeBtn);

    m_closeBtn = new TitleBarButton(TitleBarButton::Close, this);
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
            if (isMaximized()) {
                m_maximizeBtn->setType(TitleBarButton::Restore);
                m_maximizeBtn->setToolTip(tr("还原"));
            } else {
                m_maximizeBtn->setType(TitleBarButton::Maximize);
                m_maximizeBtn->setToolTip(tr("最大化"));
            }
        }
        // 最小化时隐藏顶层 overlay，恢复时重新显示
        if (isMinimized()) {
            if (m_settingsOverlay && m_settingsOverlay->isVisible())
                m_settingsOverlay->hide();
            if (m_helpOverlay && m_helpOverlay->isVisible())
                m_helpOverlay->hide();
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
                    // 最大化时：不立即还原，等 MouseMove 确认是拖拽才还原
                    m_toolbarDragPending = true;
                } else {
                    if (windowHandle())
                        windowHandle()->startSystemMove();
                }
                return true;
            }
        }
        // 最大化窗口拖拽：检测到实际鼠标移动才还原
        if (isToolbarOrSpacer && event->type() == QEvent::MouseMove
            && m_toolbarDragPending && isMaximized()) {
            m_toolbarDragPending = false;
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            QPoint gpos = me->globalPosition().toPoint();
            QPoint localInWindow = mapFromGlobal(gpos);
            showNormal();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            move(gpos.x() - localInWindow.x(), gpos.y() - localInWindow.y());
            if (windowHandle())
                windowHandle()->startSystemMove();
            return true;
        }
        if (isToolbarOrSpacer && event->type() == QEvent::MouseButtonRelease
            && m_toolbarDragPending) {
            // 单击不还原，只清除标志
            m_toolbarDragPending = false;
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
        // 点击编辑器区域时隐藏右侧面板（补全 focusChanged 无法处理焦点不变的场景）
        if (m_dockRightPanel && m_dockRightPanel->isVisible()
            && m_tabManager) {
            QWidget *clickedWidget = qobject_cast<QWidget*>(watched);
            if (clickedWidget && (watched == m_tabManager || m_tabManager->isAncestorOf(clickedWidget))) {
                m_dockRightPanel->hide();
                return true;
            }
        }
        // Close settings/help overlays when clicking overlay background
        if (watched == m_settingsOverlay && m_settingsOverlay->isVisible()) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (!m_settingsPanel->geometry().contains(me->pos())) {
                toggleSettings();
                return true;
            }
        }
        if (watched == m_helpOverlay && m_helpOverlay->isVisible()) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (!m_helpPanel->geometry().contains(me->pos())) {
                toggleHelp();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

QString MainWindow::findWikiTarget(const QString &fileName)
{
    QString root = m_explorer->rootPath();
    if (root.isEmpty()) return {};

    QString currentDir;
    EditorWidget *currentEditor = m_tabManager->currentEditor();
    if (currentEditor && !currentEditor->currentFilePath().isEmpty()) {
        currentDir = QFileInfo(currentEditor->currentFilePath()).absolutePath();
    } else {
        currentDir = root;
    }

    return m_indexManager->findWikiTarget(fileName, root, currentDir);
}

void MainWindow::onFileMovedOrRenamed(const QString &oldPath, const QString &newPath)
{
    onFileRenamedInIndex(oldPath, newPath); // 更新全局双向链接索引
    m_tabManager->updatePathsAfterMove(oldPath, newPath); // 更新所有已打开标签页的路径
    m_rightPanel->historyPanel()->replacePath(oldPath, newPath); // 更新历史记录中的路径
}


void MainWindow::onJudgeRunAll()
{
    QString filePath;

    // IDE mode: use OpenJudge embedded editor
    OpenJudgeWidget *oj = m_tabManager->findOpenJudgeWidget();
    if (oj && oj->isIdeMode()) {
        oj->saveIdeCodeToCache();
        filePath = oj->ideCacheFilePath();
        if (filePath.isEmpty())
            return;
    } else {
        EditorWidget *editor = m_tabManager->currentEditor();
        if (!editor || !editor->isCodeEdit()) {
            QMessageBox::information(this, tr("提示"),
                                      tr("请打开一个代码文件进行评测。"));
            return;
        }

        // Save current code to file (or temp file if unsaved)
        filePath = editor->currentFilePath();
        if (filePath.isEmpty() || editor->isModified()) {
            filePath = CompileRunManager::saveEditorToTempFile(editor, m_explorer->rootPath());
            if (filePath.isEmpty())
                return;
        }
    }

    // Ensure test folder is set
    if (m_judgePanel->testFolder().isEmpty()) {
        QMessageBox::information(this, tr("提示"),
                                  tr("请先在评测面板中选择测试用例文件夹。"));
        return;
    }

    showLeftPanel(2);
    m_judgePanel->runJudge(filePath);
}

void MainWindow::onOpenJudgeRequested()
{
    m_openJudgeMgr->open(m_settings);
}

// ---- .md ↔ .smd 转换 ----

void MainWindow::onConvertMdSmd()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor) return;

    QString currentPath = editor->currentFilePath();
    if (currentPath.isEmpty()) return;

    QFileInfo fi(currentPath);
    QString suffix = fi.suffix().toLower();

    if (suffix == QStringLiteral("md"))
        convertMdToSmd(editor, fi);
    else if (suffix == QStringLiteral("smd"))
        convertSmdToMd(editor, fi);
}

void MainWindow::convertMdToSmd(EditorWidget *editor, const QFileInfo &fi)
{
    // 1. Capture source state (including unsaved changes)
    QString mdContent = editor->toPlainText();
    int mdCursorLine = editor->cursorLine();
    int mdCursorColumn = editor->cursorColumn();
    bool sourceWasModified = editor->isModified();

    // 2. Run conversion with mapping
    SmdFormat::FromMarkdownResult result = SmdFormat::fromMarkdownWithMapping(mdContent);
    if (result.cells.isEmpty()) return;

    QString smdContent = SmdFormat::serialize(result.cells);

    // 3. Determine target path
    QString smdPath = fi.absolutePath() + QDir::separator() + fi.completeBaseName() + QStringLiteral(".smd");

    // 4. Map cursor from md line to SMD cell + cell line
    int mappedCellIndex = 0;
    int mappedCellLine = 0;
    if (mdCursorLine >= 0 && mdCursorLine < result.mdLineToCell.size()) {
        int ci = result.mdLineToCell[mdCursorLine];
        int cl = result.mdLineToCellLine[mdCursorLine];
        if (ci >= 0) {
            mappedCellIndex = qMin(ci, result.cells.size() - 1);
            mappedCellLine = qMax(0, cl);
        } else {
            for (int l = mdCursorLine - 1; l >= 0; --l) {
                if (result.mdLineToCell[l] >= 0) {
                    mappedCellIndex = qMin(result.mdLineToCell[l], result.cells.size() - 1);
                    break;
                }
            }
        }
    }

    // 5. Open or update target
    EditorWidget *targetEditor = m_tabManager->findEditorByPath(smdPath);
    if (targetEditor) {
        // Already open — update in-memory only, don't touch disk
        m_tabManager->openFile(smdPath);
        targetEditor->setLoading(true);
        targetEditor->setPlainText(smdContent);
        // Read disk content as baseline for modified-state comparison
        QString diskContent;
        QFile diskFile(smdPath);
        if (diskFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&diskFile);
            diskContent = in.readAll();
            diskFile.close();
        }
        targetEditor->setOriginalContent(diskContent);
        targetEditor->setLoading(false);
        SmdEditor *smdEditor = targetEditor->smdEditor();
        if (smdEditor && mappedCellIndex < smdEditor->cellCount()) {
            smdEditor->setActiveCell(mappedCellIndex);
            smdEditor->setActiveCellCursor(mappedCellLine, mdCursorColumn);
            if (SmdCell *cell = smdEditor->cellAt(mappedCellIndex)) {
                if (QWidget *w = cell->editorWidget())
                    w->setFocus();
            }
        }
    } else {
        // Not open — write to disk then open
        QFile outFile(smdPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(nullptr, tr("转换失败"),
                                 tr("无法写入文件: %1").arg(smdPath));
            return;
        }
        QTextStream out(&outFile);
        out << smdContent;
        outFile.close();

        targetEditor = m_tabManager->openFile(smdPath);
        if (targetEditor && targetEditor->isSmdEdit()) {
            SmdEditor *smdEditor = targetEditor->smdEditor();
            if (smdEditor && mappedCellIndex < smdEditor->cellCount()) {
                smdEditor->setActiveCell(mappedCellIndex);
                smdEditor->setActiveCellCursor(mappedCellLine, mdCursorColumn);
                if (SmdCell *cell = smdEditor->cellAt(mappedCellIndex)) {
                    if (QWidget *w = cell->editorWidget())
                        w->setFocus();
                }
            }
        }
    }

    // 6. Restore source modified state
    editor->setModified(sourceWasModified);
}

void MainWindow::convertSmdToMd(EditorWidget *editor, const QFileInfo &fi)
{
    // 1. Capture source state
    SmdEditor *smdEditor = editor->smdEditor();
    if (!smdEditor) return;

    int activeCell = smdEditor->activeCellIndex();
    int cursorLineInCell = smdEditor->activeCellCursorLine();
    int cursorColumn = smdEditor->activeCellCursorColumn();
    bool sourceWasModified = smdEditor->isModified();

    // 2. Get cell content (unsaved edits, no outputs)
    QList<SmdFormat::Cell> cells = smdEditor->exportCells();

    // 3. Convert with offset mapping
    SmdFormat::ToMarkdownResult mdResult = SmdFormat::toMarkdownWithMapping(cells);
    QString mdContent = mdResult.markdown;

    // 4. Map cursor using cellContentStartLine (accounts for code fence openers)
    int mdCursorLine = 0;
    if (activeCell >= 0 && activeCell < mdResult.cellContentStartLine.size())
        mdCursorLine = mdResult.cellContentStartLine[activeCell] + cursorLineInCell;


    // 5. Determine target path
    QString mdPath = fi.absolutePath() + QDir::separator() + fi.completeBaseName() + QStringLiteral(".md");

    // 6. Open or update target
    EditorWidget *targetEditor = m_tabManager->findEditorByPath(mdPath);
    if (targetEditor) {
        // Already open — update in-memory only, don't touch disk
        m_tabManager->openFile(mdPath);
        targetEditor->setLoading(true);
        targetEditor->setPlainText(mdContent);
        // Read disk content as baseline for modified-state comparison
        QString diskContent;
        QFile diskFile(mdPath);
        if (diskFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&diskFile);
            diskContent = in.readAll();
            diskFile.close();
        }
        targetEditor->setOriginalContent(diskContent);
        targetEditor->setLoading(false);
    } else {
        // Not open — write to disk then open
        QFile outFile(mdPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(nullptr, tr("转换失败"),
                                 tr("无法写入文件: %1").arg(mdPath));
            return;
        }
        QTextStream out(&outFile);
        out << mdContent;
        outFile.close();

        targetEditor = m_tabManager->openFile(mdPath);
    }

    // 7. Set cursor in target
    if (targetEditor)
        targetEditor->setCursorPosition(mdCursorLine, cursorColumn);

    // 8. Restore source modified state
    smdEditor->setModified(sourceWasModified);
    editor->setModified(sourceWasModified);
}

// ==================================================================
// Crash recovery
// ==================================================================

void MainWindow::checkCrashRecovery()
{
    m_crashRecovery->cleanStaleRecoveryFiles();
    if (!m_crashRecovery->hasRecoveryFiles())
        return;

    QDir dir(CrashRecoveryManager::recoveryDirectoryPath());
    QStringList entries = dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("恢复文件"));
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText(tr("检测到上次异常退出，发现 %1 个未保存的临时文件。").arg(entries.size()));
    msgBox.setInformativeText(tr("是否恢复未保存的内容？\n\n"
                                 "选择“恢复”将打开临时文件供您手动保存；\n"
                                 "选择“丢弃”将删除所有临时文件。"));
    QPushButton *restoreBtn = msgBox.addButton(tr("恢复(&R)"), QMessageBox::AcceptRole);
    msgBox.addButton(tr("丢弃(&D)"), QMessageBox::DestructiveRole);
    msgBox.setDefaultButton(restoreBtn);
    auto &tm = ThemeManager::instance();
    msgBox.setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   min-width: 80px; padding: 6px 16px;"
        "   background: %1; color: %2;"
        "   border: 1px solid %3; border-radius: 3px;"
        "}"
        "QPushButton:hover { background: %4; }"
        "QPushButton:default {"
        "   background: %5; color: %2;"
        "   border: 1px solid %5;"
        "}"
        "QPushButton:default:hover {"
        "   background: %6;"
        "}"
    ).arg(tm.color("button.background").name(),
          tm.color("button.foreground").name(),
          tm.color("input.border").name(),
          tm.color("button.hoverBackground").name(),
          QColor(tm.color("badge.background").red(),
                 tm.color("badge.background").green(),
                 tm.color("badge.background").blue(), 45).name(QColor::HexArgb),
          QColor(tm.color("badge.background").red(),
                 tm.color("badge.background").green(),
                 tm.color("badge.background").blue(), 80).name(QColor::HexArgb)));
    msgBox.exec();

    if (msgBox.clickedButton() == restoreBtn) {
        for (const QString &entry : entries) {
            QString filePath = dir.absoluteFilePath(entry);
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            QString content = QString::fromUtf8(file.readAll());
            file.close();

            EditorWidget *editor = m_tabManager->newFile();
            editor->setPlainText(content);
            editor->setModified(true);
            editor->setRecoveryTempPath(filePath);
            int idx = m_tabManager->indexOf(editor);
            if (idx >= 0)
                m_tabManager->setTabText(idx, tr("未命名（已恢复）"));
        }
    } else {
        m_crashRecovery->clearRecoveryDirectory();
    }
}

