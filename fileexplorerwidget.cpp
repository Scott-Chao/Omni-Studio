#include "FileExplorerWidget.h"
#include <QFileDialog>
#include <QDir>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>

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

    m_fileModel->setReadOnly(false);

    // 设置编辑触发器：F2 键 或 单击选中后再次单击（类似资源管理器）
    m_treeView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

    // 连接模型的重命名信号
    connect(m_fileModel, &QFileSystemModel::fileRenamed, this, &FileExplorerWidget::onFileRenamed);

    // 连接点击信号
    connect(m_treeView, &QTreeView::clicked, this, &FileExplorerWidget::onTreeViewClicked);

    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &FileExplorerWidget::onCustomContextMenu);
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

void FileExplorerWidget::onCustomContextMenu(const QPoint &point) {
    QModelIndex index = m_treeView->indexAt(point);
    QString path;
    bool isDir = false;

    if (index.isValid()) {
        path = m_fileModel->filePath(index);
        QFileInfo info(path);
        isDir = info.isDir();
        // 只对文件树中显示的项（文件夹或 .md/.txt）才显示完整菜单
        if (!isDir && !(info.suffix() == "md" || info.suffix() == "txt"))
            return;
    } else {
        // 点击空白区域，使用当前根目录作为上下文
        path = rootPath();
        isDir = true;
    }

    QMenu menu(this);
    QAction *newFileAction = menu.addAction(tr("新建文件"));
    QAction *newFolderAction = menu.addAction(tr("新建文件夹"));

    QAction *renameAction = nullptr;
    QAction *deleteAction = nullptr;

    if (index.isValid()) {
        menu.addSeparator();
        renameAction = menu.addAction(tr("重命名"));
        deleteAction = menu.addAction(tr("删除"));
    }

    QAction *selected = menu.exec(m_treeView->viewport()->mapToGlobal(point));
    if (!selected) return;

    if (selected == newFileAction) {
        emit requestNewFile();
    } else if (selected == newFolderAction) {
        QString parentDir = path;
        if (!isDir) {
            parentDir = QFileInfo(path).absolutePath();
        }
        emit requestNewFolder(parentDir);
    } else if (selected == renameAction) {
        m_treeView->edit(index);
    } else if (selected == deleteAction) {
        emit requestDelete(path, isDir);
    }
}

void FileExplorerWidget::createNewFolder(const QString &parentDir)
{
    bool ok;
    QString folderName = QInputDialog::getText(this, tr("新建文件夹"),
                                               tr("文件夹名称:"), QLineEdit::Normal,
                                               tr("新建文件夹"), &ok);
    if (!ok || folderName.isEmpty())
        return;

    QDir dir(parentDir);
    if (!dir.mkdir(folderName)) {
        emit operationFailed(tr("无法创建文件夹: %1").arg(dir.filePath(folderName)));
    }
    // 文件树会自动刷新
}

void FileExplorerWidget::deleteItem(const QString &path, bool isDir)
{
    bool success;
    if (isDir) {
        QDir dir(path);
        success = dir.removeRecursively();
    } else {
        QFile file(path);
        success = file.remove();
    }
    if (!success) {
        emit operationFailed(tr("删除失败，请检查权限或文件是否被其他程序占用。"));
    } else {
        emit itemDeleted(path);
    }
}

void FileExplorerWidget::onFileRenamed(const QString &path, const QString &oldName, const QString &newName)
{
    // 构造完整旧路径和新路径
    QString oldFullPath = path + QDir::separator() + oldName;
    QString newFullPath = path + QDir::separator() + newName;
    emit fileRenamed(oldFullPath, newFullPath);
}
