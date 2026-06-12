#include "terminalsession.h"

#include <QDir>
#include <QMetaObject>
#include <QThread>

#ifdef Q_OS_WIN
#include <QCoreApplication>
#include <QFileInfo>

namespace {

void closeHandleIfValid(HANDLE &handle)
{
    if (handle != INVALID_HANDLE_VALUE && handle != nullptr) {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
}

QString systemErrorMessage(DWORD code)
{
    LPWSTR buffer = nullptr;
    DWORD size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
                                    | FORMAT_MESSAGE_FROM_SYSTEM
                                    | FORMAT_MESSAGE_IGNORE_INSERTS,
                                nullptr, code, 0,
                                reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    QString message = size ? QString::fromWCharArray(buffer, int(size)).trimmed()
                           : QStringLiteral("Windows error %1").arg(code);
    if (buffer)
        LocalFree(buffer);
    return message;
}

} // namespace
#endif

TerminalSession::TerminalSession(QObject *parent)
    : QObject(parent)
{
}

TerminalSession::~TerminalSession()
{
    stop();
}

bool TerminalSession::start(const QString &workingDirectory, int columns, int rows)
{
    if (isRunning())
        return true;

    m_errorString.clear();
    m_stopping = false;

#ifdef Q_OS_WIN
    if (!createPseudoConsole(columns, rows))
        return false;
    if (!launchShell(workingDirectory)) {
        stop();
        return false;
    }

    m_running = true;
    m_readerThread = QThread::create([this]() { readLoop(); });
    connect(m_readerThread, &QThread::finished, m_readerThread, &QObject::deleteLater);
    m_readerThread->start();
    return true;
#else
    Q_UNUSED(columns)
    Q_UNUSED(rows)
    m_process = new QProcess(this);
    m_process->setProgram(QStringLiteral("powershell"));
    m_process->setWorkingDirectory(workingDirectory.isEmpty() ? QDir::homePath() : workingDirectory);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        emit outputReady(m_process->readAllStandardOutput());
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        emit outputReady(m_process->readAllStandardError());
    });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                m_running = false;
                emit exited(exitCode);
            });
    m_process->start();
    if (!m_process->waitForStarted()) {
        m_errorString = tr("Terminal backend is only fully supported on Windows ConPTY. Failed to start fallback shell.");
        emit errorOccurred(m_errorString);
        m_process->deleteLater();
        m_process = nullptr;
        return false;
    }
    m_running = true;
    return true;
#endif
}

void TerminalSession::writeInput(const QByteArray &data)
{
    if (!isRunning() || data.isEmpty())
        return;

#ifdef Q_OS_WIN
    DWORD written = 0;
    WriteFile(m_inputWrite, data.constData(), DWORD(data.size()), &written, nullptr);
#else
    if (m_process)
        m_process->write(data);
#endif
}

void TerminalSession::resizeTerminal(int columns, int rows)
{
    columns = qMax(1, columns);
    rows = qMax(1, rows);

#ifdef Q_OS_WIN
    if (m_pseudoConsole)
        ResizePseudoConsole(m_pseudoConsole, COORD{SHORT(columns), SHORT(rows)});
#else
    Q_UNUSED(columns)
    Q_UNUSED(rows)
#endif
}

void TerminalSession::stop()
{
    if (m_stopping.exchange(true))
        return;

#ifdef Q_OS_WIN
    m_running = false;

    if (m_processInfo.hProcess)
        TerminateProcess(m_processInfo.hProcess, 0);

    if (m_pseudoConsole) {
        ClosePseudoConsole(m_pseudoConsole);
        m_pseudoConsole = nullptr;
    }

    closePipeHandles();

    if (m_readerThread) {
        m_readerThread->quit();
        m_readerThread->wait(1500);
        m_readerThread = nullptr;
    }

    closeProcessHandles();
#else
    if (m_process) {
        m_process->terminate();
        if (!m_process->waitForFinished(1500))
            m_process->kill();
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_running = false;
#endif
}

bool TerminalSession::isRunning() const
{
    return m_running.load();
}

#ifdef Q_OS_WIN
bool TerminalSession::createPseudoConsole(int columns, int rows)
{
    HANDLE inputRead = INVALID_HANDLE_VALUE;
    HANDLE outputWrite = INVALID_HANDLE_VALUE;

    SECURITY_ATTRIBUTES attrs {};
    attrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    attrs.bInheritHandle = TRUE;

    if (!CreatePipe(&inputRead, &m_inputWrite, &attrs, 0)) {
        m_errorString = systemErrorMessage(GetLastError());
        emit errorOccurred(m_errorString);
        return false;
    }
    if (!CreatePipe(&m_outputRead, &outputWrite, &attrs, 0)) {
        m_errorString = systemErrorMessage(GetLastError());
        closeHandleIfValid(inputRead);
        closeHandleIfValid(m_inputWrite);
        emit errorOccurred(m_errorString);
        return false;
    }

    SetHandleInformation(m_inputWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(m_outputRead, HANDLE_FLAG_INHERIT, 0);

    HRESULT hr = CreatePseudoConsole(COORD{SHORT(qMax(1, columns)), SHORT(qMax(1, rows))},
                                     inputRead, outputWrite, 0, &m_pseudoConsole);

    closeHandleIfValid(inputRead);
    closeHandleIfValid(outputWrite);

    if (FAILED(hr)) {
        m_errorString = tr("CreatePseudoConsole failed: 0x%1")
                            .arg(QString::number(static_cast<unsigned long>(hr), 16));
        closePipeHandles();
        emit errorOccurred(m_errorString);
        return false;
    }

    return true;
}

bool TerminalSession::launchShell(const QString &workingDirectory)
{
    STARTUPINFOEXW startupInfo {};
    startupInfo.StartupInfo.cb = sizeof(STARTUPINFOEXW);

    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
    startupInfo.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        HeapAlloc(GetProcessHeap(), 0, attrListSize));
    if (!startupInfo.lpAttributeList) {
        m_errorString = tr("Failed to allocate process attribute list.");
        emit errorOccurred(m_errorString);
        return false;
    }

    if (!InitializeProcThreadAttributeList(startupInfo.lpAttributeList, 1, 0, &attrListSize)) {
        m_errorString = systemErrorMessage(GetLastError());
        HeapFree(GetProcessHeap(), 0, startupInfo.lpAttributeList);
        startupInfo.lpAttributeList = nullptr;
        emit errorOccurred(m_errorString);
        return false;
    }

    if (!UpdateProcThreadAttribute(startupInfo.lpAttributeList, 0,
                                   PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                   m_pseudoConsole, sizeof(HPCON), nullptr, nullptr)) {
        m_errorString = systemErrorMessage(GetLastError());
        DeleteProcThreadAttributeList(startupInfo.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, startupInfo.lpAttributeList);
        startupInfo.lpAttributeList = nullptr;
        emit errorOccurred(m_errorString);
        return false;
    }

    QString shell = QStringLiteral("powershell.exe");
    QString commandLine = QStringLiteral("powershell.exe -NoLogo");
    std::wstring command = commandLine.toStdWString();

    QString cwd = workingDirectory;
    if (cwd.isEmpty() || !QFileInfo(cwd).isDir())
        cwd = QDir::homePath();
    std::wstring nativeCwd = QDir::toNativeSeparators(cwd).toStdWString();

    ZeroMemory(&m_processInfo, sizeof(m_processInfo));
    BOOL ok = CreateProcessW(nullptr,
                             command.data(),
                             nullptr, nullptr,
                             FALSE,
                             EXTENDED_STARTUPINFO_PRESENT,
                             nullptr,
                             nativeCwd.c_str(),
                             &startupInfo.StartupInfo,
                             &m_processInfo);

    DeleteProcThreadAttributeList(startupInfo.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, startupInfo.lpAttributeList);

    if (!ok) {
        m_errorString = systemErrorMessage(GetLastError());
        emit errorOccurred(m_errorString);
        return false;
    }

    Q_UNUSED(shell)
    return true;
}

void TerminalSession::readLoop()
{
    QByteArray buffer;
    buffer.resize(8192);

    while (!m_stopping.load()) {
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(m_outputRead, buffer.data(), DWORD(buffer.size()), &bytesRead, nullptr);
        if (!ok || bytesRead == 0)
            break;

        QByteArray chunk = buffer.left(int(bytesRead));
        QMetaObject::invokeMethod(this, [this, chunk]() {
            emit outputReady(chunk);
        }, Qt::QueuedConnection);
    }

    m_running = false;
    DWORD exitCode = 0;
    if (m_processInfo.hProcess)
        GetExitCodeProcess(m_processInfo.hProcess, &exitCode);

    QMetaObject::invokeMethod(this, [this, exitCode]() {
        emit exited(int(exitCode));
    }, Qt::QueuedConnection);
}

void TerminalSession::closeProcessHandles()
{
    if (m_processInfo.hThread) {
        CloseHandle(m_processInfo.hThread);
        m_processInfo.hThread = nullptr;
    }
    if (m_processInfo.hProcess) {
        CloseHandle(m_processInfo.hProcess);
        m_processInfo.hProcess = nullptr;
    }
}

void TerminalSession::closePipeHandles()
{
    closeHandleIfValid(m_inputWrite);
    closeHandleIfValid(m_outputRead);
}
#endif
