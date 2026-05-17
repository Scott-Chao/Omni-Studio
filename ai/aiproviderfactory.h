#ifndef AIPROVIDERFACTORY_H
#define AIPROVIDERFACTORY_H

#include <QString>
#include <QStringList>

class AiProvider;

class AiProviderFactory
{
public:
    enum ProviderType { Anthropic, OpenAiCompatible };

    static AiProvider *createProvider(ProviderType type, QObject *parent = nullptr);
    static ProviderType typeFromString(const QString &name);
    static QStringList availableProviders();
};

#endif // AIPROVIDERFACTORY_H
