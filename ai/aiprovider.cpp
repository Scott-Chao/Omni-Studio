#include "aiprovider.h"
#include <QNetworkRequest>

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
