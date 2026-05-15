#include "completionmanager.h"

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
    // Provider creation will be added when CppCompletionProvider
    // and PythonCompletionProvider are implemented (Steps 4 & 11).
    // For now, leave m_provider as nullptr — requests are no-ops.
    Q_UNUSED(m_languageId);
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
