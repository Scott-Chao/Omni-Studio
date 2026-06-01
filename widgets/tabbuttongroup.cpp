#include "tabbuttongroup.h"
#include <QPushButton>
#include <QStackedWidget>

TabButtonGroup::TabButtonGroup(QStackedWidget *stack, QObject *parent)
    : QObject(parent)
    , m_stack(stack)
{
}

void TabButtonGroup::addTab(QPushButton *button, int index)
{
    m_map.insert(button, index);
    connect(button, &QPushButton::clicked, this, [this, index]() {
        onButtonClicked(index);
    });
}

void TabButtonGroup::setCurrentIndex(int index)
{
    if (index == m_currentIndex)
        return;
    m_currentIndex = index;
    if (m_stack)
        m_stack->setCurrentIndex(index);
    refreshStyles();
    emit currentChanged(index);
}

void TabButtonGroup::onButtonClicked(int index)
{
    setCurrentIndex(index);
}

void TabButtonGroup::refreshStyles()
{
    if (!m_provider)
        return;

    for (auto it = m_map.constBegin(); it != m_map.constEnd(); ++it) {
        QPushButton *btn = it.key();
        int idx = it.value();
        btn->setStyleSheet(m_provider(idx, idx == m_currentIndex));
    }
}
