#include "completionmanager.h"
#include "cppcompletionprovider.h"
#include "pythoncompletionprovider.h"
#include "keywordcompletionprovider.h"

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
        auto *cppProvider = new CppCompletionProvider(this);
        m_provider = cppProvider;

        connect(m_provider, &CompletionProvider::completionReady,
                this, &CompletionManager::completionReady);
        connect(m_provider, &CompletionProvider::hoverReady,
                this, &CompletionManager::hoverReady);
        connect(m_provider, &CompletionProvider::signatureHelpReady,
                this, &CompletionManager::signatureHelpReady);

        // Forward provider lifecycle signals
        connect(cppProvider, &CppCompletionProvider::serverReady,
                this, &CompletionManager::serverReady);
        connect(cppProvider, &CppCompletionProvider::serverFailed,
                this, &CompletionManager::onServerFailed);
    } else if (m_languageId == QStringLiteral("python")) {
        auto *pyProvider = new PythonCompletionProvider(this);
        m_provider = pyProvider;

        connect(m_provider, &CompletionProvider::completionReady,
                this, &CompletionManager::completionReady);
        connect(m_provider, &CompletionProvider::hoverReady,
                this, &CompletionManager::hoverReady);
        connect(m_provider, &CompletionProvider::signatureHelpReady,
                this, &CompletionManager::signatureHelpReady);

        connect(pyProvider, &PythonCompletionProvider::serverReady,
                this, &CompletionManager::serverReady);
        connect(pyProvider, &PythonCompletionProvider::serverFailed,
                this, &CompletionManager::onServerFailed);
    }
}

void CompletionManager::openDocument(const QString &uri, const QString &languageId, const QString &text)
{
    if (m_provider)
        m_provider->openDocument(uri, languageId, text);
}

void CompletionManager::updateText(const QString &text)
{
    if (m_provider)
        m_provider->updateText(text);
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

void CompletionManager::onServerFailed(const QString &reason)
{
    qWarning() << "CompletionManager: server failed:" << reason
               << "— falling back to keyword completion";

    // Forward to editor so it can show a status bar message or similar
    emit serverFailed(reason);

    // Swap to keyword fallback provider
    if (m_provider) {
        m_provider->deleteLater();
        m_provider = nullptr;
    }

    auto *kwProvider = new KeywordCompletionProvider(m_languageId, this);
    m_provider = kwProvider;

    connect(m_provider, &CompletionProvider::completionReady,
            this, &CompletionManager::completionReady);
    connect(m_provider, &CompletionProvider::hoverReady,
            this, &CompletionManager::hoverReady);
    connect(m_provider, &CompletionProvider::signatureHelpReady,
            this, &CompletionManager::signatureHelpReady);

    qDebug() << "CompletionManager: fallback provider installed for" << m_languageId;
}
