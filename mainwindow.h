#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QSplitter>
#include <QFileInfo>
#include <QLabel>

class TabManager;
class FileExplorerWidget;
class EditorWidget;
class SettingsManager;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    SettingsManager *m_settings; // 配置信息
    FileExplorerWidget *m_explorer; // 文件浏览器控件
    QSplitter *m_splitter; // 分隔条
    TabManager *m_tabManager; // 标签页栏
    QString m_currentFilePath; // 当前打开的文件路径（文件树）

private slots:
    void onFileSelected(const QString &filePath); // 选中文件（打开）
    void saveFile(); // 保存文件
    void onSaveFileAs(); // 另存为
    void newFile(); // 新建文件
    void saveSettings(); // 保存设置
    void onOpenFolder(); // 打开文件夹
    void onFolderChanged(const QString &newPath); // 记录用户打开的文件夹，实现记忆功能
    void onZoomIn(); // 放大
    void onZoomOut(); // 缩小
    void onZoomReset(); // 重置大小

    void onRequestDelete(const QString &path, bool isDir);

private:
    void closeTabsUnderPath(const QString &dirPath);

protected:
    // 当用户关闭窗口时自动保存
    void closeEvent(QCloseEvent *event) override;

private:
    EditorWidget* currentEditor() const;
    void updateTabTitle(EditorWidget *editor, bool modified);
    void loadSettings(); // 程序启动时读取配置

    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
    QAction *m_zoomResetAction;
    QLabel *m_zoomLabel; // 显示当前缩放百分比
    QMetaObject::Connection m_editorZoomConnection; // 用来管理当前编辑器的缩放信号连接

    void updateZoomLabel(); // 更新百分比标签
    void connectCurrentEditorZoomSignal(); // 连接当前编辑器的缩放信号

};
#endif // MAINWINDOW_H
