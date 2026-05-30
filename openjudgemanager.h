#ifndef OPENJUDGEMANAGER_H
#define OPENJUDGEMANAGER_H

#include <QObject>
#include <QPointer>

class TabManager;
class JudgePanel;
class QSplitter;
class SettingsManager;
class OpenJudgeWidget;
class SubmitResultPanel;
class JudgeEngine;
struct SubmissionResult;

class OpenJudgeManager : public QObject
{
    Q_OBJECT
public:
    OpenJudgeManager(TabManager *tabManager, JudgePanel *judgePanel,
                     QSplitter *rightSplitter, QObject *parent = nullptr);
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
    void onLoginStateChanged(bool loggedIn);
    void onSubmissionResultReady(const SubmissionResult &result);
    void runLocalTestsForOJError();
    void onOJErrorLocalTestsFinished(int passed, int total);

    QPointer<TabManager> m_tabManager;
    QPointer<JudgePanel> m_judgePanel;
    QPointer<QSplitter> m_rightSplitter;

    SubmitResultPanel *m_submitResultPanel = nullptr;
    JudgeEngine *m_ojErrorJudgeEngine = nullptr;
    QString m_lastSubmitSourceFile;
    QString m_lastSubmitSourceCode;
    QString m_ojErrorStatus;
};

#endif // OPENJUDGEMANAGER_H
