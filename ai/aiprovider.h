#ifndef AIPROVIDER_H
#define AIPROVIDER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

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

    void cancel();

signals:
    void partialResponse(const QString &text);
    void finished();
    void error(const QString &message);

public:
    void onTimeout();
    void onFinished();

protected:
    void handleNetworkError();
    virtual void drainBuffer() = 0;

    QNetworkAccessManager *m_net = nullptr;
    QNetworkReply *m_reply = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    QString m_buffer;
    QString m_apiKey;
    QString m_model;
    QString m_systemPrompt;
    QString m_endpoint;
    int m_maxTokens = 4096;
    bool m_finished = false;
};

#endif // AIPROVIDER_H
