#include "aihistorylistwidget.h"
#include "thememanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QMenu>
#include <QDate>
#include <QFile>

// ═══════════════════════════════════════════════════════════════════════

AiHistoryListWidget::AiHistoryListWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Search bar ──
    auto *searchBar = new QWidget(this);
    auto *searchLayout = new QHBoxLayout(searchBar);
    searchLayout->setContentsMargins(6, 6, 6, 6);
    searchLayout->setSpacing(4);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("搜索历史对话..."));
    m_searchEdit->setClearButtonEnabled(true);

    searchLayout->addWidget(m_searchEdit);

    // ── List widget ──
    m_listWidget = new QListWidget(this);
    m_listWidget->setFrameShape(QFrame::NoFrame);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listWidget->setSpacing(1);

    mainLayout->addWidget(searchBar);
    mainLayout->addWidget(m_listWidget, 1);

    // ── Connections ──
    connect(m_searchEdit, &QLineEdit::textChanged, this, &AiHistoryListWidget::onSearchChanged);
    connect(m_listWidget, &QListWidget::itemClicked, this, &AiHistoryListWidget::onItemClicked);
    connect(m_listWidget, &QListWidget::customContextMenuRequested, this, &AiHistoryListWidget::onContextMenu);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &AiHistoryListWidget::refreshStyle);
    refreshStyle();
}

// ── Public API ───────────────────────────────────────────────────────

void AiHistoryListWidget::setConversations(const QList<AiConversation> &convs)
{
    m_allConversations = convs;
    rebuildList();
}

void AiHistoryListWidget::setActiveConversationId(const QString &id)
{
    m_activeConvId = id;
    rebuildList();
}

// ── Slots ────────────────────────────────────────────────────────────

void AiHistoryListWidget::onSearchChanged(const QString &text)
{
    rebuildList();
}

void AiHistoryListWidget::onItemClicked(QListWidgetItem *item)
{
    if (!item || !(item->flags() & Qt::ItemIsSelectable))
        return;

    const QString convId = item->data(Qt::UserRole).toString();
    if (!convId.isEmpty())
        emit conversationSelected(convId);
}

void AiHistoryListWidget::onContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_listWidget->itemAt(pos);
    if (!item || !(item->flags() & Qt::ItemIsSelectable))
        return;

    const QString convId = item->data(Qt::UserRole).toString();
    if (convId.isEmpty())
        return;

    QMenu menu(this);
    menu.addAction(tr("重命名"), this, [this, convId]() { emit renameRequested(convId); });
    menu.addAction(tr("删除"), this, [this, convId]() { emit deleteRequested(convId); });
    menu.addSeparator();
    menu.addAction(tr("导出 Markdown"), this, [this, convId]() { emit exportRequested(convId); });

    menu.exec(m_listWidget->mapToGlobal(pos));
}

void AiHistoryListWidget::refreshStyle()
{
    auto &tm = ThemeManager::instance();
    const QString bg = tm.color("editor.background").name();
    const QString fg = tm.color("workbench.foreground").name();
    const QString dimFg = tm.color("editorLineNumber.foreground").name();
    const QString border = tm.color("panel.border").name();
    const QString inputBg = tm.color("input.background").name();

    m_searchEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 4px;"
        "  padding: 4px 6px;"
        "  font-size: 12px;"
        "}"
    ).arg(inputBg, fg, border));

    m_listWidget->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  font-size: 12px;"
        "}"
        "QListWidget::item {"
        "  padding: 8px 10px;"
        "  border-bottom: 1px solid %3;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: %4;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: %5;"
        "}"
    ).arg(bg, fg, border,
          tm.color("list.hoverBackground").name(),
          tm.color("list.activeSelectionBackground").name()));
}

// ── Internal ─────────────────────────────────────────────────────────

void AiHistoryListWidget::rebuildList()
{
    m_listWidget->clear();

    const QString filter = m_searchEdit->text().trimmed().toLower();
    const QDate today = QDate::currentDate();
    const QDate yesterday = today.addDays(-1);

    // Filter by search text
    QList<AiConversation> filtered;
    for (const auto &conv : m_allConversations) {
        if (filter.isEmpty() || conv.title.toLower().contains(filter))
            filtered.append(conv);
    }

    // Sort by updatedAt descending (already sorted from AiHistoryManager,
    // but re-sort to be safe since date grouping depends on it)
    std::sort(filtered.begin(), filtered.end(), [](const AiConversation &a, const AiConversation &b) {
        return a.updatedAt > b.updatedAt;
    });

    // Group by date and build list
    QString currentGroup;
    for (const auto &conv : filtered) {
        const QString group = dateGroupLabel(conv.updatedAt);
        if (group != currentGroup) {
            currentGroup = group;

            auto *headerItem = new QListWidgetItem(group, m_listWidget);
            headerItem->setFlags(headerItem->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
            QFont headerFont = headerItem->font();
            headerFont.setPointSize(10);
            headerFont.setBold(true);
            headerItem->setFont(headerFont);

            auto &tm = ThemeManager::instance();
            headerItem->setForeground(tm.color("editorLineNumber.foreground"));
        }

        // Create item widget with green dot, title, subtitle
        auto *itemWidget = new QWidget;
        auto *hLayout = new QHBoxLayout(itemWidget);
        hLayout->setContentsMargins(4, 4, 4, 4);
        hLayout->setSpacing(8);

        // Green dot for active conversation
        auto *dot = new QLabel;
        dot->setFixedSize(8, 8);
        if (conv.id == m_activeConvId) {
            dot->setStyleSheet(QStringLiteral(
                "background-color: %1; border-radius: 4px;"
            ).arg(ThemeManager::instance().color("debugIcon.startForeground").name()));
        } else {
            dot->setStyleSheet(QStringLiteral("background: transparent;"));
        }

        // Text layout
        auto *vLayout = new QVBoxLayout;
        vLayout->setSpacing(2);

        auto *titleLabel = new QLabel(conv.title);
        QFont titleFont = titleLabel->font();
        titleFont.setPointSize(11);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        titleLabel->setStyleSheet(QStringLiteral(
            "color: %1;"
        ).arg(ThemeManager::instance().color("workbench.foreground").name()));

        auto *subtitleLabel = new QLabel(
            tr("消息数: %1 · %2")
            .arg(conv.messageCount)
            .arg(conv.updatedAt.toString(QStringLiteral("yyyy-MM-dd hh:mm"))));
        QFont subFont = subtitleLabel->font();
        subFont.setPointSize(9);
        subtitleLabel->setFont(subFont);
        subtitleLabel->setStyleSheet(QStringLiteral(
            "color: %1;"
        ).arg(ThemeManager::instance().color("editorLineNumber.foreground").name()));

        vLayout->addWidget(titleLabel);
        vLayout->addWidget(subtitleLabel);

        hLayout->addWidget(dot);
        hLayout->addLayout(vLayout, 1);

        auto *listItem = new QListWidgetItem(m_listWidget);
        listItem->setData(Qt::UserRole, conv.id);
        listItem->setSizeHint(itemWidget->sizeHint());
        m_listWidget->setItemWidget(listItem, itemWidget);
    }

    if (filtered.isEmpty()) {
        auto *emptyItem = new QListWidgetItem(tr("暂无历史对话"), m_listWidget);
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        emptyItem->setForeground(ThemeManager::instance().color("editorLineNumber.foreground"));
        emptyItem->setTextAlignment(Qt::AlignCenter);
    }
}

QString AiHistoryListWidget::dateGroupLabel(const QDateTime &dt) const
{
    const QDate today = QDate::currentDate();
    const QDate date = dt.date();

    if (date == today)
        return QStringLiteral("— ") + tr("今天") + QStringLiteral(" —");
    if (date == today.addDays(-1))
        return QStringLiteral("— ") + tr("昨天") + QStringLiteral(" —");
    return QStringLiteral("— ") + tr("更早") + QStringLiteral(" —");
}
