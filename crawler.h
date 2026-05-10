#ifndef CRAWLER_H
#define CRAWLER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QList>
#include <QString>
#include <QUrl>

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
};

struct PageInfo {
    QString url;           // current page URL
    int currentPage = 1;
    bool hasPrev = false;
    bool hasNext = false;
};

class Crawler : public QObject
{
    Q_OBJECT
public:
    explicit Crawler(QObject *parent = nullptr);

    void login(const QString &username, const QString &password);
    void fetchMainPage();
    void fetchHomeworkProblems(const QString &url);
    void fetchProblemDetail(const QString &url);
    void fetchPastPage(const QString &url);

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

private:
    QNetworkAccessManager *m_manager;
    QString m_baseUrl;
    QString m_username;
    QString m_password;

    void onLoginPageFinished(QNetworkReply *reply);
    void onLoginFinished(QNetworkReply *reply);
    void onMainPageFinished(QNetworkReply *reply);

    QString extractCsrfToken(const QString &html);
    void parseMainPage(const QString &html,
                       QList<HomeworkItem> &ongoing,
                       QList<HomeworkItem> &past,
                       PageInfo &pastPage);
    ProblemDetail parseProblemDetail(const QString &html);
};

#endif // CRAWLER_H
