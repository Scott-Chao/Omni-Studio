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

    void startServer();
    void sendInitialize();
    void restartServer();
};

#endif // CPPCOMPLETIONPROVIDER_H
