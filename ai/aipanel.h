#ifndef AIPANEL_H
#define AIPANEL_H

#include <QWidget>

class ChatArea;
class QLineEdit;
class QPushButton;

class AiPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AiPanel(QWidget *parent = nullptr);

    enum { DefaultWidth = 340 };

    ChatArea *chatArea() const { return m_chatArea; }
    void addUserMessage(const QString &text);
    void addAssistantMessage(const QString &text);
    void appendToLastAssistant(const QString &text);
    void clearChat();

signals:
    void sendMessage(const QString &text);
    void clearRequested();

private:
    ChatArea *m_chatArea;
    QLineEdit *m_inputEdit;
    QPushButton *m_sendBtn;
    QPushButton *m_clearBtn;
};

#endif // AIPANEL_H
