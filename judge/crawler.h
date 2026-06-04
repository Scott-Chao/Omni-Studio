#ifndef CRAWLER_H
#define CRAWLER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QList>
#include <QString>
#include <QUrl>
#include <QTimer>

struct HomeworkItem {
    QString title;
    QString url;
    QString deadline; // DDL for ongoing homework (displayed on the right)
};

struct ProblemSection {
    QString heading;     // e.g., "描述", "输入", "输出", "样例输入", "样例输出"
    QString contentHtml; // raw HTML content (preserves <pre> etc. for multi-block display)
};

struct ProblemDetail {
    QString title;
    QList<ProblemSection> sections;
    QList<int> availableLanguages; // OJ language IDs allowed for this problem
};

struct PageInfo {
    QString url;           // current page URL
    int currentPage = 1;
    bool hasPrev = false;
    bool hasNext = false;
};

struct SubmissionResult {
    QString runId;
    QString status;       // e.g. "Accepted", "Wrong Answer", "Compile Error", etc.
    int timeMs = -1;
    int memoryKb = 0;
    QString compileError; // CE detail text
};

class Crawler : public QObject
{
    Q_OBJECT
public:
    explicit Crawler(QObject *parent = nullptr);

    void setBaseUrl(const QString &url) { m_baseUrl = url; }
    void login(const QString &username, const QString &password);
    void fetchMainPage();
    void fetchHomeworkProblems(const QString &url);
    void fetchProblemDetail(const QString &url);
    void fetchPastPage(const QString &url);

    // Submission API
    void submitCode(const QString &problemUrl, const QString &sourceCode, int languageId);
    void fetchSubmissionStatus(const QString &statusPageUrl);
    void fetchCompileError(const QString &ceUrl);
    void stopPolling();
    void clearCookies();

    static QString decodeHtmlEntities(const QString &html);
    static QString stripHtmlTags(const QString &html);

signals:
    void loginSuccess();
    void loginFailed(const QString &error);
    void homeworkListReady(const QList<HomeworkItem> &items);
    void mainPageReady(const QList<HomeworkItem> &ongoing,
                       const QList<HomeworkItem> &past,
                       const PageInfo &pastPage);
    void pastPageReady(const QList<HomeworkItem> &past,
                       const PageInfo &pageInfo);
    void homeworkProblemsReady(const QString &homeworkTitle, const QList<HomeworkItem> &problems);
    void problemDetailReady(const ProblemDetail &detail);
    void networkError(const QString &error);

    // Submission signals
    void submissionResultReady(const SubmissionResult &result);
    void submissionFailed(const QString &error);
    void submitPollTimeout();

private:
    QNetworkAccessManager *m_manager;
    QString m_baseUrl;
    QString m_username;
    QString m_password;

    void onLoginPageFinished(QNetworkReply *reply);
    void onLoginFinished(QNetworkReply *reply);
    void onMainPageFinished(QNetworkReply *reply);

    // Submission internals
    void onSubmitPageFinished(QNetworkReply *reply, const QString &problemUrl,
                              const QString &sourceCode, int languageId);
    void doPollSubmissionStatus();
    void onCompileErrorPageFinished(QNetworkReply *reply, SubmissionResult &result);

    QString extractCsrfToken(const QString &html);
    void parseMainPage(const QString &html,
                       QList<HomeworkItem> &ongoing,
                       QList<HomeworkItem> &past,
                       PageInfo &pastPage);
    ProblemDetail parseProblemDetail(const QString &html);

    QTimer *m_pollTimer = nullptr;
    int m_pollCount = 0;
    QString m_pollStatusUrl;
    QString m_pendingRunId;
    QString m_pendingCeUrl;
    bool m_isLoginFlow = false;
};

#endif // CRAWLER_H
