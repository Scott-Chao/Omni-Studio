#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "fileexplorerwidget.h"
#include "editorwidget.h"
#include "settingsmanager.h"
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_settings(new SettingsManager("config.ini"))
    , m_explorer(new FileExplorerWidget(this))
    , m_splitter(new QSplitter(Qt::Horizontal, this))
    , m_editor(new EditorWidget(this))
{
    ui->setupUi(this);

    QToolBar *toolBar = addToolBar("文件工具栏"); // 添加工具栏
    toolBar->setMovable(false);   // 禁止拖动
    toolBar->setFloatable(false); // 禁止工具栏脱离主窗口变成独立小窗口

    // 工具栏打开选项
    QAction *openDirAction = new QAction("打开目录", this);
    toolBar->addAction(openDirAction);
    connect(openDirAction, &QAction::triggered, m_explorer, &FileExplorerWidget::selectFolder);

    toolBar->addSeparator();

    // 工具栏保存选项
    QAction *saveAction = new QAction("保存", this);
    saveAction->setShortcut(QKeySequence::Save);
    toolBar->addAction(saveAction);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile); // 设置点击图标保存

    // 设置快捷键保存
    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this);
    connect(shortcut, &QShortcut::activated, this, &MainWindow::saveFile);

    // 设置布局
    m_splitter->addWidget(m_explorer);
    m_splitter->addWidget(m_editor);
    setCentralWidget(m_splitter); // 设置为窗口中心部件

    connect(m_explorer, &FileExplorerWidget::fileClicked, this, &MainWindow::onFileSelected); // 设置点击文件打开

    loadSettings(); // 加载配置
}

void MainWindow::onFileSelected(const QString &filePath)
{
    // 选中文件之后显示
    m_editor->loadFile(filePath);
}

void MainWindow::saveFile() {
    // 点击保存文件
    if (!m_editor->currentFilePath().isEmpty())
        m_editor->saveFile();
}

void MainWindow::saveSettings() {
    // 保存配置
    m_settings->setLastFolderPath(m_explorer->rootPath()); // 获取模型当前监听的根目录并保存
    m_settings->setWindowGeometry(saveGeometry()); // 保存窗口几何信息（位置和大小）
    m_settings->setSplitterState(m_splitter->saveState()); // 保存 QSplitter 的状态（左右两部分的比例）
}

// 触发关闭事件时调用
void MainWindow::closeEvent(QCloseEvent *event) {
    saveSettings();
    event->accept();
}

void MainWindow::loadSettings() {
    // 读取配置

    QString lastPath = m_settings->lastFolderPath(); // 读取上一次的路径
    m_explorer->setRootPath(lastPath);

    QByteArray geometryData = m_settings->windowGeometry(); // 恢复窗口几何信息
    if (!geometryData.isEmpty()) {
        restoreGeometry(geometryData);
    } else {
        // 设置默认窗口大小和位置
        resize(1200, 800); // 默认大小
        move(100, 100); // 默认屏幕左上角偏移
    }

    QByteArray splitterData = m_settings->splitterState(); // 恢复分隔条状态
    if (!splitterData.isEmpty()) {
        m_splitter->restoreState(splitterData);
    } else {
        // 设置初始拉伸比例，让右侧大一些
        m_splitter->setStretchFactor(1, 4);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
