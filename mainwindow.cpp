#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "fileexplorerwidget.h"
#include "editorwidget.h"
#include "settingsmanager.h"
#include "tabmanager.h"
#include "historypanel.h"

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
#include <QInputDialog>
#include <utility>
#include <QDockWidget>
#include <QDirIterator>

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

    // 工具栏最左侧插入显示/隐藏面板的按钮
    toggleHistoryAction = m_dockHistory->toggleViewAction();
    toggleHistoryAction->setToolTip(tr("显示/隐藏历史记录"));
    toggleHistoryAction->setShortcut(QKeySequence("Ctrl+H"));

    // ----- 工具栏 -----
    QToolBar *toolBar = addToolBar("文件工具栏");
    toolBar->setMovable(false);
    toolBar->setFloatable(false);

    // 历史记录
    toolBar->insertAction(nullptr, toggleHistoryAction);
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
    saveAsAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
    addAction(saveAsAction);
    toolBar->addAction(saveAsAction);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::onSaveFileAs);

    toolBar->addSeparator();

    // 预览（快捷键Ctrl+Shift+P）
    m_previewAction = new QAction("预览模式", this);
    m_previewAction->setShortcut(QKeySequence("Ctrl+Shift+P"));
    m_previewAction->setCheckable(true);
    toolBar->addAction(m_previewAction);
    connect(m_previewAction, &QAction::toggled, this, [this](bool checked) {
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor) {
            if (checked && !editor->currentFilePath().toLower().endsWith(".md")) // 仅在 .md 文件中允许使用预览
                return;
            editor->setPreviewMode(checked);
        }
    });

    // 添加缩放项
    QStatusBar *status = statusBar();

    // 创建缩放相关的 QAction
    m_zoomOutAction = new QAction(tr("缩小"), this);
    m_zoomOutAction->setShortcut(QKeySequence("Ctrl+-"));
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::onZoomOut);

    m_zoomInAction = new QAction(tr("放大"), this);
    m_zoomInAction->setShortcut(QKeySequence("Ctrl+="));
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::onZoomIn);

    m_zoomResetAction = new QAction(tr("重置缩放"), this);
    m_zoomResetAction->setShortcut(QKeySequence("Ctrl+0"));
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

    // ----- 界面布局 -----
    // 设置 TabManager 的样式（原有样式保留，可进一步调整）
    m_tabManager->setStyleSheet(
        // 标签页样式
        "QTabBar::tab {"
        "   height: 22px;"
        "   margin-right: 2px;"
        "   padding: 4px 12px;" // 上下4px，左右12px
        "   border-top-left-radius: 4px;"
        "   border-top-right-radius: 4px;"
        "}"
        // 选中标签页样式
        "QTabBar::tab:selected {"
        "   background: #2d2d2d;"
        "   color: #ffffff;"
        "}"
        // 鼠标悬停样式
        "QTabBar::tab:hover:!selected {"
        "   background: #4a4a4a;"
        "}"
        );

    m_splitter->addWidget(m_explorer);
    m_splitter->addWidget(m_tabManager);
    setCentralWidget(m_splitter);

    // 当标签页切换时，更新缩放标签并重新连接当前编辑器的缩放信号
    connect(m_tabManager, &QTabWidget::currentChanged, this, [this](int) {
        updateZoomLabel();
        connectCurrentEditorZoomSignal();
    });

    // 初始连接
    connectCurrentEditorZoomSignal();
    updateZoomLabel();

    // 连接信号：文件树点击 -> 打开文件
    connect(m_explorer, &FileExplorerWidget::fileClicked, this, &MainWindow::onFileSelected);

    // 将重命名、删除的请求直接转发给 FileExplorerWidget 的内部槽
    connect(m_explorer, &FileExplorerWidget::requestDelete, this, &MainWindow::onRequestDelete);
    // 监听重命名成功信号，更新标签管理器中的路径
    connect(m_explorer, &FileExplorerWidget::fileRenamed, this, [this](const QString &oldPath, const QString &newPath) {
        // 更新标签管理器中的路径
        m_tabManager->updateEditorFilePath(oldPath, newPath);
    });
    // 监听操作失败信号，显示错误提示
    connect(m_explorer, &FileExplorerWidget::operationFailed, this, [this](const QString &errorMsg) {
        QMessageBox::warning(this, tr("错误"), errorMsg);
    });
    qApp->installEventFilter(this);

    loadSettings();
    updatePreviewActionState();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ----- 转发给TabManager的槽函数 -----
void MainWindow::onFileSelected(const QString &filePath)
{
    m_tabManager->openFile(filePath);
    connectCurrentEditorZoomSignal();
    updateZoomLabel();
    updatePreviewActionState();
    addToRecentFiles(filePath);
}

void MainWindow::newFile()
{
    EditorWidget *current = m_tabManager->currentEditor();
    EditorWidget *newEditor = m_tabManager->newFile();
    if (newEditor && current) {
        newEditor->setZoomFactor(current->zoomFactor());  // 继承当前缩放
        connectCurrentEditorZoomSignal();
        updateZoomLabel();
    }
    updatePreviewActionState();
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
            updatePreviewActionState(); // 刷新预览按钮状态
            addToRecentFiles(editor->currentFilePath());
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
        updatePreviewActionState();
        addToRecentFiles(newFilePath);
    }
}

// ----- 转发给FileExplorerWidget的槽函数 -----
void MainWindow::onOpenFolder()
{
    m_explorer->selectFolder(m_settings->lastFolderPath(QDir::homePath())); // 将记忆目录传给 selectFolder，对话框将从这里开始浏览
}

void MainWindow::onFolderChanged(const QString &newPath)
{
    m_settings->setLastFolderPath(newPath); // 立即持久化
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
    saveSettings();
    event->accept();
}

void MainWindow::loadSettings()
{
    // 载入配置信息
    QString lastPath = m_settings->lastFolderPath();
    m_explorer->setRootPath(lastPath);

    QByteArray geometryData = m_settings->windowGeometry();
    if (!geometryData.isEmpty()) {
        restoreGeometry(geometryData);
    } else {
        resize(1200, 800);
        move(100, 100);
    }

    QByteArray splitterData = m_settings->splitterState();
    if (!splitterData.isEmpty()) {
        m_splitter->restoreState(splitterData);
    } else {
        m_splitter->setStretchFactor(1, 4);
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
        editor->zoomReset();
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
    EditorWidget *editor = m_tabManager->currentEditor();
    if (editor) {
        m_editorZoomConnection = connect(editor, &EditorWidget::zoomFactorChanged, this, &MainWindow::updateZoomLabel); // 监听缩放
        connect(editor, &EditorWidget::filePathChanged, this, &MainWindow::updatePreviewActionState); // 监听路径变化
        connect(editor, &EditorWidget::wikiLinkClicked, this, &MainWindow::onWikiLinkClicked);
    }
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
        // 无编辑器：隐藏且禁用
        m_previewAction->setVisible(false);
        m_previewAction->setEnabled(false);
        return;
    }

    QString filePath = editor->currentFilePath();
    // 检查是否为 .md 文件
    bool isMd = filePath.toLower().endsWith(".md");

    m_previewAction->setVisible(isMd);
    m_previewAction->setEnabled(isMd);

    if (!isMd && editor->isPreviewMode()) {
        // 如果不是 md 文件但当前处于预览模式，强制切回编辑模式
        editor->setPreviewMode(false);
        // 同步按钮勾选状态
        m_previewAction->setChecked(false);
    } else if (isMd) {
        // 如果是 md 文件，同步按钮的勾选状态以反映当前编辑器的预览模式
        m_previewAction->setChecked(editor->isPreviewMode());
    }
}

void MainWindow::addToRecentFiles(const QString &filePath)
{
    QString cleanPath = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
    if (!cleanPath.isEmpty())
        m_historyPanel->addFile(cleanPath);
}

void MainWindow::onHistoryFileClicked(const QString &filePath)
{
    if (!QFile::exists(filePath)) {
        QMessageBox::warning(this, tr("文件不存在"),
                             tr("无法打开文件，文件可能已被移动或删除：\n%1").arg(filePath));
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
    }
    addToRecentFiles(absolutePath); // 记录到历史（置顶）
    m_dockHistory->hide(); // 隐藏面板
}

void MainWindow::onWikiLinkClicked(const QString &fileName)
{
    QString root = m_explorer->rootPath();
    if (root.isEmpty()) return;

    QStringList filters;
    filters << fileName + ".md" << fileName + ".txt" << fileName;

    QDirIterator it(root, filters, QDir::Files, QDirIterator::Subdirectories);

    if (it.hasNext()) {
        // 找到文件，直接打开
        QString targetPath = it.next();
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
                targetDir = root;
            }

            // 拼接完整路径，默认添加 .md 后缀
            QString newFileName = fileName;
            if (!newFileName.toLower().endsWith(".md") && !newFileName.toLower().endsWith(".txt")) {
                newFileName += ".md";
            }
            QString newFilePath = targetDir + "/" + newFileName;

            // 执行创建
            QFile file(newFilePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.close();
                onFileSelected(newFilePath);
            } else {
                QMessageBox::warning(this, tr("创建失败"),
                                     tr("无法在以下位置创建文件：\n%1").arg(newFilePath));
            }
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress && m_dockHistory->isVisible()) {
        QWidget *clickedWidget = QApplication::widgetAt(QCursor::pos());
        // 如果点击的是 toggleHistoryAction 对应的按钮，让 toggle 动作自己处理
        QToolButton *btn = qobject_cast<QToolButton*>(clickedWidget);
        if (btn && btn->defaultAction() == toggleHistoryAction)
            return QMainWindow::eventFilter(watched, event);

        // 检查点击是否发生在历史面板内部
        if (!m_dockHistory->isAncestorOf(clickedWidget)) {
            m_dockHistory->hide();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}
