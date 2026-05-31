#ifndef FILEEXPLORERWIDGET_H
#define FILEEXPLORERWIDGET_H

#include <QWidget>
#include <QFileSystemModel>
#include <QFileIconProvider>
#include <QTreeView>
#include <QString>
#include <QSortFilterProxyModel>
#include <QPushButton>
#include <QLabel>
#include "flowlayout.h"

class FileSortProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

protected:
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    mutable QFileIconProvider m_iconProvider;
    mutable QIcon m_folderIcon;
    mutable QIcon m_fileIcon;
};

class DragDropTreeView;

class FileExplorerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FileExplorerWidget(QWidget *parent = nullptr);
    ~FileExplorerWidget();

    void setRootPath(const QString &path); // 设置要显示的根目录
    QString rootPath() const; // 获取当前根目录
    void selectFile(const QString &filePath); // 选中并展开到指定文件
    bool isDropTargetFolder(const QModelIndex &proxyIndex) const; // 判断某个代理索引是否当前拖拽悬停的文件夹
    void reloadShortcuts();
    void refreshTree();
    void collapseAll();
    void setItemHeight(int height);

signals:
    void fileClicked(const QString &filePath); // 单击文件 → 以预览模式打开
    void fileDoubleClicked(const QString &filePath); // 双击文件 → 永久打开
    void folderChanged(const QString &newPath);
    void requestDelete(const QString &path, bool isDir); // 请求删除
    void fileRenamed(const QString &oldPath, const QString &newPath); // 重命名成功时发出
    void itemDeleted(const QString &path); // 删除成功时发出
    void operationFailed(const QString &errorMsg); // 通用失败信号（可选）

public slots:
    void selectFolder(const QString &defaultDir = QString()); // 弹出文件夹选择对话框，并切换到用户选择的目录
    void onCustomContextMenu(const QPoint &point); // 右键菜单槽函数
    void deleteItem(const QString &path, bool isDir); // 删除文件/文件夹
    void onFileRenamed(const QString &path, const QString &oldName, const QString &newName); // 响应模型重命名信号

private slots:
    void onTreeViewClicked(const QModelIndex &index); // 处理树视图的点击事件
    void onTreeViewDoubleClicked(const QModelIndex &index); // 处理树视图的双击事件

private:
    QFileSystemModel *m_fileModel;
    QTreeView *m_treeView;

    void createNewFolderInline(const QString &parentDir);
    void createNewFileInline(const QString &parentDir);
    QSortFilterProxyModel *m_sortProxy;
    void handleDropEvent(QDropEvent *event);
    QModelIndexList m_dragSourceIndexes;   // 拖拽开始时选中的索引
    QModelIndex m_dropTargetIndex;  // 当前拖拽悬停的文件夹代理索引

    // 面包屑路径栏
    QWidget *m_breadcrumb;
    FlowLayout *m_breadcrumbLayout;
    void updateBreadcrumb();
    void refreshStyle();

    // 文件树工具栏
    QWidget *m_toolbar;
    QLabel *m_folderLabel;
    QPushButton *m_newFileBtn;
    QPushButton *m_newFolderBtn;
    QPushButton *m_refreshBtn;
    QPushButton *m_collapseAllBtn;
    QString m_folderFullName;
    void updateFolderLabel();

    // Configurable shortcuts
    QKeySequence m_deleteShortcut;

    QString m_pendingNewFile;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
};

#endif // FILEEXPLORERWIDGET_H
