#ifndef JUDGEENGINE_H
#define JUDGEENGINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>

class JudgeEngine : public QObject
{
    Q_OBJECT

public:
    struct TestCase {
        QString name;
        QString inputFile;
        QString expectedOutputFile;
    };

    struct TestResult {
        QString name;
        bool passed = false;
        QString statusCode; // AC, WA, RE, TLE, MLE
        qint64 elapsedMs = 0;
        quint64 memoryKb = 0;
        QString actualOutput;
        QString expectedOutput;
        QString detail;
    };

    explicit JudgeEngine(QObject *parent = nullptr);
    ~JudgeEngine();

    void setSourceFile(const QString &filePath) { m_sourceFile = filePath; }
    void setTestFolder(const QString &folderPath) { m_testFolder = folderPath; }
    QString testFolder() const { return m_testFolder; }
    QString sourceFile() const { return m_sourceFile; }

    QVector<TestCase> discoverTests() const;
    void start();
    void stop();
    bool isRunning() const { return m_running; }

    const QVector<TestResult> &results() const { return m_results; }

    // Time limit in milliseconds (default 1000)
    void setTimeLimit(int ms) { m_timeLimitMs = ms; }
    int timeLimit() const { return m_timeLimitMs; }

signals:
    void judgeOutput(const QString &text, bool isStderr);
    void compileFinished(bool success, const QString &errorOutput);
    void testStarted(int index, const QString &testName);
    void testFinished(int index, const JudgeEngine::TestResult &result);
    void allTestsFinished(int passed, int total);
    void judgeStopped();

private slots:
    void onCompileProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onCompileProcessError(QProcess::ProcessError error);
    void onCompileReadyReadStderr();
    void onTestProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onTestProcessError(QProcess::ProcessError error);
    void onTestReadyReadStdout();
    void onTestTimeout();
    void onMemoryCheck();

private:
    void runCompile();
    void runNextTest();
    void finishCurrentTest(bool passed, const QString &statusCode, const QString &detail);
    void cleanupCompileProcess();
    void cleanupTestProcess();
    QString readFile(const QString &path) const;
    static bool outputMatches(const QString &actual, const QString &expected);

    QString m_sourceFile;
    QString m_testFolder;
    QVector<TestCase> m_tests;
    QVector<TestResult> m_results;
    int m_currentTestIndex = -1;
    int m_passedCount = 0;
    bool m_running = false;
    int m_timeLimitMs = 1000;

    // Compile process
    QProcess *m_compileProcess = nullptr;
    QString m_compileStderr;

    // Test process
    QProcess *m_testProcess = nullptr;
    QTimer *m_testTimer = nullptr;
    QElapsedTimer m_testElapsed;
    QString m_actualOutput;
    bool m_testHandled = false;

    // Memory monitoring
    QTimer *m_memPollTimer = nullptr;
    quint64 m_peakMemoryKb = 0;
    quint64 m_memoryLimitKb = 65536;
    bool m_mleDetected = false;
};

#endif // JUDGEENGINE_H
