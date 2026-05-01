#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QSplitter;
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
    SettingsManager *m_settings;
    FileExplorerWidget *m_explorer; // 文件浏览器控件
    QSplitter *m_splitter;
    EditorWidget *m_editor;
    QString m_currentFilePath;
    void saveFile();

private slots:
    void onFileSelected(const QString &filePath); // 处理文件浏览器选中的文件

protected:
    // 重写关闭事件，当用户关闭窗口时自动保存
    void closeEvent(QCloseEvent *event) override;

private:
    void loadSettings(); // 程序启动时读取配置
    void saveSettings(); // 程序退出时保存配置
};
#endif // MAINWINDOW_H
