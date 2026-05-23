#ifndef ERRORJOURNAL_H
#define ERRORJOURNAL_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "judgeengine.h"

class AiProvider;
struct SubmissionResult;

// ── Data structures ────────────────────────────────────────────────

struct ErrorRecord {
    QString id;                // UUID
    QString problemName;       // 题目名（从 test folder 推断）
    QString sourceFile;        // 被评测的源文件路径
    QString testFolder;        // 测试用例文件夹路径
    QString testCaseName;      // 测试用例名（如 "1", "test1"）
    QString statusCode;        // WA / RE / TLE / MLE
    qint64 elapsedMs = 0;
    quint64 memoryKb = 0;
    QString inputData;         // 测试用例输入
    QString actualOutput;
    QString expectedOutput;
    QString detail;            // 错误详情
    QString aiAnalysis;        // AI 分析结果（异步填充）
    QStringList tags;          // AI 自动提取的知识点标签
    QDateTime timestamp;
    bool reviewed = false;     // 用户是否已查阅
};

// ── Singleton Journal ──────────────────────────────────────────────

class ErrorJournal : public QObject
{
    Q_OBJECT

public:
    static ErrorJournal &instance();

    // 记录一次评测失败（由 JudgePanel 或 JudgeEngine 触发）
    void recordFailure(const JudgeEngine::TestResult &result,
                       const QString &sourceFile,
                       const QString &testFolder);

    // 记录 OpenJudge 提交失败（无本地 I/O 数据时使用）
    void recordOpenJudgeFailure(const SubmissionResult &result,
                                const QString &sourceFile,
                                const QString &problemName,
                                const QString &problemUrl,
                                const QString &sourceCode);

    // 记录 OpenJudge 提交失败（含本地测试 I/O 数据）
    void recordOpenJudgeFailure(const JudgeEngine::TestResult &result,
                                const QString &sourceFile,
                                const QString &problemName,
                                const QString &problemUrl);

    // 请求 AI 分析错误原因
    void requestAnalysis(const QString &recordId);

    // 查询
    QVector<ErrorRecord> allRecords() const;
    QVector<ErrorRecord> recordsByProblem(const QString &problemName) const;
    QVector<ErrorRecord> recordsByStatus(const QString &statusCode) const;
    ErrorRecord recordById(const QString &id) const;

    // 持久化
    void load();
    void save();

    // 管理
    void deleteRecord(const QString &id);
    void clearAll();
    int recordCount() const { return m_records.size(); }
    void setRecordReviewed(const QString &id, bool reviewed);

signals:
    void analysisReady(const QString &recordId);
    void recordsChanged();

private:
    ErrorJournal(QObject *parent = nullptr);
    ~ErrorJournal() override;
    ErrorJournal(const ErrorJournal &) = delete;
    ErrorJournal &operator=(const ErrorJournal &) = delete;

    QString storagePath() const;
    AiProvider *createConfiguredProvider(const QString &systemPrompt);

    QString m_storagePath;
    QVector<ErrorRecord> m_records;
};

#endif // ERRORJOURNAL_H
