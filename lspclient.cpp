#include "lspclient.h"
#include <QDebug>

LspClient::LspClient(QObject *parent)
    : QObject(parent)
{
}

LspClient::~LspClient()
{
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            sendRequest(QStringLiteral("shutdown"), QJsonObject());
            sendNotification(QStringLiteral("exit"), QJsonObject());
            m_process->terminate();
            m_process->kill();
        }
        delete m_process;
        m_process = nullptr;
    }
}

void LspClient::start(const QString &serverPath, const QStringList &args)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qWarning() << "LspClient: already running, stopping first";
        stop();
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::started,
            this, &LspClient::onProcessStarted);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &LspClient::onReadyRead);
    connect(m_process, &QProcess::errorOccurred,
            this, &LspClient::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LspClient::onProcessFinished);

    qDebug() << "LspClient: starting" << serverPath << args;
    m_process->start(serverPath, args);
    // startup is now async — serverStarted / serverError signals indicate result
}

void LspClient::onProcessStarted()
{
    qDebug() << "LspClient: process started";
    emit serverStarted();
}

int LspClient::sendRequest(const QString &method, const QJsonObject &params)
{
    if (!isRunning()) {
        qWarning() << "LspClient: cannot send request, not running";
        return -1;
    }

    int id = m_nextId++;

    QJsonObject msg;
    msg[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    msg[QStringLiteral("id")] = id;
    msg[QStringLiteral("method")] = method;
    msg[QStringLiteral("params")] = params;

    QJsonDocument doc(msg);
    sendMessage(doc.toJson(QJsonDocument::Compact));

    return id;
}

void LspClient::sendNotification(const QString &method, const QJsonObject &params)
{
    if (!isRunning()) {
        qWarning() << "LspClient: cannot send notification, not running";
        return;
    }

    QJsonObject msg;
    msg[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    msg[QStringLiteral("method")] = method;
    msg[QStringLiteral("params")] = params;

    QJsonDocument doc(msg);
    sendMessage(doc.toJson(QJsonDocument::Compact));
}

bool LspClient::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

void LspClient::stop()
{
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            sendRequest(QStringLiteral("shutdown"), QJsonObject());
            sendNotification(QStringLiteral("exit"), QJsonObject());
            m_process->terminate();
            if (!m_process->waitForFinished(500)) {
                m_process->kill();
                m_process->waitForFinished(100);
            }
        }
        delete m_process;
        m_process = nullptr;
    }
}

void LspClient::onReadyRead()
{
    m_buffer.append(m_process->readAllStandardOutput());
    parseFrames();
}

void LspClient::onProcessError(QProcess::ProcessError error)
{
    qWarning() << "LspClient: process error" << error;
    emit serverError(error);
}

void LspClient::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    qDebug() << "LspClient: process finished, exitCode" << exitCode << "status" << status;
    emit serverStopped(exitCode, status);
}

void LspClient::parseFrames()
{
    while (true) {
        int headerEnd = m_buffer.indexOf(QByteArrayLiteral("\r\n\r\n"));
        if (headerEnd == -1)
            break;

        QByteArray header = m_buffer.left(headerEnd);
        int contentLength = 0;

        for (const QByteArray &line : header.split('\n')) {
            QByteArray trimmed = line.trimmed();
            if (trimmed.startsWith("Content-Length:")) {
                contentLength = trimmed.mid(trimmed.indexOf(':') + 1).trimmed().toInt();
                break;
            }
        }

        if (contentLength <= 0) {
            m_buffer.remove(0, headerEnd + 4);
            continue;
        }

        int frameEnd = headerEnd + 4 + contentLength;
        if (m_buffer.size() < frameEnd)
            break;

        QByteArray jsonData = m_buffer.mid(headerEnd + 4, contentLength);
        m_buffer.remove(0, frameEnd);

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData, &err);
        if (err.error != QJsonParseError::NoError) {
            qWarning() << "LspClient: JSON parse error" << err.errorString();
            continue;
        }

        QJsonObject msg = doc.object();

        // Distinguish request responses from server notifications
        if (msg.contains(QStringLiteral("id"))) {
            int id = msg.value(QStringLiteral("id")).toInt();
            if (msg.contains(QStringLiteral("result"))) {
                emit responseReceived(id, msg.value(QStringLiteral("result")).toObject());
            } else if (msg.contains(QStringLiteral("error"))) {
                QJsonObject errObj = msg.value(QStringLiteral("error")).toObject();
                qWarning() << "LspClient: error response id" << id
                           << errObj.value(QStringLiteral("message")).toString();
                emit requestFailed(id, errObj);
            }
        } else if (msg.contains(QStringLiteral("method"))) {
            QString method = msg.value(QStringLiteral("method")).toString();
            QJsonObject params = msg.value(QStringLiteral("params")).toObject();
            emit notificationReceived(method, params);
        }
    }
}

void LspClient::sendMessage(const QByteArray &json)
{
    QByteArray frame;
    frame.append(QStringLiteral("Content-Length: %1\r\n\r\n").arg(json.size()).toUtf8());
    frame.append(json);
    m_process->write(frame);
}
