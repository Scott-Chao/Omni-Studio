#ifndef AIHISTORYMANAGER_H
#define AIHISTORYMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include "aiconversation.h"

class AiHistoryManager : public QObject
{
    Q_OBJECT

public:
    static AiHistoryManager &instance();

    // Conversation management
    QString createConversation(const QString &title, const QString &sourceFile);
    void deleteConversation(const QString &id);
    void renameConversation(const QString &id, const QString &newTitle);
    void clearCurrentConversation();  // delete current + create new empty

    // Message management
    void appendMessage(const QString &convId, const AiMessage &msg);
    QList<AiMessage> loadMessages(const QString &convId) const;

    // Query
    QList<AiConversation> allConversations() const;
    QList<AiConversation> conversationsByFile(const QString &filePath) const;
    AiConversation conversationById(const QString &id) const;
    QString currentConversationId() const { return m_currentConvId; }
    void setCurrentConversation(const QString &id) { m_currentConvId = id; }

    // Export
    QString exportToMarkdown(const QString &convId) const;

signals:
    void conversationListChanged();

private:
    AiHistoryManager();
    ~AiHistoryManager() override;
    AiHistoryManager(const AiHistoryManager &) = delete;
    AiHistoryManager &operator=(const AiHistoryManager &) = delete;

    void loadIndex();
    void saveIndex();
    QString storageDir() const;
    QString conversationFilePath(const QString &id) const;

    QList<AiConversation> m_conversations;
    QString m_currentConvId;
};

#endif // AIHISTORYMANAGER_H
