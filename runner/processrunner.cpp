#include "processrunner.h"
#include "compilerutils.h"

#include <QFileInfo>
#include <QDir>

ProcessRunner::ProcessRunner(QObject *parent)
    : QObject(parent)
{
}

ProcessRunner::~ProcessRunner()
{
    if (m_currentProcess) {
        m_currentProcess->kill();
        m_currentProcess->waitForFinished(200);
        cleanupProcess();
    }
}

void ProcessRunner::startCompile(const QString &sourceFile)
{
    stop();
    m_mode = CompileOnly;
    m_sourceFile = sourceFile;
    m_outputFile = CompilerUtils::getOutputPath(sourceFile);

    CompilerInfo compiler = CompilerUtils::defaultCompiler();
    if (!compiler.available) {
        emit outputReceived(tr("错误: 未检测到编译器 (g++)。\n"), true);
        emit compileFinished(false);
        return;
    }

    QStringList args = CompilerUtils::getCompileArgs(
        compiler.id, sourceFile, m_outputFile);

    QFileInfo fi(sourceFile);
    startProcess(compiler.compilerPath, args, fi.absolutePath());
}

void ProcessRunner::startCompileOnly(const QString &sourceFile)
{
    stop();
    m_mode = CompileOnly;
    m_sourceFile = sourceFile;
    QFileInfo fi(sourceFile);
    m_outputFile = fi.absolutePath() + QStringLiteral("/") + fi.completeBaseName()
                   + QStringLiteral(".o");

    CompilerInfo compiler = CompilerUtils::defaultCompiler();
    if (!compiler.available) {
        emit outputReceived(tr("错误: 未检测到编译器 (g++)。\n"), true);
        emit compileFinished(false);
        return;
    }

    QStringList args = CompilerUtils::getCompileOnlyArgs(
        compiler.id, sourceFile, m_outputFile);

    startProcess(compiler.compilerPath, args, fi.absolutePath());
}

void ProcessRunner::startRun(const QString &executable)
{
    stop();
    m_mode = RunOnly;
    m_lastExecutable = executable;

    QFileInfo fi(executable);
    if (!fi.exists()) {
        emit outputReceived(
            tr("错误: 可执行文件不存在: %1\n").arg(executable), true);
        emit runFinished(-1);
        return;
    }

    startProcess(executable, {}, fi.absolutePath());
}

void ProcessRunner::startRunPython(const QString &sourceFile)
{
    stop();
    m_mode = RunOnly;
    m_sourceFile = sourceFile;

    CompilerInfo py = CompilerUtils::findPython();
    if (!py.available) {
        emit outputReceived(tr("错误: 未检测到 Python 解释器。\n"), true);
        emit runFinished(-1);
        return;
    }

    m_lastExecutable = sourceFile;

    QFileInfo fi(sourceFile);
    startProcess(py.compilerPath, {sourceFile}, fi.absolutePath());
}

void ProcessRunner::startCompileAndRun(const QString &sourceFile)
{
    m_mode = CompileAndRun;
    m_sourceFile = sourceFile;
    m_outputFile = CompilerUtils::getOutputPath(sourceFile);

    CompilerInfo compiler = CompilerUtils::defaultCompiler();
    if (!compiler.available) {
        emit outputReceived(tr("错误: 未检测到编译器 (g++)。\n"), true);
        emit compileFinished(false);
        return;
    }

    QStringList args = CompilerUtils::getCompileArgs(
        compiler.id, sourceFile, m_outputFile);

    QFileInfo fi(sourceFile);
    startProcess(compiler.compilerPath, args, fi.absolutePath());
}

void ProcessRunner::stop()
{
    if (m_currentProcess) {
        m_currentProcess->kill();
        cleanupProcess();
        emit processStopped();
    }
}

void ProcessRunner::writeInput(const QString &text)
{
    if (m_currentProcess && m_currentProcess->state() == QProcess::Running) {
        m_currentProcess->write(text.toUtf8() + "\n");
    }
}

void ProcessRunner::writeRaw(const QString &text)
{
    if (m_currentProcess && m_currentProcess->state() == QProcess::Running) {
        m_currentProcess->write(text.toUtf8());
    }
}

void ProcessRunner::startProcess(const QString &program,
                                  const QStringList &args,
                                  const QString &workingDir)
{
    cleanupProcess();

    m_currentProcess = new QProcess(this);
    m_currentProcess->setWorkingDirectory(workingDir);
    m_currentProcess->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_currentProcess, &QProcess::readyReadStandardOutput,
            this, &ProcessRunner::onReadyReadStdout);
    connect(m_currentProcess, &QProcess::readyReadStandardError,
            this, &ProcessRunner::onReadyReadStderr);
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProcessRunner::onProcessFinished);
    connect(m_currentProcess, &QProcess::errorOccurred,
            this, &ProcessRunner::onProcessError);

    emit processStarted();
    m_currentProcess->start(program, args);
}

void ProcessRunner::cleanupProcess()
{
    if (m_currentProcess) {
        m_currentProcess->disconnect();
        m_currentProcess->deleteLater();
        m_currentProcess = nullptr;
    }
}

void ProcessRunner::onReadyReadStdout()
{
    if (m_currentProcess) {
        QByteArray data = m_currentProcess->readAllStandardOutput();
        emit outputReceived(QString::fromUtf8(data), false);
    }
}

void ProcessRunner::onReadyReadStderr()
{
    if (m_currentProcess) {
        QByteArray data = m_currentProcess->readAllStandardError();
        emit outputReceived(QString::fromUtf8(data), true);
    }
}

void ProcessRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);

    if (m_mode == CompileOnly) {
        if (success)
            m_lastExecutable = m_outputFile;
        emit compileFinished(success);
        cleanupProcess();
        emit processStopped();
    } else if (m_mode == CompileAndRun) {
        if (success) {
            m_lastExecutable = m_outputFile;
            emit compileFinished(true);
            // Transition to run phase
            m_mode = RunOnly;
            QFileInfo fi(m_outputFile);
            startProcess(m_outputFile, {}, fi.absolutePath());
        } else {
            emit compileFinished(false);
            cleanupProcess();
            emit processStopped();
        }
    } else { // RunOnly
        emit runFinished(exitCode);
        cleanupProcess();
        emit processStopped();
    }
}

void ProcessRunner::onProcessError(QProcess::ProcessError error)
{
    QString msg;
    switch (error) {
    case QProcess::FailedToStart:
        msg = tr("错误: 进程启动失败 (找不到编译器或可执行文件)\n");
        break;
    case QProcess::Crashed:
        msg = tr("错误: 进程异常退出\n");
        break;
    case QProcess::Timedout:
        msg = tr("错误: 进程超时\n");
        break;
    default:
        msg = tr("错误: 进程出错 (代码 %1)\n").arg(static_cast<int>(error));
        break;
    }
    emit outputReceived(msg, true);

    if (m_mode == CompileOnly || m_mode == CompileAndRun) {
        emit compileFinished(false);
    } else {
        emit runFinished(-1);
    }
    cleanupProcess();
    emit processStopped();
}
