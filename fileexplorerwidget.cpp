#include "FileExplorerWidget.h"
#include <QFileDialog>
#include <QDir>
#include <QHeaderView>
#include <QVBoxLayout>

FileExplorerWidget::FileExplorerWidget(QWidget *parent)
    : QWidget(parent)
    , m_fileModel(new QFileSystemModel(this))
    , m_treeView(new QTreeView(this))
{
    // 设置布局，使树视图填满当前控件
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_treeView);
    setLayout(layout);

    // 配置树视图外观
    m_treeView->setModel(m_fileModel);
    m_treeView->setStyleSheet("QTreeView::item { height: 24px; }");
    m_treeView->header()->hide(); // 隐藏表头
    m_treeView->setIndentation(17); // 调整缩进
    m_treeView->setRootIsDecorated(true); // 显示展开/折叠箭头
    m_treeView->hideColumn(1); // 隐藏大小列
    m_treeView->hideColumn(2); // 隐藏类型列
    m_treeView->hideColumn(3); // 隐藏修改日期列

    // 连接点击信号
    connect(m_treeView, &QTreeView::clicked, this, &FileExplorerWidget::onTreeViewClicked);
}

FileExplorerWidget::~FileExplorerWidget()
{
}

void FileExplorerWidget::setRootPath(const QString &path)
{
    // 设置根目录
    if (!path.isEmpty()) {
        m_fileModel->setRootPath(path);
        m_treeView->setRootIndex(m_fileModel->index(path));
    }
}

QString FileExplorerWidget::rootPath() const
{
    // 返回根目录
    return m_fileModel->rootPath();
}

void FileExplorerWidget::selectFolder(const QString &defaultDir)
{
    // 选择文件夹，支持传入指定文件夹，否则默认为主文件夹
    QString startDir = defaultDir.isEmpty() ? QDir::homePath() : defaultDir;
    QString dirPath = QFileDialog::getExistingDirectory(this, "选择文件夹",
                                                        startDir,
                                                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dirPath.isEmpty()) {
        setRootPath(dirPath);
        emit folderChanged(dirPath);
    }
}

void FileExplorerWidget::onTreeViewClicked(const QModelIndex &index)
{
    // 点击树视图打开文件
    QString filePath = m_fileModel->filePath(index);
    QFileInfo fileInfo(filePath);
    // 只处理文件（不处理目录），并且扩展名为 .txt 或 .md
    if (!fileInfo.isDir() &&
        (fileInfo.suffix().toLower() == "txt" || fileInfo.suffix().toLower() == "md")) {
        emit fileClicked(filePath);
    }
}
