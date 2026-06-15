#include "openjudgemanager.h"
#include "editor/tabmanager.h"
#include "panels/judgepanel.h"
#include "judgeengine.h"
#include "panels/openjudgewidget.h"
#include "panels/bottompanel.h"
#include "config/settingsmanager.h"
#include "config/configmanager.h"
#include "editor/editorwidget.h"
#include "runner/compilerunmanager.h"
#include "ai/errorjournal.h"

#include <QMessageBox>
#include <QTimer>
#include <QFileInfo>
#include <QDir>

OpenJudgeManager::OpenJudgeManager(TabManager *tabManager, JudgePanel *judgePanel,
                                   BottomPanel *bottomPanel, QObject *parent)
    : QObject(parent)
    , m_tabManager(tabManager)
    , m_judgePanel(judgePanel)
    , m_bottomPanel(bottomPanel)
{
}

OpenJudgeManager::~OpenJudgeManager() = default;

void OpenJudgeManager::open(SettingsManager *settings)
{
    if (!m_tabManager)
        return;

    OpenJudgeWidget *existing = m_tabManager->findOpenJudgeWidget();
    bool isNew = (existing == nullptr);

    m_tabManager->openOpenJudge(settings);

    OpenJudgeWidget *oj = m_tabManager->findOpenJudgeWidget();
    if (!oj) return;

    if (isNew) {
        connect(oj, &OpenJudgeWidget::sampleSelected,
                this, &OpenJudgeManager::onSampleSelected);
        connect(oj, &OpenJudgeWidget::loginStateChanged,
                this, &OpenJudgeManager::onLoginStateChanged);
        connect(oj, &OpenJudgeWidget::submissionResultReady,
                this, &OpenJudgeManager::onSubmissionResultReady);
        connect(oj, &OpenJudgeWidget::submissionFailed,
                this, [this](const QString &err) { emit submissionFailed(err); });
        connect(oj, &OpenJudgeWidget::ideDiagnosticsToggleRequested,
                this, [this]() { emit diagnosticsToggleRequested(); });
        connect(oj, &OpenJudgeWidget::ideModeChanged,
                this, [this](bool ideMode) { emit ideModeChanged(ideMode); });
    }

    // Show login dialog if not logged in
    if (isNew || !oj->isLoggedIn()) {
        QTimer::singleShot(200, this, [this]() {
            OpenJudgeWidget *w = m_tabManager ? m_tabManager->findOpenJudgeWidget() : nullptr;
            if (w && !w->isLoggedIn())
                w->onReLogin();
        });
    }
}

void OpenJudgeManager::submit(const QString &rootPath)
{
    if (!m_tabManager)
        return;

    OpenJudgeWidget *oj = m_tabManager->findOpenJudgeWidget();
    if (!oj || !oj->hasCurrentProblem()) {
        bool autoLoginInitiated = oj && oj->tryAutoLogin();
        if (!autoLoginInitiated) {
            // Can't open here without settings — caller must ensure tab is visible
            emit submissionFailed(tr("请先在 OpenJudge 中选择一道题目"));
        } else {
            emit submissionFailed(tr("正在自动登录 OpenJudge，请稍后重试"));
        }
        return;
    }

    if (!oj->isLoggedIn()) {
        bool autoLoginInitiated = oj->tryAutoLogin();
        if (!autoLoginInitiated)
            emit submissionFailed(tr("请先登录 OpenJudge"));
        else
            emit submissionFailed(tr("正在自动登录 OpenJudge，请稍后重试"));
        return;
    }

    QString code;
    QString filePath;
    int langId;

    // IDE mode: submit from embedded editor
    if (oj->isIdeMode()) {
        code = oj->ideCode();
        if (code.trimmed().isEmpty()) {
            emit submissionFailed(tr("代码内容为空"));
            return;
        }
        oj->saveIdeCodeToCache();
        filePath = oj->ideCacheFilePath();
        langId = oj->currentLanguageId();
    } else {
        // Normal mode: submit from current editor tab
        auto *editor = m_tabManager->currentEditor();
        if (!editor || !editor->isCodeEdit()) {
            emit submissionFailed(tr("请打开一个代码文件进行提交"));
            return;
        }

        code = editor->toPlainText();
        if (code.trimmed().isEmpty()) {
            emit submissionFailed(tr("代码内容为空"));
            return;
        }

        filePath = editor->currentFilePath();
        if (filePath.isEmpty() || editor->isModified()) {
            filePath = CompileRunManager::saveEditorToTempFile(editor, rootPath);
            if (filePath.isEmpty()) {
                emit submissionFailed(tr("无法保存代码文件"));
                return;
            }
        }

        QString ext = QFileInfo(filePath).suffix().toLower();
        QMap<QString, int> langMap = ConfigManager::instance().openJudgeSubmissionLanguageMap();
        langId = langMap.value("." + ext, 1);
    }

    // Save submission context for error journal
    m_lastSubmitSourceFile = filePath;
    m_lastSubmitSourceCode = code;

    oj->submitCurrentProblem(code, langId);
}

void OpenJudgeManager::onSampleSelected(const QString &folderPath)
{
    if (m_judgePanel)
        m_judgePanel->setTestFolder(folderPath);
    emit showJudgePanelRequested();
}

void OpenJudgeManager::onLoginStateChanged(bool /*loggedIn*/, const QString & /*username*/)
{
    // Can be used to update UI state when login state changes
}

void OpenJudgeManager::onSubmissionResultReady(const SubmissionResult &result)
{
    if (!m_bottomPanel || !m_tabManager)
        return;

    m_bottomPanel->showSubmissionResult(result);

    // Record non-Accepted, non-CE OpenJudge results to error journal
    bool isAccepted = (result.status == QStringLiteral("Accepted")
                       || result.status == QStringLiteral("AC"));
    bool isCE = (result.status == QStringLiteral("Compile Error"));

    if (!isAccepted && !isCE && !m_lastSubmitSourceFile.isEmpty()) {
        m_ojErrorStatus = result.status;
        runLocalTestsForOJError();
    }

}

void OpenJudgeManager::runLocalTestsForOJError()
{
    if (!m_judgePanel)
        return;

    QString testFolder = m_judgePanel->testFolder();
    if (testFolder.isEmpty()) {
        // No sample data — fall back to recording without I/O
        auto *oj = m_tabManager ? m_tabManager->findOpenJudgeWidget() : nullptr;
        QString problemName = oj ? oj->currentProblemTitle() : QString();
        QString problemUrl = oj ? oj->currentProblemUrl() : QString();
        SubmissionResult fallback;
        fallback.status = m_ojErrorStatus;
        ErrorJournal::instance().recordOpenJudgeFailure(
            fallback, m_lastSubmitSourceFile, problemName, problemUrl, m_lastSubmitSourceCode);
        return;
    }

    QDir testDir(testFolder);
    QStringList inFiles = testDir.entryList({QStringLiteral("*.in")}, QDir::Files);
    if (inFiles.isEmpty()) {
        auto *oj2 = m_tabManager ? m_tabManager->findOpenJudgeWidget() : nullptr;
        QString problemName = oj2 ? oj2->currentProblemTitle() : QString();
        QString problemUrl = oj2 ? oj2->currentProblemUrl() : QString();
        SubmissionResult fallback;
        fallback.status = m_ojErrorStatus;
        ErrorJournal::instance().recordOpenJudgeFailure(
            fallback, m_lastSubmitSourceFile, problemName, problemUrl, m_lastSubmitSourceCode);
        return;
    }

    if (m_ojErrorJudgeEngine) {
        if (m_ojErrorJudgeEngine->isRunning())
            m_ojErrorJudgeEngine->stop();
        m_ojErrorJudgeEngine->deleteLater();
        m_ojErrorJudgeEngine = nullptr;
    }

    m_ojErrorJudgeEngine = new JudgeEngine(this);
    m_ojErrorJudgeEngine->setSourceFile(m_lastSubmitSourceFile);
    m_ojErrorJudgeEngine->setTestFolder(testFolder);

    connect(m_ojErrorJudgeEngine, &JudgeEngine::allTestsFinished,
            this, &OpenJudgeManager::onOJErrorLocalTestsFinished);

    m_ojErrorJudgeEngine->start();
}

void OpenJudgeManager::onOJErrorLocalTestsFinished(int passed, int total)
{
    Q_UNUSED(passed);
    Q_UNUSED(total);

    if (!m_ojErrorJudgeEngine)
        return;

    auto *oj = m_tabManager ? m_tabManager->findOpenJudgeWidget() : nullptr;
    QString problemName = oj ? oj->currentProblemTitle() : QString();
    QString problemUrl = oj ? oj->currentProblemUrl() : QString();

    const auto &results = m_ojErrorJudgeEngine->results();
    bool anyRecorded = false;

    for (const auto &tr : results) {
        if (tr.statusCode != QStringLiteral("AC")) {
            ErrorJournal::instance().recordOpenJudgeFailure(
                tr, m_lastSubmitSourceFile, problemName, problemUrl);
            anyRecorded = true;
        }
    }

    if (!anyRecorded) {
        SubmissionResult fallback;
        fallback.status = m_ojErrorStatus;
        ErrorJournal::instance().recordOpenJudgeFailure(
            fallback, m_lastSubmitSourceFile, problemName, problemUrl, m_lastSubmitSourceCode);
    }

    m_ojErrorJudgeEngine->deleteLater();
    m_ojErrorJudgeEngine = nullptr;
}
