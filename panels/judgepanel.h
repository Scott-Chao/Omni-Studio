#ifndef JUDGEPANEL_H
#define JUDGEPANEL_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QVector>

#include "judge/judgeengine.h"

class JudgePanel : public QWidget
{
    Q_OBJECT

public:
    explicit JudgePanel(QWidget *parent = nullptr);

    QString testFolder() const;
    void setTestFolder(const QString &path);
    void runJudge(const QString &sourceFile);

signals:
    void runAllRequested();
    void openJudgeRequested();
    void submitToOpenJudgeRequested();

public slots:
    void onTestStarted(int index, const QString &testName);
    void onTestFinished(int index, const JudgeEngine::TestResult &result);
    void onAllTestsFinished(int passed, int total);
    void onCompileFinished(bool success, const QString &errorOutput);
    void onJudgeOutput(const QString &text, bool isStderr);
    void onJudgeStopped();

private slots:
    void onBrowseClicked();
    void onRunAllClicked();
    void onStopClicked();
    void onTableItemClicked(int row, int column);

private:
    void setupUi();
    void clearResults();
    void updateSummaryLabel();
    void showDetailForRow(int row);
    void setInteractive(bool enabled);
    void refreshStyle();

    QLineEdit *m_folderEdit;
    QPushButton *m_browseBtn;
    QPushButton *m_openJudgeBtn;
    QPushButton *m_submitOJBtn;
    QTableWidget *m_table;
    QPlainTextEdit *m_detailEdit;
    QLabel *m_summaryLabel;
    QPushButton *m_runAllBtn;
    QPushButton *m_stopBtn;

    JudgeEngine *m_engine;
    QVector<JudgeEngine::TestResult> m_results;
};

#endif // JUDGEPANEL_H
