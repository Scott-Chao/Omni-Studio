#ifndef SMDLSPMANAGER_H
#define SMDLSPMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QString>
#include <QTimer>
#include <QProcess>

#include "completionprovider.h"
#include "smddiagnostic.h"

class LspClient;
class QProcess;

class SmdLspManager : public QObject
{
    Q_OBJECT

public:
    explicit SmdLspManager(QObject *parent = nullptr);
    ~SmdLspManager() override;

    void initialize(const QString &smdFilePath);

    // Cell lifecycle
    void cellAdded(int cellIndex, const QString &langId, const QString &content);
    void cellRemoved(int cellIndex, const QString &langId);
    void cellContentChanged(int cellIndex, const QString &langId,
                            const QString &newContent);
    void cellTypeChanged(int cellIndex, const QString &oldLangId,
                         const QString &newLangId, const QString &content);

    // Code intelligence requests for a specific cell
    void requestCompletion(int cellIndex, int cursorLine, int cursorCol);
    void requestHover(int cellIndex, int cursorLine, int cursorCol);
    void requestSignatureHelp(int cellIndex, int cursorLine, int cursorCol);

    CompletionProvider *providerForCell(int cellIndex, const QString &langId);

    QList<SmdDiagnostic> diagnosticsForCell(int cellIndex) const;

    void focusCell(int cellIndex);

    void shutdown();

signals:
    void diagnosticsUpdated(int cellIndex, QList<SmdDiagnostic> diagnostics);
    void completionReadyForCell(int cellIndex, QList<CompletionItem> items);
    void hoverReadyForCell(int cellIndex, HoverInfo info);
    void signatureHelpReadyForCell(int cellIndex, QList<SignatureInfo> signatures,
                                   int activeIndex);
    void semanticTokensReadyForCell(int cellIndex, QList<SemanticToken> tokens);
    void serverReady(const QString &langId);
    void serverFailed(const QString &langId, const QString &reason);

private:
    struct CellRange {
        int firstVirtualLine = 0;
        int localLineCount = 0;
    };

    struct LanguageServer {
        LspClient *client = nullptr;
        bool initialized = false;
        int documentVersion = 0;
        QString virtualDocUri;
        QList<int> cellOrder;
        QMap<int, CellRange> cellRanges;

        // Request tracking
        int completionRequestId = -1;
        int hoverRequestId = -1;
        int signatureHelpRequestId = -1;
        int semanticTokensRequestId = -1;
        enum RequestType { None, Completion, Hover, SignatureHelp };
        RequestType pendingType = None;
        int requestingCellIndex = -1;

        // Semantic tokens
        QStringList tokenTypeLegend;
        QStringList tokenModifierLegend;

        void reset();
    };

    LanguageServer *serverForLang(const QString &langId);
    const LanguageServer *serverForLang(const QString &langId) const;
    QString langIdToLanguageId(const QString &langId) const;

    void startCppServer();
    void startPyServer();
    void sendInitialize(const QString &langId);

    // Group computation
    int computeCppGroup(int cellIndex) const;

    // Virtual document
    void rebuildVirtualDoc(const QString &langId);
    QString buildVirtualDocContent(const QString &langId) const;
    void syncVirtualDoc(const QString &langId);
    void cellLocalToVirtual(int cellIndex, int localLine, int localCol,
                            const QString &langId, int &outLine, int &outCol) const;

    // LSP request helpers
    void sendCompletionRequest(int cellIndex, int line, int col, const QString &langId);
    void sendHoverRequest(int cellIndex, int line, int col, const QString &langId);
    void sendSignatureHelpRequest(int cellIndex, int line, int col, const QString &langId);

    // Response handlers
    void onCppResponseReceived(int id, QJsonObject result);
    void onCppNotificationReceived(QString method, QJsonObject params);
    void onCppRequestFailed(int id, QJsonObject error);
    void onCppServerStopped(int exitCode, QProcess::ExitStatus status);
    void onPyReadyRead();
    void onPyProcessError(QProcess::ProcessError err);
    void onPyProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onPyTimeout();

    void processDiagnostics(const QString &langId, const QJsonObject &params);
    void requestCppSemanticTokens();
    QList<SemanticToken> parseSemanticTokens(const QJsonObject &result);

    // Python-specific
    void startPythonProcess();
    void sendPythonRequest(const QString &action, int cellIndex, int line, int col);
    void requestPythonDiagnostics();
    void requestPythonSemanticTokens();
    void processPythonResponse(const QByteArray &line);
    void emitPythonEmptyResults();
    QString pythonVirtualDoc() const;

    // Cell content cache
    QMap<int, QString> m_cppCellContents;
    QMap<int, QString> m_pyCellContents;

    LanguageServer m_cppServer;
    LanguageServer m_pyServer;

    QString m_smdFilePath;

    // Python process
    QProcess *m_pyProcess = nullptr;
    QTimer m_pyTimeoutTimer;
    QTimer m_pyDiagnosticsTimer;
    QTimer *m_pySemanticTokensTimer = nullptr;
    QTimer *m_cppSemanticTokensTimer = nullptr;
    enum class PyPending { None, Completion, Hover, SignatureHelp, Diagnostics, SemanticTokens };
    PyPending m_pyPending = PyPending::None;
    int m_pyRequestingCell = -1;
    bool m_pyTokensPending = false;

    // Per-cell diagnostics
    QMap<int, QList<SmdDiagnostic>> m_diagnostics;

    // Program group support
    int m_activeCppGroup = 0;
    QMap<int, QMap<int, QList<SmdDiagnostic>>> m_groupDiagnostics;

    // Cell adapter providers
    class CellCompletionAdapter;
    QMap<int, CellCompletionAdapter*> m_adapters;

    bool m_shuttingDown = false;
    bool m_focusing = false;
};

#endif // SMDLSPMANAGER_H
