#ifndef FILEEXPLORERWIDGET_H
#define FILEEXPLORERWIDGET_H

#include <QWidget>
#include <QFileSystemModel>
#include <QTreeView>
#include <QString>

class FileExplorerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FileExplorerWidget(QWidget *parent = nullptr);
    ~FileExplorerWidget();

    void setRootPath(const QString &path); // 设置要显示的根目录
    QString rootPath() const; // 获取当前根目录

signals:
    void fileClicked(const QString &filePath); // 当用户点击一个文件时发出信号，参数为文件完整路径
    void folderChanged(const QString &newPath);

public slots:
    void selectFolder(const QString &defaultDir = QString()); // 弹出文件夹选择对话框，并切换到用户选择的目录

private slots:
    void onTreeViewClicked(const QModelIndex &index); // 处理树视图的点击事件

private:
    QFileSystemModel *m_fileModel;
    QTreeView *m_treeView;
};

#endif // FILEEXPLORERWIDGET_H
