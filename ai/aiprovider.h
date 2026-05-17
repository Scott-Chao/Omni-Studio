#ifndef AIPROVIDER_H
#define AIPROVIDER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>

struct Message {
    QString role;    // "user", "assistant", "system"
    QString content;
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
