#include "compilerunmanager.h"
#include "processrunner.h"
#include "panels/bottompanel.h"
#include "panels/outputpanel.h"
#include "tabmanager.h"
#include "editorwidget.h"
#include "codeeditor.h"
#include "config/settingsmanager.h"
#include "config/configmanager.h"
#include "panels/fileexplorerwidget.h"
#include "panels/openjudgewidget.h"
#include "lsp/completionprovider.h"

#include <QAction>
#include <QMenu>
#include <QToolButton>
#include <QSplitter>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QDebug>
#include <QFile>
#include <QDateTime>
#include <QPointer>
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

CompileRunManager::CompileRunManager(TabManager *tabManager, BottomPanel *bottomPanel,
                                     SettingsManager *settings, FileExplorerWidget *explorer,
                                     QSplitter *rightSplitter, QObject *parent)
    : QObject(parent)
    , m_tabManager(tabManager)
    , m_bottomPanel(bottomPanel)
    , m_settings(settings)
    , m_explorer(explorer)
    , m_rightSplitter(rightSplitter)
{
    m_processRunner = new ProcessRunner(this);
    connectProcessRunner();
    setupActions();
}

CompileRunManager::~CompileRunManager() = default;

void CompileRunManager::setupActions()
{
    const auto &cfg = ConfigManager::instance();

    m_compileAction = new QAction(tr("编译"), this);
    m_compileAction->setShortcut(QKeySequence(cfg.shortcut("compile_only", "F6")));

    m_runAction = new QAction(tr("运行"), this);
    m_runAction->setShortcut(QKeySequence(cfg.shortcut("run_only", "F7")));

    m_compileRunAction = new QAction(tr("编译运行"), this);
    m_compileRunAction->setShortcut(QKeySequence(cfg.shortcut("compile_and_run", "F5")));

    m_stopAction = new QAction(tr("终止"), this);
    m_stopAction->setShortcut(QKeySequence(cfg.shortcut("stop_process", "Ctrl+Break")));
    m_stopAction->setEnabled(false);

    connect(m_compileAction, &QAction::triggered, this, &CompileRunManager::compile);
    connect(m_runAction, &QAction::triggered, this, &CompileRunManager::run);
    connect(m_compileRunAction, &QAction::triggered, this, &CompileRunManager::compileAndRun);
    connect(m_stopAction, &QAction::triggered, this, &CompileRunManager::stop);

    // Run menu (dropdown for toolbar button)
    m_runMenu = new QMenu(tr("运行"));
    m_runMenu->addAction(m_compileAction);
    m_runMenu->addAction(m_runAction);
    m_runMenu->addSeparator();
    m_runMenu->addAction(m_compileRunAction);

    m_runToolAction = new QAction(QIcon(":/icons/run"), tr("运行"), this);
    m_runToolAction->setToolTip(tr("编译运行"));
    m_runToolAction->setVisible(false);
    connect(m_runToolAction, &QAction::triggered, this, &CompileRunManager::compileAndRun);
}

void CompileRunManager::connectProcessRunner()
{
    OutputPanel *outputPanel = m_bottomPanel ? m_bottomPanel->outputPanel() : nullptr;
    if (!outputPanel)
        return;

    connect(outputPanel, &OutputPanel::sendInput, m_processRunner, &ProcessRunner::writeInput);
    connect(outputPanel, &OutputPanel::sendRawInput, m_processRunner, &ProcessRunner::writeRaw);
    connect(m_processRunner, &ProcessRunner::outputReceived, outputPanel, &OutputPanel::appendOutput);

    connect(m_processRunner, &ProcessRunner::compileFinished, this, [this](bool success) {
        if (m_bottomPanel) {
            if (success)
                m_bottomPanel->outputPanel()->setStatus(tr("编译成功"));
            else
                m_bottomPanel->outputPanel()->setStatus(tr("编译失败"), true);
        }
        emit compileFinished(success);
    });

    connect(m_processRunner, &ProcessRunner::runFinished, this, [this](int exitCode) {
        if (m_bottomPanel) {
            m_bottomPanel->outputPanel()->appendOutput(
                QStringLiteral("\n--- ") + tr("进程退出 (代码: %1)").arg(exitCode) + QStringLiteral(" ---\n"), false);
            m_bottomPanel->outputPanel()->setStatus(
                tr("完成 (代码: %1)").arg(exitCode), exitCode != 0);
        }
        emit runFinished(exitCode);
    });

    connect(m_processRunner, &ProcessRunner::processStarted, this, [this]() {
        m_manualStop = false;
        m_running = true;
        m_stopAction->setEnabled(true);
        m_compileAction->setEnabled(false);
        m_runAction->setEnabled(false);
        m_compileRunAction->setEnabled(false);
        if (m_runToolAction)
            m_runToolAction->setEnabled(false);

        if (m_bottomPanel) {
            OutputPanel *op = m_bottomPanel->outputPanel();
            if (m_processRunner->isAcceptingInput()) {
                QTimer::singleShot(50, this, [this, op]() {
                    if (m_running)
                        op->setRunning(true);
                });
            } else {
                op->enableTextSelection(false);
            }
        }
        emit processStarted(m_processRunner->isAcceptingInput());
    });

    connect(m_processRunner, &ProcessRunner::processStopped, this, [this]() {
        m_running = false;
        m_stopAction->setEnabled(false);

        if (m_bottomPanel) {
            OutputPanel *op = m_bottomPanel->outputPanel();
            op->setRunning(false);
            op->enableTextSelection(true);
        }

        if (m_tabManager) {
            if (auto *editor = m_tabManager->currentEditor())
                editor->setFocus();
        }

        updateActions();
        emit processStopped(m_manualStop);
    });
}

bool CompileRunManager::processCodeFile(const QString &filePath, const QString &ext)
{
    if (ext == QStringLiteral("py") || ext == QStringLiteral("pyw")) {
        showOutputPanel();
        m_bottomPanel->outputPanel()->clearOutput();
        m_bottomPanel->outputPanel()->appendOutput(
            tr("Python 不需要编译，请使用 运行 (F7) 或 编译运行 (F5)。\n"), false);
        m_bottomPanel->outputPanel()->setStatus(tr("提示"), false);
        return false;
    }
    return true;
}

void CompileRunManager::compile()
{
    if (!m_tabManager || !m_bottomPanel)
        return;

    // IDE mode
    auto *oj = m_tabManager->findOpenJudgeWidget();
    if (oj && oj->isIdeMode()) {
        oj->saveIdeCodeToCache();
        QString filePath = oj->ideCacheFilePath();
        if (filePath.isEmpty())
            return;
        QString ext = QFileInfo(filePath).suffix().toLower();
        if (!processCodeFile(filePath, ext))
            return;
        showOutputPanel();
        m_bottomPanel->outputPanel()->clearOutput();
        m_bottomPanel->outputPanel()->setStatus(tr("编译中..."));
        m_processRunner->startCompile(filePath);
        return;
    }

    // Normal mode
    auto *editor = m_tabManager->currentEditor();
    if (!editor || !editor->isCodeEdit())
        return;

    QString filePath = editor->currentFilePath();
    if (filePath.isEmpty() || editor->isModified()) {
        QString root = m_explorer ? m_explorer->rootPath() : QString();
        filePath = saveEditorToTempFile(editor, root);
        if (filePath.isEmpty())
            return;
    }

    QString ext = QFileInfo(filePath).suffix().toLower();
    if (!processCodeFile(filePath, ext))
        return;

    showOutputPanel();
    m_bottomPanel->outputPanel()->clearOutput();
    m_bottomPanel->outputPanel()->setStatus(tr("编译中..."));
    m_processRunner->startCompile(filePath);
}

void CompileRunManager::run()
{
    if (!m_tabManager || !m_bottomPanel)
        return;

    // IDE mode
    auto *oj = m_tabManager->findOpenJudgeWidget();
    if (oj && oj->isIdeMode()) {
        oj->saveIdeCodeToCache();
        QString filePath = oj->ideCacheFilePath();
        if (filePath.isEmpty())
            return;
        QString ext = QFileInfo(filePath).suffix().toLower();
        if (ext == QStringLiteral("py") || ext == QStringLiteral("pyw")) {
            showOutputPanel();
            m_bottomPanel->outputPanel()->clearOutput();
            m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
            m_processRunner->startRunPython(filePath);
            return;
        }
        if (m_processRunner->lastExecutable().isEmpty()) {
            compileAndRun();
            return;
        }
        showOutputPanel();
        m_bottomPanel->outputPanel()->clearOutput();
        m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
        m_processRunner->startRun(m_processRunner->lastExecutable());
        return;
    }

    // Normal mode
    auto *editor = m_tabManager->currentEditor();
    if (!editor)
        return;

    QString filePath = editor->currentFilePath();
    if (!filePath.isEmpty()) {
        QString ext = QFileInfo(filePath).suffix().toLower();
        if (ext == QStringLiteral("py") || ext == QStringLiteral("pyw")) {
            if (filePath.isEmpty() || editor->isModified()) {
                QString root = m_explorer ? m_explorer->rootPath() : QString();
                filePath = saveEditorToTempFile(editor, root);
                if (filePath.isEmpty())
                    return;
            }
            showOutputPanel();
            m_bottomPanel->outputPanel()->clearOutput();
            m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
            m_processRunner->startRunPython(filePath);
            return;
        }
    }

    if (m_processRunner->lastExecutable().isEmpty()) {
        compileAndRun();
        return;
    }

    showOutputPanel();
    m_bottomPanel->outputPanel()->clearOutput();
    m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
    m_processRunner->startRun(m_processRunner->lastExecutable());
}

void CompileRunManager::compileAndRun()
{
    if (!m_tabManager || !m_bottomPanel) {
        return;
    }

    // IDE mode
    auto *oj = m_tabManager->findOpenJudgeWidget();
    if (oj && oj->isIdeMode()) {
        oj->saveIdeCodeToCache();
        QString filePath = oj->ideCacheFilePath();
        if (filePath.isEmpty())
            return;
        QString ext = QFileInfo(filePath).suffix().toLower();
        if (ext == QStringLiteral("py") || ext == QStringLiteral("pyw")) {
            showOutputPanel();
            m_bottomPanel->outputPanel()->clearOutput();
            m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
            m_processRunner->startRunPython(filePath);
            return;
        }
        showOutputPanel();
        m_bottomPanel->outputPanel()->clearOutput();
        m_bottomPanel->outputPanel()->setStatus(tr("编译中..."));
        m_processRunner->startCompileAndRun(filePath);
        return;
    }

    // Normal mode
    auto *editor = m_tabManager->currentEditor();
    if (!editor || !editor->isCodeEdit()) {
        return;
    }

    QString filePath = editor->currentFilePath();
    if (filePath.isEmpty() || editor->isModified()) {
        QString root = m_explorer ? m_explorer->rootPath() : QString();
        filePath = saveEditorToTempFile(editor, root);
        if (filePath.isEmpty())
            return;
    }

    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == QStringLiteral("py") || ext == QStringLiteral("pyw")) {
        showOutputPanel();
        m_bottomPanel->outputPanel()->clearOutput();
        m_bottomPanel->outputPanel()->setStatus(tr("运行中..."));
        m_processRunner->startRunPython(filePath);
        return;
    }

    showOutputPanel();
    m_bottomPanel->outputPanel()->clearOutput();
    m_bottomPanel->outputPanel()->setStatus(tr("编译中..."));
    m_processRunner->startCompileAndRun(filePath);
}

void CompileRunManager::stop()
{
    m_manualStop = true;
    m_processRunner->stop();
    if (m_bottomPanel) {
        m_bottomPanel->outputPanel()->appendOutput(
            QStringLiteral("\n--- ") + tr("已终止") + QStringLiteral(" ---\n"), false);
        m_bottomPanel->outputPanel()->setStatus(tr("已终止"), true);
    }
}

void CompileRunManager::toggleDiagnostics()
{
    if (!m_bottomPanel || !m_tabManager)
        return;

    // IDE mode
    auto *oj = m_tabManager->findOpenJudgeWidget();
    if (oj && oj->isIdeMode()) {
        if (!m_bottomPanel->isVisible()
            || m_bottomPanel->currentTab() != BottomPanel::DiagnosticsTab) {
            showOutputPanel();
            m_bottomPanel->showDiagnosticsTab();
        } else {
            m_bottomPanel->hide();
        }
        return;
    }

    // Normal mode
    auto *editor = m_tabManager->currentEditor();
    if (!editor || !editor->isCodeEdit())
        return;

    if (!m_bottomPanel->isVisible()
        || m_bottomPanel->currentTab() != BottomPanel::DiagnosticsTab) {
        showOutputPanel();
        m_bottomPanel->showDiagnosticsTab();
    } else {
        m_bottomPanel->hide();
    }
}

static void debugLog(const QString &msg)
{
    QFile f(QCoreApplication::applicationDirPath() + "/debug.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write((QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + " " + msg + "\n").toUtf8());
        f.close();
    }
}

void CompileRunManager::showOutputPanel()
{
    debugLog("showOutputPanel: enter");

    if (!m_bottomPanel || !m_rightSplitter) {
        debugLog("showOutputPanel: null check failed, return");
        return;
    }

    debugLog(QString("showOutputPanel: splitter height=%1, sizes=[%2,%3]")
        .arg(m_rightSplitter->height())
        .arg(m_rightSplitter->sizes().value(0))
        .arg(m_rightSplitter->sizes().value(1)));

    m_bottomPanel->setVisible(true);
    m_bottomPanel->showRunTab();

    debugLog("showOutputPanel: panel shown, scheduling size adjustment");

    QPointer<QSplitter> splitter = m_rightSplitter;
    QTimer::singleShot(0, this, [splitter]() {
        if (!splitter) return;
        const int totalHeight = splitter->height();
        debugLog(QString("showOutputPanel timer: splitter height=%1").arg(totalHeight));
        if (totalHeight > 0) {
            const int panelHeight = totalHeight / 3;
            splitter->setSizes({totalHeight - panelHeight, panelHeight});
            debugLog(QString("showOutputPanel timer: setSizes [%1, %2]")
                .arg(totalHeight - panelHeight).arg(panelHeight));
        }
    });

    debugLog("showOutputPanel: exit");
}

void CompileRunManager::updateActions()
{
    if (!m_tabManager)
        return;

    auto *editor = m_tabManager->currentEditor();
    auto *oj = m_tabManager->findOpenJudgeWidget();
    bool isCode = (editor && editor->isCodeEdit()) || (oj && oj->isIdeMode());
    bool running = m_running;

    m_compileAction->setEnabled(isCode && !running);
    m_runAction->setEnabled(isCode && !running);
    m_compileRunAction->setEnabled(isCode && !running);
    if (m_runToolAction) {
        m_runToolAction->setVisible(isCode);
        m_runToolAction->setEnabled(isCode && !running);
    }
}

QString CompileRunManager::saveEditorToTempFile(EditorWidget *editor, const QString &rootPath)
{
    if (!editor)
        return {};

    QString rp = rootPath;
    if (rp.isEmpty())
        rp = QDir::tempPath();

    QString content = editor->toPlainText();
    QString filePath = editor->currentFilePath();

    if (filePath.isEmpty()) {
        const auto &cfg = ConfigManager::instance();
        filePath = rp + QStringLiteral("/") + cfg.compilerTempFilePrefix()
                   + QString::number(QCoreApplication::applicationPid())
                   + cfg.compilerTempFileSuffix();
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    QTextStream out(&file);
    out << content;
    file.close();

    editor->setFilePath(filePath);
    editor->setModified(false);
    return filePath;
}
