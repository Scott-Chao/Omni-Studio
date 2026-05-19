#include "chatarea.h"
#include "chatbubble.h"

#include <QVBoxLayout>
#include <QScrollBar>

ChatArea::ChatArea(QWidget *parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet(
        "QScrollArea { background-color: #1E1E1E; border: none; }"
        "QScrollBar:vertical {"
        "  background-color: #1E1E1E;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #555555;"
        "  min-height: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );

    m_container = new QWidget(this);
    m_container->setStyleSheet("background-color: #1E1E1E;");

    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(4, 8, 4, 8);
    m_layout->setSpacing(12);
    m_layout->addStretch(); // bottom stretch to keep messages top-aligned

    setWidget(m_container);
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
