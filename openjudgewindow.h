#ifndef OPENJUDGEWINDOW_H
#define OPENJUDGEWINDOW_H

#include <QMainWindow>
#include "crawler.h"

class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTextBrowser;
class SettingsManager;

struct SubmissionResult;

enum OjViewState { OJ_HOMEWORK_LIST, OJ_PROBLEM_LIST, OJ_PROBLEM_DETAIL };

class OpenJudgeWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit OpenJudgeWindow(SettingsManager *settings, QWidget *parent = nullptr);

    bool isLoggedIn() const { return m_isLoggedIn; }
    bool hasCurrentProblem() const { return !m_currentProblemUrl.isEmpty(); }
    QString currentProblemTitle() const { return m_currentProblem.title; }
    QString currentProblemUrl() const { return m_currentProblemUrl; }
    QString loggedInUsername() const { return m_username; }
    void submitCurrentProblem(const QString &sourceCode, int languageId);
    void onReLogin();
    bool tryAutoLogin();

signals:
    void sampleSelected(const QString &folderPath);
    void loginStateChanged(bool loggedIn, const QString &username);
    void submissionResultReady(const SubmissionResult &result);
    void submissionFailed(const QString &error);

private slots:
    void onLoginSuccess();
    void onLoginFailed(const QString &error);
    void onMainPageReady(const QList<HomeworkItem> &ongoing,
                         const QList<HomeworkItem> &past,
                         const PageInfo &pastPage);
    void onPastPageReady(const QList<HomeworkItem> &past,
                         const PageInfo &pageInfo);
    void onHomeworkProblemsReady(const QString &homeworkTitle,
                                 const QList<HomeworkItem> &problems);
    void onProblemDetailReady(const ProblemDetail &detail);
    void onNetworkError(const QString &error);
    void onItemClicked(QListWidgetItem *item);
    void onSectionClicked(QListWidgetItem *item);
    void onBack();
    void onRefresh();
    void onPrevPage();
    void onNextPage();
    void onSelectClicked();
    void onLoginLogoutClicked();
    void onLogoutClicked();

private:
    void setupUi();
    void setupDetailPage();
    void showListPage();
    void showDetailPage(const ProblemDetail &detail);
    void rebuildListView();
    void refreshStyle();
    void updateSelectButtonStyle(bool selected);

    struct SamplePair {
        QString input;
        QString output;
    };
    QVector<SamplePair> extractSamples(const ProblemDetail &detail);
    QStringList extractPreBlocks(const QString &html);
    QString writeSamplesToCache(const QVector<SamplePair> &samples);

    Crawler *m_crawler;
    QListWidget *m_listWidget;
    QLabel *m_statusLabel;
    QPushButton *m_refreshBtn;
    QPushButton *m_backBtn;
    QPushButton *m_selectBtn;
    QPushButton *m_loginBtn = nullptr;
    QLabel *m_userLabel = nullptr;
    bool m_isLoggedIn = false;
    QString m_username;

    QStackedWidget *m_stackedWidget;
    QWidget *m_detailPage;
    QListWidget *m_sectionList;
    QTextBrowser *m_sectionContent;

    QPushButton *m_prevPageBtn;
    QPushButton *m_nextPageBtn;
    QLabel *m_pageLabel;
    QWidget *m_paginationBar;

    OjViewState m_viewState = OJ_HOMEWORK_LIST;
    QList<HomeworkItem> m_ongoingItems;
    QList<HomeworkItem> m_pastItems;
    QList<HomeworkItem> m_problemItems;
    ProblemDetail m_currentProblem;
    PageInfo m_pastPageInfo;
    QString m_currentHomeworkTitle;
    QString m_currentHomeworkUrl;
    QString m_currentProblemUrl;
    bool m_currentHomeworkOngoing = false;
    bool m_currentProblemSelected = false;

    SettingsManager *m_settingsManager = nullptr;
    bool m_autoLoginInProgress = false;
    bool m_pendingAutoLogin = false;
    QString m_pendingPassword;
};

#endif // OPENJUDGEWINDOW_H
