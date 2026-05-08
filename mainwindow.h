#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QSplitter>
#include <QFileInfo>
#include <QLabel>
#include <QMap>

class TabManager;
class FileExplorerWidget;
class EditorWidget;
class SettingsManager;
class QDockWidget;
class HistoryPanel;
class BacklinkIndex;
class BacklinksPanel;
class SearchPanel;

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
    void onRequestDelete(const QString &path, bool isDir); // 删除文件/文件夹
    void onHistoryFileClicked(const QString &filePath); // 打开历史记录
    void onSearchResultClicked(const QString &filePath, int lineNumber,
                               const QString &searchText); // 打开搜索结果
    void onWikiLinkClicked(const QString &fileName); // 点击双向链接
    void buildFileIndex(); // 全量更新索引
    void onFileRenamedInIndex(const QString &oldPath, const QString &newPath); // 增量更新：重命名
    void onFileDeletedInIndex(const QString &path); // 增量更新：删除
    void onFileMovedOrRenamed(const QString &oldPath, const QString &newPath); // 通过文件树进行文件移动
    void updateWikiLinksAfterRename(const QStringList &affectedSources,
                                    const QString &oldLinkText,
                                    const QString &newLinkText); // 重命名时更新所有引用的 wiki 链接文本

protected:
    void closeEvent(QCloseEvent *event) override; // 当用户关闭窗口时自动保存
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void closeTabsUnderPath(const QString &dirPath);

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
    // 预览模式激活状态调整
    QAction *m_previewAction = nullptr;
    void updatePreviewActionState();
    // 历史记录
    void addToRecentFiles(const QString &filePath);
    HistoryPanel *m_historyPanel;
    QDockWidget *m_dockHistory;
    QAction *toggleHistoryAction;
    // 反向链接
    void refreshBacklinks();
    BacklinkIndex *m_backlinkIndex;
    BacklinksPanel *m_backlinksPanel;
    QDockWidget *m_dockBacklinks;
    QAction *toggleBacklinksAction;
    // 搜索面板
    SearchPanel *m_searchPanel;
    QDockWidget *m_dockSearch;
    QAction *toggleSearchAction;

    // 键：文件名（不带路径，不带后缀，如 "笔记"）
    // 值：该文件名对应的所有绝对路径列表（处理同名文件）
    QMap<QString, QStringList> m_fileIndex;
    QString findWikiTarget(const QString &fileName); // 向上递归搜索目标文件
    void updateCurrentEditorCompletions(); // 更新当前编辑器的 WikiLink 补全列表

};
#endif // MAINWINDOW_H
