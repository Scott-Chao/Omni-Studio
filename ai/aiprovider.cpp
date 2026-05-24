#include "aiprovider.h"
#include <QNetworkRequest>

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
