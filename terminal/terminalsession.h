#ifndef TERMINALSESSION_H
#define TERMINALSESSION_H

#include <QObject>
#include <QByteArray>
#include <QProcess>
#include <QString>
#include <atomic>

#ifdef Q_OS_WIN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#endif

class QThread;

class TerminalSession : public QObject
{
    Q_OBJECT

public:
    explicit TerminalSession(QObject *parent = nullptr);
    ~TerminalSession() override;

    bool start(const QString &workingDirectory, int columns, int rows);
    void writeInput(const QByteArray &data);
    void resizeTerminal(int columns, int rows);
    void stop();
    bool isRunning() const;
    QString errorString() const { return m_errorString; }

signals:
    void outputReady(const QByteArray &data);
    void exited(int exitCode);
    void errorOccurred(const QString &message);

private:
    QString m_errorString;
    std::atomic_bool m_running { false };
    std::atomic_bool m_stopping { false };

#ifdef Q_OS_WIN
    HPCON m_pseudoConsole = nullptr;
    HANDLE m_inputWrite = INVALID_HANDLE_VALUE;
    HANDLE m_outputRead = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION m_processInfo {};
    QThread *m_readerThread = nullptr;

    bool createPseudoConsole(int columns, int rows);
    bool launchShell(const QString &workingDirectory);
    void readLoop();
    void closeProcessHandles();
    void closePipeHandles();
#else
    QProcess *m_process = nullptr;
#endif
};

#endif // TERMINALSESSION_H
