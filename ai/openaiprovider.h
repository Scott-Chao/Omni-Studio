#ifndef OPENAIProvider_H
#define OPENAIProvider_H

#include "aiprovider.h"

class OpenAiProvider : public AiProvider
{
    Q_OBJECT
public:
    explicit OpenAiProvider(QObject *parent = nullptr);

    void chatStream(const QList<Message> &messages) override;

private:
    void parseSseFrame(const QString &frame) override;
};

#endif // OPENAIProvider_H
