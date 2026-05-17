#include "aiproviderfactory.h"
#include "aiprovider.h"
#include "anthropicprovider.h"
#include "openaiprovider.h"

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
