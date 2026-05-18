#ifndef AIPROVIDER_H
#define AIPROVIDER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>

enum class MessageRole { User, Assistant, System };

struct Message {
    MessageRole role = MessageRole::User;
    QString content;

    QString roleToJson() const {
        switch (role) {
        case MessageRole::User:      return QStringLiteral("user");
        case MessageRole::Assistant: return QStringLiteral("assistant");
        case MessageRole::System:    return QStringLiteral("system");
        }
        return QStringLiteral("user");
    }
};

class AiProvider : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    virtual void setApiKey(const QString &key) = 0;
    virtual void setModel(const QString &model) = 0;
    virtual void setSystemPrompt(const QString &prompt) = 0;
    virtual void setMaxTokens(int maxTokens) = 0;
    virtual void chatStream(const QList<Message> &messages) = 0;
    virtual void cancel() = 0;

signals:
    void partialResponse(const QString &text);
    void finished();
    void error(const QString &message);
};

#endif // AIPROVIDER_H
