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
class AiHistoryListWidget;

class AiPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AiPanel(QWidget *parent = nullptr);

    enum { DefaultWidth = 340 };
    enum TabIndex { ChatTab = 0, ErrorTab = 1, HistoryTab = 2 };

    // Chat operations
    void addUserMessage(const QString &text);
    void addAssistantMessage(const QString &text);
    void appendToLastAssistant(const QString &text);
    void flushPendingUpdates();
    void clearChat();
    void setInputEnabled(bool enabled);

    // Action bar operations
    void setActionList(const QVector<AiAction> &actions);
    void clearActionList();

    // Sub-panel access
    ErrorListPanel *errorListPanel() const { return m_errorListPanel; }
    AiHistoryListWidget *historyListWidget() const { return m_historyListWidget; }
    void setCurrentTab(int index);

    // Query chat state
    QString lastAssistantContent() const;
    bool hasStreamingTarget() const;

signals:
    void sendMessage(const QString &text);
    void clearRequested();
    void newConversationRequested();
    void actionTriggered(int actionIndex);
    void errorSelected(const QString &recordId);
    void historyListVisibilityChanged(bool visible);

private slots:
    void onTabSwitch(int index);
    void refreshStyle();

private:
    static QString tabButtonStyle(int index, bool active);
    void updateErrorBadge();

    ChatArea *m_chatArea;
    ActionBar *m_actionBar;
    QLineEdit *m_inputEdit;
    QPushButton *m_sendBtn;
    QPushButton *m_clearBtn;
    QPushButton *m_newConvBtn;
    QWidget *m_titleBar;

    // Tab switching
    QStackedWidget *m_stackedWidget;
    QPushButton *m_aiTabBtn;
    QPushButton *m_errorTabBtn;
    QPushButton *m_historyTabBtn;
    class TabButtonGroup *m_tabGroup;
    ErrorListPanel *m_errorListPanel;
    AiHistoryListWidget *m_historyListWidget;
    QWidget *m_inputBar;
};

#endif // AIPANEL_H
