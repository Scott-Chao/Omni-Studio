#ifndef AIPROVIDER_H
#define AIPROVIDER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "messagerole.h"

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

    void setApiKey(const QString &key);
    void setModel(const QString &model);
    void setSystemPrompt(const QString &prompt);
    void setMaxTokens(int maxTokens);
    void setEndpoint(const QString &endpoint);

    virtual void chatStream(const QList<Message> &messages) = 0;
    void cancel();

signals:
    void partialResponse(const QString &text);
    void finished();
    void error(const QString &message);

protected:
    QNetworkReply* postStreamRequest(const QNetworkRequest &request, const QByteArray &data);
    void onReadyRead();
    void drainBuffer();
    virtual void parseSseFrame(const QString &frame) = 0;

    void handleNetworkError();

public: // called as slots from derived class constructors
    void onTimeout();
    void onFinished();

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
