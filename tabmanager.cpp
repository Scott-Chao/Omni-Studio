#include "tabmanager.h"
#include <QFileInfo>
#include <QMessageBox>
#include <QAbstractButton>
#include <QDir>

TabManager::TabManager(QWidget *parent)
    : QTabWidget(parent)
{
    setTabBar(new CustomTabBar(this)); // 使用自定义标签栏，提供拖拽边界限制的功能
    setTabsClosable(true);
    connect(this, &QTabWidget::tabCloseRequested, this, &TabManager::onTabCloseRequested);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &TabManager::refreshStyle);
    refreshStyle();
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
    editor->startAutoSave();
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

CustomTabBar::CustomTabBar(QWidget *parent)
    : QTabBar(parent)
{
    setMovable(true); // 允许用户通过拖拽重新排列标签
}

void CustomTabBar::mousePressEvent(QMouseEvent *event)
{
    QTabBar::mousePressEvent(event); // 先调用基类处理，保证基本的点击/拖拽初始化

    // 记录左键按下时的信息，用于后续拖拽边界计算
    if (event->button() == Qt::LeftButton) {
        m_dragIndex = tabAt(event->pos());  // 获取被点击的标签索引
        if (m_dragIndex != -1) {
            m_dragPressPos = event->pos();

            // 获取被点标签的几何矩形
            QRect currentTabRect = tabRect(m_dragIndex);
            m_dragTabWidth = currentTabRect.width(); // 标签宽度
            m_dragOffsetX = m_dragPressPos.x() - currentTabRect.left(); // 鼠标相对于标签左边缘的偏移

            // 尚未开始拖拽，等待 move 事件超过阈值
            m_dragStarted = false;
            m_dragInProgress = false;
        } else {
            m_dragIndex = -1; // 点击空白处，重置
        }
    }
}

void CustomTabBar::mouseMoveEvent(QMouseEvent *event)
{
    // 只有当按下左键且指向一个有效标签时才考虑拖拽
    if (m_dragIndex != -1 && (event->buttons() & Qt::LeftButton)) {
        // 判断是否超过系统拖拽起始距离，防止轻微抖动触发拖拽
        if (!m_dragStarted &&
            (event->pos() - m_dragPressPos).manhattanLength() >= QApplication::startDragDistance())
        {
            m_dragStarted = true;
            m_dragInProgress = true;
        }

        if (m_dragInProgress) {
            // 整个被拖标签（宽度 m_dragTabWidth）始终完整出现在标签栏内部，不能越界，鼠标在标签内的偏移量为 m_dragOffsetX
            int leftBound  = m_dragOffsetX; // 当鼠标 x == m_dragOffsetX 时，标签左边缘刚好对齐栏左侧。
            int rightBound = width() - (m_dragTabWidth - m_dragOffsetX); // 当鼠标 x == width() - (m_dragTabWidth - m_dragOffsetX) 时，标签右边缘刚好对齐栏右侧。

            QPoint clampedPos = event->pos();
            clampedPos.setX(qBound(leftBound, clampedPos.x(), rightBound));

            QPoint globalClamped = mapToGlobal(clampedPos); // 同步修正全局坐标，确保浮动标签渲染位置与钳制后的本地坐标一致

            // 构造一个坐标被钳制过的事件副本，转发给基类继续处理拖拽动画和重排逻辑
            QMouseEvent clampedEvent(
                event->type(),
                clampedPos,
                globalClamped,
                event->button(),
                event->buttons(),
                event->modifiers()
                );
            QTabBar::mouseMoveEvent(&clampedEvent);
            return;
        }
    }
    QTabBar::mouseMoveEvent(event); // 未处于拖拽状态，或拖拽尚未开始，按普通移动处理
}

void CustomTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    QTabBar::mouseReleaseEvent(event); // 调用基类完成标签放置
    // 重置所有拖拽相关状态
    m_dragStarted = false;
    m_dragInProgress = false;
    m_dragIndex = -1;
}

void TabManager::updatePathsAfterMove(const QString &oldBase, const QString &newBase)
{
    for (int i = 0; i < count(); ++i) {
        EditorWidget *editor = qobject_cast<EditorWidget*>(widget(i));
        if (!editor) continue;

        QString currentPath = editor->currentFilePath();
        if (currentPath.isEmpty()) continue;

        QString newPath;
        if (currentPath == oldBase) {
            newPath = newBase; // 精确匹配（移动的是文件本身）
        } else if (currentPath.startsWith(oldBase + "/")) {
            newPath = newBase + currentPath.mid(oldBase.length()); // 文件夹内文件
        } else {
            continue;
        }

        // 更新编辑器内部路径（会触发 filePathChanged 信号，从而更新标签标题等）
        editor->setFilePath(newPath);
    }
}

void TabManager::refreshStyle()
{
    auto &tm = ThemeManager::instance();
    setStyleSheet(QString(
        "QTabWidget::pane {"
        "   border: none;"
        "}"
        "QTabBar {"
        "   background: %1;"
        "}"
        "QTabBar::tab {"
        "   height: 32px;"
        "   margin-right: 2px;"
        "   padding: 4px 12px;"
        "   border-top-left-radius: 8px;"
        "   border-top-right-radius: 8px;"
        "   background: %1;"
        "   color: %2;"
        "   border: none;"
        "}"
        "QTabBar::tab:selected {"
        "   background: %3;"
        "   color: %4;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "   background: %5;"
        "}"
        "QTabBar::close-button {"
        "   image: url(:/icons/close);"
        "   subcontrol-position: right;"
        "   margin: 2px;"
        "}"
        "QTabBar::close-button:hover {"
        "   background: %6;"
        "}"
    )
    .arg(tm.color("tab.inactiveBackground").name())   // %1 bg / inactive bg
    .arg(tm.color("tab.inactiveForeground").name())   // %2 inactive fg
    .arg(tm.color("tab.activeBackground").name())     // %3 selected bg
    .arg(tm.color("tab.activeForeground").name())     // %4 selected fg
    .arg(tm.color("tab.hoverBackground").name())      // %5 hover bg
    .arg(tm.color("titleBar.buttonCloseHover").name())); // %6 close hover bg
}
