#include "openaiprovider.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>

OpenAiProvider::OpenAiProvider(QObject *parent)
    : AiProvider(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_timeoutTimer(new QTimer(this))
    , m_endpoint(QStringLiteral("https://api.deepseek.com/v1"))
{
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(30000);
    connect(m_timeoutTimer, &QTimer::timeout, this, &OpenAiProvider::onTimeout);
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
    connect(m_reply, &QNetworkReply::finished, this, &OpenAiProvider::onFinished);

    m_timeoutTimer->start();
}

void OpenAiProvider::cancel()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_timeoutTimer->stop();
}

void OpenAiProvider::onReadyRead()
{
    m_timeoutTimer->start(); // reset timeout on any data

    if (!m_reply)
        return;

    m_buffer += QString::fromUtf8(m_reply->readAll());
    parseSseBuffer();
}

void OpenAiProvider::onFinished()
{
    m_timeoutTimer->stop();

    if (!m_reply)
        return;

    // Process any remaining buffered data
    if (!m_buffer.isEmpty()) {
        parseSseBuffer();
    }

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

void OpenAiProvider::onTimeout()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    emit error(tr("响应超时"));
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
            emit finished();
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
            emit finished();
            return;
        }
    }
}
