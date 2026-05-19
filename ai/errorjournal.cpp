#include "errorjournal.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

// ── Singleton ─────────────────────────────────────────────────────

ErrorJournal &ErrorJournal::instance()
{
    static ErrorJournal s_instance;
    return s_instance;
}

ErrorJournal::ErrorJournal(QObject *parent)
    : QObject(parent)
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
    return QCoreApplication::applicationDirPath()
        + QStringLiteral("/error_journal/records.json");
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
    rec.statusCode = result.statusCode;
    rec.elapsedMs = result.elapsedMs;
    rec.memoryKb = result.memoryKb;
    rec.actualOutput = result.actualOutput;
    rec.expectedOutput = result.expectedOutput;
    rec.detail = result.detail;
    rec.timestamp = QDateTime::currentDateTime();

    m_records.append(rec);
    save();

    emit recordAdded(rec);
}

// ── AI analysis (stub — will be implemented in Phase 2 Step 6) ────

void ErrorJournal::requestAnalysis(const QString &recordId)
{
    Q_UNUSED(recordId)
    // 将在 Phase 2 Step 6 中实现：调用 AiProvider::chatStream
    // 分析结果通过 analysisReady 信号通知
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
    rec.statusCode    = obj.value(QStringLiteral("statusCode")).toString();
    rec.elapsedMs     = static_cast<qint64>(obj.value(QStringLiteral("elapsedMs")).toDouble());
    rec.memoryKb      = static_cast<quint64>(obj.value(QStringLiteral("memoryKb")).toDouble());
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
    obj[QStringLiteral("statusCode")]    = rec.statusCode;
    obj[QStringLiteral("elapsedMs")]     = static_cast<double>(rec.elapsedMs);
    obj[QStringLiteral("memoryKb")]      = static_cast<double>(rec.memoryKb);
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
    }
}

void ErrorJournal::clearAll()
{
    m_records.clear();
    save();
    emit recordsCleared();
}
