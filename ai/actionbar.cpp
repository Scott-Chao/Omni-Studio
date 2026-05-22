#include "actionbar.h"
#include "flowlayout.h"
#include "thememanager.h"

#include <QPushButton>
#include <QFrame>
#include <QVBoxLayout>
#include <QLayoutItem>

ActionBar::ActionBar(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 0);
    mainLayout->setSpacing(0);

    // Button container with FlowLayout for wrapping
    auto *container = new QWidget;
    container->setStyleSheet("background: transparent;");
    m_layout = new FlowLayout(container, 0, 4, 4);
    container->setLayout(m_layout);

    auto *containerWrapper = new QWidget;
    auto *wrapperLayout = new QVBoxLayout(containerWrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->addWidget(container);
    mainLayout->addWidget(containerWrapper);

    // Separator line
    m_separator = new QFrame(this);
    m_separator->setFrameShape(QFrame::HLine);
    mainLayout->addWidget(m_separator);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ActionBar::refreshStyle);
    refreshStyle();
}

void ActionBar::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    setStyleSheet(QStringLiteral(
        "background-color: %1;"
    ).arg(tm.color("aiAssistant.bubbleAssistant").name()));

    m_separator->setStyleSheet(QStringLiteral(
        "QFrame { color: %1; margin: 4px 0 0 0; }"
    ).arg(tm.color("panel.border").name()));
}

void ActionBar::setActions(const QVector<AiAction> &actions)
{
    m_currentActions = actions;
    rebuildButtons();
}

void ActionBar::clearActions()
{
    m_currentActions.clear();
    rebuildButtons();
}

void ActionBar::rebuildButtons()
{
    // Remove all existing button widgets
    QLayoutItem *item;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        if (QWidget *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    auto &tm = ThemeManager::instance();

    for (AiAction action : m_currentActions) {
        const ActionInfo *info = findActionInfo(action);
        if (!info)
            continue;

        auto *btn = new QPushButton(info->label);
        btn->setToolTip(info->tooltip ? QString::fromUtf8(info->tooltip) : QString());
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(26);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 4px;"
            "  padding: 2px 10px;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "  background-color: %4;"
            "  border-color: %5;"
            "  color: %2;"
            "}"
            "QPushButton:pressed {"
            "  background-color: %5;"
            "  color: %6;"
            "}"
        ).arg(tm.color("aiAssistant.actionButtonBackground").name(),
              tm.color("workbench.foreground").name(),
              tm.color("input.border").name(),
              tm.color("aiAssistant.actionButtonHoverBackground").name(),
              tm.color("activityBar.activeBorder").name(),
              tm.color("button.foreground").name()));

        connect(btn, &QPushButton::clicked, this, [this, action]() {
            emit actionTriggered(action);
        });

        m_layout->addWidget(btn);
    }
}
