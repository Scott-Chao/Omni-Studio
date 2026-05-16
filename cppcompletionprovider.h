#ifndef CPPCOMPLETIONPROVIDER_H
#define CPPCOMPLETIONPROVIDER_H

#include "completionprovider.h"
#include <QString>
#include <QProcess>
#include <QJsonObject>

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

signals:
    void serverReady();
    void serverFailed(const QString &reason);

private slots:
    void onResponseReceived(int id, QJsonObject result);
    void onNotificationReceived(QString method, QJsonObject params);
    void onRequestFailed(int id, QJsonObject error);
    void onServerError(QProcess::ProcessError err);
    void onServerStopped(int exitCode, QProcess::ExitStatus status);

private:
    LspClient *m_client = nullptr;
    bool m_initialized = false;
    int m_initRequestId = -1;

    // Request tracking
    int m_completionRequestId = -1;
    int m_hoverRequestId = -1;
    int m_signatureHelpRequestId = -1;

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
    static QString completionKindToString(int kind);
};

#endif // CPPCOMPLETIONPROVIDER_H
