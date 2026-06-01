#include "outlinepanel.h"
#include "config/configmanager.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QApplication>

// Custom delegate that elides long outline entry text with "…"
// near the right edge instead of hard-clipping.
class ElideDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Use viewport width (visible area), not item rect width (may extend beyond viewport)
        int vpWidth = opt.widget ? opt.widget->width() : opt.rect.width();
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
        // Clip to viewport bounds: right edge of text must not exceed viewport
        int textRight = qMin(textRect.right(), vpWidth - 2);
        int avail = qMax(textRight - textRect.left(), 40);
        opt.text = opt.fontMetrics.elidedText(opt.text, Qt::ElideRight, avail);

        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    }
};

static QColor levelColor(int level, const QColor &fg, const QColor &bg)
{
    // Blend foreground toward background for deeper heading levels
    static const float ratios[] = { 1.00f, 0.83f, 0.67f, 0.53f, 0.42f, 0.33f };
    int idx = qBound(0, level - 1, 5);
    float r = ratios[idx];
    return QColor(
        qRound(fg.red()   * r + bg.red()   * (1.0f - r)),
        qRound(fg.green() * r + bg.green() * (1.0f - r)),
        qRound(fg.blue()  * r + bg.blue()  * (1.0f - r)));
}

OutlinePanel::OutlinePanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(ConfigManager::instance().outlinePanelMinWidth());

    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setItemDelegate(new ElideDelegate(m_listWidget));
    connect(m_listWidget, &QListWidget::itemClicked, this, &OutlinePanel::onItemClicked);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_listWidget);

    ThemeManager::watchTheme(this, &OutlinePanel::refreshStyle);
}

void OutlinePanel::showHeadings(const QVector<HeadingItem> &headings)
{
    m_listWidget->clear();
    m_headings = headings;

    if (m_headings.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(tr("当前文件无标题"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        auto &tm = ThemeManager::instance();
        item->setForeground(levelColor(4, tm.color("sideBar.foreground"),
                                          tm.color("sideBar.background")));
        m_listWidget->addItem(item);
        return;
    }

    // Compute minimum heading level for relative indentation
    int minLevel = 6;
    for (const HeadingItem &h : std::as_const(m_headings))
        if (h.level < minLevel) minLevel = h.level;

    auto &tm = ThemeManager::instance();
    QColor fg = tm.color("sideBar.foreground");
    QColor bg = tm.color("sideBar.background");

    for (const HeadingItem &h : std::as_const(m_headings)) {
        int indent = (h.level - minLevel) * 8;

        QString display;
        display.fill(QLatin1Char(' '), indent);
        display += QString::number(h.lineNumber) + QStringLiteral("  ") + h.text;

        QListWidgetItem *item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, h.lineNumber);
        item->setData(Qt::UserRole + 1, h.text);
        item->setToolTip(QStringLiteral("L%1  %2").arg(h.lineNumber).arg(h.text));

        item->setForeground(levelColor(h.level, fg, bg));

        QFont f = m_listWidget->font();
        f.setBold(h.level <= 2);
        item->setFont(f);

        m_listWidget->addItem(item);
    }
}

void OutlinePanel::clear()
{
    m_listWidget->clear();
    m_headings.clear();
}

void OutlinePanel::refreshStyle()
{
    if (!m_headings.isEmpty())
        showHeadings(m_headings);
}

void OutlinePanel::onItemClicked(QListWidgetItem *item)
{
    int line = item->data(Qt::UserRole).toInt();
    if (line > 0) {
        QString headingText = item->data(Qt::UserRole + 1).toString();
        emit headingClicked(line, headingText);
    }
}
