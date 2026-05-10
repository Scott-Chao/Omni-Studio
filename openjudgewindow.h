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

enum OjViewState { OJ_HOMEWORK_LIST, OJ_PROBLEM_LIST, OJ_PROBLEM_DETAIL };

class OpenJudgeWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit OpenJudgeWindow(QWidget *parent = nullptr);

signals:
    void sampleSelected(const QString &folderPath);

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
    void onReLogin();
    void onPrevPage();
    void onNextPage();
    void onSelectClicked();

private:
    void setupUi();
    void setupDetailPage();
    void showListPage();
    void showDetailPage(const ProblemDetail &detail);
    void rebuildListView();

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
};

#endif // OPENJUDGEWINDOW_H
