#include "chatarea.h"
#include "chatbubble.h"
#include "thememanager.h"

#include <QVBoxLayout>
#include <QScrollBar>

ChatArea::ChatArea(QWidget *parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_container = new QWidget(this);

    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(4, 8, 4, 8);
    m_layout->setSpacing(12);
    m_layout->addStretch(); // bottom stretch to keep messages top-aligned

    setWidget(m_container);

    ThemeManager::watchTheme(this, &ChatArea::refreshStyle);
    refreshStyle();
}

void ChatArea::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    setStyleSheet(QStringLiteral(
        "QScrollArea { background-color: %1; border: none; }"
    ).arg(tm.color("editor.background").name()));

    m_container->setStyleSheet(QStringLiteral(
        "background-color: %1;"
    ).arg(tm.color("editor.background").name()));
}

void ChatArea::addMessage(ChatBubble::Role role, const QString &text)
{
    auto *bubble = new ChatBubble(role, text, m_container);

    // Insert before the trailing stretch
    m_layout->insertWidget(m_layout->count() - 1, bubble);
    m_bubbles.append(bubble);

    scrollToBottom();
}

void ChatArea::appendToLastMessage(const QString &text)
{
    if (m_bubbles.isEmpty())
        return;

    ChatBubble *last = m_bubbles.last();
    last->appendText(text);
    scrollToBottom();
}

void ChatArea::flushPendingUpdates()
{
    for (ChatBubble *bubble : m_bubbles)
        bubble->flushUpdate();
}

void ChatArea::clear()
{
    for (ChatBubble *bubble : m_bubbles) {
        m_layout->removeWidget(bubble);
        delete bubble;
    }
    m_bubbles.clear();
}

ChatBubble *ChatArea::lastBubble() const
{
    return m_bubbles.isEmpty() ? nullptr : m_bubbles.last();
}

void ChatArea::scrollToBottom()
{
    QScrollBar *sb = verticalScrollBar();
    // Don't interrupt user scrolling — only auto-scroll if the user is
    // already at or near the bottom (within 20px tolerance).
    if (sb->value() < sb->maximum() - 20)
        return;
    sb->setValue(sb->maximum());
}
