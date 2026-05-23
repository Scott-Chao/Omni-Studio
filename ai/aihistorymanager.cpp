#include "aihistorymanager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <QCoreApplication>
#include <QDateTime>

// ═══════════════════════════════════════════════════════════════════════

AiHistoryManager &AiHistoryManager::instance()
{
    static AiHistoryManager mgr;
    return mgr;
}

AiHistoryManager::AiHistoryManager()
    : QObject(nullptr)
{
    // Ensure storage directory exists
    QDir().mkpath(storageDir());
    loadIndex();
}

AiHistoryManager::~AiHistoryManager() = default;

// ── Path helpers ──────────────────────────────────────────────────────

QString AiHistoryManager::storageDir() const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/ai_history");
}

QString AiHistoryManager::conversationFilePath(const QString &id) const
{
    return storageDir() + QStringLiteral("/conv_") + id + QStringLiteral(".json");
}

// ── Index persistence ─────────────────────────────────────────────────

void AiHistoryManager::loadIndex()
{
    m_conversations.clear();
    m_currentConvId.clear();

    QFile file(storageDir() + QStringLiteral("/index.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
    for (const QJsonValue &val : arr) {
        m_conversations.append(AiConversation::fromJson(val.toObject()));
    }
}

void AiHistoryManager::saveIndex()
{
    QJsonArray arr;
    for (const auto &conv : m_conversations)
        arr.append(conv.toJson());

    const QString path = storageDir() + QStringLiteral("/index.json");
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

// ── Conversation management ───────────────────────────────────────────

QString AiHistoryManager::createConversation(const QString &title, const QString &sourceFile)
{
    AiConversation conv;
    conv.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    conv.title = title.isEmpty() ? tr("新对话") : title;
    conv.sourceFile = sourceFile;
    conv.createdAt = QDateTime::currentDateTime();
    conv.updatedAt = conv.createdAt;
    conv.messageCount = 0;

    m_conversations.prepend(conv);
    m_currentConvId = conv.id;

    // Create empty message file
    QFile file(conversationFilePath(conv.id));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(QJsonArray()).toJson(QJsonDocument::Indented));

    saveIndex();
    emit conversationListChanged();

    return conv.id;
}

void AiHistoryManager::deleteConversation(const QString &id)
{
    // Remove from index
    auto it = std::remove_if(m_conversations.begin(), m_conversations.end(),
                              [&](const AiConversation &c) { return c.id == id; });
    if (it == m_conversations.end())
        return;

    m_conversations.erase(it, m_conversations.end());

    // Remove message file
    QFile::remove(conversationFilePath(id));

    if (m_currentConvId == id)
        m_currentConvId.clear();

    saveIndex();
    emit conversationListChanged();
}

void AiHistoryManager::renameConversation(const QString &id, const QString &newTitle)
{
    for (auto &conv : m_conversations) {
        if (conv.id == id) {
            conv.title = newTitle;
            conv.updatedAt = QDateTime::currentDateTime();
            break;
        }
    }
    saveIndex();
    emit conversationListChanged();
}

void AiHistoryManager::clearCurrentConversation()
{
    if (!m_currentConvId.isEmpty())
        deleteConversation(m_currentConvId);

    createConversation(tr("新对话"), QString());
}

// ── Message management ────────────────────────────────────────────────

void AiHistoryManager::appendMessage(const QString &convId, const AiMessage &msg)
{
    // Load existing messages
    QList<AiMessage> messages = loadMessages(convId);
    messages.append(msg);

    // Write back
    QJsonArray arr;
    for (const auto &m : messages)
        arr.append(m.toJson());

    const QString path = conversationFilePath(convId);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));

    // Update conversation metadata
    for (auto &conv : m_conversations) {
        if (conv.id == convId) {
            conv.messageCount = messages.size();
            conv.updatedAt = QDateTime::currentDateTime();
            break;
        }
    }

    saveIndex();
}

QList<AiMessage> AiHistoryManager::loadMessages(const QString &convId) const
{
    QList<AiMessage> messages;

    QFile file(conversationFilePath(convId));
    if (!file.open(QIODevice::ReadOnly))
        return messages;

    const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
    for (const QJsonValue &val : arr)
        messages.append(AiMessage::fromJson(val.toObject()));

    return messages;
}

// ── Query ─────────────────────────────────────────────────────────────

QList<AiConversation> AiHistoryManager::allConversations() const
{
    // Return sorted by updatedAt descending
    QList<AiConversation> sorted = m_conversations;
    std::sort(sorted.begin(), sorted.end(), [](const AiConversation &a, const AiConversation &b) {
        return a.updatedAt > b.updatedAt;
    });
    return sorted;
}

QList<AiConversation> AiHistoryManager::conversationsByFile(const QString &filePath) const
{
    if (filePath.isEmpty())
        return allConversations();

    QList<AiConversation> filtered;
    for (const auto &conv : m_conversations) {
        if (conv.sourceFile == filePath)
            filtered.append(conv);
    }

    std::sort(filtered.begin(), filtered.end(), [](const AiConversation &a, const AiConversation &b) {
        return a.updatedAt > b.updatedAt;
    });
    return filtered;
}

AiConversation AiHistoryManager::conversationById(const QString &id) const
{
    for (const auto &conv : m_conversations) {
        if (conv.id == id)
            return conv;
    }
    return AiConversation();
}

// ── Export ────────────────────────────────────────────────────────────

QString AiHistoryManager::exportToMarkdown(const QString &convId) const
{
    const AiConversation conv = conversationById(convId);
    if (!conv.isValid())
        return {};

    const QList<AiMessage> messages = loadMessages(convId);

    QString md;
    md += QStringLiteral("# ") + conv.title + QStringLiteral("\n\n");
    md += QStringLiteral("**时间**：") + conv.createdAt.toString(QStringLiteral("yyyy-MM-dd hh:mm")) + QStringLiteral("\n");
    if (!conv.sourceFile.isEmpty())
        md += QStringLiteral("**来源**：") + conv.sourceFile + QStringLiteral("\n");
    md += QStringLiteral("\n---\n\n");

    for (const auto &msg : messages) {
        const QDateTime ts = QDateTime::fromMSecsSinceEpoch(msg.timestampMs);
        const QString timeStr = ts.toString(QStringLiteral("hh:mm"));

        switch (msg.role) {
        case MessageRole::Assistant:
            md += QStringLiteral("### 🤖 AI 助手 — ") + timeStr + QStringLiteral("\n");
            break;
        case MessageRole::User:
            md += QStringLiteral("### 👤 你 — ") + timeStr + QStringLiteral("\n");
            break;
        case MessageRole::System:
            md += QStringLiteral("### ⚙️ 系统 — ") + timeStr + QStringLiteral("\n");
            break;
        }

        md += msg.content + QStringLiteral("\n\n");
    }

    return md;
}
