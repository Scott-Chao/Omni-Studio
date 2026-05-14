#include "tagpanel.h"
#include "configmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QColor>

TagPanel::TagPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(ConfigManager::instance().tagPanelMinWidth());

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header row: back button + title
    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(4, 4, 4, 4);

    m_backButton = new QPushButton(QStringLiteral("← ") + tr("返回"), this);
    m_backButton->setFlat(true);
    m_backButton->setStyleSheet(
        "QPushButton { color: #569CD6; text-align: left; padding: 2px 6px;"
        "  background: transparent; border: none; }"
        "QPushButton:hover { color: #75B8F0; }");
    m_backButton->hide();
    connect(m_backButton, &QPushButton::clicked, this, &TagPanel::onBackClicked);
    headerLayout->addWidget(m_backButton);

    m_titleLabel = new QLabel(tr("标签"), this);
    m_titleLabel->setStyleSheet("color: #D4D4D4; font-weight: bold; padding: 4px;");
    headerLayout->addWidget(m_titleLabel, 1);

    mainLayout->addLayout(headerLayout);

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
    m_backButton->hide();
    m_titleLabel->setText(tr("所有标签"));

    if (tags.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(tr("未找到标签"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setForeground(QColor("#888"));
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
    m_backButton->show();
    m_titleLabel->setText(QStringLiteral("#") + tag);

    if (files.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(tr("无文件包含此标签"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setForeground(QColor("#888"));
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
    if (m_showingFiles) {
        QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty())
            emit fileClicked(path);
    } else {
        QString tag = item->data(Qt::UserRole).toString();
        if (!tag.isEmpty())
            emit tagClicked(tag);
    }
}

void TagPanel::onBackClicked()
{
    showAllTags(m_allTags);
}
