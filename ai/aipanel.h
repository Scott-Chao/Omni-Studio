#ifndef AIPANEL_H
#define AIPANEL_H

#include <QWidget>
#include <QVector>
#include "prompttemplates.h"

class ChatArea;
class ActionBar;
class QLineEdit;
class QPushButton;

class AiPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AiPanel(QWidget *parent = nullptr);

    enum { DefaultWidth = 340 };

    void addUserMessage(const QString &text);
    void addAssistantMessage(const QString &text);
    void appendToLastAssistant(const QString &text);
    void clearChat();
    void setInputEnabled(bool enabled);

    // High-level action bar operations (replaces leaked getter chains)
    void setActionList(const QVector<AiAction> &actions);
    void clearActionList();

    // Returns content of the last assistant bubble, or empty string
    QString lastAssistantContent() const;
    // True if the last message is an empty assistant bubble (streaming target)
    bool hasStreamingTarget() const;

signals:
    void sendMessage(const QString &text);
    void clearRequested();
    void actionTriggered(int actionIndex);

private:
    ChatArea *m_chatArea;
    ActionBar *m_actionBar;
    QLineEdit *m_inputEdit;
    QPushButton *m_sendBtn;
    QPushButton *m_clearBtn;
};

#endif // AIPANEL_H
