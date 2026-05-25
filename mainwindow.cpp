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
        : QWidget(parent)
    {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
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
#include "fileexplorerwidget.h"
#include "editorwidget.h"
#include "settingsmanager.h"
#include "configmanager.h"
#include "thememanager.h"
#include "tabmanager.h"
#include "historypanel.h"
#include "backlinkindex.h"
#include "backlinkspanel.h"
#include "searchpanel.h"
#include "fileutils.h"
#include "activitybar.h"
#include "rightpanelcontainer.h"
#include "processrunner.h"
#include "compilererrorparser.h"
#include "outputpanel.h"
#include "bottompanel.h"
#include "codeeditor.h"
#include "debuglog.h"
#include "judgepanel.h"
#include "judgeengine.h"
#include "openjudgewidget.h"
#include "submissionpanel.h"
#include "compilerutils.h"
#include "languageutils.h"
#include "tagindex.h"
#include "tagpanel.h"
#include "outlinepanel.h"
#include "outlineutils.h"
#include "settingspanel.h"
#include "helppanel.h"
#include "ai/aipanel.h"
#include "ai/aicontextmanager.h"
#include "ai/prompttemplates.h"
#include "ai/aiprovider.h"
#include "ai/aiproviderfactory.h"
#include "ai/anthropicprovider.h"
#include "ai/openaiprovider.h"
#include "ai/aihistorylistwidget.h"
#include "ai/errorjournal.h"
#include "smdformat.h"
#include "smdeditor.h"
#include "codeeditor.h"
#include <QPdfView>
#include "scrollbarhider.h"
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

    // 创建反向链接索引与标签索引（独立于面板）
    m_backlinkIndex = new BacklinkIndex;
    m_tagIndex = new TagIndex;

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
    m_dockRightPanel->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_dockRightPanel);
    m_dockRightPanel->hide();

    connect(m_dockRightPanel, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        toggleRightPanelAction->setChecked(visible);
    });

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

    OutputPanel *outputPanel = m_bottomPanel->outputPanel();
    connect(outputPanel, &OutputPanel::stopRequested, this, &MainWindow::onStopProcess);
    connect(m_bottomPanel, &BottomPanel::closeRequested, this, [this]() {
        if (m_processRunner->isRunning())
            onStopProcess();
        m_bottomPanel->hide();
    });
    connect(m_bottomPanel, &BottomPanel::diagnosticsLineClicked, this, [this](int line) {
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor && editor->isCodeEdit())
            editor->navigateEditorToLine(line);
    });

    // ----- 编译运行管理器 -----
    m_processRunner = new ProcessRunner(this);
    connect(outputPanel, &OutputPanel::sendInput, m_processRunner, &ProcessRunner::writeInput);
    connect(outputPanel, &OutputPanel::sendRawInput, m_processRunner, &ProcessRunner::writeRaw);
    connect(m_processRunner, &ProcessRunner::outputReceived, outputPanel, &OutputPanel::appendOutput);
    connect(m_processRunner, &ProcessRunner::compileFinished, this, &MainWindow::onCompileFinished);
    connect(m_processRunner, &ProcessRunner::runFinished, this, &MainWindow::onRunFinished);
    // Buffer stderr for MD code block diagnostics
    m_stderrBufferConnection = connect(m_processRunner, &ProcessRunner::outputReceived,
        this, [this](const QString &text, bool isStderr) {
            if (m_isRunningCodeBlock && isStderr)
                m_mdStderrBuffer += text;
        });
    connect(m_processRunner, &ProcessRunner::processStarted, this, [this]() {
        m_stopAction->setEnabled(true);
        m_compileAction->setEnabled(false);
        m_runAction->setEnabled(false);
        m_compileRunAction->setEnabled(false);
        if (m_runToolAction) m_runToolAction->setEnabled(false);
        OutputPanel *op = m_bottomPanel->outputPanel();
        if (m_processRunner->isAcceptingInput()) {
            QTimer::singleShot(50, this, [this, op]() {
                if (m_processRunner->isRunning())
                    op->setRunning(true);
            });
        } else {
            op->enableTextSelection(false);
        }
    });
    connect(m_processRunner, &ProcessRunner::processStopped, this, [this]() {
        m_stopAction->setEnabled(false);
        OutputPanel *op = m_bottomPanel->outputPanel();
        op->setRunning(false);
        op->enableTextSelection(true);
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor)
            editor->setFocus();
        // Re-enable buttons based on current tab
        bool isCode = editor && editor->isCodeEdit();
        m_compileAction->setEnabled(isCode);
        m_runAction->setEnabled(isCode);
        m_compileRunAction->setEnabled(isCode);
        if (m_runToolAction)
            m_runToolAction->setEnabled(isCode);
        // Clear MD code block state if stopped mid-run
        if (m_isRunningCodeBlock) {
            m_isRunningCodeBlock = false;
            m_mdStderrBuffer.clear();
        }
        m_processManuallyStopped = false;
    });

    // ----- 本地评测面板 -----
    m_judgePanel = new JudgePanel(this);

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
            this, &MainWindow::onOpenJudgeRequested);
    connect(m_judgePanel, &JudgePanel::submitToOpenJudgeRequested,
            this, &MainWindow::onSubmitToOpenJudge);

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
        QPushButton *closeBtn = new QPushButton(QString(QChar(0xD7)));
        closeBtn->setFixedSize(22, 22);
        closeBtn->setFlat(true);
        closeBtn->setCursor(Qt::PointingHandCursor);
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
        QPushButton *closeBtn = new QPushButton(QString(QChar(0xD7)));
        closeBtn->setFixedSize(22, 22);
        closeBtn->setFlat(true);
        closeBtn->setCursor(Qt::PointingHandCursor);
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
    m_fileMenuBtn->setPopupMode(QToolButton::InstantPopup);
    m_fileMenuBtn->setFixedHeight(32);

    {
        m_fileMenu = new QMenu(m_fileMenuBtn);

        QAction *openDirAct = m_fileMenu->addAction(tr("打开目录"));
        connect(openDirAct, &QAction::triggered, this, &MainWindow::onOpenFolder);

        m_fileMenu->addSeparator();

        QAction *newAct = m_fileMenu->addAction(tr("新建文件"));
        newAct->setShortcut(QKeySequence::New);
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

        m_fileMenuBtn->setMenu(m_fileMenu);
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

    // 运行 ▼ (编译 / 运行 / 编译运行)
    m_compileAction = new QAction(tr("编译"), this);
    m_compileAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("compile_only", "F6")));
    addAction(m_compileAction);
    connect(m_compileAction, &QAction::triggered, this, &MainWindow::onCompile);

    m_runAction = new QAction(tr("运行"), this);
    m_runAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("run_only", "F7")));
    addAction(m_runAction);
    connect(m_runAction, &QAction::triggered, this, &MainWindow::onRun);

    m_compileRunAction = new QAction(tr("编译运行"), this);
    m_compileRunAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("compile_and_run", "F5")));
    addAction(m_compileRunAction);
    connect(m_compileRunAction, &QAction::triggered, this, &MainWindow::onCompileAndRun);

    // 工具栏运行按钮（带下拉菜单）
    m_runMenu = new QMenu(this);

    m_runMenu->addAction(m_compileAction);
    m_runMenu->addAction(m_runAction);
    m_runMenu->addSeparator();
    m_runMenu->addAction(m_compileRunAction);
    m_runMenu->setTitle(tr("运行"));

    m_runToolAction = new QAction(QIcon(":/icons/run"), tr("运行"), this);
    m_runToolAction->setMenu(m_runMenu);
    m_runToolAction->setToolTip(tr("运行 (编译/运行/编译运行)"));
    m_runToolAction->setVisible(false); // 只对代码文件显示
    m_toolBar->addAction(m_runToolAction);

    // Title bar uses ThemeManager colors, refresh on theme change
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::refreshTitleBarStyle);
    refreshTitleBarStyle();

    // 终止 (Ctrl+Break) — 仅快捷键，不放在工具栏
    m_stopAction = new QAction(tr("终止"), this);
    m_stopAction->setShortcut(QKeySequence(ConfigManager::instance().shortcut("stop_process", "Ctrl+Break")));
    addAction(m_stopAction);
    m_stopAction->setEnabled(false);
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::onStopProcess);

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

    // 当切换标签页时，更新 AI 动作按钮列表
    connect(m_tabManager, &QTabWidget::currentChanged, this, [this](int) {
        updateAiActionBar();
    });

    // ----- 设置面板（悬浮遮罩 + 面板）-----
    // 使用 OverlayWidget（顶层 Tool 窗口）避免 Qt 裁剪 QWebEngineView
    // 的原生 HWND（子 widget 覆盖原生 child 时 Qt 通过 SetWindowRgn 裁剪，导致 Chromium 黑屏）
    m_settingsOverlay = new OverlayWidget();
    m_settingsOverlay->installEventFilter(this);
    m_settingsOverlay->hide();

    m_settingsPanel = new SettingsPanel(m_settingsOverlay);
    connect(m_settingsPanel, &SettingsPanel::closeRequested, this, &MainWindow::toggleSettings);
    connect(m_settingsPanel, &SettingsPanel::defaultZoomChanged, this, &MainWindow::onDefaultZoomChanged);
    connect(m_settingsPanel, &SettingsPanel::editorSettingChanged, this, &MainWindow::onEditorSettingChanged);
    connect(m_settingsPanel, &SettingsPanel::appearanceSettingChanged, this, &MainWindow::onAppearanceSettingChanged);
    connect(m_settingsPanel, &SettingsPanel::outputPanelSettingChanged, this, &MainWindow::onOutputPanelSettingChanged);
    connect(m_settingsPanel, &SettingsPanel::previewSettingChanged, this, &MainWindow::onPreviewSettingChanged);
    connect(m_settingsPanel, &SettingsPanel::searchSettingChanged, this, &MainWindow::onSearchSettingChanged);
    connect(m_settingsPanel, &SettingsPanel::aiSettingChanged, this, &MainWindow::onAiSettingChanged);
    connect(m_settingsPanel, &SettingsPanel::resetToDefaultsRequested, this, &MainWindow::onResetToDefaults);
    connect(m_settingsPanel, &SettingsPanel::shortcutChanged, this, &MainWindow::onShortcutChanged);

    // 应用保存的文件树条目高度
    int treeItemHeight = m_settings->value("editor.file_tree_item_height",
                                           ConfigManager::instance().editorFileTreeItemHeight()).toInt();
    m_explorer->setItemHeight(treeItemHeight);

    // ----- 帮助面板（悬浮遮罩 + 面板）-----
    m_helpOverlay = new OverlayWidget();
    m_helpOverlay->installEventFilter(this);
    m_helpOverlay->hide();

    m_helpPanel = new HelpPanel(m_helpOverlay);
    connect(m_helpPanel, &HelpPanel::closeRequested, this, &MainWindow::toggleHelp);

    // ----- AI 助手面板 -----
    m_aiPanel = new AiPanel(this);
    m_dockAi = new QDockWidget(tr("AI 助手"), this);
    m_dockAi->setWidget(m_aiPanel);
    m_dockAi->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
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
    m_shortcutActions["compile_only"] = m_compileAction;
    m_shortcutActions["run_only"] = m_runAction;
    m_shortcutActions["compile_and_run"] = m_compileRunAction;
    m_shortcutActions["stop_process"] = m_stopAction;
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
        startAiRequest(AiAction::FreeChat, text);
    });
    connect(m_aiPanel, &AiPanel::actionTriggered, this, [this](int actionIndex) {
        startAiRequest(static_cast<AiAction>(actionIndex));
    });
    connect(m_aiPanel, &AiPanel::clearRequested, this, [this]() {
        abortAiRequest();
        m_aiHistory.clear();
        AiHistoryManager::instance().clearCurrentConversation();
    });
    connect(m_aiPanel, &AiPanel::newConversationRequested, this, [this]() {
        abortAiRequest();
        m_aiHistory.clear();
        AiHistoryManager::instance().createConversation(tr("新对话"), {});
    });

    // ── History list widget connections ──
    {
        auto *historyWidget = m_aiPanel->historyListWidget();
        connect(historyWidget, &AiHistoryListWidget::conversationSelected,
                this, &MainWindow::loadAiConversation);
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
                // If deleting the current conversation, clear state
                if (convId == mgr.currentConversationId()) {
                    m_aiPanel->clearChat();
                    m_aiHistory.clear();
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
        // Update active dot
        m_aiPanel->historyListWidget()->setActiveConversationId(
            AiHistoryManager::instance().currentConversationId());
    });

    // ----- 界面布局 -----
    // 左侧活动栏（搜索/设置/导出PDF/评测）
    m_activityBar = new ActivityBar(this);

    // 右侧垂直分割线：编辑器在上，输出面板在下
    m_rightSplitter = new QSplitter(Qt::Vertical, this);
    m_rightSplitter->addWidget(m_tabManager);
    m_rightSplitter->addWidget(m_bottomPanel);
    m_rightSplitter->setStretchFactor(0, ConfigManager::instance().rightSplitterEditorStretch());
    m_rightSplitter->setStretchFactor(1, ConfigManager::instance().rightSplitterOutputStretch());

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
                    this, &MainWindow::toggleDiagnosticsInCodeEditor,
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
            });
        }

        refreshBacklinks();
        refreshTags();
        refreshOutline();
        filterAiHistoryByCurrentFile();
        updateCurrentEditorCompletions();

        // 更新编译运行按钮状态
        bool isCode = editor && editor->isCodeEdit();
        bool running = m_processRunner->isRunning();
        m_compileAction->setEnabled(isCode && !running);
        m_runAction->setEnabled(isCode && !running);
        m_compileRunAction->setEnabled(isCode && !running);
        if (m_runToolAction) {
            m_runToolAction->setVisible(isCode);
            m_runToolAction->setEnabled(isCode && !running);
        }

        // 导出PDF 仅对 .md 文件启用（按钮可见 + 快捷键生效）
        bool isMd = editor && editor->currentFilePath().toLower().endsWith(".md");
        m_exportPdfAction->setEnabled(isMd);
        m_exportPdfAction->setVisible(isMd);
        m_activityBar->setExportPdfVisible(isMd);

        // 代码编辑器诊断连接
        if (editor && editor->isCodeEdit()) {
            CodeEditor *ce = qobject_cast<CodeEditor*>(
                editor->findChild<CodeEditor*>());
            bool hasProvider = ce && ce->completionProvider();
            debugLog(QString("MainWindow: CodeEdit tab, CodeEditor=%1, hasProvider=%2")
                .arg((quintptr)ce).arg(hasProvider));
            if (ce && hasProvider) {
                // Switch provider connection to the new editor.
                m_bottomPanel->setCurrentEditor(ce);
                disconnect(m_diagnosticsProviderConnection);
                m_diagnosticsProviderConnection = connect(
                    ce->completionProvider(),
                    &CompletionProvider::diagnosticsUpdated,
                    m_bottomPanel, &BottomPanel::setDiagnostics);
                // Apply cached diagnostics immediately (the provider won't
                // re-emit unless the file changes).
                m_bottomPanel->setDiagnostics(ce->diagnostics());
            }
        } else if (editor && editor->currentFilePath().toLower().endsWith(QStringLiteral(".md"))) {
            // MD file: load cached code block diagnostics
            loadMdDiagnosticsForCurrentTab();
        } else {
            m_bottomPanel->hide();
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
    qApp->installEventFilter(this);

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
    connect(m_tabManager, &QTabWidget::currentChanged, this, [this, hider](int) {
        if (auto *editor = m_tabManager->currentEditor()) {
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

}

MainWindow::~MainWindow()
{
    if (m_scanCancelled)
        m_scanCancelled->store(true);
    if (m_fileIdxCancelled)
        m_fileIdxCancelled->store(true);
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
    // 更新运行按钮显隐
    if (auto *ed = m_tabManager->currentEditor()) {
        bool isCode = ed->isCodeEdit();
        if (m_runToolAction) {
            m_runToolAction->setVisible(isCode);
            m_runToolAction->setEnabled(isCode);
        }
    }
    // openPreview may reuse the existing preview tab without changing
    // the current index, so currentChanged is NOT emitted. Explicitly
    // refresh side panels whose content depends on the active file.
    refreshBacklinks();
    refreshTags();
    refreshOutline();
    filterAiHistoryByCurrentFile();
    updateCurrentEditorCompletions();
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
        // 异步重建文件索引，完成后更新 backlink/tag
        buildFileIndexAsync([this, newFilePath]() {
            m_backlinkIndex->rebuildFile(newFilePath, m_explorer->rootPath(), m_fileIndex);
            m_tagIndex->rebuildFile(newFilePath);
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
    clearRecoveryDirectory(); // 正常关闭，清理恢复目录
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
    if (!m_settingsOverlay || !m_settingsPanel)
        return;

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
    if (!m_helpOverlay || !m_helpPanel)
        return;

    if (m_helpOverlay->isVisible()) {
        m_helpOverlay->hide();
    } else {
        positionOverlay(m_helpOverlay, m_helpPanel, this);
        m_helpOverlay->show();
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
                editor->setCodeIndentWidth(width);
            }
        }
    } else if (key == "editor.markdown_indent_width") {
        int width = value.toInt();
        for (int i = 0; i < m_tabManager->count(); ++i) {
            if (auto *editor = qobject_cast<EditorWidget*>(m_tabManager->widget(i))) {
                editor->setMarkdownIndentWidth(width);
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

    if (key == "editor.file_tree_item_height") {
        m_explorer->setItemHeight(value.toInt());
    }

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
        QFont font = m_bottomPanel->outputPanel()->font();
        font.setPointSize(value.toInt());
        m_bottomPanel->outputPanel()->setOutputFont(font);
    } else if (key == "output_panel.max_blocks") {
        m_bottomPanel->outputPanel()->setMaxBlocks(value.toInt());
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

void MainWindow::onAiSettingChanged(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("ai.api_key")) {
        m_settings->setAiApiKey(value.toString());
    } else {
        m_settings->setSettingOverride(key, value);
    }
}

void MainWindow::onShortcutChanged(const QString &actionKey, const QString &keySequenceText)
{
    // Persist via SettingsManager override
    m_settings->setSettingOverride("shortcuts." + actionKey, keySequenceText);

    // Apply to the live QAction
    if (auto *action = m_shortcutActions.value(actionKey)) {
        action->setShortcut(QKeySequence(keySequenceText));
    }

    // Notify editor widgets to reload their configurable shortcuts
    if (auto *editor = m_tabManager->currentEditor()) {
        if (editor->isCodeEdit()) {
            if (auto *ce = editor->codeEditor())
                ce->reloadShortcuts();
        } else if (editor->isSmdEdit()) {
            if (auto *se = editor->smdEditor())
                se->reloadShortcuts();
        }
    }
    if (m_bottomPanel)
        m_bottomPanel->outputPanel()->reloadShortcuts();
    if (m_explorer)
        m_explorer->reloadShortcuts();
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
    // 打开非文件浏览面板时隐藏右侧面板
    if (index > 0)
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

// ═══════════════════════════════════════════════════════════════════════
// Context window management for multi-turn conversation history.
// m_aiHistory stores the FULL, unpruned conversation.
// Each API call gets a token-aware windowed COPY.
// ═══════════════════════════════════════════════════════════════════════

// Rough token estimation: CJK ≈1 token/char, ASCII ≈1 token/4 chars, +overhead
static int estimateTokens(const QString &text)
{
    if (text.isEmpty())
        return 0;
    int cjk = 0, ascii = 0;
    for (const QChar &c : text) {
        if (c.unicode() >= 0x2E80)
            ++cjk;
        else if (c.unicode() < 0x80)
            ++ascii;
    }
    return cjk + ascii / 4 + 2;
}

// Model → max context window token limit (conservative values from official docs)
static int modelContextLimit(const QString &model)
{
    struct Entry { const char *prefix; int limit; };
    static const Entry entries[] = {
        {"claude-3-opus-20240229", 200000},
        {"claude-3-opus-4-7",      200000},
        {"claude-sonnet-4-6",      200000},
        {"claude-haiku-4-5",       200000},
        {"claude-3-5-sonnet",      200000},
        {"claude-3-5-haiku",       200000},
        {"claude-3-sonnet",        200000},
        {"claude-3-haiku",         200000},
        {"claude-2",               100000},
        {"claude",                 200000},
        {"gpt-4-turbo",            128000},
        {"gpt-4o-mini",            128000},
        {"gpt-4o",                 128000},
        {"gpt-4",                   8192},
        {"gpt-3.5-turbo",           16385},
        {"deepseek-chat",           65536},
        {"deepseek-reasoner",       65536},
        {"gemini",                 1048576},
        {nullptr, 0}
    };
    for (const Entry *e = entries; e->prefix; ++e) {
        if (model.contains(QLatin1String(e->prefix)))
            return e->limit;
    }
    return 128000; // generous fallback for unknown / future models
}

// Build a token-aware window: keep the newest messages that fit within budget.
// Never prunes the original history — works on a copy.
static QList<Message> pruneContextWindow(const QList<Message> &history,
                                         const QString &model,
                                         int maxResponseTokens,
                                         const QString &systemPrompt)
{
    const int contextLimit = modelContextLimit(model);
    const int systemTokens = estimateTokens(systemPrompt);

    // Budget = total context - response allocation - system prompt - safety margin
    int available = contextLimit - maxResponseTokens - systemTokens;
    available = qMax(available * 9 / 10, 2048); // 10% safety, min 2048

    QList<Message> window;
    int totalTokens = 0;
    // Walk backwards (newest first) to keep the most recent context
    for (int i = history.size() - 1; i >= 0; --i) {
        const int msgTokens = estimateTokens(history[i].content) + 8; // +JSON overhead
        if (totalTokens + msgTokens > available && !window.isEmpty())
            break;
        window.prepend(history[i]);
        totalTokens += msgTokens;
    }
    return window;
}

// ═══════════════════════════════════════════════════════════════════════

void MainWindow::startAiRequest(AiAction action, const QString &freeQuery)
{
    // 1. Abort any ongoing request
    if (m_aiStreaming) {
        abortAiRequest();
    }

    m_aiStreaming = true;

    // 2. Collect context from current editor
    EditorWidget *editor = m_tabManager->currentEditor();
    ContextBundle ctx;
    if (editor)
        ctx = AiContextManager::collectContext(editor);

    // 4. Build prompt
    PromptBundle prompt = buildPrompt(action, ctx, freeQuery);

    // 5. Read AI settings
    const QString apiKey = m_settings->aiApiKey();
    if (apiKey.isEmpty()) {
        m_aiPanel->addAssistantMessage(tr("请先在设置 → AI 服务中配置 API Key"));
        m_aiStreaming = false;
        return;
    }

    const QString providerType = m_settings->value("ai.provider_type",
        ConfigManager::instance().aiProviderType()).toString();
    const QString model = m_settings->value("ai.model",
        ConfigManager::instance().aiModel()).toString();
    const QString endpoint = m_settings->value("ai.endpoint",
        ConfigManager::instance().aiEndpoint()).toString();
    const int maxTokens = m_settings->value("ai.max_tokens",
        ConfigManager::instance().aiMaxTokens()).toInt();

    // 6. Create provider (always recreate to pick up settings changes)
    if (m_aiProvider) {
        m_aiProvider->disconnect();
        m_aiProvider->deleteLater();
        m_aiProvider = nullptr;
    }

    AiProviderFactory::ProviderType type = AiProviderFactory::typeFromString(providerType);
    m_aiProvider = AiProviderFactory::createProvider(type, this);

    // 7. Configure provider
    m_aiProvider->setApiKey(apiKey);
    m_aiProvider->setModel(model);
    m_aiProvider->setMaxTokens(maxTokens);
    m_aiProvider->setSystemPrompt(prompt.systemPrompt);

    if (auto *anthropic = qobject_cast<AnthropicProvider*>(m_aiProvider)) {
        anthropic->setEndpoint(endpoint);
    } else if (auto *openai = qobject_cast<OpenAiProvider*>(m_aiProvider)) {
        openai->setEndpoint(endpoint);
    }

    // 8. Connect provider signals
    connect(m_aiProvider, &AiProvider::partialResponse,
            this, &MainWindow::onAiPartialResponse);
    connect(m_aiProvider, &AiProvider::finished,
            this, &MainWindow::onAiFinished);
    connect(m_aiProvider, &AiProvider::error,
            this, &MainWindow::onAiError);

    // 9. Display user message in chat
    QString userDisplayText;
    if (action == AiAction::FreeChat) {
        userDisplayText = freeQuery;
        m_aiPanel->addUserMessage(freeQuery);
    } else {
        const ActionInfo *info = findActionInfo(action);
        userDisplayText = info ? tr(info->label) : tr("AI 操作");
        if (!ctx.selectedText.isEmpty()) {
            userDisplayText += QStringLiteral("\n\n```\n") + ctx.selectedText + QStringLiteral("\n```");
        }
        m_aiPanel->addUserMessage(userDisplayText);
    }

    // 9b. Persist user message to AiHistoryManager
    {
        auto &mgr = AiHistoryManager::instance();
        // Create conversation on first message if none exists
        if (mgr.currentConversationId().isEmpty()) {
            QString convTitle;
            if (action == AiAction::FreeChat) {
                convTitle = freeQuery.left(30).trimmed();
                if (convTitle.isEmpty()) convTitle = tr("新对话");
            } else {
                const ActionInfo *info = findActionInfo(action);
                convTitle = info ? tr(info->label) : tr("AI 操作");
            }
            QString filePath = (action != AiAction::FreeChat) ? ctx.filePath : QString();
            mgr.createConversation(convTitle, filePath);
        }

        AiMessage histMsg;
        histMsg.role = MessageRole::User;
        histMsg.content = (action == AiAction::FreeChat) ? freeQuery : prompt.userPrompt;
        histMsg.timestampMs = QDateTime::currentMSecsSinceEpoch();
        mgr.appendMessage(mgr.currentConversationId(), histMsg);
    }

    // 10. Add empty assistant bubble as streaming target
    m_aiPanel->addAssistantMessage(QString());

    // 11. Append current user message to canonical full history (unpruned)
    Message userMsg;
    userMsg.role = MessageRole::User;
    userMsg.content = (action == AiAction::FreeChat) ? freeQuery : prompt.userPrompt;
    m_aiHistory.append(userMsg);

    // 12. Build token-aware context window for the API call (copy, never modify m_aiHistory)
    QList<Message> messages = pruneContextWindow(m_aiHistory, model, maxTokens, prompt.systemPrompt);

    // 13. Disable input and start streaming
    m_aiPanel->setInputEnabled(false);
    m_aiProvider->chatStream(messages);
}

void MainWindow::abortAiRequest()
{
    if (m_aiProvider) {
        m_aiProvider->cancel();
        m_aiProvider->disconnect();
    }
    m_aiStreaming = false;
    m_aiPanel->setInputEnabled(true);
}

void MainWindow::onAiPartialResponse(const QString &text)
{
    if (text.isEmpty())
        return;
    m_aiPanel->appendToLastAssistant(text);
}

void MainWindow::onAiFinished()
{
    // Flush any pending debounced content update so the final
    // assistant response text is fully rendered before persisting.
    m_aiPanel->flushPendingUpdates();

    // Persist assistant response to AiHistoryManager
    {
        auto &mgr = AiHistoryManager::instance();
        if (!mgr.currentConversationId().isEmpty()) {
            QString content = m_aiPanel->lastAssistantContent();
            if (!content.isEmpty()) {
                AiMessage assistantMsg;
                assistantMsg.role = MessageRole::Assistant;
                assistantMsg.content = content;
                assistantMsg.timestampMs = QDateTime::currentMSecsSinceEpoch();
                mgr.appendMessage(mgr.currentConversationId(), assistantMsg);
            }
        }
    }

    // Save assistant response to in-memory history for subsequent turns
    if (!m_aiHistory.isEmpty()) {
        QString content = m_aiPanel->lastAssistantContent();
        if (!content.isEmpty()) {
            Message assistantMsg;
            assistantMsg.role = MessageRole::Assistant;
            assistantMsg.content = content;
            m_aiHistory.append(assistantMsg);
        }
    }

    m_aiStreaming = false;
    m_aiPanel->setInputEnabled(true);
}

void MainWindow::onAiError(const QString &message)
{
    // Append error to existing streaming bubble, or create a new one
    if (m_aiPanel->hasStreamingTarget()) {
        m_aiPanel->appendToLastAssistant(
            QStringLiteral("**") + tr("错误") + QStringLiteral("：**") + message);
    } else {
        m_aiPanel->addAssistantMessage(tr("错误：") + message);
    }

    m_aiStreaming = false;
    m_aiPanel->setInputEnabled(true);
}

// ── History list integration ─────────────────────────────────────

void MainWindow::loadAiConversation(const QString &convId)
{
    // Abort any ongoing request
    if (m_aiStreaming)
        abortAiRequest();

    // Clear current state
    m_aiPanel->clearChat();
    m_aiHistory.clear();

    auto &mgr = AiHistoryManager::instance();

    // Set as current conversation
    mgr.setCurrentConversation(convId);

    // Load and display messages
    const QList<AiMessage> messages = mgr.loadMessages(convId);
    for (const auto &aiMsg : messages) {
        if (aiMsg.role == MessageRole::Assistant)
            m_aiPanel->addAssistantMessage(aiMsg.content);
        else
            m_aiPanel->addUserMessage(aiMsg.content);

        // Rebuild m_aiHistory for API context
        Message msg;
        msg.role = aiMsg.role;
        msg.content = aiMsg.content;
        m_aiHistory.append(msg);
    }

    // Update active dot in history list
    filterAiHistoryByCurrentFile();

    // Switch back to chat tab
    m_aiPanel->setCurrentTab(AiPanel::ChatTab);
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

    // Reset all shortcuts to defaults
    for (auto it = m_shortcutActions.constBegin(); it != m_shortcutActions.constEnd(); ++it) {
        QString defaultVal = cfg.shortcut(it.key(), "");
        it.value()->setShortcut(QKeySequence(defaultVal));
    }

    // Reload configurable shortcuts on all editor and explorer widgets
    for (int i = 0; i < m_tabManager->count(); ++i) {
        if (auto *editor = qobject_cast<EditorWidget*>(m_tabManager->widget(i))) {
            if (auto *ce = editor->codeEditor())
                ce->reloadShortcuts();
            if (auto *se = editor->smdEditor())
                se->reloadShortcuts();
        }
    }
    if (m_explorer)
        m_explorer->reloadShortcuts();

    // Reset output panel
    OutputPanel *op = m_bottomPanel->outputPanel();
    QFont opFont = op->font();
    opFont.setPointSize(cfg.outputPanelFontSize());
    op->setOutputFont(opFont);
    op->setMaxBlocks(cfg.outputPanelMaxBlocks());
    op->reloadShortcuts();

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
    QStringList sources = m_backlinkIndex->backlinksFor(filePath);
    m_rightPanel->backlinksPanel()->showBacklinks(sources);
}

void MainWindow::refreshTags()
{
    QStringList tags = m_tagIndex->allTags();
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
    QStringList files = m_tagIndex->filesForTag(tag);
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

void MainWindow::buildFileIndexAsync(std::function<void()> onComplete)
{
    // Cancel any in-flight file-index-only scan (but NOT the full index build)
    if (m_fileIdxCancelled)
        m_fileIdxCancelled->store(true);
    m_fileIdxCancelled = std::make_shared<std::atomic<bool>>(false);
    uint64_t scanId = ++m_fileIdxScanId;

    QString root = m_explorer->rootPath();
    if (root.isEmpty() || QDir(root).isRoot() || root == QDir::homePath()) {
        m_fileIndex.clear();
        updateCurrentEditorCompletions();
        if (onComplete)
            onComplete();
        return;
    }

    auto cancelled = m_fileIdxCancelled;
    QThread::create([this, cancelled, scanId, root,
                     onComplete = std::move(onComplete)]() {
        // Build file index on background thread
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

        // Deliver result to main thread
        QMetaObject::invokeMethod(this, [this, scanId,
                                         fileIndex = std::move(fileIndex),
                                         onComplete = std::move(onComplete)]() mutable {
            if (scanId != m_fileIdxScanId.load()) return; // Stale
            m_fileIndex = std::move(fileIndex);
            updateCurrentEditorCompletions();
            if (onComplete)
                onComplete();
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

    // 这些不依赖 m_fileIndex，立即执行
    m_backlinkIndex->onFileRenamed(oldPath, newPath);
    m_tagIndex->onFileRenamed(oldPath, newPath);

    // 异步重建文件索引，完成后更新 wiki 链接文本
    buildFileIndexAsync([this, affectedSources = std::move(affectedSources),
                         oldBaseName, newBaseName]() {
        updateWikiLinksAfterRename(affectedSources, oldBaseName, newBaseName);
        refreshBacklinks();
    });
}

void MainWindow::onFileDeletedInIndex(const QString &path)
{
    buildFileIndexAsync(); // 异步重建，后续操作不依赖 m_fileIndex
    m_backlinkIndex->onFileDeleted(path);
    m_tagIndex->onFileDeleted(path);
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
    int updateId = ++m_wikiLinkUpdateId;

    QThread::create([this, sources = std::move(sources), oldLinkText, newLinkText,
                     rootPath, updateId]() {
        if (updateId != m_wikiLinkUpdateId.load()) return;

        QStringList writtenPaths;
        writtenPaths.reserve(sources.size());
        for (const auto &src : sources) {
            if (updateId != m_wikiLinkUpdateId.load()) return;

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

        if (updateId != m_wikiLinkUpdateId.load()) return;

        QMetaObject::invokeMethod(this, [this, updateId,
                                         writtenPaths = std::move(writtenPaths),
                                         rootPath]() {
            if (updateId != m_wikiLinkUpdateId.load()) return;

            // Capture the current editor BEFORE any reloads.
            EditorWidget *currentBefore = m_tabManager->currentEditor();

            for (const QString &path : writtenPaths) {
                // rebuildFile FIRST so that fileLoaded→refreshBacklinks sees fresh data
                m_backlinkIndex->rebuildFile(path, rootPath, m_fileIndex);
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
        "QToolBar { background: %1; border: none; spacing: 0; padding: 0; }"
        "QToolBar QToolButton {"
        "  background: transparent; border: none; padding: 4px 8px;"
        "}"
        "QToolBar QToolButton:hover {"
        "  background: %2;"
        "}"
    ).arg(tm.color("titleBar.background").name(),
          tm.color("titleBar.buttonHover").name()));

    // File menu button
    m_fileMenuBtn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  color: %1; background: transparent; border: none; padding: 0 8px;"
        "  font-size: 12px;"
        "}"
        "QToolButton:hover { background: %2; }"
        "QToolButton::menu-indicator {"
        "  subcontrol-position: right center;"
        "}"
    ).arg(tm.color("titleBar.foreground").name(),
          tm.color("titleBar.buttonHover").name()));

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

    // Run menu dropdown
    m_runMenu->setStyleSheet(QStringLiteral(
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

    // Themed toolbar action icons — match activity bar icons
    QColor titleFg = tm.color("activityBar.foreground");
    m_helpAction->setIcon(coloredSvgIcon(":/icons/help", titleFg));
    toggleRightPanelAction->setIcon(coloredSvgIcon(":/icons/panel", titleFg));
    m_previewAction->setIcon(coloredSvgIcon(":/icons/preview", titleFg));
    m_splitPreviewAction->setIcon(coloredSvgIcon(":/icons/split", titleFg));
    if (m_runToolAction)
        m_runToolAction->setIcon(coloredSvgIcon(":/icons/run", titleFg));
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
                    // 最大化拖拽：还原 → 处理事件 → 定位 → 系统拖拽
                    QPoint gpos = me->globalPosition().toPoint();
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

        // Auto-hide right panel when clicking in the editor area
        if (m_dockRightPanel->isVisible()) {
            if (btn && btn->defaultAction() == toggleRightPanelAction)
                return QMainWindow::eventFilter(watched, event);
            if (!m_dockRightPanel->isAncestorOf(clickedWidget)
                && m_tabManager && m_tabManager->isAncestorOf(clickedWidget))
                m_dockRightPanel->hide();
        }

        // Close settings/help overlays when clicking overlay background
        // (Overlays are now top-level Tool windows; clicks on the dimmed area
        //  outside the panel widget close the overlay.)
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
    m_rightPanel->historyPanel()->replacePath(oldPath, newPath); // 更新历史记录中的路径
}

// ============================================================
// 编译运行相关槽函数
// ============================================================

void MainWindow::onCompile()
{
    // IDE mode: use OpenJudge embedded editor
    OpenJudgeWidget *oj = m_tabManager->findOpenJudgeWidget();
    if (oj && oj->isIdeMode()) {
        oj->saveIdeCodeToCache();
        QString filePath = oj->ideCacheFilePath();
        if (filePath.isEmpty())
            return;
        QString ext = QFileInfo(filePath).suffix().toLower();
        if (ext == QStringLiteral("py") || ext == QStringLiteral("pyw")) {
            showOutputPanel();
            m_bottomPanel->outputPanel()->clearOutput();
            m_bottomPanel->outputPanel()->appendOutput(tr("Python 不需要编译，请使用 运行 (F7) 或 编译运行 (F5)。\n"), false);
            m_bottomPanel->outputPanel()->setStatus(tr("提示"), false);
            return;
        }
        showOutputPanel();
        m_bottomPanel->outputPanel()->clearOutput();
        m_bottomPanel->outputPanel()->setStatus(tr("编译中..."));
        m_processRunner->startCompile(filePath);
        return;
    }

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

        m_bottomPanel->outputPanel()->clearOutput();
        m_bottomPanel->outputPanel()->appendOutput(tr("Python 不需要编译，请使用 运行 (F7) 或 编译运行 (F5)。\n"), false);
        m_bottomPanel->outputPanel()->setStatus(tr("提示"), false);
        return;
    }

    showOutputPanel();

    m_bottomPanel->outputPanel()->clearOutput();
    m_bottomPanel->outputPanel()->setStatus(tr("编译中..."));
    m_processRunner->startCompile(filePath);
}

void MainWindow::onRun()
{
    // IDE mode: use OpenJudge embedded editor
    OpenJudgeWidget *oj = m_tabManager->findOpenJudgeWidget();
    if (oj && oj->isIdeMode()) {
        oj->saveIdeCodeToCache();
        QString filePath = oj->ideCacheFilePath();
        if (filePath.isEmpty())
            return;
        QString ext = QFileInfo(filePath).suffix().toLower();
        if (ext == QStringLiteral("py") || ext == QStringLiteral("pyw")) {
            showOutputPanel();
            m_bottomPanel->outputPanel()->clearOutput();
            m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
            m_processRunner->startRunPython(filePath);
            return;
        }
        if (m_processRunner->lastExecutable().isEmpty()) {
            onCompileAndRun();
            return;
        }
        showOutputPanel();
        m_bottomPanel->outputPanel()->clearOutput();
        m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
        m_processRunner->startRun(m_processRunner->lastExecutable());
        return;
    }

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

            m_bottomPanel->outputPanel()->clearOutput();
            m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
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

    m_bottomPanel->outputPanel()->clearOutput();
    m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
    m_processRunner->startRun(m_processRunner->lastExecutable());
}

void MainWindow::onCompileAndRun()
{
    // IDE mode: use OpenJudge embedded editor
    OpenJudgeWidget *oj = m_tabManager->findOpenJudgeWidget();
    if (oj && oj->isIdeMode()) {
        oj->saveIdeCodeToCache();
        QString filePath = oj->ideCacheFilePath();
        if (filePath.isEmpty())
            return;
        QString ext = QFileInfo(filePath).suffix().toLower();
        if (ext == QStringLiteral("py") || ext == QStringLiteral("pyw")) {
            showOutputPanel();
            m_bottomPanel->outputPanel()->clearOutput();
            m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
            m_processRunner->startRunPython(filePath);
            return;
        }
        showOutputPanel();
        m_bottomPanel->outputPanel()->clearOutput();
        m_bottomPanel->outputPanel()->setStatus(tr("编译中..."));
        m_processRunner->startCompileAndRun(filePath);
        return;
    }

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

        m_bottomPanel->outputPanel()->clearOutput();
        m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
        m_processRunner->startRunPython(filePath);
        return;
    }

    showOutputPanel();

    m_bottomPanel->outputPanel()->clearOutput();
    m_bottomPanel->outputPanel()->setStatus(tr("编译中..."));
    m_processRunner->startCompileAndRun(filePath);
}

void MainWindow::onStopProcess()
{
    m_processManuallyStopped = true;
    m_processRunner->stop();
    m_bottomPanel->outputPanel()->appendOutput(QStringLiteral("\n--- ") + tr("已终止") + QStringLiteral(" ---\n"), false);
    m_bottomPanel->outputPanel()->setStatus(tr("已终止"), true);
}

void MainWindow::onCompileFinished(bool success)
{
    if (success) {
        m_bottomPanel->outputPanel()->setStatus(tr("编译成功"));
    } else {
        m_bottomPanel->outputPanel()->setStatus(tr("编译失败"), true);
    }

    // For MD code blocks: parse compile errors on failure
    if (m_isRunningCodeBlock && !success) {
        parseAndShowBlockDiagnostics();
    }
}

void MainWindow::onRunFinished(int exitCode)
{
    m_bottomPanel->outputPanel()->appendOutput(
        QStringLiteral("\n--- ") + tr("进程退出 (代码: %1)").arg(exitCode) + QStringLiteral(" ---\n"), false);
    m_bottomPanel->outputPanel()->setStatus(
        tr("完成 (代码: %1)").arg(exitCode), exitCode != 0);

    // For MD code blocks: parse runtime errors (Python traceback / C++ runtime stderr)
    if (m_isRunningCodeBlock) {
        parseAndShowBlockDiagnostics();
    }
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
            filePath = saveCodeToTempFile(editor);
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
    // Check if OpenJudge tab already exists
    OpenJudgeWidget *existing = m_tabManager->findOpenJudgeWidget();
    bool isNew = (existing == nullptr);

    // Open or switch to OpenJudge tab
    m_tabManager->openOpenJudge(m_settings);

    OpenJudgeWidget *oj = m_tabManager->findOpenJudgeWidget();
    if (!oj) return;

    if (isNew) {
        // Connect signals once
        connect(oj, &OpenJudgeWidget::sampleSelected,
                this, &MainWindow::onOpenJudgeSampleSelected);
        connect(oj, &OpenJudgeWidget::loginStateChanged,
                this, &MainWindow::onOpenJudgeLoginStateChanged);
        connect(oj, &OpenJudgeWidget::submissionResultReady,
                this, &MainWindow::onSubmissionResultReady);
        connect(oj, &OpenJudgeWidget::submissionFailed,
                this, [this](const QString &error) {
            QMessageBox::warning(this, tr("提交失败"), error);
        });
        connect(oj, &OpenJudgeWidget::ideDiagnosticsToggleRequested,
                this, &MainWindow::toggleDiagnosticsInCodeEditor);
    }

    // Show login dialog if not logged in (after tab is visible)
    if (isNew || !oj->isLoggedIn()) {
        QTimer::singleShot(200, this, [this]() {
            OpenJudgeWidget *w = m_tabManager->findOpenJudgeWidget();
            if (w && !w->isLoggedIn())
                w->onReLogin();
        });
    }
}

void MainWindow::onOpenJudgeSampleSelected(const QString &folderPath)
{
    m_judgePanel->setTestFolder(folderPath);
    showLeftPanel(2);
}

void MainWindow::onSubmitToOpenJudge()
{
    // Check if OpenJudge tab exists and has a problem selected
    OpenJudgeWidget *oj = m_tabManager->findOpenJudgeWidget();
    if (!oj || !oj->hasCurrentProblem()) {
        bool autoLoginInitiated = oj && oj->tryAutoLogin();
        if (!autoLoginInitiated) {
            onOpenJudgeRequested();
        }
        QMessageBox::information(this, tr("提示"),
            tr("请先在 OpenJudge 中选择一道题目"));
        return;
    }

    // Check login status
    if (!oj->isLoggedIn()) {
        bool autoLoginInitiated = oj->tryAutoLogin();
        if (!autoLoginInitiated) {
            onOpenJudgeRequested();
        }
        QMessageBox::information(this, tr("提示"),
            autoLoginInitiated ? tr("正在自动登录 OpenJudge，请稍后重试")
                               : tr("请先登录 OpenJudge"));
        return;
    }

    QString code;
    QString filePath;
    int langId;

    // IDE mode: submit from embedded editor
    if (oj->isIdeMode()) {
        code = oj->ideCode();
        if (code.trimmed().isEmpty()) {
            QMessageBox::information(this, tr("提示"),
                tr("代码内容为空"));
            return;
        }
        oj->saveIdeCodeToCache();
        filePath = oj->ideCacheFilePath();
        langId = oj->currentLanguageId();
    } else {
        // Normal mode: submit from current editor tab
        EditorWidget *editor = m_tabManager->currentEditor();
        if (!editor || !editor->isCodeEdit()) {
            QMessageBox::information(this, tr("提示"),
                tr("请打开一个代码文件进行提交"));
            return;
        }

        code = editor->toPlainText();
        if (code.trimmed().isEmpty()) {
            QMessageBox::information(this, tr("提示"),
                tr("代码内容为空"));
            return;
        }

        filePath = editor->currentFilePath();
        if (filePath.isEmpty() || editor->isModified()) {
            filePath = saveCodeToTempFile(editor);
            if (filePath.isEmpty()) {
                QMessageBox::warning(this, tr("错误"),
                    tr("无法保存代码文件"));
                return;
            }
        }

        QString ext = QFileInfo(filePath).suffix().toLower();
        QMap<QString, int> langMap = ConfigManager::instance().openJudgeSubmissionLanguageMap();
        langId = langMap.value("." + ext, 1); // default: G++
    }

    // Save submission context for error journal (in case of failure)
    m_lastSubmitSourceFile = filePath;
    m_lastSubmitSourceCode = code;

    // Submit through OpenJudgeWidget
    oj->submitCurrentProblem(code, langId);

    // Show a brief status message in the judge panel
    showLeftPanel(2);
}

void MainWindow::onSubmissionResultReady(const SubmissionResult &result)
{
    // Hide the output panel if it's visible
    m_bottomPanel->hide();

    // Create the submit result panel on first use
    if (!m_submitResultPanel) {
        m_submitResultPanel = new SubmitResultPanel(this);
        // Insert it into the right splitter, positioned where output panel is
        int outputIdx = m_rightSplitter->indexOf(m_bottomPanel);
        m_rightSplitter->insertWidget(outputIdx, m_submitResultPanel);
        connect(m_submitResultPanel, &SubmitResultPanel::hideRequested, this, [this]() {
            m_submitResultPanel->hide();
        });
    }

    m_submitResultPanel->showResult(result);
    m_submitResultPanel->setVisible(true);

    // Record non-Accepted, non-CE OpenJudge results to error journal
    bool isAccepted = (result.status == QStringLiteral("Accepted")
                       || result.status == QStringLiteral("AC"));
    bool isCE = (result.status == QStringLiteral("Compile Error"));

    if (!isAccepted && !isCE && !m_lastSubmitSourceFile.isEmpty()) {
        // Store the OpenJudge status for async recording after local tests
        m_ojErrorStatus = result.status;

        // Try running local tests against sample data to get I/O
        runLocalTestsForOJError();
    }

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

void MainWindow::runLocalTestsForOJError()
{
    // Get sample test folder from judge panel (set when user selected the problem)
    QString testFolder = m_judgePanel->testFolder();
    if (testFolder.isEmpty()) {
        // No sample data available — fall back to recording without I/O
        OpenJudgeWidget *oj = m_tabManager->findOpenJudgeWidget();
        QString problemName = oj ? oj->currentProblemTitle() : QString();
        QString problemUrl = oj ? oj->currentProblemUrl() : QString();
        SubmissionResult fallback;
        fallback.status = m_ojErrorStatus;
        ErrorJournal::instance().recordOpenJudgeFailure(
            fallback, m_lastSubmitSourceFile, problemName, problemUrl, m_lastSubmitSourceCode);
        return;
    }

    // Check if test folder has any test cases
    QDir testDir(testFolder);
    QStringList inFiles = testDir.entryList({QStringLiteral("*.in")}, QDir::Files);
    if (inFiles.isEmpty()) {
        // No test cases — fall back to recording without I/O
        OpenJudgeWidget *oj2 = m_tabManager->findOpenJudgeWidget();
        QString problemName = oj2 ? oj2->currentProblemTitle() : QString();
        QString problemUrl = oj2 ? oj2->currentProblemUrl() : QString();
        SubmissionResult fallback;
        fallback.status = m_ojErrorStatus;
        ErrorJournal::instance().recordOpenJudgeFailure(
            fallback, m_lastSubmitSourceFile, problemName, problemUrl, m_lastSubmitSourceCode);
        return;
    }

    // Clean up any previous background engine
    if (m_ojErrorJudgeEngine) {
        if (m_ojErrorJudgeEngine->isRunning())
            m_ojErrorJudgeEngine->stop();
        m_ojErrorJudgeEngine->deleteLater();
        m_ojErrorJudgeEngine = nullptr;
    }

    // Create background engine and run local tests
    m_ojErrorJudgeEngine = new JudgeEngine(this);
    m_ojErrorJudgeEngine->setSourceFile(m_lastSubmitSourceFile);
    m_ojErrorJudgeEngine->setTestFolder(testFolder);

    connect(m_ojErrorJudgeEngine, &JudgeEngine::allTestsFinished,
            this, &MainWindow::onOJErrorLocalTestsFinished);

    m_ojErrorJudgeEngine->start();
}

void MainWindow::onOJErrorLocalTestsFinished(int passed, int total)
{
    if (!m_ojErrorJudgeEngine)
        return;

    OpenJudgeWidget *oj = m_tabManager->findOpenJudgeWidget();
    QString problemName = oj ? oj->currentProblemTitle() : QString();
    QString problemUrl = oj ? oj->currentProblemUrl() : QString();

    const auto &results = m_ojErrorJudgeEngine->results();
    bool anyRecorded = false;

    // Record each failed test case with actual/expected output from local run
    for (const auto &tr : results) {
        if (tr.statusCode != QStringLiteral("AC")) {
            ErrorJournal::instance().recordOpenJudgeFailure(
                tr, m_lastSubmitSourceFile, problemName, problemUrl);
            anyRecorded = true;
        }
    }

    // If all local tests passed but OpenJudge says it failed,
    // record one entry noting the failure is on hidden test cases
    if (!anyRecorded) {
        SubmissionResult fallback;
        fallback.status = m_ojErrorStatus;
        ErrorJournal::instance().recordOpenJudgeFailure(
            fallback, m_lastSubmitSourceFile, problemName, problemUrl, m_lastSubmitSourceCode);
    }

    // Clean up
    m_ojErrorJudgeEngine->deleteLater();
    m_ojErrorJudgeEngine = nullptr;
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
    m_bottomPanel->setVisible(true);
    m_bottomPanel->showRunTab();
    double ratio = ConfigManager::instance().outputPanelDefaultHeightRatio();
    int total = m_rightSplitter->height();
    if (total > 0) {
        int outputH = qRound(total * ratio);
        int editorH = total - outputH;
        QList<int> sizes;
        sizes.reserve(m_rightSplitter->count());
        for (int i = 0; i < m_rightSplitter->count(); ++i) {
            QWidget *w = m_rightSplitter->widget(i);
            if (w == m_bottomPanel)
                sizes.append(outputH);
            else if (w == m_tabManager)
                sizes.append(editorH);
            else
                sizes.append(0);
        }
        m_rightSplitter->setSizes(sizes);
    }
}

void MainWindow::toggleDiagnosticsInCodeEditor()
{
    // IDE mode: toggle diagnostics for embedded editor
    OpenJudgeWidget *oj = m_tabManager->findOpenJudgeWidget();
    if (oj && oj->isIdeMode()) {
        if (!m_bottomPanel->isVisible()
            || m_bottomPanel->currentTab() != BottomPanel::DiagnosticsTab) {
            showOutputPanel();
            m_bottomPanel->showDiagnosticsTab();
        } else {
            m_bottomPanel->hide();
        }
        return;
    }

    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor || !editor->isCodeEdit())
        return;

    if (!m_bottomPanel->isVisible()
        || m_bottomPanel->currentTab() != BottomPanel::DiagnosticsTab) {
        showOutputPanel();
        m_bottomPanel->showDiagnosticsTab();
    } else {
        m_bottomPanel->hide();
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

void MainWindow::onCodeBlockRequested(const QString &language, const QString &code, int blockIndex)
{
    const QString normalizedLang = LanguageUtils::normalizeCodeFenceLanguage(language);

    // Track MD code block state
    EditorWidget *editor = m_tabManager->currentEditor();
    m_currentMdFilePath = editor ? editor->currentFilePath() : QString();
    m_currentBlockIndexMd = blockIndex;
    m_currentBlockLanguage = normalizedLang;
    m_isRunningCodeBlock = true;
    m_mdStderrBuffer.clear();

    // Clear previous diagnostics for this block and preview highlights immediately
    if (!m_currentMdFilePath.isEmpty()) {
        m_mdDiagnostics[m_currentMdFilePath].remove(blockIndex);
        m_lastRunBlockIndexMd[m_currentMdFilePath] = blockIndex;
    }
    if (editor)
        editor->clearBlockDiagnostics();
    m_bottomPanel->clearDiagnostics();

    if (normalizedLang.isEmpty()) {
        m_isRunningCodeBlock = false;
        showOutputPanel();
        m_bottomPanel->outputPanel()->clearOutput();
        m_bottomPanel->outputPanel()->appendOutput(
            tr("不支持的语言: %1\n当前支持: python, cpp\n").arg(language), true);
        m_bottomPanel->outputPanel()->setStatus(tr("错误"), true);
        return;
    }

    const QString filePath = saveCodeBlockToTempFile(normalizedLang, code);
    if (filePath.isEmpty()) {
        m_isRunningCodeBlock = false;
        showOutputPanel();
        m_bottomPanel->outputPanel()->clearOutput();
        m_bottomPanel->outputPanel()->appendOutput(
            tr("错误: 无法创建临时文件。\n"), true);
        m_bottomPanel->outputPanel()->setStatus(tr("错误"), true);
        return;
    }

    showOutputPanel();
    m_bottomPanel->outputPanel()->clearOutput();

    if (normalizedLang == QStringLiteral("python")) {
        m_bottomPanel->outputPanel()->appendOutput(
            QStringLiteral("--- ") + tr("运行 Python 代码块 ---\n"), false);
        m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
        m_processRunner->startRunPython(filePath);
    } else if (normalizedLang == QStringLiteral("cpp")) {
        m_bottomPanel->outputPanel()->appendOutput(
            QStringLiteral("--- ") + tr("编译运行 C++ 代码块 ---\n"), false);
        m_bottomPanel->outputPanel()->setStatus(tr("编译中..."));
        m_processRunner->startCompileAndRun(filePath);
    }
}

void MainWindow::parseAndShowBlockDiagnostics()
{
    m_isRunningCodeBlock = false;

    // Skip diagnostics when the process was manually stopped
    if (m_processManuallyStopped) {
        m_processManuallyStopped = false;
        return;
    }

    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor || editor->currentFilePath() != m_currentMdFilePath)
        return;

    QList<SmdDiagnostic> diags;
    if (m_currentBlockLanguage == QStringLiteral("cpp")) {
        diags = CompilerErrorParser::parseCompileErrors(m_mdStderrBuffer, m_currentBlockIndexMd);
    } else if (m_currentBlockLanguage == QStringLiteral("python")) {
        diags = CompilerErrorParser::parsePythonTraceback(m_mdStderrBuffer, m_currentBlockIndexMd);
    }

    // Store diagnostics per file per block
    m_mdDiagnostics[m_currentMdFilePath][m_currentBlockIndexMd] = diags;

    // Update bottom panel
    m_bottomPanel->setDiagnostics(diags);

    // Update preview highlights
    QMap<int, QList<SmdDiagnostic>> blockMap;
    blockMap[m_currentBlockIndexMd] = diags;
    editor->applyBlockDiagnostics(blockMap);
}

void MainWindow::loadMdDiagnosticsForCurrentTab()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor) return;
    QString filePath = editor->currentFilePath();

    disconnect(m_diagnosticsProviderConnection);
    m_bottomPanel->setCurrentEditor(nullptr);

    int lastBlock = m_lastRunBlockIndexMd.value(filePath, -1);
    if (lastBlock >= 0 && m_mdDiagnostics.contains(filePath)
        && m_mdDiagnostics[filePath].contains(lastBlock)) {
        QList<SmdDiagnostic> diags = m_mdDiagnostics[filePath][lastBlock];
        m_bottomPanel->setDiagnostics(diags);

        QMap<int, QList<SmdDiagnostic>> blockMap;
        blockMap[lastBlock] = diags;
        editor->applyBlockDiagnostics(blockMap);
    } else {
        m_bottomPanel->hide();
        editor->clearBlockDiagnostics();
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
    msgBox.addButton(tr("丢弃(&D)"), QMessageBox::DestructiveRole);
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
