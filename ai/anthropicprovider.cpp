#include "anthropicprovider.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>

AnthropicProvider::AnthropicProvider(QObject *parent)
    : AiProvider(parent)
{
    m_net = new QNetworkAccessManager(this);
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(30000);
    m_endpoint = QStringLiteral("https://api.anthropic.com/v1");
    connect(m_timeoutTimer, &QTimer::timeout, this, &AiProvider::onTimeout);
}

void AnthropicProvider::chatStream(const QList<Message> &messages)
{
    QString url = m_endpoint;
    if (!url.endsWith(QLatin1Char('/')))
        url += QLatin1Char('/');
    url += QStringLiteral("messages");

    QNetworkRequest request(QUrl(url));
    request.setRawHeader("x-api-key", m_apiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");
    request.setRawHeader("Content-Type", "application/json");

    QJsonObject body;
    body["model"] = m_model;
    body["max_tokens"] = m_maxTokens;
    body["stream"] = true;

    if (!m_systemPrompt.isEmpty())
        body["system"] = m_systemPrompt;

    QJsonArray msgArray;
    for (const auto &msg : messages) {
        QJsonObject m;
        m["role"] = msg.roleToJson();
        QJsonArray content;
        QJsonObject textBlock;
        textBlock["type"] = "text";
        textBlock["text"] = msg.content;
        content.append(textBlock);
        m["content"] = content;
        msgArray.append(m);
    }
    body["messages"] = msgArray;

    QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);
    postStreamRequest(request, postData);
}

void AnthropicProvider::parseSseFrame(const QString &frame)
{
    if (frame.trimmed().isEmpty())
        return;

    QString eventType;
    QString dataStr;

    const QStringList lines = frame.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("event: ")))
            eventType = line.mid(7).trimmed();
        else if (line.startsWith(QStringLiteral("data: ")))
            dataStr = line.mid(6).trimmed();
    }

    if (dataStr.isEmpty())
        return;

    QJsonDocument doc = QJsonDocument::fromJson(dataStr.toUtf8());
    if (!doc.isObject())
        return;

    QJsonObject obj = doc.object();

    if (eventType == QStringLiteral("content_block_delta")) {
        QJsonObject delta = obj.value("delta").toObject();
        QString text = delta.value("text").toString();
        if (!text.isEmpty())
            emit partialResponse(text);
    } else if (eventType == QStringLiteral("message_stop")) {
        if (!m_finished) {
            m_finished = true;
            emit finished();
        }
    } else if (eventType == QStringLiteral("error")) {
        QJsonObject errorObj = obj.value("error").toObject();
        QString errorMsg = errorObj.value("message").toString();
        if (errorMsg.isEmpty())
            errorMsg = tr("AI 未返回有效结果，请重试");
        emit error(errorMsg);
    }
}
