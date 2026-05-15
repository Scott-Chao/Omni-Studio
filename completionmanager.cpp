#include "completionmanager.h"
#include "cppcompletionprovider.h"

CompletionManager::CompletionManager(QObject *parent)
    : QObject(parent)
{
}

CompletionManager::~CompletionManager()
{
    // m_provider is parented to this, Qt will auto-delete
}

void CompletionManager::setLanguage(const QString &langId)
{
    if (m_languageId == langId && m_provider)
        return;

    m_languageId = langId;

    // Delete old provider before creating new one
    if (m_provider) {
        m_provider->deleteLater();
        m_provider = nullptr;
    }

    createProvider();
}

void CompletionManager::createProvider()
{
    if (m_languageId == QStringLiteral("cpp")) {
        m_provider = new CppCompletionProvider(this);
        connect(m_provider, &CompletionProvider::completionReady,
                this, &CompletionManager::completionReady);
        connect(m_provider, &CompletionProvider::hoverReady,
                this, &CompletionManager::hoverReady);
        connect(m_provider, &CompletionProvider::signatureHelpReady,
                this, &CompletionManager::signatureHelpReady);
    }
    // Python support will be added in Step 11
}

void CompletionManager::requestCompletion(const QString &text, int cursorPos)
{
    if (m_provider)
        m_provider->requestCompletion(text, cursorPos);
}

void CompletionManager::requestHover(const QString &text, int cursorPos)
{
    if (m_provider)
        m_provider->requestHover(text, cursorPos);
}

void CompletionManager::requestSignatureHelp(const QString &text, int cursorPos)
{
    if (m_provider)
        m_provider->requestSignatureHelp(text, cursorPos);
}
