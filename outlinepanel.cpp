#include "outlinepanel.h"
#include "configmanager.h"
#include <QVBoxLayout>
#include <QFileInfo>
#include <QColor>
#include <QFont>

OutlinePanel::OutlinePanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(ConfigManager::instance().outlinePanelMinWidth());

    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    connect(m_listWidget, &QListWidget::itemClicked, this, &OutlinePanel::onItemClicked);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_listWidget);
}

void OutlinePanel::showHeadings(const QVector<HeadingItem> &headings)
{
    m_listWidget->clear();
    m_headings = headings;

    if (m_headings.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(tr("当前文件无标题"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setForeground(QColor("#888"));
        m_listWidget->addItem(item);
        return;
    }

    // Heading level colors: h1 brightest, h6 progressively dimmer
    static const QColor levelColors[] = {
        QColor("#D4D4D4"), // h1
        QColor("#B0B0B0"), // h2
        QColor("#9A9A9A"), // h3
        QColor("#808080"), // h4
        QColor("#6A6A6A"), // h5
        QColor("#585858")  // h6
    };

    for (const HeadingItem &h : std::as_const(m_headings)) {
        int indent = (h.level - 1) * 8;

        QString display;
        display.fill(QLatin1Char(' '), indent);
        display += QString::number(h.lineNumber) + QStringLiteral("  ") + h.text;

        QListWidgetItem *item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, h.lineNumber);
        item->setData(Qt::UserRole + 1, h.text);
        item->setToolTip(QStringLiteral("L%1  %2").arg(h.lineNumber).arg(h.text));

        int colorIdx = qBound(0, h.level - 1, 5);
        item->setForeground(levelColors[colorIdx]);

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

void OutlinePanel::onItemClicked(QListWidgetItem *item)
{
    int line = item->data(Qt::UserRole).toInt();
    if (line > 0) {
        QString headingText = item->data(Qt::UserRole + 1).toString();
        emit headingClicked(line, headingText);
    }
}
