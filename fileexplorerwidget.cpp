#include "FileExplorerWidget.h"
#include <QFileDialog>
#include <QDir>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QKeyEvent>
#include <QApplication>
#include <QLineEdit>

class NoGhostDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    // 让编辑框只覆盖文本区域，不覆盖图标区域
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        // 获取文本区域的矩形（相对于视图 viewport）
        QRect textRect = opt.widget->style()->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
        // 将编辑框移动到文本区域（注意坐标转换）
        editor->setGeometry(textRect);
    }

    // 创建编辑器时确保背景不透明，样式干净
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        QLineEdit *editor = qobject_cast<QLineEdit*>(
            QStyledItemDelegate::createEditor(parent, option, index));
        if (editor) {
            editor->setAutoFillBackground(true);
            editor->setContentsMargins(0, 0, 0, 0);
            editor->setFrame(false);
            // 根据系统主题设置合适的背景色（避免透明）
            editor->setStyleSheet("background: palette(base);");
        }
        return editor;
    }

    // 编辑时仍然绘制背景和图标，但不绘制文本
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        if (option.state & QStyle::State_Editing) {
            // 复制一份 option，清空文本，这样基类只会绘制图标、背景等，不会绘制文本
            QStyleOptionViewItem opt = option;
            opt.text = QString();
            QStyledItemDelegate::paint(painter, opt, index);
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
    }

    void setEditorData(QWidget *editor, const QModelIndex &proxyIndex) const override
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
        if (!lineEdit)
            return;

        // 获取代理模型和源模型
        const QSortFilterProxyModel *proxy = qobject_cast<const QSortFilterProxyModel*>(proxyIndex.model());
        if (!proxy) return;
        const QFileSystemModel *model = qobject_cast<const QFileSystemModel*>(proxy->sourceModel());
        if (!model) return;

        QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);
        QFileInfo info = model->fileInfo(sourceIndex);
        QString fullName = info.fileName();

        lineEdit->setText(fullName);
        if (info.isDir()) {
            lineEdit->selectAll();
        } else {
            int selLen = fullName.length();
            QString suffix = info.suffix();
            if (!suffix.isEmpty() && fullName.endsWith("." + suffix))
                selLen = fullName.length() - suffix.length() - 1;
            if (selLen <= 0) selLen = fullName.length();
            // 延迟设置选区，覆盖视图可能的全选
            QTimer::singleShot(0, lineEdit, [lineEdit, selLen]() {
                lineEdit->setSelection(0, selLen);
            });
        }
    }
};

class DeleteKeyFilter : public QObject {
public:
    bool deletePressed = false;
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Delete) {
                deletePressed = true;
                if (auto *menu = qobject_cast<QMenu*>(obj)) {
                    menu->close();  // 关闭菜单
                }
                return true;
            }
        }
        return false;
    }
};

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

    m_fileModel->setReadOnly(false);

    // 创建排序代理
    m_sortProxy = new FileSortProxyModel(this);
    m_sortProxy->setSourceModel(m_fileModel);
    m_sortProxy->setSortRole(Qt::DisplayRole);
    m_sortProxy->setDynamicSortFilter(false);
    m_sortProxy->sort(0, Qt::AscendingOrder);

    // 视图绑定代理模型
    m_treeView->setModel(m_sortProxy);

    // 配置树视图外观
    m_treeView->setStyleSheet("QTreeView::item { height: 24px; }");
    m_treeView->header()->hide(); // 隐藏表头
    m_treeView->setIndentation(17); // 调整缩进
    m_treeView->setRootIsDecorated(true); // 显示展开/折叠箭头
    m_treeView->hideColumn(1); // 隐藏大小列
    m_treeView->hideColumn(2); // 隐藏类型列
    m_treeView->hideColumn(3); // 隐藏修改日期列

    // 设置编辑触发器：F2 键 或 单击选中后再次单击（类似资源管理器）
    m_treeView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

    m_treeView->setItemDelegate(new NoGhostDelegate(m_treeView)); // 设置委托

    // 连接模型的重命名信号
    connect(m_fileModel, &QFileSystemModel::fileRenamed, this, &FileExplorerWidget::onFileRenamed);

    // 连接点击信号
    connect(m_treeView, &QTreeView::clicked, this, &FileExplorerWidget::onTreeViewClicked);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &FileExplorerWidget::onCustomContextMenu);

    m_treeView->installEventFilter(this);
}

FileExplorerWidget::~FileExplorerWidget()
{
}

bool FileExplorerWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_treeView && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Delete) {
            // 如果焦点在编辑器（如重命名时），不拦截，让 Delete 作为文本删除
            QWidget *fw = QApplication::focusWidget();
            if (fw && (fw->parent() == m_treeView->viewport() || fw->parent() == m_treeView)) {
                if (qobject_cast<QLineEdit*>(fw))
                    return false;   // 交给内联编辑器处理
            }

            // 获取当前选中项
            QModelIndex proxyIndex = m_treeView->currentIndex();
            if (!proxyIndex.isValid())
                return false;

            QModelIndex sourceIndex = m_sortProxy->mapToSource(proxyIndex);
            QString path = m_fileModel->filePath(sourceIndex);
            QFileInfo info(path);

            if (info.isDir() ||
                info.suffix().toLower() == "md" ||
                info.suffix().toLower() == "txt")
            {
                emit requestDelete(path, info.isDir());
                return true;
            }
        }
    }
    return false;
}

void FileExplorerWidget::setRootPath(const QString &path)
{
    // 设置根目录
    if (!path.isEmpty()) {
        m_fileModel->setRootPath(path);
        QModelIndex sourceRoot = m_fileModel->index(path);
        QModelIndex proxyRoot = m_sortProxy->mapFromSource(sourceRoot);
        m_treeView->setRootIndex(proxyRoot);
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

void FileExplorerWidget::onTreeViewClicked(const QModelIndex &proxyIndex)
{
    // 点击树视图打开文件
    QModelIndex sourceIndex = m_sortProxy->mapToSource(proxyIndex);
    QString filePath = m_fileModel->filePath(sourceIndex);
    QFileInfo fileInfo(filePath);
    // 只处理文件（不处理目录），并且扩展名为 .txt 或 .md
    if (!fileInfo.isDir() &&
        (fileInfo.suffix().toLower() == "txt" || fileInfo.suffix().toLower() == "md")) {
        emit fileClicked(filePath);
    }
}

void FileExplorerWidget::onCustomContextMenu(const QPoint &point) {
    QModelIndex proxyIndex = m_treeView->indexAt(point);
    QModelIndex sourceIndex;
    QString path;
    bool isDir = false;

    if (proxyIndex.isValid()) {
        sourceIndex = m_sortProxy->mapToSource(proxyIndex);
        path = m_fileModel->filePath(sourceIndex);
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

    if (proxyIndex.isValid()) {
        menu.addSeparator();
        renameAction = menu.addAction(tr("重命名"));
        deleteAction = menu.addAction(tr("删除"));
        deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));
    }

    DeleteKeyFilter filter;
    menu.installEventFilter(&filter);

    QAction *selected = menu.exec(m_treeView->viewport()->mapToGlobal(point));

    // 移除过滤器（可选，但无害）
    menu.removeEventFilter(&filter);

    // 优先处理通过 Delete 键触发的删除
    if (filter.deletePressed) {
        // 仅当菜单中包含删除选项时才有效（即 proxyIndex 有效）
        if (proxyIndex.isValid()) {
            emit requestDelete(path, isDir);
        }
        return;
    }

    if (!selected) return;

    if (selected == newFileAction) {
        QString parentDir;
        if (proxyIndex.isValid()) {
            if (isDir)
                parentDir = path;
            else
                parentDir = QFileInfo(path).absolutePath();
        } else {
            parentDir = rootPath();
        }
        createNewFileInline(parentDir);
    } else if (selected == newFolderAction) {
        QString parentDir;
        if (proxyIndex.isValid()) {
            if (isDir)
                parentDir = path;
            else
                parentDir = QFileInfo(path).absolutePath();
        } else {
            parentDir = rootPath();
        }
        createNewFolderInline(parentDir);
    } else if (selected == renameAction) {
        m_treeView->edit(proxyIndex);
    } else if (selected == deleteAction) {
        emit requestDelete(path, isDir);
    }
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

    m_sortProxy->invalidate();
    m_sortProxy->sort(0, Qt::AscendingOrder);
}

static QString uniqueDefaultName(const QDir &dir, const QString &baseName, const QString &extension = QString())
{
    QString candidate = baseName + extension;
    if (!dir.exists(candidate))
        return candidate;

    int i = 1;
    while (true) {
        candidate = baseName + "(" + QString::number(i) + ")" + extension;
        if (!dir.exists(candidate))
            return candidate;
        ++i;
    }
}

void FileExplorerWidget::createNewFolderInline(const QString &parentDir)
{
    QDir dir(parentDir);
    QString folderName = uniqueDefaultName(dir, tr("新建文件夹"));
    if (!dir.mkdir(folderName)) {
        emit operationFailed(tr("无法创建文件夹: %1").arg(dir.filePath(folderName)));
        return;
    }

    // 源模型索引
    QModelIndex sourceIdx = m_fileModel->index(dir.filePath(folderName));
    m_sortProxy->sort(0, Qt::AscendingOrder); // 排序
    // 映射到代理模型的索引（排序后自动有效）
    QModelIndex proxyIdx = m_sortProxy->mapFromSource(sourceIdx);
    m_treeView->setCurrentIndex(proxyIdx);
    m_treeView->scrollTo(proxyIdx);
    m_treeView->edit(proxyIdx);
}

void FileExplorerWidget::createNewFileInline(const QString &parentDir)
{
    QDir dir(parentDir);
    QString fileName = uniqueDefaultName(dir, tr("新建文件"), QStringLiteral(".md"));
    QFile file(dir.filePath(fileName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        emit operationFailed(tr("无法创建文件: %1").arg(file.fileName()));
        return;
    }
    file.close();

    QModelIndex sourceIdx = m_fileModel->index(dir.filePath(fileName));
    m_sortProxy->sort(0, Qt::AscendingOrder);
    QModelIndex proxyIdx = m_sortProxy->mapFromSource(sourceIdx);
    m_treeView->setCurrentIndex(proxyIdx);
    m_treeView->scrollTo(proxyIdx);
    m_treeView->edit(proxyIdx);
}

bool FileSortProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    QFileSystemModel *fsModel = qobject_cast<QFileSystemModel*>(sourceModel());
    if (!fsModel)
        return QSortFilterProxyModel::lessThan(source_left, source_right);

    QFileInfo leftInfo = fsModel->fileInfo(source_left);
    QFileInfo rightInfo = fsModel->fileInfo(source_right);
    bool leftDir = leftInfo.isDir();
    bool rightDir = rightInfo.isDir();

    if (leftDir == rightDir)
        return QString::localeAwareCompare(leftInfo.fileName(), rightInfo.fileName()) < 0;
    return leftDir; // 目录在前
}
