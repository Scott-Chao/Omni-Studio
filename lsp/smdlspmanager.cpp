#include "smdlspmanager.h"
#include "config/configmanager.h"
#include "lspclient.h"
#include "lsputils.h"
#include "core/utilities.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QStandardPaths>
#include <QDebug>

// ─── CellCompletionAdapter ───────────────────────────────────────────

class SmdLspManager::CellCompletionAdapter : public CompletionProvider
{
public:
    CellCompletionAdapter(SmdLspManager *mgr, int cellIndex)
        : CompletionProvider(mgr), m_mgr(mgr), m_cellIndex(cellIndex) {}

    void requestCompletion(const QString &text, int cursorPos) override
    {
        int line, col;
        LspUtils::cursorToLineCol(text, cursorPos, line, col);
        m_mgr->requestCompletion(m_cellIndex, line, col);
    }

    void requestHover(const QString &text, int cursorPos) override
    {
        int line, col;
        LspUtils::cursorToLineCol(text, cursorPos, line, col);
        m_mgr->requestHover(m_cellIndex, line, col);
    }

    void requestSignatureHelp(const QString &text, int cursorPos) override
    {
        int line, col;
        LspUtils::cursorToLineCol(text, cursorPos, line, col);
        m_mgr->requestSignatureHelp(m_cellIndex, line, col);
    }

private:
    SmdLspManager *m_mgr;
    int m_cellIndex;
};

// ─── LanguageServer helper ───────────────────────────────────────────

void SmdLspManager::LanguageServer::reset()
{
    if (client) {
        client->stop();
        client->deleteLater();
        client = nullptr;
    }
    initialized = false;
    documentVersion = 0;
    virtualDocUri.clear();
    cellOrder.clear();
    cellRanges.clear();
    completionRequestId = -1;
    hoverRequestId = -1;
    signatureHelpRequestId = -1;
    semanticTokensRequestId = -1;
    pendingType = None;
    requestingCellIndex = -1;
    tokenTypeLegend.clear();
    tokenModifierLegend.clear();
}

// ─── Constructor / Destructor ────────────────────────────────────────

SmdLspManager::SmdLspManager(QObject *parent)
    : QObject(parent)
{
    m_pyTimeoutTimer.setSingleShot(true);
    connect(&m_pyTimeoutTimer, &QTimer::timeout, this, &SmdLspManager::onPyTimeout);

    m_pyDiagnosticsTimer.setSingleShot(true);
    m_pyDiagnosticsTimer.setInterval(500);
    connect(&m_pyDiagnosticsTimer, &QTimer::timeout, this, &SmdLspManager::requestPythonDiagnostics);

    m_cppSemanticTokensTimer = new QTimer(this);
    m_cppSemanticTokensTimer->setSingleShot(true);
    m_cppSemanticTokensTimer->setInterval(300);
    connect(m_cppSemanticTokensTimer, &QTimer::timeout,
            this, &SmdLspManager::requestCppSemanticTokens);

    m_pySemanticTokensTimer = new QTimer(this);
    m_pySemanticTokensTimer->setSingleShot(true);
    m_pySemanticTokensTimer->setInterval(300);
    connect(m_pySemanticTokensTimer, &QTimer::timeout,
            this, &SmdLspManager::requestPythonSemanticTokens);

    // Forward per-cell results to the appropriate adapter
    connect(this, &SmdLspManager::completionReadyForCell, this,
            [this](int cellIndex, QList<CompletionItem> items) {
                if (auto *adapter = m_adapters.value(cellIndex))
                    emit adapter->completionReady(items);
            });
    connect(this, &SmdLspManager::hoverReadyForCell, this,
            [this](int cellIndex, HoverInfo info) {
                if (auto *adapter = m_adapters.value(cellIndex))
                    emit adapter->hoverReady(info);
            });
    connect(this, &SmdLspManager::signatureHelpReadyForCell, this,
            [this](int cellIndex, QList<SignatureInfo> signatures, int activeIndex) {
                if (auto *adapter = m_adapters.value(cellIndex))
                    emit adapter->signatureHelpReady(signatures, activeIndex);
            });
    connect(this, &SmdLspManager::semanticTokensReadyForCell, this,
            [this](int cellIndex, QList<SemanticToken> tokens) {
                if (auto *adapter = m_adapters.value(cellIndex))
                    emit adapter->semanticTokensReady(tokens);
            });
}

SmdLspManager::~SmdLspManager()
{
    shutdown();
}

void SmdLspManager::shutdown()
{
    m_shuttingDown = true;

    if (m_cppSemanticTokensTimer) {
        m_cppSemanticTokensTimer->stop();
    }
    if (m_pySemanticTokensTimer) {
        m_pySemanticTokensTimer->stop();
    }
    m_pyTokensPending = false;

    disconnect();

    m_cppServer.reset();
    m_pyServer.reset();

    if (m_pyProcess) {
        m_pyProcess->disconnect();
        if (m_pyProcess->state() != QProcess::NotRunning) {
            m_pyProcess->kill();
            m_pyProcess->waitForFinished(200);
        }
        m_pyProcess->deleteLater();
        m_pyProcess = nullptr;
    }
    m_pyTimeoutTimer.stop();

    qDeleteAll(m_adapters);
    m_adapters.clear();
    m_activeCppGroup = 0;
    m_groupDiagnostics.clear();
}

// ─── Initialization ──────────────────────────────────────────────────

void SmdLspManager::initialize(const QString &smdFilePath)
{
    m_smdFilePath = smdFilePath;
    QFileInfo fi(smdFilePath);
    QString baseName = fi.completeBaseName();

    m_cppServer.virtualDocUri = QStringLiteral("file:///%1_smd_cpp.cpp").arg(baseName);
    m_pyServer.virtualDocUri  = QStringLiteral("file:///%1_smd_py.py").arg(baseName);
}

// ─── Language helpers ────────────────────────────────────────────────

SmdLspManager::LanguageServer *SmdLspManager::serverForLang(const QString &langId)
{
    if (langId == QStringLiteral("cpp"))
        return &m_cppServer;
    if (langId == QStringLiteral("python"))
        return &m_pyServer;
    return nullptr;
}

const SmdLspManager::LanguageServer *SmdLspManager::serverForLang(const QString &langId) const
{
    if (langId == QStringLiteral("cpp"))
        return &m_cppServer;
    if (langId == QStringLiteral("python"))
        return &m_pyServer;
    return nullptr;
}

QString SmdLspManager::langIdToLanguageId(const QString &langId) const
{
    if (langId == QStringLiteral("cpp"))    return QStringLiteral("cpp");
    if (langId == QStringLiteral("python")) return QStringLiteral("python");
    return langId;
}

// ─── Cell lifecycle ──────────────────────────────────────────────────

void SmdLspManager::cellAdded(int cellIndex, const QString &langId,
                               const QString &content)
{
    LanguageServer *srv = serverForLang(langId);
    if (!srv) {
        return;
    }

    // Cache content
    if (langId == QStringLiteral("cpp"))
        m_cppCellContents[cellIndex] = content;
    else if (langId == QStringLiteral("python"))
        m_pyCellContents[cellIndex] = content;

    // Guard: skip if already in cellOrder (duplicate signal connection defense)
    if (srv->cellOrder.contains(cellIndex)) {
        rebuildVirtualDoc(langId);
        if (langId == QStringLiteral("python")) {
            m_pyDiagnosticsTimer.start();
            m_pySemanticTokensTimer->start();
        }
        return;
    }

    // Insert in order
    int insertPos = 0;
    while (insertPos < srv->cellOrder.size() && srv->cellOrder[insertPos] < cellIndex)
        ++insertPos;
    srv->cellOrder.insert(insertPos, cellIndex);

    // Create adapter
    if (!m_adapters.contains(cellIndex)) {
        auto *adapter = new CellCompletionAdapter(this, cellIndex);
        m_adapters[cellIndex] = adapter;
    }

    // Lazy-start server on first cell.
    // start() is now non-blocking (no waitForStarted), so it is safe to
    // call synchronously from setPlainText via cellAdded.
    if (!srv->client && langId == QStringLiteral("cpp"))
        startCppServer();
    if (!m_pyProcess && langId == QStringLiteral("python"))
        startPythonProcess();

    rebuildVirtualDoc(langId);

    if (langId == QStringLiteral("python")) {
        m_pyDiagnosticsTimer.start();
        m_pySemanticTokensTimer->start();
    }
}

void SmdLspManager::cellRemoved(int cellIndex, const QString &langId)
{
    LanguageServer *srv = serverForLang(langId);
    if (!srv) return;

    srv->cellOrder.removeAll(cellIndex);
    if (langId == QStringLiteral("cpp"))
        m_cppCellContents.remove(cellIndex);
    else if (langId == QStringLiteral("python"))
        m_pyCellContents.remove(cellIndex);

    if (m_adapters.contains(cellIndex)) {
        delete m_adapters[cellIndex];
        m_adapters.remove(cellIndex);
    }

    m_diagnostics.remove(cellIndex);

    rebuildVirtualDoc(langId);
}

void SmdLspManager::cellContentChanged(int cellIndex, const QString &langId,
                                        const QString &newContent)
{
    // Guard: skip if the content didn't actually change.  rehighlight()
    // can trigger QTextDocument::contentsChanged which bubbles up as a
    // spurious contentChanged emission — processing it would restart the
    // semantic tokens timer and create an infinite request loop.
    const QMap<int, QString> &contents =
        (langId == QStringLiteral("cpp")) ? m_cppCellContents : m_pyCellContents;
    if (contents.value(cellIndex) == newContent)
        return;

    if (langId == QStringLiteral("cpp"))
        m_cppCellContents[cellIndex] = newContent;
    else if (langId == QStringLiteral("python"))
        m_pyCellContents[cellIndex] = newContent;

    rebuildVirtualDoc(langId);

    if (langId == QStringLiteral("python")) {
        m_pyDiagnosticsTimer.start();
        m_pySemanticTokensTimer->start();
    }
}

void SmdLspManager::cellTypeChanged(int cellIndex, const QString &oldLangId,
                                     const QString &newLangId, const QString &content)
{
    cellRemoved(cellIndex, oldLangId);
    cellAdded(cellIndex, newLangId, content);
}

// ─── Virtual document ────────────────────────────────────────────────

QString SmdLspManager::buildVirtualDocContent(const QString &langId) const
{
    const LanguageServer *srv = serverForLang(langId);
    if (!srv) return {};

    const QMap<int, QString> &contents =
        (langId == QStringLiteral("cpp")) ? m_cppCellContents : m_pyCellContents;
    QString comment = (langId == QStringLiteral("cpp")) ? QStringLiteral("//") : QStringLiteral("#");

    QStringList lines;
    for (int ci : srv->cellOrder) {
        // Filter: only include cells from the active C++ group
        if (langId == QStringLiteral("cpp") && m_activeCppGroup >= 0
            && computeCppGroup(ci) != m_activeCppGroup)
            continue;
        lines.append(QStringLiteral("%1 --- smd:cell:%2 ---").arg(comment).arg(ci));
        QString content = contents.value(ci);
        if (!content.isEmpty())
            lines.append(content.split(QLatin1Char('\n')));
    }
    return lines.join(QLatin1Char('\n'));
}

void SmdLspManager::rebuildVirtualDoc(const QString &langId)
{
    LanguageServer *srv = serverForLang(langId);
    if (!srv) return;

    // Build line mapping
    const QMap<int, QString> &contents =
        (langId == QStringLiteral("cpp")) ? m_cppCellContents : m_pyCellContents;
    srv->cellRanges.clear();
    int virtualLine = 0;
    for (int ci : srv->cellOrder) {
        // Filter: only include cells from the active C++ group
        if (langId == QStringLiteral("cpp") && m_activeCppGroup >= 0
            && computeCppGroup(ci) != m_activeCppGroup)
            continue;
        QString content = contents.value(ci);
        int localLines = content.isEmpty() ? 1 : content.count(QLatin1Char('\n')) + 1;
        srv->cellRanges[ci] = {virtualLine + 1, localLines}; // +1 for separator line
        virtualLine = virtualLine + 1 + localLines;
    }

    syncVirtualDoc(langId);
}

void SmdLspManager::syncVirtualDoc(const QString &langId)
{
    LanguageServer *srv = serverForLang(langId);
    if (!srv || !srv->initialized || srv->virtualDocUri.isEmpty()) return;

    if (langId == QStringLiteral("cpp") && srv->client) {
        srv->documentVersion++;
        QString text = buildVirtualDocContent(langId);

        QJsonObject textDocument;
        textDocument[QStringLiteral("uri")] = srv->virtualDocUri;
        textDocument[QStringLiteral("version")] = srv->documentVersion;

        QJsonObject change;
        change[QStringLiteral("text")] = text;

        QJsonArray contentChanges;
        contentChanges.append(change);

        QJsonObject params;
        params[QStringLiteral("textDocument")] = textDocument;
        params[QStringLiteral("contentChanges")] = contentChanges;

        srv->client->sendNotification(QStringLiteral("textDocument/didChange"), params);

        // Schedule debounced semantic tokens request after content change
        m_cppSemanticTokensTimer->start();
    }
    // Python is stateless, but schedule semantic tokens refresh
    if (langId == QStringLiteral("python") && m_pyServer.initialized) {
        m_pySemanticTokensTimer->start();
    }
}

void SmdLspManager::requestCppSemanticTokens()
{
    if (m_shuttingDown || !m_cppServer.client || !m_cppServer.initialized)
        return;
    if (m_cppServer.virtualDocUri.isEmpty())
        return;

    QJsonObject textDocument;
    textDocument[QStringLiteral("uri")] = m_cppServer.virtualDocUri;

    QJsonObject params;
    params[QStringLiteral("textDocument")] = textDocument;

    m_cppServer.semanticTokensRequestId =
        m_cppServer.client->sendRequest(
            QStringLiteral("textDocument/semanticTokens/full"), params);
}

QList<SemanticToken> SmdLspManager::parseSemanticTokens(const QJsonObject &result)
{
    return LspUtils::parseSemanticTokens(
        result, m_cppServer.tokenTypeLegend, m_cppServer.tokenModifierLegend);
}

QMap<int, QList<SemanticToken>> SmdLspManager::mapVirtualTokensToCells(
    const QList<SemanticToken> &tokens, const LanguageServer &srv) const
{
    QMap<int, QList<SemanticToken>> tokensByCell;
    for (const SemanticToken &token : tokens) {
        for (auto it = srv.cellRanges.begin(); it != srv.cellRanges.end(); ++it) {
            int ci = it.key();
            CellRange cr = it.value();
            if (token.line >= cr.firstVirtualLine
                && token.line < cr.firstVirtualLine + cr.localLineCount) {
                SemanticToken localToken = token;
                localToken.line = token.line - cr.firstVirtualLine;
                tokensByCell[ci].append(localToken);
                break;
            }
        }
    }
    return tokensByCell;
}

void SmdLspManager::cellLocalToVirtual(int cellIndex, int localLine, int localCol,
                                        const QString &langId,
                                        int &outLine, int &outCol) const
{
    const LanguageServer *srv = serverForLang(langId);
    if (!srv || !srv->cellRanges.contains(cellIndex)) {
        outLine = 0; outCol = 0;
        return;
    }
    CellRange cr = srv->cellRanges[cellIndex];
    outLine = cr.firstVirtualLine + localLine;
    outCol = localCol;
}

// ─── C++ LSP server ──────────────────────────────────────────────────

void SmdLspManager::startCppServer()
{
    if (m_shuttingDown || m_cppServer.client) {
        return;
    }
    QString clangdPath;
    QString configuredPath = ConfigManager::instance().toolClangdPath();
    if (!configuredPath.isEmpty() && QFileInfo::exists(configuredPath)) {
        clangdPath = configuredPath;
    } else {
        clangdPath = QStandardPaths::findExecutable(QStringLiteral("clangd"));
    }
    if (clangdPath.isEmpty()) {
        qWarning() << "SmdLspManager: clangd not found";
        emit serverFailed(QStringLiteral("cpp"),
                          tr("clangd not found. Please install clangd for C++ code intelligence."));
        return;
    }

    m_cppServer.client = new LspClient(this);

    connect(m_cppServer.client, &LspClient::serverStarted, this, [this]() {
        sendInitialize(QStringLiteral("cpp"));
    });
    connect(m_cppServer.client, &LspClient::serverError, this, [this](QProcess::ProcessError err) {
        // Only handle startup failures; runtime errors are handled by serverStopped
        if (err != QProcess::FailedToStart) return;
        if (m_shuttingDown || !m_cppServer.client) return;
        qWarning() << "SmdLspManager: clangd failed to start";
        emit serverFailed(QStringLiteral("cpp"), tr("Failed to start clangd process."));
        m_cppServer.client->deleteLater();
        m_cppServer.client = nullptr;
    });
    connect(m_cppServer.client, &LspClient::responseReceived,
            this, &SmdLspManager::onCppResponseReceived);
    connect(m_cppServer.client, &LspClient::notificationReceived,
            this, &SmdLspManager::onCppNotificationReceived);
    connect(m_cppServer.client, &LspClient::requestFailed,
            this, &SmdLspManager::onCppRequestFailed);
    connect(m_cppServer.client, &LspClient::serverStopped,
            this, &SmdLspManager::onCppServerStopped);

    QString argsStr = ConfigManager::instance().toolClangdArgs();
    QStringList args;
    if (!argsStr.isEmpty())
        args = argsStr.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    m_cppServer.client->start(clangdPath, args);
    // startup is async — serverStarted / serverError signals handle result
}

void SmdLspManager::sendInitialize(const QString &langId)
{
    LanguageServer *srv = serverForLang(langId);
    if (!srv || !srv->client) return;

    QJsonObject params = LspUtils::buildInitializeParams();
    srv->client->sendRequest(QStringLiteral("initialize"), params);
}

void SmdLspManager::onCppResponseReceived(int id, QJsonObject result)
{
    if (m_shuttingDown || !m_cppServer.client) return;

    if (!m_cppServer.initialized) {
        // Extract semantic tokens legend from server capabilities
        QJsonObject serverCaps = result.value(QStringLiteral("capabilities")).toObject();
        QJsonObject tokenProvider = serverCaps.value(QStringLiteral("semanticTokensProvider")).toObject();
        QJsonObject legend = tokenProvider.value(QStringLiteral("legend")).toObject();
        QJsonArray tokenTypes = legend.value(QStringLiteral("tokenTypes")).toArray();
        QJsonArray tokenModifiers = legend.value(QStringLiteral("tokenModifiers")).toArray();
        m_cppServer.tokenTypeLegend.clear();
        m_cppServer.tokenModifierLegend.clear();
        for (const QJsonValue &v : tokenTypes)
            m_cppServer.tokenTypeLegend.append(v.toString());
        for (const QJsonValue &v : tokenModifiers)
            m_cppServer.tokenModifierLegend.append(v.toString());

        m_cppServer.client->sendNotification(QStringLiteral("initialized"), QJsonObject());
        m_cppServer.initialized = true;

        // Send didOpen with current virtual document
        m_cppServer.documentVersion = 1;
        QString text = buildVirtualDocContent(QStringLiteral("cpp"));

        QJsonObject textDocument;
        textDocument[QStringLiteral("uri")] = m_cppServer.virtualDocUri;
        textDocument[QStringLiteral("languageId")] = QStringLiteral("cpp");
        textDocument[QStringLiteral("version")] = m_cppServer.documentVersion;
        textDocument[QStringLiteral("text")] = text;

        QJsonObject params;
        params[QStringLiteral("textDocument")] = textDocument;

        m_cppServer.client->sendNotification(
            QStringLiteral("textDocument/didOpen"), params);

        emit serverReady(QStringLiteral("cpp"));

        // Request initial semantic tokens after didOpen is processed
        m_cppSemanticTokensTimer->start();
        return;
    }

    if (id == m_cppServer.completionRequestId) {
        m_cppServer.completionRequestId = -1;
        int cellIndex = m_cppServer.requestingCellIndex;
        m_cppServer.pendingType = LanguageServer::None;

        QList<CompletionItem> items;
        QJsonValue itemsVal = result.value(QStringLiteral("items"));
        if (itemsVal.isUndefined())
            itemsVal = result;
        QJsonArray arr;
        if (itemsVal.isObject() && itemsVal.toObject().contains(QStringLiteral("items")))
            arr = itemsVal.toObject().value(QStringLiteral("items")).toArray();
        else if (itemsVal.isArray())
            arr = itemsVal.toArray();

        for (const QJsonValue &val : arr)
            items.append(LspUtils::parseCompletionItem(val.toObject()));
        emit completionReadyForCell(cellIndex, items);

    } else if (id == m_cppServer.hoverRequestId) {
        m_cppServer.hoverRequestId = -1;
        int cellIndex = m_cppServer.requestingCellIndex;
        m_cppServer.pendingType = LanguageServer::None;

        HoverInfo info = LspUtils::parseHoverContents(
            result.value(QStringLiteral("contents")));
        emit hoverReadyForCell(cellIndex, info);

    } else if (id == m_cppServer.signatureHelpRequestId) {
        m_cppServer.signatureHelpRequestId = -1;
        int cellIndex = m_cppServer.requestingCellIndex;
        m_cppServer.pendingType = LanguageServer::None;

        QList<SignatureInfo> signatures;
        QJsonValue sigsVal = result.value(QStringLiteral("signatures"));
        if (sigsVal.isArray()) {
            for (const QJsonValue &sv : sigsVal.toArray())
                signatures.append(LspUtils::parseSignatureInfo(sv.toObject()));
        }
        int activeSig = result.value(QStringLiteral("activeSignature")).toInt(0);
        int activeParam = result.value(QStringLiteral("activeParameter")).toInt(0);
        if (activeSig >= 0 && activeSig < signatures.size())
            signatures[activeSig].activeParameter = activeParam;
        emit signatureHelpReadyForCell(cellIndex, signatures, activeSig);
    } else if (id == m_cppServer.semanticTokensRequestId) {
        m_cppServer.semanticTokensRequestId = -1;

        QList<SemanticToken> allTokens = parseSemanticTokens(result);
        if (allTokens.isEmpty())
            return;

        QMap<int, QList<SemanticToken>> tokensByCell = mapVirtualTokensToCells(allTokens, m_cppServer);
        for (auto it = tokensByCell.begin(); it != tokensByCell.end(); ++it)
            emit semanticTokensReadyForCell(it.key(), it.value());
    }
}

void SmdLspManager::onCppNotificationReceived(QString method, QJsonObject params)
{
    if (m_shuttingDown || !m_cppServer.client) return;
    if (method == QStringLiteral("textDocument/publishDiagnostics"))
        processDiagnostics(QStringLiteral("cpp"), params);
}

void SmdLspManager::onCppRequestFailed(int id, QJsonObject error)
{
    Q_UNUSED(error);
    if (m_shuttingDown) return;
    if (id == m_cppServer.completionRequestId) {
        m_cppServer.completionRequestId = -1;
        emit completionReadyForCell(m_cppServer.requestingCellIndex, {});
    } else if (id == m_cppServer.hoverRequestId) {
        m_cppServer.hoverRequestId = -1;
        emit hoverReadyForCell(m_cppServer.requestingCellIndex, {});
    } else if (id == m_cppServer.signatureHelpRequestId) {
        m_cppServer.signatureHelpRequestId = -1;
        emit signatureHelpReadyForCell(m_cppServer.requestingCellIndex, {}, 0);
    } else if (id == m_cppServer.semanticTokensRequestId) {
        m_cppServer.semanticTokensRequestId = -1;
        // Semantic tokens failure is non-critical — silently ignore
    }
}

void SmdLspManager::onCppServerStopped(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    if (m_shuttingDown || !m_cppServer.client) return;
    m_cppServer.initialized = false;
    if (status == QProcess::CrashExit) {
        m_cppServer.client->deleteLater();
        m_cppServer.client = nullptr;
        QTimer::singleShot(1000, this, [this]() {
            if (!m_shuttingDown) startCppServer();
        });
    }
}

void SmdLspManager::processDiagnostics(const QString &langId,
                                        const QJsonObject &params)
{
    LanguageServer *srv = serverForLang(langId);
    if (!srv) {
        return;
    }

    QJsonArray diagnostics = params.value(QStringLiteral("diagnostics")).toArray();
    QMap<int, QList<SmdDiagnostic>> newDiags;

    for (const QJsonValue &val : diagnostics) {
        QJsonObject diag = val.toObject();
        QJsonObject range = diag.value(QStringLiteral("range")).toObject();
        QJsonObject start = range.value(QStringLiteral("start")).toObject();
        QJsonObject end = range.value(QStringLiteral("end")).toObject();

        int virtualLine = start.value(QStringLiteral("line")).toInt();

        // Find the cell containing this virtual line
        for (auto it = srv->cellRanges.begin(); it != srv->cellRanges.end(); ++it) {
            int ci = it.key();
            CellRange cr = it.value();
            if (virtualLine >= cr.firstVirtualLine
                && virtualLine < cr.firstVirtualLine + cr.localLineCount) {
                SmdDiagnostic d;
                d.cellIndex = ci;
                d.startLine = virtualLine - cr.firstVirtualLine;
                d.startCol = start.value(QStringLiteral("character")).toInt();
                d.endLine = end.value(QStringLiteral("line")).toInt() - cr.firstVirtualLine;
                d.endCol = end.value(QStringLiteral("character")).toInt();
                d.message = diag.value(QStringLiteral("message")).toString();
                d.severity = diag.value(QStringLiteral("severity")).toInt();
                newDiags[ci].append(d);
                break;
            }
        }
    }

    // Emit per-cell
    for (auto it = newDiags.begin(); it != newDiags.end(); ++it) {
        m_diagnostics[it.key()] = it.value();
        emit diagnosticsUpdated(it.key(), it.value());
    }

    // Clear diagnostics for cells of this language that no longer have any.
    // Only touch cells belonging to this language (srv->cellRanges keys),
    // so that C++ diagnostics don't clear Python diagnostics and vice versa.
    const QList<int> diagKeys = m_diagnostics.keys();
    for (int ci : diagKeys) {
        if (!newDiags.contains(ci) && srv->cellRanges.contains(ci)) {
            m_diagnostics.remove(ci);
            emit diagnosticsUpdated(ci, {});
        }
    }
}

// ─── C++ LSP requests ────────────────────────────────────────────────

void SmdLspManager::sendCompletionRequest(int cellIndex, int line, int col,
                                           const QString &langId)
{
    LanguageServer *srv = serverForLang(langId);
    if (!srv || !srv->client || !srv->initialized) {
        emit completionReadyForCell(cellIndex, {});
        return;
    }

    int vl, vc;
    cellLocalToVirtual(cellIndex, line, col, langId, vl, vc);

    QJsonObject position;
    position[QStringLiteral("line")] = vl;
    position[QStringLiteral("character")] = vc;

    QJsonObject textDocument;
    textDocument[QStringLiteral("uri")] = srv->virtualDocUri;

    QJsonObject params;
    params[QStringLiteral("textDocument")] = textDocument;
    params[QStringLiteral("position")] = position;

    QJsonObject context;
    context[QStringLiteral("triggerKind")] = 2;
    params[QStringLiteral("context")] = context;

    srv->completionRequestId =
        srv->client->sendRequest(QStringLiteral("textDocument/completion"), params);
    srv->pendingType = LanguageServer::Completion;
    srv->requestingCellIndex = cellIndex;
}

void SmdLspManager::sendHoverRequest(int cellIndex, int line, int col,
                                      const QString &langId)
{
    LanguageServer *srv = serverForLang(langId);
    if (!srv || !srv->client || !srv->initialized) {
        emit hoverReadyForCell(cellIndex, {});
        return;
    }

    int vl, vc;
    cellLocalToVirtual(cellIndex, line, col, langId, vl, vc);

    QJsonObject position;
    position[QStringLiteral("line")] = vl;
    position[QStringLiteral("character")] = vc;

    QJsonObject textDocument;
    textDocument[QStringLiteral("uri")] = srv->virtualDocUri;

    QJsonObject params;
    params[QStringLiteral("textDocument")] = textDocument;
    params[QStringLiteral("position")] = position;

    srv->hoverRequestId =
        srv->client->sendRequest(QStringLiteral("textDocument/hover"), params);
    srv->pendingType = LanguageServer::Hover;
    srv->requestingCellIndex = cellIndex;
}

void SmdLspManager::sendSignatureHelpRequest(int cellIndex, int line, int col,
                                               const QString &langId)
{
    LanguageServer *srv = serverForLang(langId);
    if (!srv || !srv->client || !srv->initialized) {
        emit signatureHelpReadyForCell(cellIndex, {}, 0);
        return;
    }

    int vl, vc;
    cellLocalToVirtual(cellIndex, line, col, langId, vl, vc);

    QJsonObject position;
    position[QStringLiteral("line")] = vl;
    position[QStringLiteral("character")] = vc;

    QJsonObject textDocument;
    textDocument[QStringLiteral("uri")] = srv->virtualDocUri;

    QJsonObject params;
    params[QStringLiteral("textDocument")] = textDocument;
    params[QStringLiteral("position")] = position;

    srv->signatureHelpRequestId =
        srv->client->sendRequest(QStringLiteral("textDocument/signatureHelp"), params);
    srv->pendingType = LanguageServer::SignatureHelp;
    srv->requestingCellIndex = cellIndex;
}

// ─── Public request API ──────────────────────────────────────────────

void SmdLspManager::requestCompletion(int cellIndex, int cursorLine, int cursorCol)
{
    // Determine language from which cache has the cell
    if (m_cppCellContents.contains(cellIndex))
        sendCompletionRequest(cellIndex, cursorLine, cursorCol, QStringLiteral("cpp"));
    else if (m_pyCellContents.contains(cellIndex))
        sendPythonRequest(QStringLiteral("complete"), cellIndex, cursorLine, cursorCol);
}

void SmdLspManager::requestHover(int cellIndex, int cursorLine, int cursorCol)
{
    if (m_cppCellContents.contains(cellIndex))
        sendHoverRequest(cellIndex, cursorLine, cursorCol, QStringLiteral("cpp"));
    else if (m_pyCellContents.contains(cellIndex))
        sendPythonHoverLocal(cellIndex, cursorLine, cursorCol);
}

void SmdLspManager::requestSignatureHelp(int cellIndex, int cursorLine, int cursorCol)
{
    if (m_cppCellContents.contains(cellIndex))
        sendSignatureHelpRequest(cellIndex, cursorLine, cursorCol, QStringLiteral("cpp"));
    else if (m_pyCellContents.contains(cellIndex))
        sendPythonRequest(QStringLiteral("signature"), cellIndex, cursorLine, cursorCol);
}

CompletionProvider *SmdLspManager::providerForCell(int cellIndex, const QString &langId)
{
    if (m_adapters.contains(cellIndex))
        return m_adapters[cellIndex];
    Q_UNUSED(langId);
    return nullptr;
}

QList<SmdDiagnostic> SmdLspManager::diagnosticsForCell(int cellIndex) const
{
    return m_diagnostics.value(cellIndex);
}

// ─── Program group support ───────────────────────────────────────────

int SmdLspManager::computeCppGroup(int cellIndex) const
{
    static const QRegularExpression mainRe(QStringLiteral("\\bmain\\s*\\("));
    int group = 0;
    for (int ci : m_cppServer.cellOrder) {
        if (ci >= cellIndex)
            break;
        const QString &content = m_cppCellContents.value(ci);
        if (mainRe.match(content).hasMatch())
            ++group;
    }
    return group;
}

void SmdLspManager::focusCell(int cellIndex)
{
    if (m_focusing || m_shuttingDown)
        return;
    if (!m_cppCellContents.contains(cellIndex))
        return;

    int newGroup = computeCppGroup(cellIndex);
    if (newGroup == m_activeCppGroup)
        return;

    m_focusing = true;

    // Save current diagnostics to cache
    if (m_activeCppGroup >= 0 && !m_diagnostics.isEmpty())
        m_groupDiagnostics[m_activeCppGroup] = m_diagnostics;

    // Clear squiggles for old group
    for (auto it = m_diagnostics.begin(); it != m_diagnostics.end(); ++it)
        emit diagnosticsUpdated(it.key(), {});

    // Clear semantic tokens for old group cells
    for (auto it = m_cppServer.cellRanges.begin();
         it != m_cppServer.cellRanges.end(); ++it) {
        emit semanticTokensReadyForCell(it.key(), {});
    }

    m_diagnostics.clear();
    m_activeCppGroup = newGroup;

    // Restore cached diagnostics for new group
    if (m_groupDiagnostics.contains(newGroup)) {
        const auto &cached = m_groupDiagnostics[newGroup];
        for (auto it = cached.begin(); it != cached.end(); ++it) {
            m_diagnostics[it.key()] = it.value();
            emit diagnosticsUpdated(it.key(), it.value());
        }
    }

    rebuildVirtualDoc(QStringLiteral("cpp"));

    m_focusing = false;
}

// ─── Python process ──────────────────────────────────────────────────

void SmdLspManager::startPythonProcess()
{
    if (m_shuttingDown || m_pyProcess) return;

    QString appDir = QCoreApplication::applicationDirPath();
    QString scriptPath;
    QStringList candidates = {
        appDir + QStringLiteral("/completion_helper.py"),
        appDir + QStringLiteral("/lsp/completion_helper.py"),
        appDir + QStringLiteral("/../completion_helper.py"),
        appDir + QStringLiteral("/../lsp/completion_helper.py"),
        QStringLiteral("completion_helper.py"),
        QStringLiteral("lsp/completion_helper.py"),
    };
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c)) {
            scriptPath = QFileInfo(c).absoluteFilePath();
            break;
        }
    }
    if (scriptPath.isEmpty()) {
        qWarning() << "SmdLspManager: completion_helper.py not found";
        emit serverFailed(QStringLiteral("python"), tr("completion_helper.py not found"));
        return;
    }

    QString python;
    QString configuredPath = ConfigManager::instance().toolPythonPath();
    if (!configuredPath.isEmpty() && QFileInfo::exists(configuredPath)) {
        python = configuredPath;
    } else {
        python = QStandardPaths::findExecutable(QStringLiteral("python"));
        if (python.isEmpty()) python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    }
    if (python.isEmpty()) {
        qWarning() << "SmdLspManager: python not found";
        emit serverFailed(QStringLiteral("python"), tr("Python not found."));
        return;
    }

    m_pyProcess = new QProcess(this);
    m_pyProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_pyProcess, &QProcess::started, this, [this]() {
        m_pyServer.initialized = true;
        emit serverReady(QStringLiteral("python"));
        // Request initial diagnostics and semantic tokens if there are Python cells
        if (!m_pyServer.cellOrder.isEmpty()) {
            m_pyDiagnosticsTimer.start();
            m_pySemanticTokensTimer->start();
        }
    });
    connect(m_pyProcess, &QProcess::readyReadStandardOutput,
            this, &SmdLspManager::onPyReadyRead);
    connect(m_pyProcess, &QProcess::errorOccurred,
            this, &SmdLspManager::onPyProcessError);
    connect(m_pyProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SmdLspManager::onPyProcessFinished);

    m_pyProcess->start(python, {scriptPath});
    // startup is async — started / errorOccurred signals handle result
}

QString SmdLspManager::pythonVirtualDoc() const
{
    QStringList lines;
    for (int ci : m_pyServer.cellOrder) {
        lines.append(QStringLiteral("# --- smd:cell:%1 ---").arg(ci));
        QString content = StringUtils::sanitizeForPython(m_pyCellContents.value(ci));
        if (!content.isEmpty())
            lines.append(content.split(QLatin1Char('\n')));
    }
    return lines.join(QLatin1Char('\n'));
}

void SmdLspManager::sendPythonRequest(const QString &action, int cellIndex,
                                       int line, int col)
{
    if (!m_pyProcess || m_pyProcess->state() != QProcess::Running) {
        if (action == QStringLiteral("complete"))
            emit completionReadyForCell(cellIndex, {});
        else if (action == QStringLiteral("hover"))
            emit hoverReadyForCell(cellIndex, {});
        else
            emit signatureHelpReadyForCell(cellIndex, {}, 0);
        return;
    }

    int vl, vc;
    cellLocalToVirtual(cellIndex, line, col, QStringLiteral("python"), vl, vc);

    QString virtualDoc = pythonVirtualDoc();
    // Base64-encode to match completion_helper.py's expectation.
    QByteArray codeBase64 = virtualDoc.toUtf8().toBase64();

    QJsonObject req;
    req[QStringLiteral("action")] = action;
    req[QStringLiteral("code")] = QString::fromLatin1(codeBase64);
    QJsonArray cursor;
    cursor.append(vl);
    cursor.append(vc);
    req[QStringLiteral("cursor")] = cursor;

    QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
    m_pyProcess->write(payload);

    if (action == QStringLiteral("complete"))
        m_pyPending = PyPending::Completion;
    else if (action == QStringLiteral("hover"))
        m_pyPending = PyPending::Hover;
    else
        m_pyPending = PyPending::SignatureHelp;

    m_pyRequestingCell = cellIndex;
    m_pyTimeoutTimer.start(500);
}

void SmdLspManager::sendPythonHoverLocal(int cellIndex, int line, int col)
{
    if (!m_pyProcess || m_pyProcess->state() != QProcess::Running) {
        emit hoverReadyForCell(cellIndex, {});
        return;
    }

    // Send only this cell's code (base64) with cell-local coordinates,
    // bypassing the virtual-document mapping entirely.  This makes hover
    // immune to off-by-one errors in rebuildVirtualDoc / cellLocalToVirtual
    // and is consistent with how diagnostics uses cell-local coordinates.
    QString code = StringUtils::sanitizeForPython(m_pyCellContents.value(cellIndex));
    QByteArray codeB64 = code.toUtf8().toBase64();

    QJsonObject req;
    req[QStringLiteral("action")] = QStringLiteral("hover");
    req[QStringLiteral("code_b64")] = QString::fromUtf8(codeB64);
    QJsonArray cursor;
    cursor.append(line);
    cursor.append(col);
    req[QStringLiteral("cursor")] = cursor;

    QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
    m_pyProcess->write(payload);

    m_pyPending = PyPending::Hover;
    m_pyRequestingCell = cellIndex;
    m_pyTimeoutTimer.start(500);
}

void SmdLspManager::requestPythonDiagnostics()
{
    if (!m_pyProcess || m_pyProcess->state() != QProcess::Running) {
        return;
    }

    // Send each Python cell's code individually, so the Python side can
    // compile each one in isolation and return cell-local line numbers.
    QJsonArray cells;
    for (int ci : m_pyServer.cellOrder) {
        QJsonObject cell;
        cell[QStringLiteral("cellIndex")] = ci;
        QString code = StringUtils::sanitizeForPython(m_pyCellContents.value(ci));
        // Base64-encode to avoid JSON newline escaping issues
        cell[QStringLiteral("code")] = QString::fromLatin1(
            code.toUtf8().toBase64());
        cells.append(cell);
    }

    QJsonObject req;
    req[QStringLiteral("action")] = QStringLiteral("diagnostics");
    req[QStringLiteral("cells")] = cells;

    QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
    m_pyProcess->write(payload);

    m_pyPending = PyPending::Diagnostics;
    m_pyRequestingCell = -1;  // diagnostics cover all Python cells
    m_pyTimeoutTimer.start(500);
}

void SmdLspManager::requestPythonSemanticTokens()
{
    if (m_shuttingDown || !m_pyServer.initialized) {
        return;
    }
    if (!m_pyProcess || m_pyProcess->state() != QProcess::Running) {
        return;
    }
    if (m_pyCellContents.isEmpty()) {
        return;
    }

    // Concatenate all Python cells into a virtual document
    QString code = pythonVirtualDoc();
    // Skip large documents to avoid performance issues
    if (code.length() > 200 * 1024) {
        return;
    }


    QJsonObject req;
    req[QStringLiteral("action")] = QStringLiteral("tokens");
    QByteArray codeBase64 = code.toUtf8().toBase64();
    req[QStringLiteral("code")] = QString::fromLatin1(codeBase64);
    QJsonArray cursor;
    cursor.append(0);
    cursor.append(0);
    req[QStringLiteral("cursor")] = cursor;

    QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
    m_pyProcess->write(payload);

    // Fire-and-forget — the response may arrive after other requests
    // have been processed.  Detected by structure in processPythonResponse.
    m_pyTokensPending = true;
}

void SmdLspManager::onPyReadyRead()
{
    if (m_shuttingDown || !m_pyProcess) return;
    while (m_pyProcess->canReadLine()) {
        QByteArray line = m_pyProcess->readLine().trimmed();
        if (!line.isEmpty())
            processPythonResponse(line);
    }
}

void SmdLspManager::processPythonResponse(const QByteArray &line)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError) {
        emitPythonEmptyResults();
        return;
    }

    QJsonObject msg = doc.object();
    if (!msg.value(QStringLiteral("ok")).toBool()) {
        emitPythonEmptyResults();
        return;
    }

    QJsonValue dataVal = msg.value(QStringLiteral("data"));

    // Detect in-flight semantic tokens response by structure, so late
    // arrivals (past the timeout of another request) are still processed.
    // Process tokens BEFORE clearing m_pyPending so cross-talk between the
    // tokens and diagnostics timers doesn't drop diagnostics on file open.
    if (m_pyTokensPending) {
        QJsonArray arr = dataVal.toArray();
        if (!arr.isEmpty()) {
            QJsonObject first = arr[0].toObject();
            if (first.contains(QStringLiteral("line"))
                && first.contains(QStringLiteral("type"))) {
                QList<SemanticToken> allTokens;
                for (const QJsonValue &v : arr) {
                    QJsonObject obj = v.toObject();
                    SemanticToken t;
                    t.line = obj.value(QStringLiteral("line")).toInt();
                    t.startChar = obj.value(QStringLiteral("startChar")).toInt();
                    t.length = obj.value(QStringLiteral("length")).toInt();
                    t.type = obj.value(QStringLiteral("type")).toString();
                    allTokens.append(t);
                }

                QMap<int, QList<SemanticToken>> tokensByCell = mapVirtualTokensToCells(allTokens, m_pyServer);
                m_pyTokensPending = false;
                for (auto it = tokensByCell.begin(); it != tokensByCell.end(); ++it)
                    emit semanticTokensReadyForCell(it.key(), it.value());
                // Only clear the pending state if THIS response was expected
                if (m_pyPending == PyPending::SemanticTokens) {
                    m_pyPending = PyPending::None;
                    m_pyTimeoutTimer.stop();
                    m_pyRequestingCell = -1;
                }
                return;
            }
        }
    }

    m_pyTimeoutTimer.stop();
    PyPending pending = m_pyPending;
    m_pyPending = PyPending::None;
    int cellIndex = m_pyRequestingCell;
    m_pyRequestingCell = -1;

    switch (pending) {
    case PyPending::Completion: {
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
        emit completionReadyForCell(cellIndex, items);
        break;
    }
    case PyPending::Hover: {
        HoverInfo info;
        QJsonObject obj = dataVal.toObject();
        info.signature = obj.value(QStringLiteral("signature")).toString();
        info.doc = obj.value(QStringLiteral("doc")).toString();
        info.definition = obj.value(QStringLiteral("definition")).toString();
        emit hoverReadyForCell(cellIndex, info);
        break;
    }
    case PyPending::SignatureHelp: {
        QList<SignatureInfo> signatures;
        QJsonArray arr = dataVal.toArray();
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
        emit signatureHelpReadyForCell(cellIndex, signatures, 0);
        break;
    }
    case PyPending::Diagnostics: {
        // Python returns per-cell diagnostics with cell-local line numbers.
        // Emit directly instead of going through processDiagnostics which
        // was designed for virtual-document coordinate mapping.
        QJsonArray arr = dataVal.toArray();

        QMap<int, QList<SmdDiagnostic>> newDiags;
        for (const QJsonValue &v : arr) {
            QJsonObject f = v.toObject();
            SmdDiagnostic d;
            d.cellIndex = f.value(QStringLiteral("cellIndex")).toInt();
            d.startLine = f.value(QStringLiteral("startLine")).toInt();
            d.startCol = f.value(QStringLiteral("startCol")).toInt();
            d.endLine = f.value(QStringLiteral("endLine")).toInt();
            d.endCol = f.value(QStringLiteral("endCol")).toInt();
            d.message = f.value(QStringLiteral("message")).toString();
            d.severity = f.value(QStringLiteral("severity")).toInt();
            newDiags[d.cellIndex].append(d);
        }

        // Emit per-cell
        for (auto it = newDiags.begin(); it != newDiags.end(); ++it) {
            m_diagnostics[it.key()] = it.value();
            emit diagnosticsUpdated(it.key(), it.value());
        }

        // Clear stale Python diagnostics for cells that no longer have any
        const QList<int> diagKeys = m_diagnostics.keys();
        for (int ci : diagKeys) {
            if (!newDiags.contains(ci) && m_pyCellContents.contains(ci)) {
                m_diagnostics.remove(ci);
                emit diagnosticsUpdated(ci, {});
            }
        }
        break;
    }
    case PyPending::SemanticTokens: {
        QList<SemanticToken> allTokens;
        QJsonArray arr = dataVal.toArray();
        for (const QJsonValue &v : arr) {
            QJsonObject obj = v.toObject();
            SemanticToken t;
            t.line = obj.value(QStringLiteral("line")).toInt();
            t.startChar = obj.value(QStringLiteral("startChar")).toInt();
            t.length = obj.value(QStringLiteral("length")).toInt();
            t.type = obj.value(QStringLiteral("type")).toString();
            allTokens.append(t);
        }

        QMap<int, QList<SemanticToken>> tokensByCell = mapVirtualTokensToCells(allTokens, m_pyServer);
        for (auto it = tokensByCell.begin(); it != tokensByCell.end(); ++it)
            emit semanticTokensReadyForCell(it.key(), it.value());
        break;
    }
    default:
        break;
    }
}

void SmdLspManager::emitPythonEmptyResults()
{
    PyPending pending = m_pyPending;
    m_pyPending = PyPending::None;
    int cellIndex = m_pyRequestingCell;
    m_pyRequestingCell = -1;
    m_pyTimeoutTimer.stop();

    switch (pending) {
    case PyPending::Completion:
        emit completionReadyForCell(cellIndex, {});
        break;
    case PyPending::Hover:
        emit hoverReadyForCell(cellIndex, {});
        break;
    case PyPending::SignatureHelp:
        emit signatureHelpReadyForCell(cellIndex, {}, 0);
        break;
    case PyPending::Diagnostics:
        // On timeout/error, don't clear diagnostics — the Python helper
        // may just be busy. Only clear when we get an explicit empty result.
        break;
    case PyPending::SemanticTokens:
        m_pyTokensPending = false;
        for (int ci : m_pyServer.cellOrder)
            emit semanticTokensReadyForCell(ci, {});
        break;
    default:
        break;
    }
}

void SmdLspManager::onPyTimeout()
{
    qWarning() << "SmdLspManager: Python request timed out";
    emitPythonEmptyResults();
}

void SmdLspManager::onPyProcessError(QProcess::ProcessError err)
{
    if (m_shuttingDown || !m_pyProcess) return;
    m_pyTokensPending = false;
    emitPythonEmptyResults();
    if (err == QProcess::FailedToStart) {
        qWarning() << "SmdLspManager: Python process failed to start";
        m_pyServer.initialized = false;
        m_pyProcess->deleteLater();
        m_pyProcess = nullptr;
        emit serverFailed(QStringLiteral("python"), tr("Failed to start Python helper."));
    }
}

void SmdLspManager::onPyProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    if (m_shuttingDown || !m_pyProcess) return;
    m_pyTokensPending = false;
    emitPythonEmptyResults();
    m_pyServer.initialized = false;
    if (status == QProcess::CrashExit) {
        m_pyProcess->deleteLater();
        m_pyProcess = nullptr;
        QTimer::singleShot(1000, this, [this]() {
            if (!m_shuttingDown) startPythonProcess();
        });
    }
}
