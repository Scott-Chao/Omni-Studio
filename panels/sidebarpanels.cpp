#include "sidebarpanels.h"
#include "config/configmanager.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QFileInfo>
#include <QColor>

// ── BacklinksPanel ──────────────────────────────────────────────────────────

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

// ── TagPanel ────────────────────────────────────────────────────────────────

TagPanel::TagPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(ConfigManager::instance().tagPanelMinWidth());

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    connect(m_listWidget, &QListWidget::itemClicked, this, &TagPanel::onItemClicked);
    mainLayout->addWidget(m_listWidget);
}

void TagPanel::showAllTags(const QStringList &tags)
{
    m_listWidget->clear();
    m_showingFiles = false;
    m_allTags = tags;

    if (tags.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(tr("未找到标签"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setForeground(ThemeManager::instance().color("tab.inactiveForeground"));
        m_listWidget->addItem(item);
        return;
    }

    for (const QString &tag : tags) {
        QListWidgetItem *item = new QListWidgetItem(QStringLiteral("#") + tag);
        item->setData(Qt::UserRole, tag);
        m_listWidget->addItem(item);
    }
}

void TagPanel::showFilesForTag(const QString &tag, const QStringList &files)
{
    m_listWidget->clear();
    m_showingFiles = true;

    QListWidgetItem *backItem = new QListWidgetItem(QStringLiteral("← ") + tr("返回"));
    backItem->setData(Qt::UserRole, QStringLiteral("__back__"));
    m_listWidget->addItem(backItem);

    if (files.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(tr("无文件包含此标签"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setForeground(ThemeManager::instance().color("tab.inactiveForeground"));
        m_listWidget->addItem(item);
        return;
    }

    for (const QString &path : files) {
        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setToolTip(path);
        item->setData(Qt::UserRole, path);
        m_listWidget->addItem(item);
    }
}

void TagPanel::onItemClicked(QListWidgetItem *item)
{
    QString data = item->data(Qt::UserRole).toString();
    if (data == QStringLiteral("__back__")) {
        showAllTags(m_allTags);
        return;
    }
    if (m_showingFiles) {
        if (!data.isEmpty())
            emit fileClicked(data);
    } else {
        if (!data.isEmpty())
            emit tagClicked(data);
    }
}
