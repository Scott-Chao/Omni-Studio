#ifndef AIREQUESTHANDLER_H
#define AIREQUESTHANDLER_H

#include <QObject>
#include <QString>
#include <QList>
#include "aiproviders.h"
#include "prompttemplates.h"

class AiPanel;
class TabManager;
class SettingsManager;

class AiRequestHandler : public QObject
{
    Q_OBJECT
public:
    explicit AiRequestHandler(QObject *parent = nullptr);
    ~AiRequestHandler();

    void setAiPanel(AiPanel *panel);
    void setTabManager(TabManager *manager);
    void setSettingsManager(SettingsManager *settings);

    void startAiRequest(AiAction action, const QString &freeQuery = QString());
    void abortAiRequest();
    void loadAiConversation(const QString &convId);
    void clearConversation();
    void newConversation();

    bool isStreaming() const { return m_aiStreaming; }
    const QList<Message> &history() const { return m_aiHistory; }
    void clearHistory();

    // Token estimation utilities
    static int estimateTokens(const QString &text);
    static int modelContextLimit(const QString &model);
    static QList<Message> pruneContextWindow(const QList<Message> &history,
                                              const QString &model,
                                              int maxResponseTokens,
                                              const QString &systemPrompt);

signals:
    void streamingStateChanged(bool streaming);

private slots:
    void onAiPartialResponse(const QString &text);
    void onAiFinished();
    void onAiError(const QString &message);

private:
    AiPanel *m_aiPanel = nullptr;
    TabManager *m_tabManager = nullptr;
    SettingsManager *m_settings = nullptr;

    AiProvider *m_aiProvider = nullptr;
    QList<Message> m_aiHistory;
    bool m_aiStreaming = false;
};

#endif // AIREQUESTHANDLER_H
