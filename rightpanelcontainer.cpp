#include "rightpanelcontainer.h"
#include "thememanager.h"
#include "historypanel.h"
#include "outlinepanel.h"
#include "tagpanel.h"
#include "backlinkspanel.h"
#include "settingsmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>

RightPanelContainer::RightPanelContainer(SettingsManager *settings, QWidget *parent)
    : QWidget(parent)
{
    // Tab bar
    m_tabBar = new QWidget(this);
    m_tabBar->setFixedHeight(32);

    QHBoxLayout *tabLayout = new QHBoxLayout(m_tabBar);
    tabLayout->setContentsMargins(4, 0, 4, 0);
    tabLayout->setSpacing(0);

    struct TabInfo { QString label; QString icon; };
    const TabInfo tabs[] = {
        {tr("历史"), QStringLiteral(":/icons/history")},
        {tr("大纲"), QStringLiteral(":/icons/outline")},
        {tr("标签"), QStringLiteral(":/icons/tags")},
        {tr("反链"), QStringLiteral(":/icons/backlinks")},
    };

    for (int i = 0; i < 4; ++i) {
        QPushButton *btn = createTabButton(tabs[i].label, QIcon(tabs[i].icon), i);
        m_tabButtons.append(btn);
        tabLayout->addWidget(btn);
    }
    tabLayout->addStretch();

    // Panels
    m_historyPanel = new HistoryPanel(settings, this);
    m_outlinePanel = new OutlinePanel(this);
    m_tagPanel = new TagPanel(this);
    m_backlinksPanel = new BacklinksPanel(this);

    // Stack
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_historyPanel);
    m_stack->addWidget(m_outlinePanel);
    m_stack->addWidget(m_tagPanel);
    m_stack->addWidget(m_backlinksPanel);

    // Layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_tabBar);
    mainLayout->addWidget(m_stack);

    // Default to first tab
    setActivePanel(0);

    // Forward signals from panels
    connect(m_historyPanel, &HistoryPanel::fileClicked,
            this, &RightPanelContainer::fileClicked);
    connect(m_outlinePanel, &OutlinePanel::headingClicked,
            this, &RightPanelContainer::headingClicked);
    connect(m_tagPanel, &TagPanel::fileClicked,
            this, &RightPanelContainer::fileClicked);
    connect(m_tagPanel, &TagPanel::tagClicked,
            this, &RightPanelContainer::tagClicked);
    connect(m_backlinksPanel, &BacklinksPanel::fileClicked,
            this, &RightPanelContainer::fileClicked);

    ThemeManager::watchTheme(this, &RightPanelContainer::refreshStyle);
    refreshStyle();
}

void RightPanelContainer::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    m_tabBar->setStyleSheet(QString("background-color: %1;")
        .arg(tm.color("editorLineNumber.background").name()));

    setStyleSheet(QString(
        "RightPanelContainer { background-color: %1; }"
        "QListWidget { background-color: %1; color: %2; border: none; outline: none; }"
        "QListWidget::item { padding: 4px 8px; }"
        "QListWidget::item:hover { background-color: %3; }")
        .arg(tm.color("sideBar.background").name(),
             tm.color("sideBar.foreground").name(),
             tm.color("list.hoverBackground").name()));

    updateTabStyles(m_currentPanel);
}

QPushButton *RightPanelContainer::createTabButton(const QString &text, const QIcon &icon, int index)
{
    QPushButton *btn = new QPushButton(icon, text, m_tabBar);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(32);
    btn->setIconSize(QSize(14, 14));

    connect(btn, &QPushButton::clicked, this, [this, index]() {
        setActivePanel(index);
    });

    return btn;
}

void RightPanelContainer::setActivePanel(int index)
{
    if (index < 0 || index >= m_tabButtons.size())
        return;

    m_currentPanel = index;
    m_stack->setCurrentIndex(index);
    updateTabStyles(index);
    emit activePanelChanged(index);
}

void RightPanelContainer::updateTabStyles(int activeIndex)
{
    auto &tm = ThemeManager::instance();
    for (int i = 0; i < m_tabButtons.size(); ++i) {
        bool active = (i == activeIndex);
        if (active) {
            m_tabButtons[i]->setStyleSheet(QString(
                "QPushButton {"
                "  background: transparent;"
                "  border: none;"
                "  border-bottom: 2px solid %1;"
                "  color: %2;"
                "  padding: 0 12px;"
                "  font-size: 12px;"
                "}"
            ).arg(tm.color("activityBar.activeBorder").name(),
                  tm.color("tab.activeForeground").name()));
        } else {
            m_tabButtons[i]->setStyleSheet(QString(
                "QPushButton {"
                "  background: transparent;"
                "  border: none;"
                "  border-bottom: 2px solid transparent;"
                "  color: %1;"
                "  padding: 0 12px;"
                "  font-size: 12px;"
                "}"
                "QPushButton:hover {"
                "  color: %2;"
                "  background: %3;"
                "}"
            ).arg(tm.color("tab.inactiveForeground").name(),
                  tm.color("sideBar.foreground").name(),
                  tm.color("list.hoverBackground").name()));
        }
    }
}
