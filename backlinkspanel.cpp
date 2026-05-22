#include "backlinkspanel.h"
#include "configmanager.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QFileInfo>
#include <QColor>

BacklinksPanel::BacklinksPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(ConfigManager::instance().backlinksPanelMinWidth());

    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    connect(m_listWidget, &QListWidget::itemClicked, this, &BacklinksPanel::onItemClicked);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_listWidget);
}

void BacklinksPanel::showBacklinks(const QStringList &sourceFiles)
{
    m_listWidget->clear();
    m_sourceFiles = sourceFiles;

    if (m_sourceFiles.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(tr("无反向链接"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setForeground(ThemeManager::instance().color("tab.inactiveForeground"));
        m_listWidget->addItem(item);
        return;
    }

    for (const QString &path : std::as_const(m_sourceFiles)) {
        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setToolTip(path);
        item->setData(Qt::UserRole, path);
        m_listWidget->addItem(item);
    }
}

void BacklinksPanel::onItemClicked(QListWidgetItem *item)
{
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty())
        emit fileClicked(path);
}
