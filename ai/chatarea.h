#ifndef CHATAREA_H
#define CHATAREA_H

#include <QScrollArea>
#include <QList>

#include "chatbubble.h"

class QVBoxLayout;

class ChatArea : public QScrollArea
{
    Q_OBJECT

public:
    explicit ChatArea(QWidget *parent = nullptr);

    void addMessage(ChatBubble::Role role, const QString &text);
    void appendToLastMessage(const QString &text);
    void clear();
    int messageCount() const { return m_bubbles.size(); }
    ChatBubble *lastBubble() const;

private:
    void scrollToBottom();
    void refreshStyle();

    QWidget *m_container;
    QVBoxLayout *m_layout;
    QList<ChatBubble*> m_bubbles;
};

#endif // CHATAREA_H
