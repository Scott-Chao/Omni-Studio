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
#include <QPainter>

class NoGhostDelegate : public QStyledItemDelegate
{
public:
    NoGhostDelegate(FileExplorerWidget *explorer, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_explorer(explorer) {}

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

        // 额外绘制：如果是当前拖拽目标文件夹，底部画条
        if (m_explorer && m_explorer->isDropTargetFolder(index)) {
            QRect r = option.rect;
            int barHeight = 3;
            QRect bar(r.left() + 2, r.bottom() - barHeight, r.width() - 4, barHeight);
            painter->fillRect(bar, QColor("#2196F3"));  // 蓝色
        }
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &proxyIndex) const override
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
        if (!lineEdit) {
            QStyledItemDelegate::setModelData(editor, model, proxyIndex);
            return;
        }

        QString newText = lineEdit->text().trimmed();

        const QSortFilterProxyModel *proxy = qobject_cast<const QSortFilterProxyModel*>(proxyIndex.model());
        if (!proxy) {
            QStyledItemDelegate::setModelData(editor, model, proxyIndex);
            return;
        }
        const QFileSystemModel *fsModel = qobject_cast<const QFileSystemModel*>(proxy->sourceModel());
        if (!fsModel) {
            QStyledItemDelegate::setModelData(editor, model, proxyIndex);
            return;
        }

        QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);
        QFileInfo info = fsModel->fileInfo(sourceIndex);

        if (info.isDir()) {
            if (newText.isEmpty())
                newText = info.fileName();
        } else {
            if (newText.isEmpty() || newText.startsWith('.'))
                newText = info.fileName();
        }

        lineEdit->setText(newText);
        QStyledItemDelegate::setModelData(editor, model, proxyIndex);
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
private:
    FileExplorerWidget *m_explorer;
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

    // 允许拖动
    m_treeView->setDragEnabled(true);
    m_treeView->setAcceptDrops(true);
    m_treeView->setDropIndicatorShown(true);
    m_treeView->setDragDropMode(QAbstractItemView::DragDrop);
    m_treeView->setDefaultDropAction(Qt::MoveAction);

    // 设置编辑触发器：F2 键 或 单击选中后再次单击（类似资源管理器）
    m_treeView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

    m_treeView->setItemDelegate(new NoGhostDelegate(this, m_treeView)); // 设置委托

    // 连接模型的重命名信号
    connect(m_fileModel, &QFileSystemModel::fileRenamed, this, &FileExplorerWidget::onFileRenamed);

    // 连接点击信号
    connect(m_treeView, &QTreeView::clicked, this, &FileExplorerWidget::onTreeViewClicked);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &FileExplorerWidget::onCustomContextMenu);

    m_treeView->installEventFilter(this);
    m_treeView->viewport()->installEventFilter(this);

    m_dropTargetIndex = QModelIndex();
}

FileExplorerWidget::~FileExplorerWidget()
{
}

bool FileExplorerWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_treeView || obj == m_treeView->viewport()) {
        // 接管拖拽事件，阻止 QFileSystemModel 的默认移动
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            QDragMoveEvent *de = static_cast<QDragMoveEvent*>(event);
            if (de->source() == m_treeView) {
                m_dragSourceIndexes = m_treeView->selectionModel()->selectedRows();
                // 更新悬停文件夹索引
                QModelIndex proxyIdx = m_treeView->indexAt(de->position().toPoint());
                if (proxyIdx.isValid()) {
                    QModelIndex srcIdx = m_sortProxy->mapToSource(proxyIdx);
                    if (m_fileModel->isDir(srcIdx))
                        m_dropTargetIndex = proxyIdx;
                    else
                        m_dropTargetIndex = QModelIndex();
                } else {
                    m_dropTargetIndex = QModelIndex();
                }
                m_treeView->viewport()->update();   // 触发重绘
                de->acceptProposedAction();
            } else {
                de->ignore();
            }
            return true;
        }

        if (event->type() == QEvent::Drop) {
            handleDropEvent(static_cast<QDropEvent*>(event));
            return true;
        }

        if (event->type() == QEvent::DragLeave || event->type() == QEvent::Drop) {
            m_dropTargetIndex = QModelIndex();
            m_treeView->viewport()->update();
            // Drop 的具体调用仍由 handleDropEvent 完成（DragLeave 不调用）
        }

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
                emit requestDelete(path, QFileInfo(path).isDir());
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

void FileExplorerWidget::selectFile(const QString &filePath)
{
    if (filePath.isEmpty())
        return;

    QModelIndex sourceIndex = m_fileModel->index(filePath);
    if (!sourceIndex.isValid())
        return;

    QModelIndex proxyIndex = m_sortProxy->mapFromSource(sourceIndex);
    if (!proxyIndex.isValid())
        return;

    // 展开所有父级目录，确保目标文件可见
    QModelIndex parent = proxyIndex.parent();
    QModelIndex rootIdx = m_treeView->rootIndex();
    while (parent.isValid() && parent != rootIdx) {
        m_treeView->setExpanded(parent, true);
        parent = parent.parent();
    }

    m_treeView->setCurrentIndex(proxyIndex);
    m_treeView->scrollTo(proxyIndex, QAbstractItemView::EnsureVisible);
}

void FileExplorerWidget::onTreeViewClicked(const QModelIndex &proxyIndex)
{
    // 点击树视图打开文件
    QModelIndex sourceIndex = m_sortProxy->mapToSource(proxyIndex);
    QString filePath = m_fileModel->filePath(sourceIndex);
    QFileInfo fileInfo(filePath);
    // 只处理文件（不处理目录）
    if (!fileInfo.isDir()) {
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
    // 使用 QDir::absoluteFilePath 规范化路径（确保与 BacklinkIndex 存储的路径格式一致）
    QString oldFullPath = QDir(path).absoluteFilePath(oldName);
    QString newFullPath = QDir(path).absoluteFilePath(newName);
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

void FileExplorerWidget::handleDropEvent(QDropEvent *event)
{
    m_dropTargetIndex = QModelIndex();
    m_treeView->viewport()->update();

    // 用本地副本接管拖拽数据，并清空成员
    QModelIndexList draggedIndexes = m_dragSourceIndexes;
    m_dragSourceIndexes.clear();

    if (draggedIndexes.isEmpty() || event->proposedAction() != Qt::MoveAction) {
        event->ignore();
        return;
    }

    QModelIndex targetProxyIndex = m_treeView->indexAt(event->position().toPoint());
    if (!targetProxyIndex.isValid()) {
        event->ignore();
        return;
    }

    QModelIndex sourceProxyIndex = draggedIndexes.first();   // 用本地副本

    // 映射到源模型
    QModelIndex targetSourceIndex = m_sortProxy->mapToSource(targetProxyIndex);
    QModelIndex sourceSourceIndex = m_sortProxy->mapToSource(sourceProxyIndex);

    QString oldPath = QDir::cleanPath(m_fileModel->filePath(sourceSourceIndex));
    QFileInfo targetInfo(m_fileModel->filePath(targetSourceIndex));

    // 确定目标文件夹（如果目标不是目录，则取其父目录）
    QString targetDir;
    if (targetInfo.isDir()) {
        targetDir = targetInfo.absoluteFilePath();
    } else {
        targetDir = targetInfo.absolutePath();
    }
    targetDir = QDir::cleanPath(targetDir);

    // 确保目标目录在当前根目录内
    QString root = QDir::cleanPath(m_fileModel->rootPath());
    if (!targetDir.startsWith(root, Qt::CaseInsensitive) || oldPath.isEmpty()) {
        event->ignore();
        return;
    }

    // 构建新路径
    QString newPath = targetDir + "/" + QFileInfo(oldPath).fileName();
    newPath = QDir::cleanPath(newPath);
    if (oldPath == newPath) {
        event->ignore();
        return;
    }

    // 检查新路径是否已存在（避免覆盖）
    if (QFile::exists(newPath)) {
        event->ignore();
        return;
    }

    // 执行文件系统移动
    QFile file(oldPath);
    if (!file.rename(newPath)) {
        event->ignore();
        return;
    }

    // 刷新模型以显示新结构
    m_fileModel->revert();

    // 发出重命名/移动信号，主窗口会同步更新标签页、历史记录等
    emit fileRenamed(oldPath, newPath);

    event->acceptProposedAction();
}

bool FileExplorerWidget::isDropTargetFolder(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || !m_dropTargetIndex.isValid())
        return false;
    // 必须同一个索引且源模型对应的是目录
    if (proxyIndex != m_dropTargetIndex)
        return false;
    QModelIndex srcIdx = m_sortProxy->mapToSource(proxyIndex);
    return m_fileModel->isDir(srcIdx);
}
