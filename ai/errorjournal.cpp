#include "errorjournal.h"
#include "aiproviderfactory.h"
#include "aiprovider.h"
#include "anthropicprovider.h"
#include "openaiprovider.h"
#include "prompttemplates.h"
#include "aicontextmanager.h"
#include "configmanager.h"
#include "settingsmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <algorithm>
#include <memory>

// ── Singleton ─────────────────────────────────────────────────────

ErrorJournal &ErrorJournal::instance()
{
    static ErrorJournal s_instance;
    return s_instance;
}

ErrorJournal::ErrorJournal(QObject *parent)
    : QObject(parent)
    , m_storagePath(QCoreApplication::applicationDirPath()
                    + QStringLiteral("/error_journal/records.json"))
{
    load();
}

ErrorJournal::~ErrorJournal()
{
    save();
}

// ── Internal helpers ──────────────────────────────────────────────

QString ErrorJournal::storagePath() const
{
    return m_storagePath;
}

// ── Create configured AI provider ──────────────────────────────────

AiProvider *ErrorJournal::createConfiguredProvider(const QString &systemPrompt)
{
    SettingsManager settings;
    const QString apiKey = settings.aiApiKey();
    if (apiKey.isEmpty())
        return nullptr;

    const QString providerType = settings.value(QStringLiteral("ai.provider_type"),
        ConfigManager::instance().aiProviderType()).toString();
    const QString model = settings.value(QStringLiteral("ai.model"),
        ConfigManager::instance().aiModel()).toString();
    const QString endpoint = settings.value(QStringLiteral("ai.endpoint"),
        ConfigManager::instance().aiEndpoint()).toString();
    const int maxTokens = settings.value(QStringLiteral("ai.max_tokens"),
        ConfigManager::instance().aiMaxTokens()).toInt();

    AiProviderFactory::ProviderType type = AiProviderFactory::typeFromString(providerType);
    AiProvider *provider = AiProviderFactory::createProvider(type, this);
    if (!provider)
        return nullptr;

    provider->setApiKey(apiKey);
    provider->setModel(model);
    provider->setMaxTokens(maxTokens);
    provider->setSystemPrompt(systemPrompt);

    if (auto *anthropic = qobject_cast<AnthropicProvider *>(provider))
        anthropic->setEndpoint(endpoint);
    else if (auto *openai = qobject_cast<OpenAiProvider *>(provider))
        openai->setEndpoint(endpoint);

    return provider;
}

// ── Record failure ────────────────────────────────────────────────

void ErrorJournal::recordFailure(const JudgeEngine::TestResult &result,
                                  const QString &sourceFile,
                                  const QString &testFolder)
{
    ErrorRecord rec;
    rec.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    rec.problemName = QDir(testFolder).dirName();
    rec.sourceFile = sourceFile;
    rec.testFolder = testFolder;
    rec.testCaseName = result.name;
    rec.statusCode = result.statusCode;
    rec.elapsedMs = result.elapsedMs;
    rec.memoryKb = result.memoryKb;
    rec.actualOutput = result.actualOutput;
    rec.expectedOutput = result.expectedOutput;
    rec.detail = result.detail;
    rec.inputData = result.inputData;
    rec.timestamp = QDateTime::currentDateTime();

    m_records.append(rec);
    save();

    emit recordsChanged();
}

// ── AI analysis ────────────────────────────────────────────────────

void ErrorJournal::requestAnalysis(const QString &recordId)
{
    // 1. Find record
    auto it = std::find_if(m_records.begin(), m_records.end(),
                           [&](const ErrorRecord &r) { return r.id == recordId; });
    if (it == m_records.end())
        return;

    const ErrorRecord &rec = *it;
    QString recId = rec.id;

    // 2. Read source code from disk
    QString sourceCode;
    QFile srcFile(rec.sourceFile);
    if (srcFile.open(QIODevice::ReadOnly))
        sourceCode = QString::fromUtf8(srcFile.readAll());

    // 3. Build ContextBundle for the prompt template
    ContextBundle ctx;
    ctx.filePath = rec.sourceFile;
    ctx.fileContent = sourceCode;
    ctx.language = AiContextManager::languageForFile(rec.sourceFile);
    ctx.errorStatusCode = rec.statusCode;
    ctx.elapsedMs = rec.elapsedMs;
    ctx.memoryKb = rec.memoryKb;
    ctx.actualOutput = rec.actualOutput;
    ctx.expectedOutput = rec.expectedOutput;
    ctx.inputData = rec.inputData;
    ctx.errorDetail = rec.detail;

    // 4. Build prompt
    PromptBundle prompt = buildPrompt(AiAction::ErrorAnalysis, ctx);

    // 5. Helper: finalize analysis result (set text, save, emit)
    auto finalizeAnalysis = [this](const QString &id, const QString &text) {
        for (auto &r : m_records) {
            if (r.id == id) {
                r.aiAnalysis = text;
                r.reviewed = true;
                save();
                break;
            }
        }
        emit analysisReady(id);
    };

    // 6. Create and configure provider
    AiProvider *provider = createConfiguredProvider(prompt.systemPrompt);
    if (!provider) {
        SettingsManager settings;
        finalizeAnalysis(recId, settings.aiApiKey().isEmpty()
            ? QStringLiteral("⚠ 请先在设置 → AI 服务中配置 API Key 后再进行错题分析。")
            : QStringLiteral("分析失败：无法创建 AI Provider。"));
        return;
    }

    // 7. Collect streaming response into a shared string
    auto analysis = std::make_shared<QString>();

    connect(provider, &AiProvider::partialResponse, provider,
            [analysis](const QString &text) { *analysis += text; });

    connect(provider, &AiProvider::finished, this,
            [recId, analysis, provider, finalizeAnalysis]() {
        finalizeAnalysis(recId, *analysis);
        provider->deleteLater();
    });

    connect(provider, &AiProvider::error, this,
            [recId, provider, finalizeAnalysis](const QString &errorMsg) {
        finalizeAnalysis(recId, QStringLiteral("分析失败：%1").arg(errorMsg));
        provider->deleteLater();
    });

    // 8. Build message list and start streaming
    QList<Message> messages;
    Message userMsg;
    userMsg.role = MessageRole::User;
    userMsg.content = prompt.userPrompt;
    messages.append(userMsg);

    provider->chatStream(messages);
}

// ── Query ──────────────────────────────────────────────────────────

QVector<ErrorRecord> ErrorJournal::allRecords() const
{
    return m_records;
}

QVector<ErrorRecord> ErrorJournal::recordsByProblem(const QString &problemName) const
{
    QVector<ErrorRecord> filtered;
    for (const auto &rec : m_records) {
        if (rec.problemName == problemName)
            filtered.append(rec);
    }
    return filtered;
}

QVector<ErrorRecord> ErrorJournal::recordsByStatus(const QString &statusCode) const
{
    QVector<ErrorRecord> filtered;
    for (const auto &rec : m_records) {
        if (rec.statusCode == statusCode)
            filtered.append(rec);
    }
    return filtered;
}

ErrorRecord ErrorJournal::recordById(const QString &id) const
{
    for (const auto &rec : m_records) {
        if (rec.id == id)
            return rec;
    }
    return ErrorRecord{};
}

// ── Persistence ───────────────────────────────────────────────────

static ErrorRecord recordFromJson(const QJsonObject &obj)
{
    ErrorRecord rec;
    rec.id            = obj.value(QStringLiteral("id")).toString();
    rec.problemName   = obj.value(QStringLiteral("problemName")).toString();
    rec.sourceFile    = obj.value(QStringLiteral("sourceFile")).toString();
    rec.testFolder    = obj.value(QStringLiteral("testFolder")).toString();
    rec.testCaseName  = obj.value(QStringLiteral("testCaseName")).toString();
    rec.statusCode    = obj.value(QStringLiteral("statusCode")).toString();
    rec.elapsedMs     = static_cast<qint64>(obj.value(QStringLiteral("elapsedMs")).toDouble());
    rec.memoryKb      = static_cast<quint64>(obj.value(QStringLiteral("memoryKb")).toDouble());
    rec.inputData     = obj.value(QStringLiteral("inputData")).toString();
    rec.actualOutput  = obj.value(QStringLiteral("actualOutput")).toString();
    rec.expectedOutput = obj.value(QStringLiteral("expectedOutput")).toString();
    rec.detail        = obj.value(QStringLiteral("detail")).toString();
    rec.aiAnalysis    = obj.value(QStringLiteral("aiAnalysis")).toString();

    const QJsonArray tagsArr = obj.value(QStringLiteral("tags")).toArray();
    for (const auto &t : tagsArr)
        rec.tags.append(t.toString());

    rec.timestamp = QDateTime::fromString(
        obj.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
    rec.reviewed = obj.value(QStringLiteral("reviewed")).toBool();

    return rec;
}

static QJsonObject recordToJson(const ErrorRecord &rec)
{
    QJsonObject obj;
    obj[QStringLiteral("id")]            = rec.id;
    obj[QStringLiteral("problemName")]   = rec.problemName;
    obj[QStringLiteral("sourceFile")]    = rec.sourceFile;
    obj[QStringLiteral("testFolder")]    = rec.testFolder;
    obj[QStringLiteral("testCaseName")]  = rec.testCaseName;
    obj[QStringLiteral("statusCode")]    = rec.statusCode;
    obj[QStringLiteral("elapsedMs")]     = static_cast<double>(rec.elapsedMs);
    obj[QStringLiteral("memoryKb")]      = static_cast<double>(rec.memoryKb);
    obj[QStringLiteral("inputData")]     = rec.inputData;
    obj[QStringLiteral("actualOutput")]   = rec.actualOutput;
    obj[QStringLiteral("expectedOutput")] = rec.expectedOutput;
    obj[QStringLiteral("detail")]        = rec.detail;
    obj[QStringLiteral("aiAnalysis")]    = rec.aiAnalysis;

    QJsonArray tagsArr;
    for (const auto &tag : rec.tags)
        tagsArr.append(tag);
    obj[QStringLiteral("tags")] = tagsArr;

    obj[QStringLiteral("timestamp")] = rec.timestamp.toString(Qt::ISODate);
    obj[QStringLiteral("reviewed")]  = rec.reviewed;

    return obj;
}

void ErrorJournal::save()
{
    const QString path = storagePath();

    // Ensure directory exists
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject root;
    root[QStringLiteral("version")] = 1;

    QJsonArray arr;
    for (const auto &rec : m_records)
        arr.append(recordToJson(rec));
    root[QStringLiteral("records")] = arr;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void ErrorJournal::load()
{
    const QString path = storagePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return;

    const QJsonObject root = doc.object();
    const QJsonArray arr = root.value(QStringLiteral("records")).toArray();

    m_records.clear();
    m_records.reserve(arr.size());
    for (const auto &v : arr)
        m_records.append(recordFromJson(v.toObject()));
}

// ── Management ────────────────────────────────────────────────────

void ErrorJournal::deleteRecord(const QString &id)
{
    auto it = std::remove_if(m_records.begin(), m_records.end(),
                             [&](const ErrorRecord &r) { return r.id == id; });
    if (it != m_records.end()) {
        m_records.erase(it, m_records.end());
        save();
        emit recordsChanged();
    }
}

void ErrorJournal::clearAll()
{
    m_records.clear();
    save();
    emit recordsChanged();
}

void ErrorJournal::setRecordReviewed(const QString &id, bool reviewed)
{
    for (auto &r : m_records) {
        if (r.id == id) {
            r.reviewed = reviewed;
            save();
            return;
        }
    }
}
