#ifndef TABMANAGER_H
#define TABMANAGER_H

#include <QTabWidget>
#include "editorwidget.h"

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
