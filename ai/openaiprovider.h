#ifndef OPENAIProvider_H
#define OPENAIProvider_H

#include "aiprovider.h"

class OpenAiProvider : public AiProvider
{
    Q_OBJECT
public:
    explicit OpenAiProvider(QObject *parent = nullptr);

    void setApiKey(const QString &key) override;
    void setModel(const QString &model) override;
    void setSystemPrompt(const QString &prompt) override;
    void setMaxTokens(int maxTokens) override;
    void setEndpoint(const QString &endpoint);
    void chatStream(const QList<Message> &messages) override;

private slots:
    void onReadyRead();
    void onFinished();

private:
    void parseSseBuffer();
};

#endif // OPENAIProvider_H
