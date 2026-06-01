#ifndef LSPCLIENT_H
#define LSPCLIENT_H

#include <QObject>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QMap>

class LspClient : public QObject
{
    Q_OBJECT

public:
    explicit LspClient(QObject *parent = nullptr);
    ~LspClient() override;

    void start(const QString &serverPath, const QStringList &args = {});
    int sendRequest(const QString &method, const QJsonObject &params);
    void sendNotification(const QString &method, const QJsonObject &params);
    bool isRunning() const;
    void stop();

signals:
    void responseReceived(int id, QJsonObject result);
    void requestFailed(int id, QJsonObject error);
    void notificationReceived(QString method, QJsonObject params);
    void serverStarted();
    void serverError(QProcess::ProcessError err);
    void serverStopped(int exitCode, QProcess::ExitStatus status);

private slots:
    void onProcessStarted();
    void onReadyRead();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess *m_process = nullptr;
    QByteArray m_buffer;
    int m_nextId = 1;

    void parseFrames();
    void sendMessage(const QByteArray &json);
};

#endif // LSPCLIENT_H
