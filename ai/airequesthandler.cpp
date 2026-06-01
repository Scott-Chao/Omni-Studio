#include "airequesthandler.h"
#include "aipanel.h"
#include "aicontextmanager.h"
#include "aiproviders.h"
#include "aihistorymanager.h"
#include "editor/tabmanager.h"
#include "editor/editorwidget.h"
#include "config/settingsmanager.h"
#include "config/configmanager.h"

#include <QDateTime>
#include <QDebug>

// ── Token estimation ────────────────────────────────────────────────

int AiRequestHandler::estimateTokens(const QString &text)
{
    if (text.isEmpty())
        return 0;
    int cjk = 0, ascii = 0;
    for (const QChar &c : text) {
        if (c.unicode() >= 0x2E80)
            ++cjk;
        else if (c.unicode() < 0x80)
            ++ascii;
    }
    return cjk + ascii / 4 + 2;
}

int AiRequestHandler::modelContextLimit(const QString &model)
{
    struct Entry { const char *prefix; int limit; };
    static const Entry entries[] = {
        {"claude-3-opus-20240229", 200000},
        {"claude-3-opus-4-7",      200000},
        {"claude-sonnet-4-6",      200000},
        {"claude-haiku-4-5",       200000},
        {"claude-3-5-sonnet",      200000},
        {"claude-3-5-haiku",       200000},
        {"claude-3-sonnet",        200000},
        {"claude-3-haiku",         200000},
        {"claude-2",               100000},
        {"claude",                 200000},
        {"gpt-4-turbo",            128000},
        {"gpt-4o-mini",            128000},
        {"gpt-4o",                 128000},
        {"gpt-4",                   8192},
        {"gpt-3.5-turbo",           16385},
        {"deepseek-chat",           65536},
        {"deepseek-reasoner",       65536},
        {"gemini",                 1048576},
        {nullptr, 0}
    };
    for (const Entry *e = entries; e->prefix; ++e) {
        if (model.contains(QLatin1String(e->prefix)))
            return e->limit;
    }
    return 128000;
}

QList<Message> AiRequestHandler::pruneContextWindow(const QList<Message> &history,
                                                     const QString &model,
                                                     int maxResponseTokens,
                                                     const QString &systemPrompt)
{
    const int contextLimit = modelContextLimit(model);
    const int systemTokens = estimateTokens(systemPrompt);

    int available = contextLimit - maxResponseTokens - systemTokens;
    available = qMax(available * 9 / 10, 2048);

    QList<Message> window;
    int totalTokens = 0;
    for (int i = history.size() - 1; i >= 0; --i) {
        const int msgTokens = estimateTokens(history[i].content) + 8;
        if (totalTokens + msgTokens > available && !window.isEmpty())
            break;
        window.prepend(history[i]);
        totalTokens += msgTokens;
    }
    return window;
}

// ═══════════════════════════════════════════════════════════════════════

AiRequestHandler::AiRequestHandler(QObject *parent)
    : QObject(parent)
{
}

AiRequestHandler::~AiRequestHandler()
{
    if (m_aiProvider) {
        m_aiProvider->disconnect();
        m_aiProvider->deleteLater();
        m_aiProvider = nullptr;
    }
}

void AiRequestHandler::setAiPanel(AiPanel *panel)
{
    m_aiPanel = panel;
}

void AiRequestHandler::setTabManager(TabManager *manager)
{
    m_tabManager = manager;
}

void AiRequestHandler::setSettingsManager(SettingsManager *settings)
{
    m_settings = settings;
}

void AiRequestHandler::startAiRequest(AiAction action, const QString &freeQuery)
{
    if (!m_aiPanel || !m_tabManager || !m_settings)
        return;

    // 1. Abort any ongoing request
    if (m_aiStreaming)
        abortAiRequest();

    m_aiStreaming = true;
    emit streamingStateChanged(true);

    // 2. Collect context from current editor
    EditorWidget *editor = m_tabManager->currentEditor();
    ContextBundle ctx;
    if (editor)
        ctx = AiContextManager::collectContext(editor);

    // 3. Build prompt
    PromptBundle prompt = buildPrompt(action, ctx, freeQuery);

    // 4. Read AI settings
    const QString apiKey = m_settings->aiApiKey();
    if (apiKey.isEmpty()) {
        m_aiPanel->addAssistantMessage(tr("请先在设置 → AI 服务中配置 API Key"));
        m_aiStreaming = false;
        emit streamingStateChanged(false);
        return;
    }

    const QString providerType = m_settings->value("ai.provider_type",
        ConfigManager::instance().aiProviderType()).toString();
    const QString model = m_settings->value("ai.model",
        ConfigManager::instance().aiModel()).toString();
    const QString endpoint = m_settings->value("ai.endpoint",
        ConfigManager::instance().aiEndpoint()).toString();
    const int maxTokens = m_settings->value("ai.max_tokens",
        ConfigManager::instance().aiMaxTokens()).toInt();

    // 5. Create provider (always recreate to pick up settings changes)
    if (m_aiProvider) {
        m_aiProvider->disconnect();
        m_aiProvider->deleteLater();
        m_aiProvider = nullptr;
    }

    AiProviderFactory::ProviderType type = AiProviderFactory::typeFromString(providerType);
    m_aiProvider = AiProviderFactory::createProvider(type, this);

    // 6. Configure provider
    m_aiProvider->setApiKey(apiKey);
    m_aiProvider->setModel(model);
    m_aiProvider->setMaxTokens(maxTokens);
    m_aiProvider->setSystemPrompt(prompt.systemPrompt);

    if (auto *anthropic = qobject_cast<AnthropicProvider*>(m_aiProvider)) {
        anthropic->setEndpoint(endpoint);
    } else if (auto *openai = qobject_cast<OpenAiProvider*>(m_aiProvider)) {
        openai->setEndpoint(endpoint);
    }

    // 7. Connect provider signals
    connect(m_aiProvider, &AiProvider::partialResponse,
            this, &AiRequestHandler::onAiPartialResponse);
    connect(m_aiProvider, &AiProvider::finished,
            this, &AiRequestHandler::onAiFinished);
    connect(m_aiProvider, &AiProvider::error,
            this, &AiRequestHandler::onAiError);

    // 8. Display user message in chat
    QString userDisplayText;
    if (action == AiAction::FreeChat) {
        userDisplayText = freeQuery;
        m_aiPanel->addUserMessage(freeQuery);
    } else {
        const ActionInfo *info = findActionInfo(action);
        userDisplayText = info ? tr(info->label) : tr("AI 操作");
        if (!ctx.selectedText.isEmpty()) {
            userDisplayText += QStringLiteral("\n\n```\n") + ctx.selectedText + QStringLiteral("\n```");
        }
        m_aiPanel->addUserMessage(userDisplayText);
    }

    // 9. Persist user message to AiHistoryManager
    {
        auto &mgr = AiHistoryManager::instance();
        if (mgr.currentConversationId().isEmpty()) {
            QString convTitle;
            if (action == AiAction::FreeChat) {
                convTitle = freeQuery.left(30).trimmed();
                if (convTitle.isEmpty()) convTitle = tr("新对话");
            } else {
                const ActionInfo *info = findActionInfo(action);
                convTitle = info ? tr(info->label) : tr("AI 操作");
            }
            QString filePath = (action != AiAction::FreeChat) ? ctx.filePath : QString();
            mgr.createConversation(convTitle, filePath);
        }

        AiMessage histMsg;
        histMsg.role = MessageRole::User;
        histMsg.content = (action == AiAction::FreeChat) ? freeQuery : prompt.userPrompt;
        histMsg.timestampMs = QDateTime::currentMSecsSinceEpoch();
        mgr.appendMessage(mgr.currentConversationId(), histMsg);
    }

    // 10. Add empty assistant bubble as streaming target
    m_aiPanel->addAssistantMessage(QString());

    // 11. Append current user message to canonical full history (unpruned)
    Message userMsg;
    userMsg.role = MessageRole::User;
    userMsg.content = (action == AiAction::FreeChat) ? freeQuery : prompt.userPrompt;
    m_aiHistory.append(userMsg);

    // 12. Build token-aware context window for the API call
    QList<Message> messages = pruneContextWindow(m_aiHistory, model, maxTokens, prompt.systemPrompt);

    // 13. Disable input and start streaming
    m_aiPanel->setInputEnabled(false);
    m_aiProvider->chatStream(messages);
}

void AiRequestHandler::abortAiRequest()
{
    if (m_aiProvider) {
        m_aiProvider->cancel();
        m_aiProvider->disconnect();
    }
    m_aiStreaming = false;
    emit streamingStateChanged(false);
    if (m_aiPanel)
        m_aiPanel->setInputEnabled(true);
}

void AiRequestHandler::clearConversation()
{
    abortAiRequest();
    m_aiHistory.clear();
    if (m_aiPanel)
        m_aiPanel->clearChat();

    auto &mgr = AiHistoryManager::instance();
    QString currentId = mgr.currentConversationId();
    if (currentId.isEmpty())
        return;  // No conversation exists, nothing to clear

    AiConversation conv = mgr.conversationById(currentId);
    if (conv.isValid() && conv.messageCount == 0)
        return;  // Already empty, no need to create a new empty conversation

    mgr.clearCurrentConversation();
}

void AiRequestHandler::newConversation()
{
    abortAiRequest();
    m_aiHistory.clear();
    if (m_aiPanel)
        m_aiPanel->clearChat();
    AiHistoryManager::instance().createConversation(tr("新对话"), {});
}

void AiRequestHandler::clearHistory()
{
    m_aiHistory.clear();
}

void AiRequestHandler::onAiPartialResponse(const QString &text)
{
    if (text.isEmpty())
        return;
    if (m_aiPanel)
        m_aiPanel->appendToLastAssistant(text);
}

void AiRequestHandler::onAiFinished()
{
    if (m_aiPanel)
        m_aiPanel->flushPendingUpdates();

    // Persist assistant response to AiHistoryManager
    {
        auto &mgr = AiHistoryManager::instance();
        if (!mgr.currentConversationId().isEmpty()) {
            QString content = m_aiPanel ? m_aiPanel->lastAssistantContent() : QString();
            if (!content.isEmpty()) {
                AiMessage assistantMsg;
                assistantMsg.role = MessageRole::Assistant;
                assistantMsg.content = content;
                assistantMsg.timestampMs = QDateTime::currentMSecsSinceEpoch();
                mgr.appendMessage(mgr.currentConversationId(), assistantMsg);
            }
        }
    }

    // Save assistant response to in-memory history for subsequent turns
    if (!m_aiHistory.isEmpty()) {
        QString content = m_aiPanel ? m_aiPanel->lastAssistantContent() : QString();
        if (!content.isEmpty()) {
            Message assistantMsg;
            assistantMsg.role = MessageRole::Assistant;
            assistantMsg.content = content;
            m_aiHistory.append(assistantMsg);
        }
    }

    m_aiStreaming = false;
    emit streamingStateChanged(false);
    if (m_aiPanel)
        m_aiPanel->setInputEnabled(true);
}

void AiRequestHandler::onAiError(const QString &message)
{
    if (!m_aiPanel)
        return;

    if (m_aiPanel->hasStreamingTarget()) {
        m_aiPanel->appendToLastAssistant(
            QStringLiteral("**") + tr("错误") + QStringLiteral("：**") + message);
    } else {
        m_aiPanel->addAssistantMessage(tr("错误：") + message);
    }

    m_aiStreaming = false;
    emit streamingStateChanged(false);
    m_aiPanel->setInputEnabled(true);
}

void AiRequestHandler::loadAiConversation(const QString &convId)
{
    if (!m_aiPanel)
        return;

    // Abort any ongoing request
    if (m_aiStreaming)
        abortAiRequest();

    // Clear current state
    m_aiPanel->clearChat();
    m_aiHistory.clear();

    auto &mgr = AiHistoryManager::instance();

    // Set as current conversation
    mgr.setCurrentConversation(convId);

    // Load and display messages
    const QList<AiMessage> messages = mgr.loadMessages(convId);
    for (const auto &aiMsg : messages) {
        if (aiMsg.role == MessageRole::Assistant)
            m_aiPanel->addAssistantMessage(aiMsg.content);
        else
            m_aiPanel->addUserMessage(aiMsg.content);

        // Rebuild m_aiHistory for API context
        Message msg;
        msg.role = aiMsg.role;
        msg.content = aiMsg.content;
        m_aiHistory.append(msg);
    }

    // Switch back to chat tab
    m_aiPanel->setCurrentTab(AiPanel::ChatTab);
}
