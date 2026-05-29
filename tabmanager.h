#ifndef TABMANAGER_H
#define TABMANAGER_H

#include <QTabWidget>
#include "editorwidget.h"
#include <QTabBar>
#include <QMouseEvent>
#include <QApplication>


// 拖拽时在关闭按钮 widget 之上渲染被拖标签的覆盖层
class DragOverlay : public QWidget
{
public:
    explicit DragOverlay(QWidget *parent);
    void setPixmap(const QPixmap &pm);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QPixmap m_pixmap;
};

// 为了修复默认 QTabBar 拖拽时的视觉问题，改用自定义 CustomTabBar
// 功能：限制被拖标签整体不超出标签栏左右边界
class CustomTabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit CustomTabBar(QWidget *parent = nullptr);

    void setEqualWidth(bool enabled);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void initStyleOption(QStyleOptionTab *option, int tabIndex) const override;
    QSize tabSizeHint(int index) const override;

private:
    bool m_dragStarted = false;
    bool m_dragInProgress = false;
    int  m_dragIndex = -1;
    QPoint m_dragPressPos;
    int  m_dragTabWidth = 0;
    int  m_dragOffsetX = 0;
    QPoint m_dragCurrentPos;
    EditorWidget *m_dragEditor = nullptr;
    int  m_lastMoveCenterX = 0;
    DragOverlay *m_dragOverlay = nullptr;
    bool m_equalWidth = false;
};

class SettingsManager;
class OpenJudgeWidget;

class TabManager : public QTabWidget
{
    Q_OBJECT

public:
    explicit TabManager(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

public:
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
    void updatePathsAfterMove(const QString &oldBase, const QString &newBase); // 拖动文件改变路径后更新

    // OpenJudge 集成
    void openOpenJudge(SettingsManager *settings);
    OpenJudgeWidget* findOpenJudgeWidget() const;

public:
    EditorWidget* findEditorByPath(const QString &filePath) const;
    QStringList allOpenedFilePaths() const;   // 返回所有已打开文件的绝对路径（未保存的新文件除外）
    bool closeTabByPath(const QString &filePath, bool askSave = true);
    void updateEditorFilePath(const QString &oldPath, const QString &newPath);

    // 预览标签页（临时标签页，单击替换内容，编辑后自动提升为永久）
    EditorWidget* openPreview(const QString &filePath);
    void promotePreviewToPermanent();
    bool isPreviewEditor(EditorWidget* editor) const;
    EditorWidget* previewEditor() const;

signals:
    void tabCountChanged(int count); // 当标签数量变化时发出
    void previewTabPromoted(EditorWidget *editor); // 预览标签页提升为永久时发出

private slots:
    void onTabCloseRequested(int index);
    void onEditorModificationChanged(EditorWidget *editor, bool modified);
    void onEditorFileSaved(EditorWidget *editor, const QString &newPath);

private:
    void connectEditorSignals(EditorWidget *editor);
    void updateTabTitle(EditorWidget *editor);
    EditorWidget* m_previewEditor = nullptr; // 当前预览编辑器，nullptr 表示无预览标签页
};

#endif // TABMANAGER_H
