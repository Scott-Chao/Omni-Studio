#include "tabmanager.h"
#include <QFileInfo>
#include <QMessageBox>
#include <QAbstractButton>
#include <QDir>

TabManager::TabManager(QWidget *parent)
    : QTabWidget(parent)
{
    setTabsClosable(true);
    setMovable(true);
    connect(this, &QTabWidget::tabCloseRequested, this, &TabManager::onTabCloseRequested);
}

EditorWidget* TabManager::openFile(const QString &filePath)
{
    QString normalized = QFileInfo(filePath).absoluteFilePath();
    // 检查文件是否已经打开，防止重复
    for (int i = 0; i < count(); ++i) {
        EditorWidget *editor = qobject_cast<EditorWidget*>(widget(i));
        if (editor && editor->currentFilePath().compare(normalized, Qt::CaseInsensitive) == 0) {
            setCurrentIndex(i);
            return editor;
        }
    }

    // 新建标签页并加载文件
    EditorWidget *editor = new EditorWidget(this);
    if (editor->loadFile(normalized)) {   // 传入标准化路径
        int index = addTab(editor, QFileInfo(normalized).fileName());
        setCurrentIndex(index);
        connectEditorSignals(editor);
        emit tabCountChanged(count());
        return editor;
    } else {
        delete editor;
        return nullptr;
    }
}

EditorWidget* TabManager::newFile()
{
    EditorWidget *editor = new EditorWidget(this);
    int index = addTab(editor, "未命名");
    setCurrentIndex(index);
    connectEditorSignals(editor);
    emit tabCountChanged(count());
    return editor;
}

EditorWidget* TabManager::currentEditor() const
{
    return qobject_cast<EditorWidget*>(currentWidget());
}

void TabManager::saveCurrentFile()
{
    EditorWidget *editor = currentEditor();
    if (!editor) return;

    if (editor->currentFilePath().isEmpty()) {
        editor->saveAsFile();
    } else {
        editor->saveFile();
    }
}

void TabManager::closeCurrentTab()
{
    int index = currentIndex();
    if (index >= 0)
        closeTab(index);
}

bool TabManager::closeTab(int index)
{
    EditorWidget *editor = qobject_cast<EditorWidget*>(widget(index));
    if (!editor) return true;

    if (editor->isModified()) {
        QString fileName = editor->currentFilePath().isEmpty()
        ? "未命名"
        : QFileInfo(editor->currentFilePath()).fileName();

        QMessageBox msgBox(this);
        msgBox.setWindowTitle("未保存的更改");
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setText(QString("%1 已被修改。").arg(fileName));
        msgBox.setInformativeText("是否保存更改？");

        // 添加标准按钮
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        // 修改按钮文字（支持快捷键）
        msgBox.button(QMessageBox::Save)->setText("保存(&S)");
        msgBox.button(QMessageBox::Discard)->setText("不保存(&D)");
        msgBox.button(QMessageBox::Cancel)->setText("取消(&C)");
        msgBox.setDefaultButton(QMessageBox::Save);

        // 对话框样式更改
        msgBox.setStyleSheet(
            "QMessageBox {"
            "   min-width: 400px;"
            "   min-height: 200px;"
            "}"
            "QPushButton {"
            "   min-width: 80px;"
            "   padding: 4px;"
            "}"
            );

        int ret = msgBox.exec();   // 返回 QMessageBox::StandardButton 枚举值

        if (ret == QMessageBox::Save) {
            if (editor->currentFilePath().isEmpty())
                editor->saveAsFile();
            else
                editor->saveFile();

            if (editor->isModified())
                return false;   // 用户取消了保存操作
        } else if (ret == QMessageBox::Cancel) {
            return false;       // 用户直接取消关闭
        }
        // 如果点击的是 Discard（不保存），则继续执行关闭操作
    }

    removeTab(index);
    editor->deleteLater();
    emit tabCountChanged(count());
    return true;
}
bool TabManager::closeAllTabs()
{
    // 从最后一个标签开始关闭，避免索引变化带来的问题
    while (count() > 0) {
        // 始终尝试关闭第一个标签
        if (!closeTab(0)) {
            return false; // 用户取消了某个标签的关闭，中断流程
        }
    }
    return true;
}

// 接受
void TabManager::onTabCloseRequested(int index)
{
    // 用户点击标签页上的关闭按钮时触发
    closeTab(index);
}

void TabManager::connectEditorSignals(EditorWidget *editor)
{
    // 为每个新创建的编辑器（EditorWidget）连接必要的信号，以实现标签标题的实时更新
    connect(editor, &EditorWidget::modificationChanged,
            this, [this, editor](bool /*modified*/) {
                updateTabTitle(editor);
            });
    connect(editor, &EditorWidget::fileSaved,
            this, [this, editor](const QString &/*newPath*/) {
                updateTabTitle(editor);
            });
}

void TabManager::onEditorModificationChanged(EditorWidget *editor, bool /*modified*/)
{
    // 响应编辑器的 modificationChanged信号
    updateTabTitle(editor);
}

void TabManager::onEditorFileSaved(EditorWidget *editor, const QString &/*newPath*/)
{
    // 响应编辑器的 fileSaved信号
    updateTabTitle(editor);
}

void TabManager::updateTabTitle(EditorWidget *editor)
{
    // 更新标签页标题
    int idx = indexOf(editor);
    if (idx < 0) return;

    QString title = editor->currentFilePath().isEmpty()
                        ? "未命名"
                        : QFileInfo(editor->currentFilePath()).fileName();

    if (editor->isModified())
        title += " *";

    setTabText(idx, title);
}

EditorWidget* TabManager::findEditorByPath(const QString &filePath) const {
    QString normalized = QFileInfo(filePath).absoluteFilePath();
    for (int i = 0; i < count(); ++i) {
        EditorWidget *editor = qobject_cast<EditorWidget*>(widget(i));
        if (editor && editor->currentFilePath().compare(normalized, Qt::CaseInsensitive) == 0)
            return editor;
    }
    return nullptr;
}

bool TabManager::closeTabByPath(const QString &filePath, bool askSave) {
    QString normalized = QFileInfo(filePath).absoluteFilePath();
    for (int i = 0; i < count(); ++i) {
        EditorWidget *editor = qobject_cast<EditorWidget*>(widget(i));
        if (editor && editor->currentFilePath().compare(normalized, Qt::CaseInsensitive) == 0) {
            if (askSave)
                return closeTab(i);
            else {
                editor->setModified(false);
                return closeTab(i);
            }
        }
    }
    return true;
}

QStringList TabManager::allOpenedFilePaths() const {
    QStringList paths;
    for (int i = 0; i < count(); ++i) {
        EditorWidget *editor = qobject_cast<EditorWidget*>(widget(i));
        if (editor && !editor->currentFilePath().isEmpty()) {
            paths << QDir::fromNativeSeparators(editor->currentFilePath());
        }
    }
    return paths;
}

void TabManager::updateEditorFilePath(const QString &oldPath, const QString &newPath) {
    EditorWidget *editor = findEditorByPath(oldPath);
    if (editor) {
        editor->setFilePath(newPath);
        // 更新标签页标题：移除可能存在的星号，显示新文件名
        int idx = indexOf(editor);
        if (idx != -1) {
            QString title = QFileInfo(newPath).fileName();
            if (editor->isModified())
                title += " *";
            setTabText(idx, title);
            setTabToolTip(idx, newPath);
        }
    }
}
