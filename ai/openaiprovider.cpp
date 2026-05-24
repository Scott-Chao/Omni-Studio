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

void OpenAiProvider::setApiKey(const QString &key)
{
    m_apiKey = key;
}

void OpenAiProvider::setModel(const QString &model)
{
    m_model = model;
}

void OpenAiProvider::setSystemPrompt(const QString &prompt)
{
    m_systemPrompt = prompt;
}

void OpenAiProvider::setMaxTokens(int maxTokens)
{
    m_maxTokens = maxTokens;
}

void OpenAiProvider::setEndpoint(const QString &endpoint)
{
    m_endpoint = endpoint;
}

void OpenAiProvider::chatStream(const QList<Message> &messages)
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    m_buffer.clear();
    m_finished = false;

    QString url = m_endpoint;
    if (!url.endsWith(QLatin1Char('/')))
        url += QLatin1Char('/');
    url += QStringLiteral("chat/completions");

    QUrl requestUrl(url);
    QNetworkRequest request(requestUrl);
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

    m_reply = m_net->post(request, postData);
    connect(m_reply, &QNetworkReply::readyRead, this, &OpenAiProvider::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &AiProvider::onFinished);

    m_timeoutTimer->start();
}

void OpenAiProvider::onReadyRead()
{
    m_timeoutTimer->start(); // reset timeout on any data

    if (!m_reply)
        return;

    m_buffer += QString::fromUtf8(m_reply->readAll());
    parseSseBuffer();
}

void OpenAiProvider::drainBuffer()
{
    if (!m_buffer.isEmpty())
        parseSseBuffer();
}

void OpenAiProvider::parseSseBuffer()
{
    // Process complete SSE frames (delimited by "\n\n")
    while (true) {
        int idx = m_buffer.indexOf(QStringLiteral("\n\n"));
        if (idx < 0)
            break;

        QString frame = m_buffer.left(idx);
        m_buffer = m_buffer.mid(idx + 2);

        QString dataStr;
        const QStringList lines = frame.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (line.startsWith(QStringLiteral("data: "))) {
                dataStr = line.mid(6).trimmed();
                break;
            }
        }

        if (dataStr.isEmpty())
            continue;

        // OpenAI end-of-stream marker
        if (dataStr == QStringLiteral("[DONE]")) {
            if (!m_finished) {
                m_finished = true;
                emit finished();
            }
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(dataStr.toUtf8());
        if (!doc.isObject())
            continue;

        QJsonObject obj = doc.object();

        // Check for error
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
            continue;

        QJsonObject choice = choices.first().toObject();

        // Skip non-content deltas (e.g., role or finish_reason only)
        if (!choice.contains("delta"))
            continue;

        QJsonObject delta = choice.value("delta").toObject();
        QString content = delta.value("content").toString();
        if (!content.isEmpty())
            emit partialResponse(content);

        // Check for finish reason
        QString finishReason = choice.value("finish_reason").toString();
        if (!finishReason.isEmpty() && finishReason != "null") {
            if (!m_finished) {
                m_finished = true;
                emit finished();
            }
            return;
        }
    }
}
