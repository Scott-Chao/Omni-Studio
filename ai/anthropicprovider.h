#ifndef ANTHROPICPROVIDER_H
#define ANTHROPICPROVIDER_H

#include "aiprovider.h"

class AnthropicProvider : public AiProvider
{
    Q_OBJECT
public:
    explicit AnthropicProvider(QObject *parent = nullptr);

    void chatStream(const QList<Message> &messages) override;

private:
    void parseSseFrame(const QString &frame) override;
};

#endif // ANTHROPICPROVIDER_H
