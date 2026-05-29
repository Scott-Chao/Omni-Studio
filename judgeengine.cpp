#include "judgeengine.h"
#include "compilerutils.h"
#include "configmanager.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QCoreApplication>
#include <QEventLoop>

#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

JudgeEngine::JudgeEngine(QObject *parent)
    : QObject(parent)
{
    m_testTimer = new QTimer(this);
    m_testTimer->setSingleShot(true);
    connect(m_testTimer, &QTimer::timeout, this, &JudgeEngine::onTestTimeout);

    m_memPollTimer = new QTimer(this);
    m_memPollTimer->setInterval(ConfigManager::instance().judgeMemoryPollMs());
    connect(m_memPollTimer, &QTimer::timeout, this, &JudgeEngine::onMemoryCheck);
}

JudgeEngine::~JudgeEngine()
{
    stop();
}

QVector<JudgeEngine::TestCase> JudgeEngine::discoverTests() const
{
    QVector<TestCase> tests;
    QDir dir(m_testFolder);
    if (!dir.exists())
        return tests;

    const QStringList inFiles = dir.entryList({ConfigManager::instance().judgeInputFilePattern()}, QDir::Files, QDir::Name);
    for (const QString &inFile : inFiles) {
        QFileInfo inInfo(dir.absoluteFilePath(inFile));
        const QString baseName = inInfo.completeBaseName();
        const QString outPath = dir.absoluteFilePath(baseName + QStringLiteral(".out"));
        QFileInfo outInfo(outPath);
        // Skip if .out does not exist or is empty (no expected output to compare)
        if (outInfo.exists() && outInfo.size() > 0) {
            TestCase tc;
            tc.name = baseName;
            tc.inputFile = inInfo.absoluteFilePath();
            tc.expectedOutputFile = outPath;
            tc.inputData = readFile(tc.inputFile);
            tc.expectedOutput = readFile(tc.expectedOutputFile);
            tests.append(tc);
        }
    }
    return tests;
}

void JudgeEngine::start()
{
    if (m_running)
        return;

    m_results.clear();
    m_passedCount = 0;
    m_currentTestIndex = -1;

    if (m_sourceFile.isEmpty()) {
        emit judgeOutput(tr("错误: 未设置源文件。\n"), true);
        emit allTestsFinished(0, 0);
        return;
    }
    if (m_testFolder.isEmpty() || !QDir(m_testFolder).exists()) {
        emit judgeOutput(tr("错误: 测试用例文件夹不存在。\n"), true);
        emit allTestsFinished(0, 0);
        return;
    }

    m_tests = discoverTests();
    if (m_tests.isEmpty()) {
        emit judgeOutput(tr("错误: 在文件夹中未找到 .in/.out 测试用例对: %1\n").arg(m_testFolder), true);
        emit allTestsFinished(0, 0);
        return;
    }

    m_running = true;
    emit judgeOutput(tr("找到 %1 个测试用例。\n").arg(m_tests.size()), false);

    m_isPython = QFileInfo(m_sourceFile).suffix().compare("py", Qt::CaseInsensitive) == 0;
    if (m_isPython) {
        emit judgeOutput(tr("Python 脚本，无需编译。\n"), false);
        emit compileFinished(true, QString());
        runWarmup();
    } else {
        runCompile();
    }
}

void JudgeEngine::stop()
{
    m_running = false;
    cleanupCompileProcess();
    cleanupWarmupProcess();
    cleanupTestProcess();
    if (m_testTimer)
        m_testTimer->stop();
    if (m_memPollTimer)
        m_memPollTimer->stop();
    emit judgeStopped();
}

// ---- Compile phase ----

void JudgeEngine::runCompile()
{
    cleanupCompileProcess();
    m_compileStderr.clear();

    CompilerInfo compiler = CompilerUtils::defaultCompiler();
    if (!compiler.available) {
        emit judgeOutput(tr("错误: 未检测到 C++ 编译器 (g++)。\n"), true);
        emit compileFinished(false, tr("未检测到 C++ 编译器 (g++ 或 MSVC)。"));
        m_running = false;
        emit allTestsFinished(0, m_tests.size());
        return;
    }

    const QString outputPath = CompilerUtils::getOutputPath(m_sourceFile);
    const QStringList args = CompilerUtils::getCompileArgs(compiler.id, m_sourceFile, outputPath);

    m_compileProcess = new QProcess(this);
    m_compileProcess->setProcessChannelMode(QProcess::SeparateChannels);
    m_compileProcess->setWorkingDirectory(QFileInfo(m_sourceFile).absolutePath());

    connect(m_compileProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &JudgeEngine::onCompileProcessFinished);
    connect(m_compileProcess, &QProcess::errorOccurred,
            this, &JudgeEngine::onCompileProcessError);
    connect(m_compileProcess, &QProcess::readyReadStandardError,
            this, &JudgeEngine::onCompileReadyReadStderr);

    emit judgeOutput(tr("编译中: %1 ...\n").arg(QFileInfo(m_sourceFile).fileName()), false);
    m_compileProcess->start(compiler.compilerPath, args);
}

void JudgeEngine::onCompileReadyReadStderr()
{
    m_compileStderr += QString::fromUtf8(m_compileProcess->readAllStandardError());
}

void JudgeEngine::onCompileProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
    if (success) {
        emit judgeOutput(tr("编译成功。\n"), false);
        emit compileFinished(true, QString());
        runWarmup(); // warmup before first test to populate OS cache
    } else {
        m_compileStderr += QString::fromUtf8(m_compileProcess->readAllStandardError());
        emit judgeOutput(tr("编译失败。\n"), true);
        emit judgeOutput(m_compileStderr, true);
        emit compileFinished(false, m_compileStderr);
        m_running = false;
        emit allTestsFinished(0, m_tests.size());
    }
    cleanupCompileProcess();
}

void JudgeEngine::onCompileProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    emit judgeOutput(tr("编译进程出错。\n"), true);
    emit compileFinished(false, m_compileStderr);
    m_running = false;
    emit allTestsFinished(0, m_tests.size());
    cleanupCompileProcess();
}

void JudgeEngine::cleanupCompileProcess()
{
    if (m_compileProcess) {
        if (m_compileProcess->state() != QProcess::NotRunning)
            m_compileProcess->kill();
        m_compileProcess->disconnect();
        m_compileProcess->deleteLater();
        m_compileProcess = nullptr;
    }
}

// ---- Warmup phase ----

void JudgeEngine::runWarmup()
{
    cleanupWarmupProcess();

    QString program;
    QStringList args;

    if (m_isPython) {
        CompilerInfo py = CompilerUtils::findPython();
        if (!py.available) {
            emit judgeOutput(tr("警告: 未检测到 Python 解释器，跳过预热。\n"), true);
            runNextTest();
            return;
        }
        program = py.compilerPath;
        args.append(m_sourceFile);
    } else {
        program = CompilerUtils::getOutputPath(m_sourceFile);
        QFileInfo exeInfo(program);
        if (!exeInfo.exists()) {
            emit judgeOutput(tr("警告: 可执行文件不存在，跳过预热。\n"), true);
            runNextTest();
            return;
        }
    }

    emit judgeOutput(tr("预热中...\n"), false);

    m_warmupProcess = new QProcess(this);
    m_warmupProcess->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_warmupProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &JudgeEngine::onWarmupFinished);

    m_warmupProcess->start(program, args);
    m_warmupProcess->closeWriteChannel();
}

void JudgeEngine::onWarmupFinished()
{
    emit judgeOutput(tr("预热完成。\n"), false);
    cleanupWarmupProcess();
    runNextTest();
}

void JudgeEngine::cleanupWarmupProcess()
{
    if (m_warmupProcess) {
        if (m_warmupProcess->state() != QProcess::NotRunning)
            m_warmupProcess->kill();
        m_warmupProcess->disconnect();
        m_warmupProcess->deleteLater();
        m_warmupProcess = nullptr;
    }
}

// ---- Test phase ----

void JudgeEngine::runNextTest()
{
    if (!m_running)
        return;

    ++m_currentTestIndex;
    if (m_currentTestIndex >= m_tests.size()) {
        m_running = false;
        emit judgeOutput(tr("\n所有测试已完成。通过: %1/%2\n")
                            .arg(m_passedCount).arg(m_tests.size()), false);
        emit allTestsFinished(m_passedCount, m_tests.size());
        return;
    }

    const TestCase &tc = m_tests[m_currentTestIndex];
    emit testStarted(m_currentTestIndex, tc.name);
    emit judgeOutput(tr("[%1/%2] 运行: %3 ... ")
                        .arg(m_currentTestIndex + 1).arg(m_tests.size()).arg(tc.name), false);

    cleanupTestProcess();
    m_actualOutput.clear();
    m_testHandled = false;

    m_testProcess = new QProcess(this);
    m_testProcess->setProcessChannelMode(QProcess::SeparateChannels);
    m_testProcess->setWorkingDirectory(QFileInfo(m_sourceFile).absolutePath());

    connect(m_testProcess, &QProcess::readyReadStandardOutput,
            this, &JudgeEngine::onTestReadyReadStdout);
    connect(m_testProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &JudgeEngine::onTestProcessFinished);
    connect(m_testProcess, &QProcess::errorOccurred,
            this, &JudgeEngine::onTestProcessError);

    QString program;
    QStringList args;
    if (m_isPython) {
        CompilerInfo py = CompilerUtils::findPython();
        if (!py.available) {
            finishCurrentTest(false, QStringLiteral("RE"),
                              tr("未检测到 Python 解释器"));
            return;
        }
        program = py.compilerPath;
        args.append(m_sourceFile);
    } else {
        program = CompilerUtils::getOutputPath(m_sourceFile);
        QFileInfo exeInfo(program);
        if (!exeInfo.exists()) {
            finishCurrentTest(false, QStringLiteral("RE"),
                              tr("可执行文件不存在"));
            return;
        }
    }

    m_testProcess->start(program, args);

    // Capture initial memory while the process is alive (it may exit before
    // the 100ms poll timer fires for the first time)
    m_peakMemoryKb = 0;
    m_mleDetected = false;
    captureMemory();

    // Write .in content to stdin
    m_testProcess->write(tc.inputData.toUtf8());
    m_testProcess->closeWriteChannel();

    m_testElapsed.start();
    m_testTimer->start(m_timeLimitMs);

    // Memory monitoring (polling for long-running processes)
    m_memPollTimer->start();
}

void JudgeEngine::onTestReadyReadStdout()
{
    m_actualOutput += QString::fromUtf8(m_testProcess->readAllStandardOutput());
}

void JudgeEngine::onTestProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_testHandled)
        return;
    m_testHandled = true;
    m_testTimer->stop();
    m_memPollTimer->stop();

    // Read any remaining output
    m_actualOutput += QString::fromUtf8(m_testProcess->readAllStandardOutput());

    // Final memory reading for fast processes that exited before poll timer fired
    captureMemory();

    if (m_mleDetected) {
        return; // finishCurrentTest already called by onMemoryCheck
    }

    const bool normalExit = (exitStatus == QProcess::NormalExit);
    const QString expected = m_tests[m_currentTestIndex].expectedOutput;

    if (!normalExit) {
        finishCurrentTest(false, QStringLiteral("RE"),
                          tr("进程崩溃"));
    } else if (exitCode != 0) {
        finishCurrentTest(false, QStringLiteral("RE"),
                          tr("非零退出代码 %1").arg(exitCode));
    } else if (outputMatches(m_actualOutput, expected)) {
        finishCurrentTest(true, QStringLiteral("AC"), QString());
    } else {
        finishCurrentTest(false, QStringLiteral("WA"),
                          tr("答案错误"));
    }
}

void JudgeEngine::onTestProcessError(QProcess::ProcessError error)
{
    if (m_testHandled)
        return;
    Q_UNUSED(error);
    m_testHandled = true;
    m_testTimer->stop();

    finishCurrentTest(false, QStringLiteral("RE"),
                      tr("进程启动失败"));
}

void JudgeEngine::onTestTimeout()
{
    if (m_testHandled)
        return;
    m_testHandled = true;
    m_memPollTimer->stop();

    if (m_testProcess) {
        m_testProcess->disconnect();
        m_testProcess->kill();
    }

    finishCurrentTest(false, QStringLiteral("TLE"),
                      tr("超出时间限制 (%1 ms)").arg(m_timeLimitMs));
}

void JudgeEngine::onMemoryCheck()
{
    if (!m_testProcess || m_testProcess->state() != QProcess::Running)
        return;

    captureMemory();

    if (m_peakMemoryKb > m_memoryLimitKb && !m_mleDetected) {
        m_mleDetected = true;
        m_testHandled = true;
        m_testTimer->stop();
        m_memPollTimer->stop();

        if (m_testProcess) {
            m_testProcess->disconnect();
            m_testProcess->kill();
        }

        finishCurrentTest(false, QStringLiteral("MLE"),
                          tr("超出内存限制 (%1 KB)").arg(m_memoryLimitKb));
    }
}

void JudgeEngine::finishCurrentTest(bool passed, const QString &statusCode, const QString &detail)
{
    const TestCase &tc = m_tests[m_currentTestIndex];

    if (m_memPollTimer)
        m_memPollTimer->stop();

    TestResult result;
    result.name = tc.name;
    result.passed = passed;
    result.statusCode = statusCode;
    result.elapsedMs = m_testElapsed.elapsed();
    result.memoryKb = m_peakMemoryKb;
    // Only capture actual output for non-AC results (save memory)
    if (!passed)
        result.actualOutput = m_actualOutput.trimmed();
    result.expectedOutput = tc.expectedOutput;
    result.inputData = tc.inputData;
    result.detail = detail;

    m_results.append(result);
    if (passed) {
        ++m_passedCount;
        emit judgeOutput(tr("AC (%1 ms, %2 KB)\n").arg(result.elapsedMs).arg(result.memoryKb), false);
    } else {
        emit judgeOutput(tr("%1 (%2 ms, %3 KB) - %4\n").arg(statusCode).arg(result.elapsedMs).arg(result.memoryKb).arg(detail), true);
    }

    emit testFinished(m_currentTestIndex, result);
    cleanupTestProcess();

    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    runNextTest();
}

void JudgeEngine::cleanupTestProcess()
{
    if (m_testProcess) {
        if (m_testProcess->state() != QProcess::NotRunning)
            m_testProcess->kill();
        m_testProcess->disconnect();
        m_testProcess->deleteLater();
        m_testProcess = nullptr;
    }
}

// ---- Memory monitoring ----

void JudgeEngine::captureMemory()
{
    if (!m_testProcess)
        return;

    const DWORD pid = static_cast<DWORD>(m_testProcess->processId());
    if (pid == 0)
        return;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess)
        return;

    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        quint64 memKb = pmc.PeakWorkingSetSize / 1024;
        if (memKb > m_peakMemoryKb)
            m_peakMemoryKb = memKb;
    }
    CloseHandle(hProcess);
}

// ---- Utilities ----

QString JudgeEngine::readFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream in(&file);
    return in.readAll();
}

bool JudgeEngine::outputMatches(const QString &actual, const QString &expected)
{
    const QStringList actualLines = actual.split(QLatin1Char('\n'));
    const QStringList expectedLines = expected.split(QLatin1Char('\n'));

    auto stripTrailingBlanks = [](QStringList &lines) {
        while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
            lines.removeLast();
    };

    QStringList act = actualLines;
    QStringList exp = expectedLines;
    stripTrailingBlanks(act);
    stripTrailingBlanks(exp);

    if (act.size() != exp.size())
        return false;

    for (int i = 0; i < act.size(); ++i) {
        if (act[i].trimmed() != exp[i].trimmed())
            return false;
    }
    return true;
}
