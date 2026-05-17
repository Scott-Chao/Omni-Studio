#include "smdlspmanager.h"
#include "lspclient.h"
#include "debuglog.h"

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
        int line = 0, col = 0;
        int len = qMin(cursorPos, text.length());
        for (int i = 0; i < len; ++i) {
            if (text.at(i) == QLatin1Char('\n')) { ++line; col = 0; }
            else { ++col; }
        }
        m_mgr->requestCompletion(m_cellIndex, line, col);
    }

    void requestHover(const QString &text, int cursorPos) override
    {
        int line = 0, col = 0;
        int len = qMin(cursorPos, text.length());
        for (int i = 0; i < len; ++i) {
            if (text.at(i) == QLatin1Char('\n')) { ++line; col = 0; }
            else { ++col; }
        }
        m_mgr->requestHover(m_cellIndex, line, col);
    }

    void requestSignatureHelp(const QString &text, int cursorPos) override
    {
        int line = 0, col = 0;
        int len = qMin(cursorPos, text.length());
        for (int i = 0; i < len; ++i) {
            if (text.at(i) == QLatin1Char('\n')) { ++line; col = 0; }
            else { ++col; }
        }
        m_mgr->requestSignatureHelp(m_cellIndex, line, col);
    }

private:
    SmdLspManager *m_mgr;
    int m_cellIndex;
};

// ─── Completion kind helper ──────────────────────────────────────────

static QString completionKindToString(int kind)
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
    case 22: return QStringLiteral("Struct");
    default: return QStringLiteral("Text");
    }
}

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
    pendingType = None;
    requestingCellIndex = -1;
}

// ─── Constructor / Destructor ────────────────────────────────────────

SmdLspManager::SmdLspManager(QObject *parent)
    : QObject(parent)
{
    m_pyTimeoutTimer.setSingleShot(true);
    connect(&m_pyTimeoutTimer, &QTimer::timeout, this, &SmdLspManager::onPyTimeout);

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
}

SmdLspManager::~SmdLspManager()
{
    shutdown();
}

void SmdLspManager::shutdown()
{
    debugLog(QStringLiteral("SmdLspManager::shutdown — begin"));
    m_shuttingDown = true;
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
    debugLog(QStringLiteral("SmdLspManager::shutdown — done"));
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
    debugLog(QStringLiteral("SmdLspManager::cellAdded — idx=%1, lang=%2, contentLen=%3")
        .arg(cellIndex).arg(langId).arg(content.length()));
    LanguageServer *srv = serverForLang(langId);
    if (!srv) {
        debugLog(QStringLiteral("SmdLspManager::cellAdded — no server for lang, skip"));
        return;
    }

    // Cache content
    if (langId == QStringLiteral("cpp"))
        m_cppCellContents[cellIndex] = content;
    else if (langId == QStringLiteral("python"))
        m_pyCellContents[cellIndex] = content;

    // Guard: skip if already in cellOrder (duplicate signal connection defense)
    if (srv->cellOrder.contains(cellIndex)) {
        debugLog(QStringLiteral("SmdLspManager::cellAdded — idx=%1 already in cellOrder, skip insert")
            .arg(cellIndex));
        rebuildVirtualDoc(langId);
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

    // Lazy-start server on first cell
    if (!srv->client && langId == QStringLiteral("cpp"))
        startCppServer();
    if (!m_pyProcess && langId == QStringLiteral("python"))
        startPythonProcess();

    rebuildVirtualDoc(langId);
}

void SmdLspManager::cellRemoved(int cellIndex, const QString &langId)
{
    debugLog(QStringLiteral("SmdLspManager::cellRemoved — idx=%1, lang=%2")
        .arg(cellIndex).arg(langId));
    LanguageServer *srv = serverForLang(langId);
    if (!srv) return;

    srv->cellOrder.removeAll(cellIndex);
    if (langId == QStringLiteral("cpp"))
        m_cppCellContents.remove(cellIndex);
    else if (langId == QStringLiteral("python"))
        m_pyCellContents.remove(cellIndex);

    if (m_adapters.contains(cellIndex)) {
        debugLog(QStringLiteral("SmdLspManager::cellRemoved — deleting adapter %1 for idx=%2")
            .arg(reinterpret_cast<quintptr>(m_adapters[cellIndex]), 0, 16).arg(cellIndex));
        delete m_adapters[cellIndex];
        m_adapters.remove(cellIndex);
        debugLog(QStringLiteral("SmdLspManager::cellRemoved — adapter deleted"));
    }

    m_diagnostics.remove(cellIndex);

    rebuildVirtualDoc(langId);
}

void SmdLspManager::cellContentChanged(int cellIndex, const QString &langId,
                                        const QString &newContent)
{
    if (langId == QStringLiteral("cpp"))
        m_cppCellContents[cellIndex] = newContent;
    else if (langId == QStringLiteral("python"))
        m_pyCellContents[cellIndex] = newContent;

    rebuildVirtualDoc(langId);
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
    }
    // Python is stateless — no sync needed
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
        debugLog(QStringLiteral("SmdLspManager::startCppServer — skip (shuttingDown=%1, client=%2)")
            .arg(m_shuttingDown).arg(m_cppServer.client != nullptr));
        return;
    }

    debugLog(QStringLiteral("SmdLspManager::startCppServer — starting clangd"));
    QString clangdPath = QStandardPaths::findExecutable(QStringLiteral("clangd"));
    if (clangdPath.isEmpty()) {
        qWarning() << "SmdLspManager: clangd not found";
        emit serverFailed(QStringLiteral("cpp"),
                          tr("clangd not found. Please install clangd for C++ code intelligence."));
        return;
    }

    m_cppServer.client = new LspClient(this);

    connect(m_cppServer.client, &LspClient::responseReceived,
            this, &SmdLspManager::onCppResponseReceived);
    connect(m_cppServer.client, &LspClient::notificationReceived,
            this, &SmdLspManager::onCppNotificationReceived);
    connect(m_cppServer.client, &LspClient::requestFailed,
            this, &SmdLspManager::onCppRequestFailed);
    connect(m_cppServer.client, &LspClient::serverStopped,
            this, &SmdLspManager::onCppServerStopped);

    QStringList args = { QStringLiteral("--fallback-style=Google") };

    if (!m_cppServer.client->start(clangdPath, args)) {
        qWarning() << "SmdLspManager: failed to start clangd";
        emit serverFailed(QStringLiteral("cpp"), tr("Failed to start clangd process."));
        delete m_cppServer.client;
        m_cppServer.client = nullptr;
        return;
    }

    sendInitialize(QStringLiteral("cpp"));
}

void SmdLspManager::sendInitialize(const QString &langId)
{
    LanguageServer *srv = serverForLang(langId);
    if (!srv || !srv->client) return;

    QJsonObject params;
    params[QStringLiteral("processId")] = QJsonValue::Null;
    params[QStringLiteral("rootUri")] = QJsonValue::Null;
    params[QStringLiteral("capabilities")] = QJsonObject();

    int id = srv->client->sendRequest(QStringLiteral("initialize"), params);
    Q_UNUSED(id);
}

void SmdLspManager::onCppResponseReceived(int id, QJsonObject result)
{
    debugLog(QStringLiteral("SmdLspManager::onCppResponseReceived — id=%1, shuttingDown=%2, client=%3")
        .arg(id).arg(m_shuttingDown).arg(m_cppServer.client != nullptr));
    if (m_shuttingDown || !m_cppServer.client) return;

    if (!m_cppServer.initialized) {
        // Assume initialize response
        debugLog(QStringLiteral("SmdLspManager — clangd initialized, sending didOpen"));
        qDebug() << "SmdLspManager: clangd initialized";
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

        for (const QJsonValue &val : arr) {
            QJsonObject item = val.toObject();
            CompletionItem ci;
            QString insertText;
            QJsonValue te = item.value(QStringLiteral("textEdit"));
            if (te.isObject())
                insertText = te.toObject().value(QStringLiteral("newText")).toString();
            if (insertText.isEmpty())
                insertText = item.value(QStringLiteral("insertText")).toString();
            if (insertText.isEmpty())
                insertText = item.value(QStringLiteral("label")).toString();
            ci.name = insertText.trimmed();
            ci.detail = item.value(QStringLiteral("detail")).toString();
            int kind = item.value(QStringLiteral("kind")).toInt(0);
            ci.type = completionKindToString(kind);
            ci.signature = ci.name;
            QJsonValue docVal = item.value(QStringLiteral("documentation"));
            if (docVal.isString())
                ci.doc = docVal.toString();
            else if (docVal.isObject())
                ci.doc = docVal.toObject().value(QStringLiteral("value")).toString();
            items.append(ci);
        }
        qDebug() << "SmdLspManager: completion returned" << items.size() << "items for cell" << cellIndex;
        emit completionReadyForCell(cellIndex, items);

    } else if (id == m_cppServer.hoverRequestId) {
        m_cppServer.hoverRequestId = -1;
        int cellIndex = m_cppServer.requestingCellIndex;
        m_cppServer.pendingType = LanguageServer::None;

        HoverInfo info;
        QJsonValue contentsVal = result.value(QStringLiteral("contents"));
        if (!contentsVal.isUndefined() && !contentsVal.isNull()) {
            if (contentsVal.isObject()) {
                QJsonObject c = contentsVal.toObject();
                if (c.contains(QStringLiteral("language")))
                    info.signature = c.value(QStringLiteral("value")).toString();
                else
                    info.doc = c.value(QStringLiteral("value")).toString();
            } else if (contentsVal.isString()) {
                info.doc = contentsVal.toString();
            } else if (contentsVal.isArray()) {
                QJsonArray arr = contentsVal.toArray();
                QStringList parts;
                for (const QJsonValue &v : arr) {
                    if (v.isObject()) {
                        QJsonObject o = v.toObject();
                        if (o.contains(QStringLiteral("language")))
                            info.signature = o.value(QStringLiteral("value")).toString();
                        else
                            parts.append(o.value(QStringLiteral("value")).toString());
                    } else if (v.isString()) {
                        parts.append(v.toString());
                    }
                }
                info.doc = parts.join(QStringLiteral("\n"));
            }
        }
        emit hoverReadyForCell(cellIndex, info);

    } else if (id == m_cppServer.signatureHelpRequestId) {
        m_cppServer.signatureHelpRequestId = -1;
        int cellIndex = m_cppServer.requestingCellIndex;
        m_cppServer.pendingType = LanguageServer::None;

        QList<SignatureInfo> signatures;
        QJsonValue sigsVal = result.value(QStringLiteral("signatures"));
        if (sigsVal.isArray()) {
            for (const QJsonValue &sv : sigsVal.toArray()) {
                QJsonObject sig = sv.toObject();
                SignatureInfo si;
                si.label = sig.value(QStringLiteral("label")).toString();
                si.doc = sig.value(QStringLiteral("documentation")).toString();
                QJsonValue paramsVal = sig.value(QStringLiteral("parameters"));
                if (paramsVal.isArray()) {
                    for (const QJsonValue &pv : paramsVal.toArray()) {
                        QJsonObject po = pv.toObject();
                        QJsonValue lv = po.value(QStringLiteral("label"));
                        if (lv.isString())
                            si.parameters.append(lv.toString());
                        else if (lv.isArray()) {
                            QJsonArray offsets = lv.toArray();
                            if (offsets.size() >= 2) {
                                int s = offsets.at(0).toInt();
                                int e = offsets.at(1).toInt();
                                if (s >= 0 && e <= si.label.length() && s < e)
                                    si.parameters.append(si.label.mid(s, e - s));
                            }
                        }
                    }
                }
                signatures.append(si);
            }
        }
        int activeSig = result.value(QStringLiteral("activeSignature")).toInt(0);
        int activeParam = result.value(QStringLiteral("activeParameter")).toInt(0);
        if (activeSig >= 0 && activeSig < signatures.size())
            signatures[activeSig].activeParameter = activeParam;
        emit signatureHelpReadyForCell(cellIndex, signatures, activeSig);
    }
}

void SmdLspManager::onCppNotificationReceived(QString method, QJsonObject params)
{
    debugLog(QStringLiteral("SmdLspManager::onCppNotificationReceived — method=%1").arg(method));
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
    }
}

void SmdLspManager::onCppServerStopped(int exitCode, QProcess::ExitStatus status)
{
    if (m_shuttingDown || !m_cppServer.client) return;
    qDebug() << "SmdLspManager: clangd stopped, exitCode" << exitCode;
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
    if (!srv) return;

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

    // Clear diagnostics for cells that no longer have any
    for (int ci : srv->cellOrder) {
        if (!newDiags.contains(ci) && m_diagnostics.contains(ci)) {
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
        sendPythonRequest(QStringLiteral("hover"), cellIndex, cursorLine, cursorCol);
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

// ─── Python process ──────────────────────────────────────────────────

void SmdLspManager::startPythonProcess()
{
    debugLog(QStringLiteral("SmdLspManager::startPythonProcess"));
    if (m_shuttingDown || m_pyProcess) return;

    QString appDir = QCoreApplication::applicationDirPath();
    QString scriptPath;
    QStringList candidates = {
        appDir + QStringLiteral("/completion_helper.py"),
        appDir + QStringLiteral("/../completion_helper.py"),
        QStringLiteral("completion_helper.py"),
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

    QString python = QStandardPaths::findExecutable(QStringLiteral("python"));
    if (python.isEmpty()) python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (python.isEmpty()) {
        qWarning() << "SmdLspManager: python not found";
        emit serverFailed(QStringLiteral("python"), tr("Python not found."));
        return;
    }

    m_pyProcess = new QProcess(this);
    m_pyProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_pyProcess, &QProcess::readyReadStandardOutput,
            this, &SmdLspManager::onPyReadyRead);
    connect(m_pyProcess, &QProcess::errorOccurred,
            this, &SmdLspManager::onPyProcessError);
    connect(m_pyProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SmdLspManager::onPyProcessFinished);

    m_pyProcess->start(python, {scriptPath});
    if (!m_pyProcess->waitForStarted(5000)) {
        qWarning() << "SmdLspManager: failed to start Python process";
        delete m_pyProcess;
        m_pyProcess = nullptr;
        emit serverFailed(QStringLiteral("python"), tr("Failed to start Python helper."));
        return;
    }
    m_pyServer.initialized = true;
    emit serverReady(QStringLiteral("python"));
}

QString SmdLspManager::pythonVirtualDoc() const
{
    QStringList lines;
    for (int ci : m_pyServer.cellOrder) {
        lines.append(QStringLiteral("# --- smd:cell:%1 ---").arg(ci));
        QString content = m_pyCellContents.value(ci);
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

    QJsonObject req;
    req[QStringLiteral("action")] = action;
    req[QStringLiteral("code")] = pythonVirtualDoc();
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

void SmdLspManager::onPyReadyRead()
{
    debugLog(QStringLiteral("SmdLspManager::onPyReadyRead"));
    if (m_shuttingDown || !m_pyProcess) return;
    while (m_pyProcess->canReadLine()) {
        QByteArray line = m_pyProcess->readLine().trimmed();
        if (!line.isEmpty())
            processPythonResponse(line);
    }
}

void SmdLspManager::processPythonResponse(const QByteArray &line)
{
    m_pyTimeoutTimer.stop();
    PyPending pending = m_pyPending;
    m_pyPending = PyPending::None;
    int cellIndex = m_pyRequestingCell;
    m_pyRequestingCell = -1;

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
    debugLog(QStringLiteral("SmdLspManager::onPyProcessError — err=%1").arg(err));
    Q_UNUSED(err);
    if (m_shuttingDown || !m_pyProcess) return;
    emitPythonEmptyResults();
}

void SmdLspManager::onPyProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    debugLog(QStringLiteral("SmdLspManager::onPyProcessFinished — exitCode=%1 status=%2")
        .arg(exitCode).arg(status));
    if (m_shuttingDown || !m_pyProcess) return;
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
