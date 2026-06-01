#ifndef AICONVERSATION_H
#define AICONVERSATION_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include "aiproviders.h"

struct AiConversation {
    QString id;
    QString title;
    QString sourceFile;
    QDateTime createdAt;
    QDateTime updatedAt;
    int messageCount = 0;

    QJsonObject toJson() const {
        return {
            {QStringLiteral("id"), id},
            {QStringLiteral("title"), title},
            {QStringLiteral("sourceFile"), sourceFile},
            {QStringLiteral("createdAt"), createdAt.toString(Qt::ISODate)},
            {QStringLiteral("updatedAt"), updatedAt.toString(Qt::ISODate)},
            {QStringLiteral("messageCount"), messageCount},
        };
    }

    static AiConversation fromJson(const QJsonObject &obj) {
        AiConversation c;
        c.id = obj.value(QStringLiteral("id")).toString();
        c.title = obj.value(QStringLiteral("title")).toString();
        c.sourceFile = obj.value(QStringLiteral("sourceFile")).toString();
        c.createdAt = QDateTime::fromString(obj.value(QStringLiteral("createdAt")).toString(), Qt::ISODate);
        c.updatedAt = QDateTime::fromString(obj.value(QStringLiteral("updatedAt")).toString(), Qt::ISODate);
        c.messageCount = obj.value(QStringLiteral("messageCount")).toInt();
        return c;
    }

    bool isValid() const { return !id.isEmpty(); }
};

struct AiMessage {
    MessageRole role = MessageRole::User;
    QString content;
    qint64 timestampMs = 0;

    // Implicit conversion to Message for API calls, eliminating
    // manual field-by-field conversion in callers.
    operator Message() const {
        Message msg;
        msg.role = role;
        msg.content = content;
        return msg;
    }

    QJsonObject toJson() const {
        QString roleStr;
        switch (role) {
        case MessageRole::User:      roleStr = QStringLiteral("user"); break;
        case MessageRole::Assistant: roleStr = QStringLiteral("assistant"); break;
        case MessageRole::System:    roleStr = QStringLiteral("system"); break;
        }

        return {
            {QStringLiteral("role"), roleStr},
            {QStringLiteral("content"), content},
            {QStringLiteral("timestampMs"), timestampMs},
        };
    }

    static AiMessage fromJson(const QJsonObject &obj) {
        AiMessage m;
        const QString roleStr = obj.value(QStringLiteral("role")).toString();
        if (roleStr == QStringLiteral("assistant"))
            m.role = MessageRole::Assistant;
        else if (roleStr == QStringLiteral("system"))
            m.role = MessageRole::System;
        else
            m.role = MessageRole::User;
        m.content = obj.value(QStringLiteral("content")).toString();
        m.timestampMs = static_cast<qint64>(obj.value(QStringLiteral("timestampMs")).toDouble());
        return m;
    }
};

#endif // AICONVERSATION_H
