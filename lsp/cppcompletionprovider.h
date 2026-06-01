#ifndef CPPCOMPLETIONPROVIDER_H
#define CPPCOMPLETIONPROVIDER_H

#include "completionprovider.h"
#include <QString>
#include <QProcess>
#include <QJsonObject>
#include <QTimer>
#include <QStringList>

class LspClient;

class CppCompletionProvider : public CompletionProvider
{
    Q_OBJECT

public:
    explicit CppCompletionProvider(QObject *parent = nullptr);
    ~CppCompletionProvider() override;

    void requestCompletion(const QString &text, int cursorPos) override;
    void requestHover(const QString &text, int cursorPos) override;
    void requestSignatureHelp(const QString &text, int cursorPos) override;

    // Document sync (textDocument/didOpen + didChange)
    void openDocument(const QString &uri, const QString &languageId, const QString &text) override;
    void updateText(const QString &text) override;

    bool isServerReady() const { return m_initialized; }

    void shutdown();

private slots:
    void onResponseReceived(int id, QJsonObject result);
    void onNotificationReceived(QString method, QJsonObject params);
    void onRequestFailed(int id, QJsonObject error);
    void onServerError(QProcess::ProcessError err);
    void onServerStopped(int exitCode, QProcess::ExitStatus status);
    void onRequestTimeout();
    void requestSemanticTokens();

private:
    LspClient *m_client = nullptr;
    bool m_initialized = false;
    int m_initRequestId = -1;

    // Request tracking
    int m_completionRequestId = -1;
    int m_hoverRequestId = -1;
    int m_signatureHelpRequestId = -1;
    int m_semanticTokensRequestId = -1;

    // Semantic tokens debounce
    QTimer m_semanticTokensTimer;

    // Semantic tokens legend from initialize response
    QStringList m_tokenTypeLegend;
    QStringList m_tokenModifierLegend;

    // Document sync state
    QString m_documentUri;
    QString m_documentLanguageId;
    int m_documentVersion = 0;
    bool m_pendingOpen = false;
    bool m_documentOpen = false;  // true once didOpen has been sent after init
    QString m_pendingText;

    void startServer();
    void sendInitialize();
    void restartServer();
    void sendDidOpen(const QString &text);
    void sendDidChange(const QString &text);

    QList<CompletionItem> parseCompletionResponse(const QJsonObject &result);
    HoverInfo parseHoverResponse(const QJsonObject &result);
    SignatureInfo parseSignatureHelpItem(const QJsonObject &sig);
    QList<SmdDiagnostic> parseDiagnostics(const QJsonArray &diags);
    QList<SemanticToken> parseSemanticTokens(const QJsonObject &result);
};

#endif // CPPCOMPLETIONPROVIDER_H
