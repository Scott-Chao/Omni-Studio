#include "tabmanager.h"
#include "panels/openjudgewidget.h"
#include "config/settingsmanager.h"
#include "thememanager.h"
#include <QFileInfo>
#include <QMessageBox>
#include <QAbstractButton>
#include <QDir>
#include <QStyleOptionTab>
#include <QStyle>
#include <QPainter>

TabManager::TabManager(QWidget *parent)
    : QTabWidget(parent)
{
    setTabBar(new CustomTabBar(this)); // 使用自定义标签栏，提供拖拽边界限制的功能
    setTabsClosable(true);
    connect(this, &QTabWidget::tabCloseRequested, this, &TabManager::onTabCloseRequested);
}

void TabManager::paintEvent(QPaintEvent *)
{
    // 标签页栏所在矩形（标签栏顶部 → pane 顶部）填充为未激活标签背景色
    QPainter p(this);
    int tabBarBottom = tabBar()->geometry().bottom();
    QRect topArea(0, 0, width(), tabBarBottom);
    p.fillRect(topArea, ThemeManager::instance().color("tab.inactiveBackground"));
}

EditorWidget* TabManager::openFile(const QString &filePath)
{
    QString normalized = QFileInfo(filePath).absoluteFilePath();
    // 检查文件是否已经打开，防止重复
    for (int i = 0; i < count(); ++i) {
        EditorWidget *editor = qobject_cast<EditorWidget*>(widget(i));
        if (editor && editor->currentFilePath().compare(normalized, Qt::CaseInsensitive) == 0) {
            setCurrentIndex(i);
            // 如果文件当前在预览标签页中，自动提升为永久
            if (editor == m_previewEditor) {
                promotePreviewToPermanent();
            }
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

EditorWidget* TabManager::openPreview(const QString &filePath)
{
    QString normalized = QFileInfo(filePath).absoluteFilePath();

    // 情况 1：文件已作为永久标签页打开 → 切换到该标签页
    for (int i = 0; i < count(); ++i) {
        EditorWidget *editor = qobject_cast<EditorWidget*>(widget(i));
        if (editor && editor != m_previewEditor
            && editor->currentFilePath().compare(normalized, Qt::CaseInsensitive) == 0) {
            setCurrentWidget(editor);
            return editor;
        }
    }

    // 情况 2：文件已在当前预览标签页中 → 仅切换
    if (m_previewEditor
        && m_previewEditor->currentFilePath().compare(normalized, Qt::CaseInsensitive) == 0) {
        setCurrentWidget(m_previewEditor);
        return m_previewEditor;
    }

    // 情况 3：预览标签页存在但文件不同 → 替换预览内容
    if (m_previewEditor) {
        m_previewEditor->loadFile(normalized);
        updateTabTitle(m_previewEditor);
        setCurrentWidget(m_previewEditor);
        return m_previewEditor;
    }

    // 情况 4：无预览标签页 → 新建
    EditorWidget *editor = new EditorWidget(this);
    if (!editor->loadFile(normalized)) {
        delete editor;
        return nullptr;
    }
    int index = addTab(editor, QFileInfo(normalized).fileName());
    setCurrentIndex(index);
    m_previewEditor = editor;
    connectEditorSignals(editor);
    updateTabTitle(editor); // 应用预览样式
    emit tabCountChanged(count());
    return editor;
}

void TabManager::promotePreviewToPermanent()
{
    if (!m_previewEditor)
        return;
    EditorWidget *editor = m_previewEditor;
    m_previewEditor = nullptr;
    updateTabTitle(editor);
    emit previewTabPromoted(editor);
}

bool TabManager::isPreviewEditor(EditorWidget* editor) const
{
    return editor && editor == m_previewEditor;
}

EditorWidget* TabManager::previewEditor() const
{
    return m_previewEditor;
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
    if (!editor) {
        // Non-EditorWidget tab (e.g. OpenJudgeWidget): remove directly
        QWidget *w = widget(index);
        removeTab(index);
        if (w) w->deleteLater();
        emit tabCountChanged(count());
        return true;
    }

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
        auto &tm = ThemeManager::instance();
        msgBox.setStyleSheet(QStringLiteral(
            "QMessageBox {"
            "   min-width: 400px;"
            "   min-height: 200px;"
            "}"
            "QPushButton {"
            "   min-width: 80px;"
            "   padding: 6px 16px;"
            "   background: %1;"
            "   color: %2;"
            "   border: 1px solid %3;"
            "   border-radius: 3px;"
            "}"
            "QPushButton:hover {"
            "   background: %4;"
            "}"
            "QPushButton:default {"
            "   background: %5;"
            "   color: %2;"
            "   border: 1px solid %5;"
            "}"
            "QPushButton:default:hover {"
            "   background: %6;"
            "}"
            ).arg(tm.color("button.background").name(),
                  tm.color("button.foreground").name(),
                  tm.color("input.border").name(),
                  tm.color("button.hoverBackground").name(),
                  QColor(tm.color("badge.background").red(),
                         tm.color("badge.background").green(),
                         tm.color("badge.background").blue(), 45).name(QColor::HexArgb),
                  QColor(tm.color("badge.background").red(),
                         tm.color("badge.background").green(),
                         tm.color("badge.background").blue(), 80).name(QColor::HexArgb)));

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

    // 如果关闭的是预览标签页，先清理指针
    if (editor == m_previewEditor) {
        m_previewEditor = nullptr;
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

    // 预览标签页自动提升：内容被修改时自动变为永久标签页
    if (editor == m_previewEditor) {
        connect(editor, &EditorWidget::modificationChanged,
                this, [this](bool modified) {
                    if (modified && m_previewEditor) {
                        promotePreviewToPermanent();
                    }
                });
    }
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
    // 预览标签页的斜体样式由 CustomTabBar::paintEvent 处理
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

// DragOverlay 实现：z-order 高于所有关闭按钮 widget
DragOverlay::DragOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void DragOverlay::setPixmap(const QPixmap &pm)
{
    m_pixmap = pm;
    setFixedSize(pm.size());
    update();
}

void DragOverlay::paintEvent(QPaintEvent *)
{
    if (!m_pixmap.isNull()) {
        QPainter p(this);
        p.drawPixmap(0, 0, m_pixmap);
    }
}

CustomTabBar::CustomTabBar(QWidget *parent)
    : QTabBar(parent)
{
    setMovable(true);
    setExpanding(false);
    setElideMode(Qt::ElideNone);
}

void CustomTabBar::setEqualWidth(bool enabled)
{
    if (m_equalWidth == enabled) return;
    m_equalWidth = enabled;
    setExpanding(enabled);
    setElideMode(enabled ? Qt::ElideRight : Qt::ElideNone);
    updateGeometry();
    update();
}

void CustomTabBar::mousePressEvent(QMouseEvent *event)
{
    QTabBar::mousePressEvent(event);

    if (event->button() == Qt::LeftButton) {
        m_dragIndex = tabAt(event->pos());
        if (m_dragIndex != -1) {
            m_dragPressPos = event->pos();

            QRect currentTabRect = tabRect(m_dragIndex);
            m_dragTabWidth = currentTabRect.width();
            m_dragOffsetX = m_dragPressPos.x() - currentTabRect.left();

            const TabManager *tm = qobject_cast<const TabManager*>(parent());
            m_dragEditor = tm ? qobject_cast<EditorWidget*>(tm->widget(m_dragIndex)) : nullptr;

            m_dragStarted = false;
            m_dragInProgress = false;
        } else {
            m_dragIndex = -1;
            m_dragEditor = nullptr;
        }
    }
}

void CustomTabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragIndex != -1 && (event->buttons() & Qt::LeftButton)) {
        if (!m_dragStarted &&
            (event->pos() - m_dragPressPos).manhattanLength() >= QApplication::startDragDistance())
        {
            m_dragStarted = true;
            m_dragInProgress = true;
            m_dragOverlay = new DragOverlay(this);
            m_dragOverlay->show();
            m_dragOverlay->raise();
        }

        if (m_dragInProgress) {
            int leftBound  = m_dragOffsetX;
            int rightBound = width() - (m_dragTabWidth - m_dragOffsetX);
            QPoint clampedPos = event->pos();
            clampedPos.setX(qBound(leftBound, clampedPos.x(), rightBound));
            m_dragCurrentPos = clampedPos;

            int dragCenterX = clampedPos.x() - m_dragOffsetX + m_dragTabWidth / 2;
            int targetIdx = tabAt(QPoint(dragCenterX, clampedPos.y()));

            if (targetIdx >= 0 &&
                qAbs(dragCenterX - m_lastMoveCenterX) >= (m_equalWidth ? m_dragTabWidth / 3 : m_dragTabWidth / 4)) {
                const TabManager *tm = qobject_cast<const TabManager*>(parent());
                if (tm && m_dragEditor) {
                    int currentIdx = -1;
                    for (int i = 0; i < count(); ++i) {
                        if (qobject_cast<EditorWidget*>(tm->widget(i)) == m_dragEditor) {
                            currentIdx = i;
                            break;
                        }
                    }
                    if (currentIdx >= 0 && currentIdx != targetIdx) {
                        bool pastThreshold = false;
                        if (m_equalWidth) {
                            // 等宽模式：滞回，拖拽中心必须完全退出当前标签 rect 才允许交换
                            QRect curR = tabRect(currentIdx);
                            if (targetIdx > currentIdx)
                                pastThreshold = (dragCenterX > curR.right());
                            else
                                pastThreshold = (dragCenterX < curR.left());
                        } else {
                            // 非等宽模式：拖拽标签的边界超过目标标签中心时交换
                            int targetCenterX = tabRect(targetIdx).center().x();
                            if (targetIdx > currentIdx)
                                pastThreshold = (dragCenterX + m_dragTabWidth / 2 > targetCenterX);
                            else
                                pastThreshold = (dragCenterX - m_dragTabWidth / 2 < targetCenterX);
                        }

                        if (pastThreshold) {
                            moveTab(currentIdx, targetIdx);
                            m_lastMoveCenterX = dragCenterX;
                        }
                    }
                }
            }

            update();
            return;
        }
    }
    QTabBar::mouseMoveEvent(event);
}

void CustomTabBar::initStyleOption(QStyleOptionTab *option, int tabIndex) const
{
    QTabBar::initStyleOption(option, tabIndex);
    const TabManager *tm = qobject_cast<const TabManager*>(parent());
    if (tm) {
        EditorWidget *editor = qobject_cast<EditorWidget*>(tm->widget(tabIndex));
        if (editor && tm->isPreviewEditor(editor)) {
            QFont f = font();
            f.setItalic(true);
            option->fontMetrics = QFontMetrics(f);
        }
    }
}

void CustomTabBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    const TabManager *tm = qobject_cast<const TabManager*>(parent());
    if (!tm) {
        QTabBar::paintEvent(event);
        return;
    }

    QPainter painter(this);
    painter.fillRect(rect(), ThemeManager::instance().color("tab.inactiveBackground"));
    QFont normalFont = font();
    QFont italicFont = normalFont;
    italicFont.setItalic(true);

    int draggedIdx = -1;
    bool draggedIsPreview = false;
    if (m_dragInProgress && m_dragEditor) {
        for (int i = 0; i < count(); ++i) {
            if (qobject_cast<EditorWidget*>(tm->widget(i)) == m_dragEditor) {
                draggedIdx = i;
                draggedIsPreview = tm->isPreviewEditor(m_dragEditor);
                break;
            }
        }
    }

    // 绘制所有标签，被拖标签在原位以低透明度 ghost 形式显示
    for (int i = 0; i < count(); ++i) {
        QStyleOptionTab opt;
        initStyleOption(&opt, i);

        EditorWidget *editor = qobject_cast<EditorWidget*>(tm->widget(i));
        if (editor && tm->isPreviewEditor(editor))
            painter.setFont(italicFont);
        else
            painter.setFont(normalFont);

        if (i == draggedIdx)
            painter.setOpacity(0.3);
        else
            painter.setOpacity(1.0);

        if (m_equalWidth) {
            // 等宽模式：先绘制无文字的标签背景，再左对齐绘制文字
            QString tabText = opt.text;
            opt.text = QString();
            style()->drawControl(QStyle::CE_TabBarTab, &opt, &painter, this);
            opt.text = tabText;
            QRect r = tabRect(i);
            r.setLeft(r.left() + 8);
            QWidget *cb = tabButton(i, QTabBar::RightSide);
            if (!cb) cb = tabButton(i, QTabBar::LeftSide);
            if (cb) r.setRight(cb->pos().x() - 4);
            QString elided = fontMetrics().elidedText(tabText, Qt::ElideRight, r.width());
            painter.setPen(ThemeManager::instance().color(
                (opt.state & QStyle::State_Selected) ? "tab.activeForeground" : "tab.inactiveForeground"));
            painter.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, elided);
        } else if (editor && tm->isPreviewEditor(editor)) {
            // 非等宽模式斜体预览标签：手动绘制文字以利用标签页完整宽度，避免被 style 裁切
            QString tabText = opt.text;
            opt.text = QString();
            style()->drawControl(QStyle::CE_TabBarTab, &opt, &painter, this);
            opt.text = tabText;
            QRect r = tabRect(i);
            r.setLeft(r.left() + 8);
            QWidget *cb = tabButton(i, QTabBar::RightSide);
            if (!cb) cb = tabButton(i, QTabBar::LeftSide);
            if (cb) r.setRight(cb->pos().x() - 4);
            painter.setPen(ThemeManager::instance().color(
                (opt.state & QStyle::State_Selected) ? "tab.activeForeground" : "tab.inactiveForeground"));
            painter.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, tabText);
        } else {
            style()->drawControl(QStyle::CE_TabBarTab, &opt, &painter, this);
        }
    }
    painter.setOpacity(1.0);

    // 相邻未激活标签之间绘制分割线（不完全贯穿，上下留空）
    for (int i = 0; i < count() - 1; ++i) {
        QStyleOptionTab optA, optB;
        initStyleOption(&optA, i);
        initStyleOption(&optB, i + 1);
        if (!(optA.state & QStyle::State_Selected) && !(optB.state & QStyle::State_Selected)) {
            QRect ra = tabRect(i);
            QRect rb = tabRect(i + 1);
            int sepX = (ra.right() + rb.left()) / 2;
            int margin = 6;
            painter.setPen(QPen(ThemeManager::instance().color("tab.inactiveSeparator"), 1));
            painter.drawLine(sepX, ra.top() + margin, sepX, ra.bottom() - margin);
        }
    }

    // 激活标签底部蓝色指示线
    for (int i = 0; i < count(); ++i) {
        QStyleOptionTab opt;
        initStyleOption(&opt, i);
        if (opt.state & QStyle::State_Selected) {
            QRect tr = tabRect(i);
            painter.setPen(QPen(ThemeManager::instance().color("tab.activeIndicator"), 2));
            painter.drawLine(tr.left(), tr.bottom(), tr.right(), tr.bottom());
            break;
        }
    }

    // 拖拽中：将标签+关闭按钮合成到 pixmap，置于 overlay（高于所有 widget）
    if (draggedIdx >= 0 && m_dragOverlay) {
        QRect tabR = tabRect(draggedIdx);
        QPixmap pm(m_dragTabWidth, tabR.height());
        pm.fill(Qt::transparent);
        {
            QPainter pp(&pm);
            QStyleOptionTab opt;
            initStyleOption(&opt, draggedIdx);
            opt.rect = QRect(0, 0, m_dragTabWidth, tabR.height());
            opt.position = QStyleOptionTab::Moving;
            pp.setFont(draggedIsPreview ? italicFont : normalFont);
            if (m_equalWidth || draggedIsPreview) {
                QString overlayText = opt.text;
                opt.text = QString();
                style()->drawControl(QStyle::CE_TabBarTab, &opt, &pp, this);
                opt.text = overlayText;
                QRect overlayR = opt.rect;
                overlayR.setLeft(overlayR.left() + 8);
                QWidget *cb2 = tabButton(draggedIdx, QTabBar::RightSide);
                if (!cb2) cb2 = tabButton(draggedIdx, QTabBar::LeftSide);
                if (cb2) overlayR.setRight(cb2->pos().x() - tabR.left() - 4);
                else overlayR.setRight(overlayR.right() - 8);
                if (m_equalWidth) {
                    QString overlayElided = pp.fontMetrics().elidedText(overlayText, Qt::ElideRight,
                                                                        overlayR.width());
                    pp.setPen(ThemeManager::instance().color(
                        (opt.state & QStyle::State_Selected) ? "tab.activeForeground" : "tab.inactiveForeground"));
                    pp.drawText(overlayR, Qt::AlignLeft | Qt::AlignVCenter, overlayElided);
                } else {
                    pp.setPen(ThemeManager::instance().color(
                        (opt.state & QStyle::State_Selected) ? "tab.activeForeground" : "tab.inactiveForeground"));
                    pp.drawText(overlayR, Qt::AlignLeft | Qt::AlignVCenter, overlayText);
                }
            } else {
                style()->drawControl(QStyle::CE_TabBarTab, &opt, &pp, this);
            }

            // 把关闭按钮 widget 渲染到 pixmap 上
            QWidget *btn = tabButton(draggedIdx, QTabBar::RightSide);
            if (!btn) btn = tabButton(draggedIdx, QTabBar::LeftSide);
            if (btn) {
                QPoint relPos = btn->pos() - tabR.topLeft();
                btn->render(&pp, relPos, QRegion(), QWidget::DrawChildren);
            }
            // 激活标签底部蓝色指示线
            if (opt.state & QStyle::State_Selected) {
                pp.setPen(QPen(ThemeManager::instance().color("tab.activeIndicator"), 2));
                pp.drawLine(0, tabR.height() - 1, m_dragTabWidth, tabR.height() - 1);
            }
        }
        m_dragOverlay->setPixmap(pm);
        m_dragOverlay->move(m_dragCurrentPos.x() - m_dragOffsetX, tabR.top());
        m_dragOverlay->raise();
    }
}

QSize CustomTabBar::tabSizeHint(int index) const
{
    QSize size = QTabBar::tabSizeHint(index);
    if (m_equalWidth)
        size.setWidth(140);
    return size;
}

void CustomTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    delete m_dragOverlay;
    m_dragOverlay = nullptr;

    QTabBar::mouseReleaseEvent(event);
    m_dragStarted = false;
    m_dragInProgress = false;
    m_dragIndex = -1;
    m_dragEditor = nullptr;
    m_lastMoveCenterX = 0;

    update();
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

// ======================================================================
// OpenJudge integration
// ======================================================================

void TabManager::openOpenJudge(SettingsManager *settings)
{
    // Check if OpenJudge tab already exists
    for (int i = 0; i < count(); ++i) {
        OpenJudgeWidget *oj = qobject_cast<OpenJudgeWidget*>(widget(i));
        if (oj) {
            setCurrentIndex(i);
            return;
        }
    }

    // Create new OpenJudge tab
    OpenJudgeWidget *ojWidget = new OpenJudgeWidget(settings, this);
    addTab(ojWidget, QStringLiteral("OpenJudge"));
    setCurrentWidget(ojWidget);
    emit tabCountChanged(count());
}

OpenJudgeWidget* TabManager::findOpenJudgeWidget() const
{
    for (int i = 0; i < count(); ++i) {
        OpenJudgeWidget *oj = qobject_cast<OpenJudgeWidget*>(widget(i));
        if (oj) return oj;
    }
    return nullptr;
}

