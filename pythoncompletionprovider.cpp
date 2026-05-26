#include "pythoncompletionprovider.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDebug>

PythonCompletionProvider::PythonCompletionProvider(QObject *parent)
    : CompletionProvider(parent)
{
    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &PythonCompletionProvider::onTimeout);

    m_diagnosticsTimer.setSingleShot(true);
    m_diagnosticsTimer.setInterval(500);
    connect(&m_diagnosticsTimer, &QTimer::timeout,
            this, &PythonCompletionProvider::onDiagnosticsDebounce);

    m_semanticTokensTimer.setSingleShot(true);
    m_semanticTokensTimer.setInterval(300);
    connect(&m_semanticTokensTimer, &QTimer::timeout,
            this, &PythonCompletionProvider::requestSemanticTokens);

    startProcess();
}

PythonCompletionProvider::~PythonCompletionProvider()
{
    shutdown();
}

void PythonCompletionProvider::shutdown()
{
    disconnect();

    if (m_process) {
        m_process->disconnect();
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(200);
        }
        m_process->deleteLater();
        m_process = nullptr;
    }

    m_timeoutTimer.stop();
    m_diagnosticsTimer.stop();
    m_semanticTokensTimer.stop();
    m_pendingRequest = PendingRequest::None;
    m_tokensPending = false;
}

void PythonCompletionProvider::startProcess()
{
    if (!m_jediAvailable)
        return;

    // Locate the Jedi helper script
    QString appDir = QCoreApplication::applicationDirPath();
    QString scriptPath;
    QStringList candidates = {
        appDir + QStringLiteral("/completion_helper.py"),
        appDir + QStringLiteral("/../completion_helper.py"),
        QStringLiteral("completion_helper.py"),
    };
    for (const QString &c : candidates) {
        QFileInfo fi(c);
        if (fi.exists()) {
            scriptPath = fi.absoluteFilePath();
            break;
        }
    }

    if (scriptPath.isEmpty()) {
        qWarning() << "PythonCompletionProvider: completion_helper.py not found in any candidate path";
        emit serverFailed(tr("completion_helper.py not found"));
        m_jediAvailable = false;
        return;
    }

    // Find Python interpreter
    QString python = QStandardPaths::findExecutable(QStringLiteral("python"));
    if (python.isEmpty())
        python = QStandardPaths::findExecutable(QStringLiteral("python3"));

    if (python.isEmpty()) {
        qWarning() << "PythonCompletionProvider: python not found in PATH";
        emit serverFailed(tr("Python not found. Please install Python to enable code completion."));
        m_jediAvailable = false;
        return;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::started, this, [this]() {
        qDebug() << "PythonCompletionProvider: helper process started, pid" << m_process->processId();
        emit serverReady();
    });
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &PythonCompletionProvider::onReadyRead);
    connect(m_process, &QProcess::errorOccurred,
            this, &PythonCompletionProvider::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PythonCompletionProvider::onProcessFinished);

    qDebug() << "PythonCompletionProvider: starting" << python << scriptPath;
    m_process->start(python, {scriptPath});
    // startup is async — started / errorOccurred signals handle result
}

void PythonCompletionProvider::restartProcess()
{
    if (!m_jediAvailable)
        return;
    if (m_process) {
        m_process->disconnect();
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(200);
        }
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_pendingRequest = PendingRequest::None;
    m_tokensPending = false;
    startProcess();
}

// ---- Document sync ----

void PythonCompletionProvider::openDocument(const QString &uri, const QString &languageId, const QString &text)
{
    Q_UNUSED(uri);
    Q_UNUSED(languageId);
    m_lastDiagnosticsText = text;
    sendDiagnosticsRequest(text);
    m_semanticTokensTimer.start();
}

void PythonCompletionProvider::updateText(const QString &text)
{
    m_lastDiagnosticsText = text;
    m_diagnosticsTimer.start();
    m_semanticTokensTimer.start();
}

void PythonCompletionProvider::onDiagnosticsDebounce()
{
    sendDiagnosticsRequest(m_lastDiagnosticsText);
}

// ---- Request sending ----

void PythonCompletionProvider::sendRequest(const QString &action, const QString &text, int cursorPos)
{
    if (!m_jediAvailable) {
        emitEmptyResults();
        return;
    }

    if (!m_process) {
        startProcess();
        emitEmptyResults();
        return;
    }
    if (m_process->state() == QProcess::NotRunning) {
        restartProcess();
        emitEmptyResults();
        return;
    }
    if (m_process->state() != QProcess::Running) {
        // Still starting — results won't be ready yet
        emitEmptyResults();
        return;
    }

    // Cancel any previous pending request
    m_pendingRequest = PendingRequest::None;
    m_timeoutTimer.stop();

    // Convert absolute cursorPos to 0-based line/col
    int line = 0, col = 0;
    int len = qMin(cursorPos, text.length());
    for (int i = 0; i < len; ++i) {
        if (text.at(i) == QLatin1Char('\n')) {
            ++line;
            col = 0;
        } else {
            ++col;
        }
    }

    QJsonObject req;
    req[QStringLiteral("action")] = action;
    req[QStringLiteral("code")] = text;
    QJsonArray cursor;
    cursor.append(line);
    cursor.append(col);
    req[QStringLiteral("cursor")] = cursor;

    QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
    m_process->write(payload);

    // Track the pending request type
    if (action == QStringLiteral("complete"))
        m_pendingRequest = PendingRequest::Completion;
    else if (action == QStringLiteral("hover"))
        m_pendingRequest = PendingRequest::Hover;
    else if (action == QStringLiteral("signature"))
        m_pendingRequest = PendingRequest::SignatureHelp;

    m_timeoutTimer.start(500);
}

void PythonCompletionProvider::sendDiagnosticsRequest(const QString &text)
{
    // Diagnostics don't require Jedi (uses compile()), so skip jedi check.
    if (!m_process) {
        startProcess();
        return;
    }
    if (m_process->state() == QProcess::NotRunning) {
        restartProcess();
        return;
    }
    if (m_process->state() != QProcess::Running) {
        // Still starting — the started callback will re-trigger
        return;
    }

    // Cancel previous pending request
    m_pendingRequest = PendingRequest::None;
    m_timeoutTimer.stop();

    // Base64-encode the code to avoid JSON escaping issues (matching SMD cell format)
    QByteArray codeBase64 = text.toUtf8().toBase64();

    QJsonObject req;
    req[QStringLiteral("action")] = QStringLiteral("diagnostics");
    QJsonArray cells;
    QJsonObject cell;
    cell[QStringLiteral("cellIndex")] = 0;
    cell[QStringLiteral("code")] = QString::fromLatin1(codeBase64);
    cells.append(cell);
    req[QStringLiteral("cells")] = cells;

    QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
    m_process->write(payload);

    m_pendingRequest = PendingRequest::Diagnostics;
    m_timeoutTimer.start(500);
}

void PythonCompletionProvider::requestSemanticTokens()
{
    if (!m_jediAvailable)
        return;
    if (!m_process) {
        startProcess();
        return;
    }
    if (m_process->state() == QProcess::NotRunning) {
        restartProcess();
        return;
    }
    if (m_process->state() != QProcess::Running) {
        // Still starting — the started callback will re-trigger
        return;
    }

    QString text = m_lastDiagnosticsText;
    // Skip large files to avoid performance issues
    if (text.length() > 200 * 1024)
        return;

    QJsonObject req;
    req[QStringLiteral("action")] = QStringLiteral("tokens");
    req[QStringLiteral("code")] = text;
    QJsonArray cursor;
    cursor.append(0);
    cursor.append(0);
    req[QStringLiteral("cursor")] = cursor;

    QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
    m_process->write(payload);

    // Tokens are fire-and-forget — the response may arrive after other
    // requests have been processed.  We flag that tokens are in-flight
    // and detect them by response structure rather than by m_pendingRequest.
    m_tokensPending = true;
}

void PythonCompletionProvider::requestCompletion(const QString &text, int cursorPos)
{
    sendRequest(QStringLiteral("complete"), text, cursorPos);
}

void PythonCompletionProvider::requestHover(const QString &text, int cursorPos)
{
    sendRequest(QStringLiteral("hover"), text, cursorPos);
}

void PythonCompletionProvider::requestSignatureHelp(const QString &text, int cursorPos)
{
    sendRequest(QStringLiteral("signature"), text, cursorPos);
}

// ---- Response handling ----

void PythonCompletionProvider::onReadyRead()
{
    if (!m_process)
        return;
    while (m_process->canReadLine()) {
        QByteArray line = m_process->readLine().trimmed();
        if (line.isEmpty())
            continue;
        processResponse(line);
    }
}

void PythonCompletionProvider::processResponse(const QByteArray &line)
{
    m_timeoutTimer.stop();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "PythonCompletionProvider: JSON parse error:" << err.errorString();
        emitEmptyResults();
        return;
    }

    QJsonObject msg = doc.object();

    // Check for protocol-level errors (jedi unavailable, etc.)
    if (!msg.value(QStringLiteral("ok")).toBool()) {
        QString error = msg.value(QStringLiteral("error")).toString();
        qWarning() << "PythonCompletionProvider:" << error;

        // If jedi is unavailable, mark it and terminate the process
        if (!m_jediAvailable) {
            // Already known — just emit empty
            emitEmptyResults();
            return;
        }

        // This might be a transient error — but if it's about jedi, mark permanently
        if (error.contains(QStringLiteral("jedi"), Qt::CaseInsensitive)) {
            m_jediAvailable = false;
            qDebug() << "PythonCompletionProvider: jedi unavailable, disabling provider";
            if (m_process) {
                m_process->kill();
            }
            // Signal the CodeEditor to swap to keyword fallback
            emit serverFailed(error);
            return;
        }

        emitEmptyResults();
        return;
    }

    QJsonValue dataVal = msg.value(QStringLiteral("data"));

    // Detect in-flight tokens response by structure rather than by
    // pending-request state, so late arrivals (past the 500ms timeout
    // of another request) are still processed.
    if (m_tokensPending) {
        QJsonArray arr = dataVal.toArray();
        if (!arr.isEmpty()) {
            QJsonObject first = arr[0].toObject();
            if (first.contains(QStringLiteral("line"))
                && first.contains(QStringLiteral("type"))) {
                QList<SemanticToken> tokens;
                for (const QJsonValue &v : arr) {
                    QJsonObject obj = v.toObject();
                    SemanticToken t;
                    t.line = obj.value(QStringLiteral("line")).toInt();
                    t.startChar = obj.value(QStringLiteral("startChar")).toInt();
                    t.length = obj.value(QStringLiteral("length")).toInt();
                    t.type = obj.value(QStringLiteral("type")).toString();
                    tokens.append(t);
                }
                m_tokensPending = false;
                emit semanticTokensReady(tokens);
                // Also clear if it was the main pending request
                if (m_pendingRequest == PendingRequest::SemanticTokens) {
                    m_pendingRequest = PendingRequest::None;
                    m_timeoutTimer.stop();
                }
                return;
            }
        }
    }

    PendingRequest pending = m_pendingRequest;
    m_pendingRequest = PendingRequest::None;

    switch (pending) {
    case PendingRequest::Completion: {
        QList<CompletionItem> items;
        QJsonArray arr = dataVal.toArray();
        for (const QJsonValue &v : arr) {
            QJsonObject obj = v.toObject();
            CompletionItem ci;
            ci.name = obj.value(QStringLiteral("name")).toString();
            ci.type = obj.value(QStringLiteral("type")).toString();
            ci.signature = obj.value(QStringLiteral("signature")).toString();
            ci.detail = obj.value(QStringLiteral("detail")).toString();
            ci.doc = obj.value(QStringLiteral("doc")).toString();
            items.append(ci);
        }
        qDebug() << "PythonCompletionProvider: completion returned" << items.size() << "items";
        emit completionReady(items);
        break;
    }
    case PendingRequest::Hover: {
        HoverInfo info;
        QJsonObject obj = dataVal.toObject();
        info.signature = obj.value(QStringLiteral("signature")).toString();
        info.doc = obj.value(QStringLiteral("doc")).toString();
        info.definition = obj.value(QStringLiteral("definition")).toString();
        qDebug() << "PythonCompletionProvider: hover returned";
        emit hoverReady(info);
        break;
    }
    case PendingRequest::SignatureHelp: {
        QList<SignatureInfo> signatures;
        QJsonArray arr = dataVal.toArray();
        int activeIndex = 0;
        for (const QJsonValue &v : arr) {
            QJsonObject obj = v.toObject();
            SignatureInfo si;
            si.label = obj.value(QStringLiteral("label")).toString();
            si.doc = obj.value(QStringLiteral("doc")).toString();
            si.activeParameter = obj.value(QStringLiteral("activeParameter")).toInt(0);
            QJsonArray params = obj.value(QStringLiteral("parameters")).toArray();
            for (const QJsonValue &pv : params)
                si.parameters.append(pv.toString());
            signatures.append(si);
        }
        qDebug() << "PythonCompletionProvider: signatureHelp returned" << signatures.size() << "signatures";
        emit signatureHelpReady(signatures, activeIndex);
        break;
    }
    case PendingRequest::SemanticTokens: {
        QList<SemanticToken> tokens;
        QJsonArray arr = dataVal.toArray();
        for (const QJsonValue &v : arr) {
            QJsonObject obj = v.toObject();
            SemanticToken t;
            t.line = obj.value(QStringLiteral("line")).toInt();
            t.startChar = obj.value(QStringLiteral("startChar")).toInt();
            t.length = obj.value(QStringLiteral("length")).toInt();
            t.type = obj.value(QStringLiteral("type")).toString();
            tokens.append(t);
        }
        emit semanticTokensReady(tokens);
        break;
    }
    case PendingRequest::Diagnostics: {
        QList<SmdDiagnostic> diagnostics;
        QJsonArray arr = dataVal.toArray();
        for (const QJsonValue &v : arr) {
            QJsonObject obj = v.toObject();
            SmdDiagnostic d;
            d.startLine = obj.value(QStringLiteral("startLine")).toInt();
            d.startCol  = obj.value(QStringLiteral("startCol")).toInt();
            d.endLine   = obj.value(QStringLiteral("endLine")).toInt();
            d.endCol    = obj.value(QStringLiteral("endCol")).toInt();
            d.message   = obj.value(QStringLiteral("message")).toString();
            d.severity  = obj.value(QStringLiteral("severity")).toInt(1);
            diagnostics.append(d);
        }
        qDebug() << "PythonCompletionProvider: diagnostics returned" << diagnostics.size() << "items";
        emit diagnosticsUpdated(diagnostics);
        break;
    }
    default:
        break;
    }
}

void PythonCompletionProvider::emitEmptyResults()
{
    PendingRequest pending = m_pendingRequest;
    m_pendingRequest = PendingRequest::None;
    m_timeoutTimer.stop();

    switch (pending) {
    case PendingRequest::Completion:
        emit completionReady({});
        break;
    case PendingRequest::Hover:
        emit hoverReady({});
        break;
    case PendingRequest::SignatureHelp:
        emit signatureHelpReady({}, 0);
        break;
    case PendingRequest::SemanticTokens:
        m_tokensPending = false;
        emit semanticTokensReady({});
        break;
    default:
        break;
    }
}

// ---- Timeout ----

void PythonCompletionProvider::onTimeout()
{
    if (!m_process)
        return;
    qWarning() << "PythonCompletionProvider: request timed out after 500ms";
    emitEmptyResults();
}

// ---- Process lifecycle ----

void PythonCompletionProvider::onProcessError(QProcess::ProcessError err)
{
    if (!m_process)
        return;
    qWarning() << "PythonCompletionProvider: process error" << err;
    m_tokensPending = false;
    emitEmptyResults();
    if (err == QProcess::FailedToStart) {
        qWarning() << "PythonCompletionProvider: Python helper failed to start";
        m_jediAvailable = false;
        m_process->deleteLater();
        m_process = nullptr;
        emit serverFailed(tr("Failed to start Python helper process."));
    }
}

void PythonCompletionProvider::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_process)
        return;
    qDebug() << "PythonCompletionProvider: process finished, exitCode" << exitCode
             << "status" << status;

    m_tokensPending = false;
    emitEmptyResults();

    // Restart on crash (but not if jedi was unavailable — that's permanent)
    if (status == QProcess::CrashExit && m_jediAvailable) {
        qDebug() << "PythonCompletionProvider: helper crashed, restarting in 1s...";
        QTimer::singleShot(1000, this, &PythonCompletionProvider::restartProcess);
    } else {
        m_process->deleteLater();
        m_process = nullptr;
    }
}
