#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "fileexplorerwidget.h"
#include "editorwidget.h"
#include "settingsmanager.h"
#include "tabmanager.h"

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_settings(new SettingsManager("config.ini"))
    , m_explorer(new FileExplorerWidget(this))
    , m_splitter(new QSplitter(Qt::Horizontal, this))
    , m_tabManager(new TabManager(this))
{
    ui->setupUi(this);

    // ----- 工具栏 -----
    QToolBar *toolBar = addToolBar("文件工具栏");
    toolBar->setMovable(false);
    toolBar->setFloatable(false);

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
    QAction *previewAction = new QAction("预览模式", this);
    previewAction->setShortcut(QKeySequence("Ctrl+Shift+P"));
    previewAction->setCheckable(true); // 可选中状态，表示当前是否处于预览模式
    toolBar->addAction(previewAction);
    connect(previewAction, &QAction::toggled, this, [this](bool checked) {
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor) {
            editor->setPreviewMode(checked);
        }
    });

    // 当切换标签页时，更新预览按钮的选中状态
    connect(m_tabManager, &QTabWidget::currentChanged, this, [previewAction, this](int) {
        EditorWidget *editor = m_tabManager->currentEditor();
        if (editor) {
            previewAction->setChecked(editor->isPreviewMode());
        } else {
            previewAction->setChecked(false);
        }
    });

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

    // 连接信号：文件树点击 -> 打开文件
    connect(m_explorer, &FileExplorerWidget::fileClicked, this, &MainWindow::onFileSelected);

    loadSettings();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ----- 转发给TabManager的槽函数 -----
void MainWindow::onFileSelected(const QString &filePath)
{
    m_tabManager->openFile(filePath);
}

void MainWindow::newFile()
{
    m_tabManager->newFile();
}

void MainWindow::saveFile()
{
    EditorWidget *editor = m_tabManager->currentEditor();
    if (!editor)
        return;
    if (editor->currentFilePath().isEmpty()) {
        onSaveFileAs(); // 无路径情况下另存为，更新记忆路径
    } else {
        editor->saveFile(); // 保存已有文件，不修改另存为记忆
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
