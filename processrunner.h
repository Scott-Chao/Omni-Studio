#ifndef PROCESSRUNNER_H
#define PROCESSRUNNER_H

#include <QObject>
#include <QProcess>
#include <QString>

class ProcessRunner : public QObject
{
    Q_OBJECT

public:
    enum Mode { CompileOnly, RunOnly, CompileAndRun };

    explicit ProcessRunner(QObject *parent = nullptr);
    ~ProcessRunner();

    void startCompile(const QString &sourceFile);
    void startRun(const QString &executable);
    void startCompileAndRun(const QString &sourceFile);
    void startRunPython(const QString &sourceFile);
    void stop();
    void writeInput(const QString &text);
    bool isRunning() const { return m_currentProcess != nullptr; }
    QString lastExecutable() const { return m_lastExecutable; }

signals:
    void outputReceived(const QString &text, bool isStderr);
    void compileFinished(bool success);
    void runFinished(int exitCode);
    void processStarted();
    void processStopped();

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    void startProcess(const QString &program, const QStringList &args,
                      const QString &workingDir);
    void cleanupProcess();

    QProcess *m_currentProcess = nullptr;
    Mode m_mode = CompileOnly;
    QString m_sourceFile;
    QString m_outputFile;
    QString m_lastExecutable;
};

#endif // PROCESSRUNNER_H
