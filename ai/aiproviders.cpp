#include "aiproviders.h"
#include <QNetworkRequest>
#include <QJsonArray>
#include <QUrl>

// ── AiProvider ──────────────────────────────────────────────────────────────

void AiProvider::setApiKey(const QString &key) { m_apiKey = key; }
void AiProvider::setModel(const QString &model) { m_model = model; }
void AiProvider::setSystemPrompt(const QString &prompt) { m_systemPrompt = prompt; }
void AiProvider::setMaxTokens(int maxTokens) { m_maxTokens = maxTokens; }
void AiProvider::setEndpoint(const QString &endpoint) { m_endpoint = endpoint; }

QNetworkReply* AiProvider::postStreamRequest(const QNetworkRequest &request, const QByteArray &data)
{
    cancel();
    m_buffer.clear();
    m_finished = false;

    m_reply = m_net->post(request, data);
    connect(m_reply, &QNetworkReply::readyRead, this, &AiProvider::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &AiProvider::onFinished);

    m_timeoutTimer->start();
    return m_reply;
}

void AiProvider::cancel()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_timeoutTimer)
        m_timeoutTimer->stop();
}

void AiProvider::onReadyRead()
{
    m_timeoutTimer->start();

    if (!m_reply)
        return;

    m_buffer += QString::fromUtf8(m_reply->readAll());

    while (true) {
        int idx = m_buffer.indexOf(QStringLiteral("\n\n"));
        if (idx < 0)
            break;

        QString frame = m_buffer.left(idx);
        m_buffer = m_buffer.mid(idx + 2);
        parseSseFrame(frame);
    }
}

void AiProvider::drainBuffer()
{
    if (!m_buffer.isEmpty()) {
        parseSseFrame(m_buffer);
        m_buffer.clear();
    }
}

void AiProvider::onFinished()
{
    m_timeoutTimer->stop();

    if (!m_reply)
        return;

    drainBuffer();
    handleNetworkError();

    m_reply->deleteLater();
    m_reply = nullptr;
}

void AiProvider::onTimeout()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    emit error(tr("响应超时"));
}

void AiProvider::handleNetworkError()
{
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
}

// ── AnthropicProvider ───────────────────────────────────────────────────────

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

    QNetworkRequest request{QUrl(url)};
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

// ── OpenAiProvider ─────────────────────────────────────────────────────────

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

    QNetworkRequest request{QUrl(url)};
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

// ── AiProviderFactory ───────────────────────────────────────────────────────

AiProvider *AiProviderFactory::createProvider(ProviderType type, QObject *parent)
{
    switch (type) {
    case Anthropic:
        return new AnthropicProvider(parent);
    case OpenAiCompatible:
        return new OpenAiProvider(parent);
    }
    return nullptr;
}

AiProviderFactory::ProviderType AiProviderFactory::typeFromString(const QString &name)
{
    if (name.compare("Anthropic", Qt::CaseInsensitive) == 0)
        return Anthropic;
    return OpenAiCompatible;
}

QStringList AiProviderFactory::availableProviders()
{
    return {QStringLiteral("Anthropic"), QStringLiteral("OpenAI 兼容")};
}
