#include "openaiprovider.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>

OpenAiProvider::OpenAiProvider(QObject *parent)
    : AiProvider(parent)
{
    m_net = new QNetworkAccessManager(this);
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(30000);
    m_endpoint = QStringLiteral("https://api.deepseek.com/v1");
    connect(m_timeoutTimer, &QTimer::timeout, this, &AiProvider::onTimeout);
}

void OpenAiProvider::chatStream(const QList<Message> &messages)
{
    QString url = m_endpoint;
    if (!url.endsWith(QLatin1Char('/')))
        url += QLatin1Char('/');
    url += QStringLiteral("chat/completions");

    QNetworkRequest request(QUrl(url));
    request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
    request.setRawHeader("Content-Type", "application/json");

    QJsonObject body;
    body["model"] = m_model;
    body["max_tokens"] = m_maxTokens;
    body["stream"] = true;

    QJsonArray msgArray;
    if (!m_systemPrompt.isEmpty()) {
        QJsonObject sysMsg;
        sysMsg["role"] = QStringLiteral("system");
        sysMsg["content"] = m_systemPrompt;
        msgArray.append(sysMsg);
    }
    for (const auto &msg : messages) {
        QJsonObject m;
        m["role"] = msg.roleToJson();
        m["content"] = msg.content;
        msgArray.append(m);
    }
    body["messages"] = msgArray;

    QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);
    postStreamRequest(request, postData);
}

void OpenAiProvider::parseSseFrame(const QString &frame)
{
    QString dataStr;
    const QStringList lines = frame.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("data: "))) {
            dataStr = line.mid(6).trimmed();
            break;
        }
    }

    if (dataStr.isEmpty())
        return;

    if (dataStr == QStringLiteral("[DONE]")) {
        if (!m_finished) {
            m_finished = true;
            emit finished();
        }
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(dataStr.toUtf8());
    if (!doc.isObject())
        return;

    QJsonObject obj = doc.object();

    if (obj.contains("error")) {
        QJsonObject errorObj = obj.value("error").toObject();
        QString errorMsg = errorObj.value("message").toString();
        if (errorMsg.isEmpty())
            errorMsg = tr("AI 返回了错误");
        emit error(errorMsg);
        return;
    }

    QJsonArray choices = obj.value("choices").toArray();
    if (choices.isEmpty())
        return;

    QJsonObject choice = choices.first().toObject();

    if (!choice.contains("delta"))
        return;

    QJsonObject delta = choice.value("delta").toObject();
    QString content = delta.value("content").toString();
    if (!content.isEmpty())
        emit partialResponse(content);

    QString finishReason = choice.value("finish_reason").toString();
    if (!finishReason.isEmpty() && finishReason != "null") {
        if (!m_finished) {
            m_finished = true;
            emit finished();
        }
    }
}
