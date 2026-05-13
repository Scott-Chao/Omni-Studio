#include "rightpanelcontainer.h"
#include "historypanel.h"
#include "outlinepanel.h"
#include "tagpanel.h"
#include "backlinkspanel.h"
#include "settingsmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

RightPanelContainer::RightPanelContainer(SettingsManager *settings, QWidget *parent)
    : QWidget(parent)
{
    // Tab bar
    m_tabBar = new QWidget(this);
    m_tabBar->setFixedHeight(32);
    m_tabBar->setStyleSheet(
        "background-color: #252525;"
    );

    QHBoxLayout *tabLayout = new QHBoxLayout(m_tabBar);
    tabLayout->setContentsMargins(4, 0, 4, 0);
    tabLayout->setSpacing(0);

    struct TabInfo { QString label; };
    const TabInfo tabs[] = {
        {tr("历史")},      // 历史
        {tr("大纲")},      // 大纲
        {tr("标签")},      // 标签
        {tr("反链")},      // 反链
    };

    for (int i = 0; i < 4; ++i) {
        QPushButton *btn = createTabButton(tabs[i].label, i);
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

    // Apply dark theme to list widgets in all sub-panels
    setStyleSheet(
        "RightPanelContainer { background-color: #1E1E1E; }"
        "QListWidget { background-color: #1E1E1E; color: #D4D4D4; border: none; outline: none; }"
        "QListWidget::item { padding: 4px 8px; }"
        "QListWidget::item:hover { background-color: #2a2d2e; }"
    );
}

QPushButton *RightPanelContainer::createTabButton(const QString &text, int index)
{
    QPushButton *btn = new QPushButton(text, m_tabBar);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(32);
    btn->setStyleSheet(
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "  color: #888;"
        "  padding: 0 12px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  color: #ccc;"
        "  background: #2d2d2d;"
        "}"
    );

    connect(btn, &QPushButton::clicked, this, [this, index]() {
        setActivePanel(index);
    });

    return btn;
}

void RightPanelContainer::setActivePanel(int index)
{
    if (index < 0 || index >= m_tabButtons.size())
        return;

    m_stack->setCurrentIndex(index);
    updateTabStyles(index);
}

void RightPanelContainer::updateTabStyles(int activeIndex)
{
    for (int i = 0; i < m_tabButtons.size(); ++i) {
        bool active = (i == activeIndex);
        m_tabButtons[i]->setStyleSheet(
            active ?
            "QPushButton {"
            "  background: transparent;"
            "  border: none;"
            "  border-bottom: 2px solid #0078D4;"
            "  color: #ffffff;"
            "  padding: 0 12px;"
            "  font-size: 12px;"
            "}"
            :
            "QPushButton {"
            "  background: transparent;"
            "  border: none;"
            "  border-bottom: 2px solid transparent;"
            "  color: #888;"
            "  padding: 0 12px;"
            "  font-size: 12px;"
            "}"
            "QPushButton:hover {"
            "  color: #ccc;"
            "  background: #2d2d2d;"
            "}"
        );
    }
}
