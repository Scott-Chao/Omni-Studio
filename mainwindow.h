#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QSplitter>
#include <QFileInfo>

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
    QString m_currentFilePath; // 当前打开的文件路径

private slots:
    void onFileSelected(const QString &filePath); // 选中文件（打开）
    void saveFile(); // 保存文件
    void newFile(); // 新建文件
    void saveSettings(); // 保存设置

protected:
    // 当用户关闭窗口时自动保存
    void closeEvent(QCloseEvent *event) override;

private:
    EditorWidget* currentEditor() const;
    void updateTabTitle(EditorWidget *editor, bool modified);
    void loadSettings(); // 程序启动时读取配置
};
#endif // MAINWINDOW_H
