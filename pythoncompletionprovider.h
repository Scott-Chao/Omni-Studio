#ifndef PYTHONCOMPLETIONPROVIDER_H
#define PYTHONCOMPLETIONPROVIDER_H

#include "completionprovider.h"
#include <QProcess>
#include <QTimer>

class PythonCompletionProvider : public CompletionProvider
{
    Q_OBJECT

public:
    explicit PythonCompletionProvider(QObject *parent = nullptr);
    ~PythonCompletionProvider() override;

    void requestCompletion(const QString &text, int cursorPos) override;
    void requestHover(const QString &text, int cursorPos) override;
    void requestSignatureHelp(const QString &text, int cursorPos) override;

    void shutdown();

    // Jedi reads full code per request — no incremental sync needed
    void openDocument(const QString &uri, const QString &languageId, const QString &text) override;
    void updateText(const QString &text) override;

signals:
    void serverReady();
    void serverFailed(const QString &reason);

private slots:
    void onReadyRead();
    void onProcessError(QProcess::ProcessError err);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onTimeout();
    void onDiagnosticsDebounce();
    void requestSemanticTokens();

private:
    void startProcess();
    void restartProcess();
    void sendRequest(const QString &action, const QString &text, int cursorPos);
    void sendDiagnosticsRequest(const QString &text);
    void processResponse(const QByteArray &line);
    void emitEmptyResults();

    QProcess *m_process = nullptr;
    QTimer m_timeoutTimer;
    QTimer m_diagnosticsTimer;
    QTimer m_semanticTokensTimer;
    QString m_lastDiagnosticsText;
    bool m_jediAvailable = true;
    bool m_tokensPending = false;

    enum class PendingRequest { None, Completion, Hover, SignatureHelp, Diagnostics, SemanticTokens };
    PendingRequest m_pendingRequest = PendingRequest::None;
};

#endif // PYTHONCOMPLETIONPROVIDER_H
