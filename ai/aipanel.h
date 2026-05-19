#ifndef AIPANEL_H
#define AIPANEL_H

#include <QWidget>
#include <QVector>
#include "prompttemplates.h"

class ChatArea;
class ActionBar;
class ErrorListPanel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class ErrorJournal;

class AiPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AiPanel(QWidget *parent = nullptr);

    enum { DefaultWidth = 340 };
    enum TabIndex { ChatTab = 0, ErrorTab = 1 };

    // Chat operations
    void addUserMessage(const QString &text);
    void addAssistantMessage(const QString &text);
    void appendToLastAssistant(const QString &text);
    void clearChat();
    void setInputEnabled(bool enabled);

    // Action bar operations
    void setActionList(const QVector<AiAction> &actions);
    void clearActionList();

    // Error list panel access
    ErrorListPanel *errorListPanel() const { return m_errorListPanel; }
    void setCurrentTab(int index);

    // Query chat state
    QString lastAssistantContent() const;
    bool hasStreamingTarget() const;

signals:
    void sendMessage(const QString &text);
    void clearRequested();
    void actionTriggered(int actionIndex);
    void errorSelected(const QString &recordId);

private slots:
    void onTabSwitch(int index);

private:
    void updateTabButtonStyle();
    void updateErrorBadge();

    ChatArea *m_chatArea;
    ActionBar *m_actionBar;
    QLineEdit *m_inputEdit;
    QPushButton *m_sendBtn;
    QPushButton *m_clearBtn;

    // Tab switching
    QStackedWidget *m_stackedWidget;
    QPushButton *m_aiTabBtn;
    QPushButton *m_errorTabBtn;
    ErrorListPanel *m_errorListPanel;
    QWidget *m_inputBar;
    int m_currentTab = ChatTab;
};

#endif // AIPANEL_H
