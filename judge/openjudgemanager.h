#ifndef OPENJUDGEMANAGER_H
#define OPENJUDGEMANAGER_H

#include <QObject>

class TabManager;
class JudgePanel;
class SettingsManager;
class OpenJudgeWidget;
class BottomPanel;
class JudgeEngine;
struct SubmissionResult;

class OpenJudgeManager : public QObject
{
    Q_OBJECT
public:
    OpenJudgeManager(TabManager *tabManager, JudgePanel *judgePanel,
                     BottomPanel *bottomPanel, QObject *parent = nullptr);
    ~OpenJudgeManager() override;

    void open(SettingsManager *settings);
    void submit(const QString &rootPath);

signals:
    void submissionFailed(const QString &errorMessage);
    void ideModeChanged(bool ideMode);
    void diagnosticsToggleRequested();
    void showJudgePanelRequested();

private:
    void onSampleSelected(const QString &folderPath);
    void onLoginStateChanged(bool loggedIn, const QString &username);
    void onSubmissionResultReady(const SubmissionResult &result);
    void runLocalTestsForOJError();
    void onOJErrorLocalTestsFinished(int passed, int total);

    TabManager *m_tabManager = nullptr;
    JudgePanel *m_judgePanel = nullptr;
    BottomPanel *m_bottomPanel = nullptr;
    JudgeEngine *m_ojErrorJudgeEngine = nullptr;
    QString m_lastSubmitSourceFile;
    QString m_lastSubmitSourceCode;
    QString m_ojErrorStatus;
};

#endif // OPENJUDGEMANAGER_H
