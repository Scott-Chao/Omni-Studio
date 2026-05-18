#ifndef AIPANEL_H
#define AIPANEL_H

#include <QWidget>

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

    ChatArea *chatArea() const { return m_chatArea; }
    ActionBar *actionBar() const { return m_actionBar; }
    void addUserMessage(const QString &text);
    void addAssistantMessage(const QString &text);
    void appendToLastAssistant(const QString &text);
    void clearChat();
    void setInputEnabled(bool enabled);

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
