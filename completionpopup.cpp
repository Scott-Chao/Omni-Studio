#include "completionpopup.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QKeyEvent>

// ============================================================
// CompletionItemDelegate
// ============================================================

void CompletionItemDelegate::paint(QPainter *painter,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    painter->save();

    auto &tm = ThemeManager::instance();

    // Background
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, tm.color("list.activeBackground"));
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect, tm.color("list.hoverBackground"));
    } else {
        painter->fillRect(option.rect, tm.color("sideBar.background"));
    }

    QRect r = option.rect.adjusted(4, 0, -8, 0);
    QFont f = option.font;
    f.setPointSize(qMax(f.pointSize() - 1, 8));

    // Icon
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    if (!icon.isNull()) {
        QRect iconRect(r.left(), r.top() + (r.height() - 16) / 2, 16, 16);
        icon.paint(painter, iconRect);
        r.adjust(22, 0, 0, 0);
    }

    // Name
    QString name = index.data(Qt::DisplayRole).toString();
    painter->setFont(f);
    painter->setPen(tm.color("editor.foreground"));
    QRect nameRect(r.left(), r.top(), r.width() * 3 / 5, r.height());
    QString elidedName = painter->fontMetrics().elidedText(name, Qt::ElideRight, nameRect.width());
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);

    // Type label
    QString typeLabel = index.data(Qt::UserRole).toString();
    if (!typeLabel.isEmpty()) {
        painter->setPen(tm.color("tab.inactiveForeground"));
        QFont typeFont = f;
        typeFont.setPointSize(qMax(f.pointSize() - 2, 7));
        painter->setFont(typeFont);
        QRect typeRect(nameRect.right() + 4, r.top(), r.width() * 2 / 5 - 4, r.height());
        QString elidedType = painter->fontMetrics().elidedText(typeLabel, Qt::ElideRight, typeRect.width());
        painter->drawText(typeRect, Qt::AlignRight | Qt::AlignVCenter, elidedType);
    }

    painter->restore();
}

QSize CompletionItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(0, 24);
}

// ============================================================
// CompletionPopup
// ============================================================

CompletionPopup::CompletionPopup(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_TranslucentBackground, false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // List widget
    m_listWidget = new QListWidget(this);
    m_listWidget->setItemDelegate(new CompletionItemDelegate(this));
    m_listWidget->setFrameShape(QFrame::NoFrame);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listWidget->setCursor(Qt::ArrowCursor);
    m_listWidget->installEventFilter(this);
    layout->addWidget(m_listWidget);

    // Hint bar
    m_hintLabel = new QLabel(tr("Tab 接受    ↑↓ 选择    Esc 关闭"), this);
    m_hintLabel->setFixedHeight(22);
    m_hintLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(m_hintLabel);

    // Item activation (mouse double-click)
    connect(m_listWidget, &QListWidget::itemActivated, this, [this]() {
        CompletionItem item = selectedItem();
        if (!item.name.isEmpty())
            emit itemSelected(item);
        hide();
    });

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &CompletionPopup::refreshStyle);
    refreshStyle();
}

void CompletionPopup::refreshStyle()
{
    auto &tm = ThemeManager::instance();
    m_listWidget->setStyleSheet(QStringLiteral(
        "QListWidget { background: %1; border: 1px solid %2; }"
        "QListWidget::item { border: none; }"
        "QListWidget::item:selected { background: transparent; }"
        "QScrollBar:vertical { width: 10px; background: %1; margin: 0; }"
        "QScrollBar::handle:vertical { background: %3; border-radius: 5px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        )
        .arg(tm.color("sideBar.background").name(),
             tm.color("panel.border").name(),
             tm.color("scrollbarSlider.hoverBackground").name()));
    m_hintLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; font-size: 11px; padding: 2px 8px; }")
        .arg(tm.color("editor.background").name(),
             tm.color("tab.inactiveForeground").name()));
}

void CompletionPopup::showItems(const QList<CompletionItem> &items)
{
    m_items = items;
    m_listWidget->clear();

    for (const CompletionItem &ci : items) {
        auto *item = new QListWidgetItem(ci.name);
        item->setIcon(iconForType(ci.type));
        // UserRole = type label, UserRole+1 = detail (for potential use in tooltip)
        item->setData(Qt::UserRole, ci.type);
        m_listWidget->addItem(item);
    }

    if (m_listWidget->count() > 0)
        m_listWidget->setCurrentRow(0);

    // Size: width 350-450, max 10 visible items
    int visible = qMin(m_listWidget->count(), 10);
    int listHeight = visible * 24 + 2;
    int w = qMin(450, qMax(350, m_listWidget->sizeHintForColumn(0) + 60));
    resize(w, listHeight + m_hintLabel->height());

    // Ensure within screen bounds
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect sg = screen->availableGeometry();
        QRect geo = geometry();
        if (geo.right() > sg.right()) move(sg.right() - geo.width(), geo.y());
        if (geo.left() < sg.left())   move(sg.left(), geo.y());
        if (geo.bottom() > sg.bottom()) move(geo.x(), sg.bottom() - geo.height());
        if (geo.top() < sg.top())     move(geo.x(), sg.top());
    }

    show();
    raise();
}

void CompletionPopup::selectNext()
{
    int row = m_listWidget->currentRow();
    if (row < m_listWidget->count() - 1)
        m_listWidget->setCurrentRow(row + 1);
}

void CompletionPopup::selectPrevious()
{
    int row = m_listWidget->currentRow();
    if (row > 0)
        m_listWidget->setCurrentRow(row - 1);
}

CompletionItem CompletionPopup::selectedItem() const
{
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_items.size())
        return {};
    return m_items.at(row);
}

// ---- Icon generation ----

QIcon CompletionPopup::iconForType(const QString &type) const
{
    auto &tm = ThemeManager::instance();
    QColor color;
    int shape = 0; // 0=rounded rect, 1=square, 2=diamond, 3=circle, 4=triangle

    if (type == QStringLiteral("Method") || type == QStringLiteral("Function")
        || type == QStringLiteral("Constructor")) {
        color = tm.color("syntax.preprocessor"); shape = 1;
    } else if (type == QStringLiteral("Class") || type == QStringLiteral("Struct")
               || type == QStringLiteral("Interface")) {
        color = tm.color("syntax.types"); shape = 0;
    } else if (type == QStringLiteral("Enum") || type == QStringLiteral("EnumMember")) {
        color = tm.color("syntax.numbers"); shape = 4;
    } else if (type == QStringLiteral("Variable") || type == QStringLiteral("Field")
               || type == QStringLiteral("Property")) {
        color = tm.color("syntax.keywords"); shape = 2;
    } else if (type == QStringLiteral("Module") || type == QStringLiteral("Reference")) {
        color = QColor("#DCDCAA"); shape = 0;
    } else if (type == QStringLiteral("Keyword")) {
        color = tm.color("editor.foreground"); shape = 3;
    } else if (type == QStringLiteral("Constant")) {
        color = QColor("#4FC1FF"); shape = 1;
    } else if (type == QStringLiteral("TypeParameter")) {
        color = tm.color("syntax.types"); shape = 2;
    } else {
        color = tm.color("tab.inactiveForeground"); shape = 3;
    }

    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    {
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(color);
        p.setPen(Qt::NoPen);

        switch (shape) {
        case 1: // Square
            p.drawRoundedRect(2, 2, 12, 12, 2, 2);
            break;
        case 2: // Diamond
        {
            QPolygonF diamond;
            diamond << QPointF(8, 2) << QPointF(14, 8) << QPointF(8, 14) << QPointF(2, 8);
            p.drawPolygon(diamond);
            break;
        }
        case 3: // Circle
            p.drawEllipse(3, 3, 10, 10);
            break;
        case 4: // Triangle
        {
            QPolygonF tri;
            tri << QPointF(8, 2) << QPointF(14, 13) << QPointF(2, 13);
            p.drawPolygon(tri);
            break;
        }
        default: // Rounded rect
            p.drawRoundedRect(3, 3, 10, 10, 3, 3);
            break;
        }
    }
    return QIcon(pixmap);
}

// ---- Event handling ----

bool CompletionPopup::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_listWidget && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
