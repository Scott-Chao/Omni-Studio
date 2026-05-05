#ifndef TABMANAGER_H
#define TABMANAGER_H

#include <QTabWidget>
#include "editorwidget.h"
#include <QTabBar>
#include <QMouseEvent>
#include <QApplication>


// 为了修复默认 QTabBar 拖拽时的视觉问题，改用自定义 CustomTabBar
// 功能：限制被拖标签整体不超出标签栏左右边界
class CustomTabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit CustomTabBar(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool m_dragStarted = false;
    bool m_dragInProgress = false;
    int  m_dragIndex = -1;
    QPoint m_dragPressPos;
    int  m_dragTabWidth = 0; // 被拖标签的宽度
    int  m_dragOffsetX = 0; // 鼠标在标签内的横向偏移（相对于标签左边缘）
};

class TabManager : public QTabWidget
{
    Q_OBJECT

public:
    explicit TabManager(QWidget *parent = nullptr);

    // 打开文件（若已存在则切换，否则新建标签）
    EditorWidget* openFile(const QString &filePath);
    // 新建空白文件
    EditorWidget* newFile();
    // 获取当前活动的编辑器
    EditorWidget* currentEditor() const;
    // 保存当前文件（若没有路径则自动弹出另存为对话框）
    void saveCurrentFile();
    // 关闭标签页（带保存提示）
    void closeCurrentTab();
    // 关闭标签页，返回 true 表示已关闭/放弃，false 表示用户取消
    bool closeTab(int index);
    // 关闭所有标签页，若任一取消则返回 false
    bool closeAllTabs();

public:
    EditorWidget* findEditorByPath(const QString &filePath) const;
    QStringList allOpenedFilePaths() const;   // 返回所有已打开文件的绝对路径（未保存的新文件除外）
    bool closeTabByPath(const QString &filePath, bool askSave = true);
    void updateEditorFilePath(const QString &oldPath, const QString &newPath);

signals:
    void tabCountChanged(int count); // 当标签数量变化时发出

private slots:
    void onTabCloseRequested(int index);
    void onEditorModificationChanged(EditorWidget *editor, bool modified);
    void onEditorFileSaved(EditorWidget *editor, const QString &newPath);

private:
    void connectEditorSignals(EditorWidget *editor);
    void updateTabTitle(EditorWidget *editor);
};

#endif // TABMANAGER_H
