#include "cppcompletionprovider.h"
#include "lspclient.h"
#include <QJsonObject>
#include <QDebug>
#include <QStandardPaths>
#include <QTimer>

CppCompletionProvider::CppCompletionProvider(QObject *parent)
    : CompletionProvider(parent)
{
    startServer();
}

CppCompletionProvider::~CppCompletionProvider()
{
    if (m_client) {
        m_client->stop();
    }
}

void CppCompletionProvider::startServer()
{
    QString clangdPath = QStandardPaths::findExecutable(QStringLiteral("clangd"));
    if (clangdPath.isEmpty()) {
        qWarning() << "CppCompletionProvider: clangd not found in PATH";
        emit serverFailed(tr("clangd not found. Please install clangd to enable C++ code completion."));
        return;
    }

    qDebug() << "CppCompletionProvider: found clangd at" << clangdPath;

    m_client = new LspClient(this);

    connect(m_client, &LspClient::responseReceived,
            this, &CppCompletionProvider::onResponseReceived);
    connect(m_client, &LspClient::notificationReceived,
            this, &CppCompletionProvider::onNotificationReceived);
    connect(m_client, &LspClient::requestFailed,
            this, &CppCompletionProvider::onRequestFailed);
    connect(m_client, &LspClient::serverError,
            this, &CppCompletionProvider::onServerError);
    connect(m_client, &LspClient::serverStopped,
            this, &CppCompletionProvider::onServerStopped);

    QStringList args = {
        QStringLiteral("--fallback-style=Google")
    };

    if (!m_client->start(clangdPath, args)) {
        qWarning() << "CppCompletionProvider: failed to start clangd";
        emit serverFailed(tr("Failed to start clangd process."));

        delete m_client;
        m_client = nullptr;
        return;
    }

    sendInitialize();
}

void CppCompletionProvider::sendInitialize()
{
    QJsonObject params;
    params[QStringLiteral("processId")] = QJsonValue::Null;
    params[QStringLiteral("rootUri")] = QJsonValue::Null;
    params[QStringLiteral("capabilities")] = QJsonObject();

    m_initRequestId = m_client->sendRequest(QStringLiteral("initialize"), params);
    qDebug() << "CppCompletionProvider: sent initialize request, id =" << m_initRequestId;
}

void CppCompletionProvider::onResponseReceived(int id, QJsonObject result)
{
    if (id == m_initRequestId) {
        Q_UNUSED(result);
        qDebug() << "CppCompletionProvider: clangd initialized successfully";

        m_client->sendNotification(QStringLiteral("initialized"), QJsonObject());
        m_initialized = true;

        emit serverReady();
    }
}

void CppCompletionProvider::onNotificationReceived(QString method, QJsonObject params)
{
    Q_UNUSED(params);
    qDebug() << "CppCompletionProvider: server notification" << method;
}

void CppCompletionProvider::onRequestFailed(int id, QJsonObject error)
{
    qWarning() << "CppCompletionProvider: request" << id << "failed:" << error;
    if (id == m_initRequestId) {
        emit serverFailed(tr("clangd initialization failed."));
    }
}

void CppCompletionProvider::onServerError(QProcess::ProcessError err)
{
    qWarning() << "CppCompletionProvider: server error" << err;
    m_initialized = false;
}

void CppCompletionProvider::onServerStopped(int exitCode, QProcess::ExitStatus status)
{
    qDebug() << "CppCompletionProvider: server stopped, exitCode" << exitCode
             << "status" << status;
    m_initialized = false;

    if (status == QProcess::CrashExit) {
        qDebug() << "CppCompletionProvider: clangd crashed, restarting in 1s...";
        QTimer::singleShot(1000, this, &CppCompletionProvider::restartServer);
    }
}

void CppCompletionProvider::restartServer()
{
    if (m_client) {
        m_client->stop();
        m_client->deleteLater();
        m_client = nullptr;
    }
    m_initialized = false;
    startServer();
}

// ---- Completion / Hover / Signature (stubs for later steps) ----

void CppCompletionProvider::requestCompletion(const QString &text, int cursorPos)
{
    Q_UNUSED(text);
    Q_UNUSED(cursorPos);
}

void CppCompletionProvider::requestHover(const QString &text, int cursorPos)
{
    Q_UNUSED(text);
    Q_UNUSED(cursorPos);
}

void CppCompletionProvider::requestSignatureHelp(const QString &text, int cursorPos)
{
    Q_UNUSED(text);
    Q_UNUSED(cursorPos);
}
