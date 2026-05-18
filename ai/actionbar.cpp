#include "actionbar.h"
#include "flowlayout.h"

#include <QPushButton>
#include <QFrame>
#include <QVBoxLayout>
#include <QLayoutItem>

ActionBar::ActionBar(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("background-color: #2d2d2d;");

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
    m_separator->setStyleSheet(
        "QFrame { color: #3c3c3c; margin: 4px 0 0 0; }"
    );
    mainLayout->addWidget(m_separator);
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

    for (AiAction action : m_currentActions) {
        const ActionInfo *info = findActionInfo(action);
        if (!info)
            continue;

        auto *btn = new QPushButton(info->label);
        btn->setToolTip(info->tooltip ? QString::fromUtf8(info->tooltip) : QString());
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(26);
        btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #3c3c3c;"
            "  color: #cccccc;"
            "  border: 1px solid #555555;"
            "  border-radius: 4px;"
            "  padding: 2px 10px;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #4c4c4c;"
            "  border-color: #0078d4;"
            "  color: #ffffff;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #0078d4;"
            "  color: #ffffff;"
            "}"
        );

        connect(btn, &QPushButton::clicked, this, [this, action]() {
            emit actionTriggered(action);
        });

        m_layout->addWidget(btn);
    }
}
