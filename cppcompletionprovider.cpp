#include "cppcompletionprovider.h"
#include "lspclient.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
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

        // Tell the owner (CodeEditor) that we're ready — it will call openDocument()
        emit serverReady();

    } else if (id == m_completionRequestId) {
        m_completionRequestId = -1;

        QList<CompletionItem> items = parseCompletionResponse(result);
        qDebug() << "CppCompletionProvider: completion returned" << items.size() << "items";
        emit completionReady(items);
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
    m_documentOpen = false;
}

void CppCompletionProvider::onServerStopped(int exitCode, QProcess::ExitStatus status)
{
    qDebug() << "CppCompletionProvider: server stopped, exitCode" << exitCode
             << "status" << status;
    m_initialized = false;
    m_documentOpen = false;

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

// ---- Document sync ----

void CppCompletionProvider::openDocument(const QString &uri, const QString &languageId, const QString &text)
{
    m_documentUri = uri;
    m_documentLanguageId = languageId;
    m_documentVersion = 1;

    if (!m_initialized) {
        // Server not ready yet — save text so onServerReady can pick it up
        qDebug() << "CppCompletionProvider: queueing text for pending open on" << uri;
        m_pendingText = text;
        m_pendingOpen = true;
        return;
    }

    qDebug() << "CppCompletionProvider: sending didOpen for" << uri
             << "(re-open:" << m_documentOpen << ")";
    sendDidOpen(text);
    m_documentOpen = true;
}

void CppCompletionProvider::updateText(const QString &text)
{
    if (!m_initialized) {
        // Server not ready yet — just keep the latest text for when didOpen fires
        m_pendingText = text;
        return;
    }

    if (m_documentUri.isEmpty())
        return;

    m_documentVersion++;
    sendDidChange(text);
}

void CppCompletionProvider::sendDidOpen(const QString &text)
{
    QJsonObject textDocument;
    textDocument[QStringLiteral("uri")] = m_documentUri;
    textDocument[QStringLiteral("languageId")] = m_documentLanguageId;
    textDocument[QStringLiteral("version")] = m_documentVersion;
    textDocument[QStringLiteral("text")] = text;

    QJsonObject params;
    params[QStringLiteral("textDocument")] = textDocument;

    m_client->sendNotification(QStringLiteral("textDocument/didOpen"), params);
    qDebug() << "CppCompletionProvider: didOpen sent, version" << m_documentVersion;
}

void CppCompletionProvider::sendDidChange(const QString &text)
{
    QJsonObject textDocument;
    textDocument[QStringLiteral("uri")] = m_documentUri;
    textDocument[QStringLiteral("version")] = m_documentVersion;

    QJsonObject change;
    change[QStringLiteral("text")] = text;

    QJsonArray contentChanges;
    contentChanges.append(change);

    QJsonObject params;
    params[QStringLiteral("textDocument")] = textDocument;
    params[QStringLiteral("contentChanges")] = contentChanges;

    m_client->sendNotification(QStringLiteral("textDocument/didChange"), params);
}

// ---- Completion request ----

void CppCompletionProvider::requestCompletion(const QString &text, int cursorPos)
{
    if (!m_initialized || m_documentUri.isEmpty() || !m_client)
        return;

    // Convert absolute cursorPos to LSP line (0-based) and character (0-based)
    int line = 0;
    int col = 0;
    int len = qMin(cursorPos, text.length());
    for (int i = 0; i < len; ++i) {
        if (text.at(i) == QLatin1Char('\n')) {
            ++line;
            col = 0;
        } else {
            ++col;
        }
    }

    QJsonObject position;
    position[QStringLiteral("line")] = line;
    position[QStringLiteral("character")] = col;

    QJsonObject textDocument;
    textDocument[QStringLiteral("uri")] = m_documentUri;

    QJsonObject params;
    params[QStringLiteral("textDocument")] = textDocument;
    params[QStringLiteral("position")] = position;

    // Context: we are triggering explicitly (not automatically on type)
    QJsonObject context;
    context[QStringLiteral("triggerKind")] = 2; // 1=Invoked, 2=TriggerCharacter, 3=ContentChange
    params[QStringLiteral("context")] = context;

    m_completionRequestId = m_client->sendRequest(QStringLiteral("textDocument/completion"), params);
    qDebug() << "CppCompletionProvider: sent completion request id=" << m_completionRequestId
             << "at (" << line << "," << col << ")";
}

QString CppCompletionProvider::completionKindToString(int kind)
{
    switch (kind) {
    case 1:  return QStringLiteral("Text");
    case 2:  return QStringLiteral("Method");
    case 3:  return QStringLiteral("Function");
    case 4:  return QStringLiteral("Constructor");
    case 5:  return QStringLiteral("Field");
    case 6:  return QStringLiteral("Variable");
    case 7:  return QStringLiteral("Class");
    case 8:  return QStringLiteral("Interface");
    case 9:  return QStringLiteral("Module");
    case 10: return QStringLiteral("Property");
    case 11: return QStringLiteral("Unit");
    case 12: return QStringLiteral("Value");
    case 13: return QStringLiteral("Enum");
    case 14: return QStringLiteral("Keyword");
    case 15: return QStringLiteral("Snippet");
    case 18: return QStringLiteral("Reference");
    case 20: return QStringLiteral("EnumMember");
    case 21: return QStringLiteral("Constant");
    case 22: return QStringLiteral("Struct");
    case 23: return QStringLiteral("Event");
    case 24: return QStringLiteral("Operator");
    case 25: return QStringLiteral("TypeParameter");
    default: return QStringLiteral("Text");
    }
}

QList<CompletionItem> CppCompletionProvider::parseCompletionResponse(const QJsonObject &result)
{
    QList<CompletionItem> items;

    // LSP can return CompletionList { isIncomplete, items[] } or bare CompletionItem[]
    QJsonArray itemArray;
    if (result.contains(QStringLiteral("items"))) {
        itemArray = result.value(QStringLiteral("items")).toArray();
    }

    for (const QJsonValue &val : itemArray) {
        QJsonObject item = val.toObject();

        CompletionItem ci;
        ci.name = item.value(QStringLiteral("label")).toString();
        ci.detail = item.value(QStringLiteral("detail")).toString();

        // Map LSP CompletionItemKind to our type string
        int kind = item.value(QStringLiteral("kind")).toInt(0);
        ci.type = completionKindToString(kind);

        // Build a basic signature: for functions, label is already the signature
        ci.signature = ci.name;

        // Extract documentation if present
        QJsonValue docVal = item.value(QStringLiteral("documentation"));
        if (docVal.isString()) {
            ci.doc = docVal.toString();
        } else if (docVal.isObject()) {
            ci.doc = docVal.toObject().value(QStringLiteral("value")).toString();
        }

        qDebug() << "  completion item:" << ci.name << "type=" << ci.type
                 << "detail=" << ci.detail;

        items.append(ci);
    }

    return items;
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
