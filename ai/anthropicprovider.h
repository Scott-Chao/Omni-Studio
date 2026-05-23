#ifndef ANTHROPICPROVIDER_H
#define ANTHROPICPROVIDER_H

#include "aiprovider.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

class AnthropicProvider : public AiProvider
{
    Q_OBJECT
public:
    explicit AnthropicProvider(QObject *parent = nullptr);

    void setApiKey(const QString &key) override;
    void setModel(const QString &model) override;
    void setSystemPrompt(const QString &prompt) override;
    void setMaxTokens(int maxTokens) override;
    void setEndpoint(const QString &endpoint);
    void chatStream(const QList<Message> &messages) override;
    void cancel() override;

private slots:
    void onReadyRead();
    void onFinished();
    void onTimeout();

private:
    void parseSseFrame(const QString &frame);

    QNetworkAccessManager *m_net;
    QNetworkReply *m_reply = nullptr;
    QTimer *m_timeoutTimer;
    QString m_buffer;
    QString m_apiKey;
    QString m_model;
    QString m_systemPrompt;
    QString m_endpoint;
    int m_maxTokens = 4096;
    bool m_finished = false;
};

#endif // ANTHROPICPROVIDER_H
