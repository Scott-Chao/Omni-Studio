#ifndef OPENJUDGEWIDGET_H
#define OPENJUDGEWIDGET_H

#include <QWidget>
#include "crawler.h"

class CodeEditor;
class QComboBox;
class QFrame;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTextBrowser;
class SettingsManager;

struct SubmissionResult;

enum OjViewState { OJ_HOMEWORK_LIST, OJ_PROBLEM_LIST, OJ_PROBLEM_DETAIL };

class OpenJudgeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OpenJudgeWidget(SettingsManager *settings, QWidget *parent = nullptr);

    bool isLoggedIn() const { return m_isLoggedIn; }
    bool hasCurrentProblem() const { return !m_currentProblemUrl.isEmpty(); }
    QString currentProblemTitle() const { return m_currentProblem.title; }
    QString currentProblemUrl() const { return m_currentProblemUrl; }
    QString loggedInUsername() const { return m_username; }
    void submitCurrentProblem(const QString &sourceCode, int languageId);
    void onReLogin();
    bool tryAutoLogin();

    // IDE mode
    bool isIdeMode() const { return m_ideMode; }
    CodeEditor *ideCodeEditor() const { return m_ideCodeEditor; }
    QString ideCode() const;
    int currentLanguageId() const { return m_currentLangId; }
    QString ideCacheFilePath() const;
    void saveIdeCodeToCache();

signals:
    void sampleSelected(const QString &folderPath);
    void loginStateChanged(bool loggedIn, const QString &username);
    void submissionResultReady(const SubmissionResult &result);
    void submissionFailed(const QString &error);
    void ideDiagnosticsToggleRequested();

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
    void onContentScrolled();
    void onBack();
    void onRefresh();
    void onPrevPage();
    void onNextPage();
    void onSelectClicked();
    void onToggleSidebar();
    void onToggleIdeMode();
    void onIdeLanguageChanged(int index);
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
    QString buildCombinedHtml(const ProblemDetail &detail) const;
    void recordSectionPositions();

    void setupIdeMode();
    void loadIdeCodeFromCache();
    QString ideCacheDir() const;
    QString ideLangCacheFilePath() const;
    void saveLastIdeLanguage(int langId = -1);
    int loadLastIdeLanguage() const;
    QString sanitizeFileName(const QString &name) const;

    struct OjLangOption {
        QString display;
        int ojId;
        QString codeLang;
        QString ext;
    };
    QVector<OjLangOption> ideLanguageOptions() const;

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
    QPushButton *m_toggleSidebarBtn = nullptr;
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

    QWidget *m_toolbar = nullptr;
    QFrame *m_separator = nullptr;
    int m_currentSectionIndex = -1;
    QVector<int> m_sectionYOffsets;
    bool m_scrollingFromClick = false;
    bool m_sidebarVisible = false;

    // IDE mode
    QPushButton *m_ideBtn = nullptr;
    QComboBox *m_langCombo = nullptr;
    QSplitter *m_ideSplitter = nullptr;
    QWidget *m_ideEditorContainer = nullptr;
    CodeEditor *m_ideCodeEditor = nullptr;
    bool m_ideMode = false;
    bool m_ideLangChanging = false;
    int m_currentLangId = 1;       // OJ language ID (default: G++)
    QString m_currentCodeLangId;   // CodeEditor language ID (e.g. "cpp")

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

#endif // OPENJUDGEWIDGET_H
