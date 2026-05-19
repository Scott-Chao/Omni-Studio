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
    m_requestTimer.setSingleShot(true);
    connect(&m_requestTimer, &QTimer::timeout, this, &CppCompletionProvider::onRequestTimeout);

    startServer();
}

CppCompletionProvider::~CppCompletionProvider()
{
    // LspClient child is destroyed via Qt parent-child chain without blocking.
    // Do NOT call shutdown() here — its waitForFinished() blocks the main
    // thread and causes freeze/crash when closing files.
}

void CppCompletionProvider::shutdown()
{
    // Disconnect all signals so no pending responses reach us
    disconnect();

    if (m_client) {
        m_client->stop();
        m_client->deleteLater();
        m_client = nullptr;
    }

    m_requestTimer.stop();
    m_initialized = false;
    m_documentOpen = false;
    m_pendingRequest = PendingRequest::None;
    m_completionRequestId = -1;
    m_hoverRequestId = -1;
    m_signatureHelpRequestId = -1;
}

void CppCompletionProvider::startServer()
{
    QString clangdPath = QStandardPaths::findExecutable(QStringLiteral("clangd"));
    if (clangdPath.isEmpty()) {
        qWarning() << "CppCompletionProvider: clangd not found in PATH";
        emit serverFailed(tr("clangd not found. Please install clangd to enable C++ code completion."));
        return;
    }


    m_client = new LspClient(this);

    connect(m_client, &LspClient::serverStarted, this, [this]() {
        sendInitialize();
    });
    connect(m_client, &LspClient::serverError, this, [this](QProcess::ProcessError err) {
        // Only handle startup failures; runtime errors are handled by serverStopped
        if (err != QProcess::FailedToStart) return;
        if (!m_client) return;
        qWarning() << "CppCompletionProvider: clangd failed to start";
        emit serverFailed(tr("Failed to start clangd process."));
        m_client->deleteLater();
        m_client = nullptr;
    });
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

    m_client->start(clangdPath, args);
    // startup is async — serverStarted / serverError signals handle result
}

void CppCompletionProvider::sendInitialize()
{
    QJsonObject params;
    params[QStringLiteral("processId")] = QJsonValue::Null;
    params[QStringLiteral("rootUri")] = QJsonValue::Null;
    params[QStringLiteral("capabilities")] = QJsonObject();

    m_initRequestId = m_client->sendRequest(QStringLiteral("initialize"), params);
}

void CppCompletionProvider::onResponseReceived(int id, QJsonObject result)
{
    if (!m_client)
        return;

    // Stop timeout on any response
    m_requestTimer.stop();
    m_pendingRequest = PendingRequest::None;

    if (id == m_initRequestId) {
        Q_UNUSED(result);
        qDebug() << "CppCompletionProvider: clangd initialized successfully";

        m_client->sendNotification(QStringLiteral("initialized"), QJsonObject());
        m_initialized = true;

        // Tell the owner (CodeEditor) that we're ready — it will call openDocument()
        emit serverReady();

    } else if (!m_initialized) {
        // Ignore responses before initialization completes
        return;

    } else if (id == m_completionRequestId) {
        m_completionRequestId = -1;

        QList<CompletionItem> items = parseCompletionResponse(result);
        qDebug() << "CppCompletionProvider: completion returned" << items.size() << "items";
        emit completionReady(items);

    } else if (id == m_hoverRequestId) {
        m_hoverRequestId = -1;

        HoverInfo info = parseHoverResponse(result);
        emit hoverReady(info);

    } else if (id == m_signatureHelpRequestId) {
        m_signatureHelpRequestId = -1;

        QList<SignatureInfo> signatures;
        int activeIndex = 0;

        QJsonValue signaturesVal = result.value(QStringLiteral("signatures"));
        if (signaturesVal.isArray()) {
            QJsonArray sigs = signaturesVal.toArray();
            for (const QJsonValue &sv : sigs) {
                signatures.append(parseSignatureHelpItem(sv.toObject()));
            }
        }

        activeIndex = result.value(QStringLiteral("activeSignature")).toInt(0);

        // activeParameter is at the SignatureHelp top level (LSP spec),
        // NOT inside each SignatureInformation. Apply it to the active signature.
        int activeParam = result.value(QStringLiteral("activeParameter")).toInt(0);
        if (activeIndex >= 0 && activeIndex < signatures.size())
            signatures[activeIndex].activeParameter = activeParam;

        qDebug() << "CppCompletionProvider: signatureHelp returned" << signatures.size() << "signatures";

        emit signatureHelpReady(signatures, activeIndex);
    }
}

void CppCompletionProvider::onNotificationReceived(QString method, QJsonObject params)
{
    if (!m_client || !m_initialized)
        return;
    Q_UNUSED(method);
    Q_UNUSED(params);
}

void CppCompletionProvider::onRequestFailed(int id, QJsonObject error)
{
    if (!m_client || !m_initialized)
        return;
    qWarning() << "CppCompletionProvider: request" << id << "failed:" << error;
    m_requestTimer.stop();
    m_pendingRequest = PendingRequest::None;
    if (id == m_initRequestId) {
        emit serverFailed(tr("clangd initialization failed."));
    }
}

void CppCompletionProvider::onServerError(QProcess::ProcessError err)
{
    if (!m_client)
        return;
    qWarning() << "CppCompletionProvider: server error" << err;
    m_initialized = false;
    m_documentOpen = false;
}

void CppCompletionProvider::onServerStopped(int exitCode, QProcess::ExitStatus status)
{
    if (!m_client)
        return;
    m_initialized = false;
    m_documentOpen = false;

    if (status == QProcess::CrashExit) {
        qDebug() << "CppCompletionProvider: clangd crashed, restarting in 1s...";
        QTimer::singleShot(1000, this, &CppCompletionProvider::restartServer);
    } else {
        // Normal exit — server is gone, clean up
        m_client->deleteLater();
        m_client = nullptr;
    }
}

void CppCompletionProvider::restartServer()
{
    if (!m_client)
        return;
    m_client->stop();
    m_client->deleteLater();
    m_client = nullptr;
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
        m_pendingText = text;
        m_pendingOpen = true;
        return;
    }

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

    m_pendingRequest = PendingRequest::Completion;
    m_requestTimer.start(500);
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

        // Prefer insertText over label for the actual insertion text.
        // textEdit can be { newText, textEdit } or { range, newText }.
        QString insertText;
        QJsonValue textEditVal = item.value(QStringLiteral("textEdit"));
        if (textEditVal.isObject()) {
            insertText = textEditVal.toObject().value(QStringLiteral("newText")).toString();
        }
        if (insertText.isEmpty())
            insertText = item.value(QStringLiteral("insertText")).toString();
        if (insertText.isEmpty())
            insertText = item.value(QStringLiteral("label")).toString();

        ci.name = insertText.trimmed();
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

        items.append(ci);
    }

    return items;
}

void CppCompletionProvider::requestHover(const QString &text, int cursorPos)
{
    if (!m_initialized || m_documentUri.isEmpty() || !m_client)
        return;

    // Convert cursorPos to LSP line (0-based) and character (0-based)
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

    m_hoverRequestId = m_client->sendRequest(QStringLiteral("textDocument/hover"), params);

    m_pendingRequest = PendingRequest::Hover;
    m_requestTimer.start(500);
}

HoverInfo CppCompletionProvider::parseHoverResponse(const QJsonObject &result)
{
    HoverInfo info;

    QJsonValue contentsVal = result.value(QStringLiteral("contents"));
    if (contentsVal.isUndefined() || contentsVal.isNull())
        return info;

    // Array: typically [MarkedString, MarkedString | string, ...]
    if (contentsVal.isArray()) {
        QJsonArray arr = contentsVal.toArray();
        QStringList docParts;
        for (const QJsonValue &v : arr) {
            if (v.isObject()) {
                QJsonObject obj = v.toObject();
                // MarkedString: { language, value }
                if (obj.contains(QStringLiteral("language"))) {
                    info.signature = obj.value(QStringLiteral("value")).toString();
                } else {
                    // MarkupContent: { kind, value }
                    docParts.append(obj.value(QStringLiteral("value")).toString());
                }
            } else if (v.isString()) {
                docParts.append(v.toString());
            }
        }
        info.doc = docParts.join(QStringLiteral("\n"));
        return info;
    }

    // String: plain text
    if (contentsVal.isString()) {
        info.doc = contentsVal.toString();
        return info;
    }

    // Object: MarkupContent { kind, value } or MarkedString { language, value }
    QJsonObject contents = contentsVal.toObject();
    if (contents.contains(QStringLiteral("language"))) {
        // MarkedString
        info.signature = contents.value(QStringLiteral("value")).toString();
    } else {
        // MarkupContent
        info.doc = contents.value(QStringLiteral("value")).toString();
    }

    return info;
}

SignatureInfo CppCompletionProvider::parseSignatureHelpItem(const QJsonObject &sig)
{
    SignatureInfo info;

    info.label = sig.value(QStringLiteral("label")).toString();
    info.doc = sig.value(QStringLiteral("documentation")).toString();

    // activeParameter is read from the top-level SignatureHelp result,
    // not from each SignatureInformation. We set it in onResponseReceived.

    // Parse parameters list
    QJsonValue paramsVal = sig.value(QStringLiteral("parameters"));
    if (paramsVal.isArray()) {
        QJsonArray params = paramsVal.toArray();
        for (const QJsonValue &pv : params) {
            QJsonObject pObj = pv.toObject();
            QJsonValue labelVal = pObj.value(QStringLiteral("label"));
            if (labelVal.isString()) {
                info.parameters.append(labelVal.toString());
            } else if (labelVal.isArray()) {
                // LSP allows [start, end] offset pair into the signature label
                QJsonArray offsets = labelVal.toArray();
                if (offsets.size() >= 2) {
                    int start = offsets.at(0).toInt();
                    int end = offsets.at(1).toInt();
                    if (start >= 0 && end <= info.label.length() && start < end) {
                        info.parameters.append(
                            info.label.mid(start, end - start));
                    }
                }
            }
        }
    }

    return info;
}

void CppCompletionProvider::requestSignatureHelp(const QString &text, int cursorPos)
{
    if (!m_initialized || m_documentUri.isEmpty() || !m_client)
        return;

    // Convert cursorPos to LSP line (0-based) and character (0-based)
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

    m_signatureHelpRequestId = m_client->sendRequest(
        QStringLiteral("textDocument/signatureHelp"), params);

    m_pendingRequest = PendingRequest::SignatureHelp;
    m_requestTimer.start(500);
}

// ---- Request timeout ----

void CppCompletionProvider::onRequestTimeout()
{
    if (!m_client || !m_initialized)
        return;
    qWarning() << "CppCompletionProvider: request timed out after 500ms";
    m_requestTimer.stop();

    PendingRequest timedOut = m_pendingRequest;
    m_pendingRequest = PendingRequest::None;

    switch (timedOut) {
    case PendingRequest::Completion:
        m_completionRequestId = -1;
        emit completionReady({});
        break;
    case PendingRequest::Hover:
        m_hoverRequestId = -1;
        emit hoverReady({});
        break;
    case PendingRequest::SignatureHelp:
        m_signatureHelpRequestId = -1;
        emit signatureHelpReady({}, 0);
        break;
    default:
        break;
    }
}
