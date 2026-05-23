#include "anthropicprovider.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>

AnthropicProvider::AnthropicProvider(QObject *parent)
    : AiProvider(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_timeoutTimer(new QTimer(this))
    , m_endpoint(QStringLiteral("https://api.anthropic.com/v1"))
{
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(30000);
    connect(m_timeoutTimer, &QTimer::timeout, this, &AnthropicProvider::onTimeout);
}

void AnthropicProvider::setApiKey(const QString &key)
{
    m_apiKey = key;
}

void AnthropicProvider::setModel(const QString &model)
{
    m_model = model;
}

void AnthropicProvider::setSystemPrompt(const QString &prompt)
{
    m_systemPrompt = prompt;
}

void AnthropicProvider::setMaxTokens(int maxTokens)
{
    m_maxTokens = maxTokens;
}

void AnthropicProvider::setEndpoint(const QString &endpoint)
{
    m_endpoint = endpoint;
}

void AnthropicProvider::chatStream(const QList<Message> &messages)
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
    url += QStringLiteral("messages");

    QUrl requestUrl(url);
    QNetworkRequest request(requestUrl);
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

    m_reply = m_net->post(request, postData);
    connect(m_reply, &QNetworkReply::readyRead, this, &AnthropicProvider::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &AnthropicProvider::onFinished);

    m_timeoutTimer->start();
}

void AnthropicProvider::cancel()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_timeoutTimer->stop();
}

void AnthropicProvider::onReadyRead()
{
    m_timeoutTimer->start(); // reset timeout on any data

    if (!m_reply)
        return;

    m_buffer += QString::fromUtf8(m_reply->readAll());

    // Process complete SSE frames (delimited by "\n\n")
    while (true) {
        int idx = m_buffer.indexOf(QStringLiteral("\n\n"));
        if (idx < 0)
            break;

        QString frame = m_buffer.left(idx);
        m_buffer = m_buffer.mid(idx + 2);
        parseSseFrame(frame);
    }
}

void AnthropicProvider::onFinished()
{
    m_timeoutTimer->stop();

    if (!m_reply)
        return;

    // Process any remaining buffered data
    if (!m_buffer.isEmpty()) {
        parseSseFrame(m_buffer);
        m_buffer.clear();
    }

    // Check for errors if we didn't get a proper finish via SSE
    if (m_reply->error() != QNetworkReply::NoError
        && m_reply->error() != QNetworkReply::OperationCanceledError) {
        int httpStatus = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString errorMsg;

        if (httpStatus == 401 || httpStatus == 403)
            errorMsg = tr("API Key 无效，请在设置中检查");
        else if (httpStatus == 429)
            errorMsg = tr("请求过于频繁，请稍后再试");
        else
            errorMsg = tr("网络连接失败: %1").arg(m_reply->errorString());

        emit error(errorMsg);
    }

    m_reply->deleteLater();
    m_reply = nullptr;
}

void AnthropicProvider::onTimeout()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    emit error(tr("响应超时"));
}

void AnthropicProvider::parseSseFrame(const QString &frame)
{
    // Skip empty frames
    if (frame.trimmed().isEmpty())
        return;

    // Anthropic SSE format: "event: <type>\ndata: <json>"
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
    // Ignore other event types: message_start, ping, content_block_start, content_block_delta, etc.
}
