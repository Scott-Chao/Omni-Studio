#include "compilerunmanager.h"

#include "config/configmanager.h"
#include "editor/codeeditor.h"
#include "editor/editorwidget.h"
#include "editor/tabmanager.h"
#include "panels/bottompanel.h"
#include "panels/fileexplorerwidget.h"
#include "panels/openjudgewidget.h"
#include "panels/runterminalpanel.h"
#include "panels/terminalpanel.h"
#include "compilerutils.h"
#include "processrunner.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QPointer>
#include <QProcess>
#include <QSplitter>
#include <QTimer>
#include <QTextStream>

namespace {

QString psQuote(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'") + escaped + QStringLiteral("'");
}

QStringList shellArgs(QStringList args)
{
    QStringList result;
    for (const QString &arg : args) {
        if ((arg.startsWith(QLatin1Char('-')) || arg.startsWith(QLatin1Char('/')))
            && arg.contains(QLatin1Char(' '))) {
            result.append(QProcess::splitCommand(arg));
        } else {
            result.append(arg);
        }
    }
    return result;
}

QString psInvoke(const QString &program, const QStringList &args = {})
{
    QString command = QStringLiteral("& ") + psQuote(QDir::toNativeSeparators(program));
    for (const QString &arg : shellArgs(args))
        command += QLatin1Char(' ') + psQuote(QDir::toNativeSeparators(arg));
    return command;
}

QString psWriteError(const QString &message)
{
    return QStringLiteral("Write-Host ") + psQuote(message) + QStringLiteral(" -ForegroundColor Red");
}

QString cppReuseKey(const QString &sourceFile)
{
    return QStringLiteral("run:cpp:") + QFileInfo(sourceFile).absoluteFilePath();
}

} // namespace

CompileRunManager::CompileRunManager(const CompileRunDependencies &deps, QObject *parent)
    : QObject(parent)
    , m_tabManager(deps.tabManager)
    , m_bottomPanel(deps.bottomPanel)
    , m_explorer(deps.explorer)
    , m_rightSplitter(deps.rightSplitter)
{
    m_processRunner = new ProcessRunner(this);
    connectProcessRunner();
    setupActions();
}

CompileRunManager::~CompileRunManager() = default;

void CompileRunManager::setupActions()
{
    const auto &cfg = ConfigManager::instance();

    m_compileAction = new QAction(tr("Compile"), this);
    m_compileAction->setShortcut(QKeySequence(cfg.shortcut("compile_only", "F6")));

    m_runAction = new QAction(tr("Run"), this);
    m_runAction->setShortcut(QKeySequence(cfg.shortcut("run_only", "F7")));

    m_compileRunAction = new QAction(tr("Compile and Run"), this);
    m_compileRunAction->setShortcut(QKeySequence(cfg.shortcut("compile_and_run", "F5")));

    m_stopAction = new QAction(tr("Stop"), this);
    m_stopAction->setShortcut(QKeySequence(cfg.shortcut("stop_process", "Ctrl+Break")));
    m_stopAction->setEnabled(false);

    connect(m_compileAction, &QAction::triggered, this, &CompileRunManager::compile);
    connect(m_runAction, &QAction::triggered, this, &CompileRunManager::run);
    connect(m_compileRunAction, &QAction::triggered, this, &CompileRunManager::compileAndRun);
    connect(m_stopAction, &QAction::triggered, this, &CompileRunManager::stop);

    m_runMenu = new QMenu(tr("Run"));
    m_runMenu->addAction(m_compileAction);
    m_runMenu->addAction(m_runAction);
    m_runMenu->addSeparator();
    m_runMenu->addAction(m_compileRunAction);

    m_runToolAction = new QAction(QIcon(":/icons/run"), tr("Run"), this);
    m_runToolAction->setToolTip(tr("Compile and run"));
    m_runToolAction->setVisible(false);
    connect(m_runToolAction, &QAction::triggered, this, &CompileRunManager::compileAndRun);
}

void CompileRunManager::connectProcessRunner()
{
    RunTerminalPanel *terminal = m_bottomPanel ? m_bottomPanel->runTerminal() : nullptr;
    if (!terminal)
        return;

    connect(terminal, &RunTerminalPanel::sendRawInput, m_processRunner, &ProcessRunner::writeRaw);
    connect(terminal, &RunTerminalPanel::stopRequested, this, &CompileRunManager::stop);
    connect(m_processRunner, &ProcessRunner::outputReceived, terminal, &RunTerminalPanel::appendOutput);

    connect(m_processRunner, &ProcessRunner::compileFinished, this, [this](bool success) {
        if (m_bottomPanel && m_bottomPanel->runTerminal()) {
            m_bottomPanel->runTerminal()->setStatus(success ? tr("Compile success")
                                                            : tr("Compile failed"), !success);
        }
        emit compileFinished(success);
    });

    connect(m_processRunner, &ProcessRunner::runFinished, this, [this](int exitCode) {
        if (m_bottomPanel && m_bottomPanel->runTerminal()) {
            m_bottomPanel->runTerminal()->appendOutput(
                QStringLiteral("\n--- ") + tr("Process exited (code: %1)").arg(exitCode)
                + QStringLiteral(" ---\n"),
                false);
            m_bottomPanel->runTerminal()->setStatus(
                tr("Finished (code: %1)").arg(exitCode), exitCode != 0);
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

        if (m_bottomPanel && m_bottomPanel->runTerminal()) {
            RunTerminalPanel *terminal = m_bottomPanel->runTerminal();
            terminal->setRunning(true);
            terminal->setInputEnabled(false);
            if (m_processRunner->isAcceptingInput()) {
                QTimer::singleShot(50, this, [this, terminal]() {
                    if (m_running)
                        terminal->setInputEnabled(true);
                });
            }
        }

        emit processStarted(m_processRunner->isAcceptingInput());
    });

    connect(m_processRunner, &ProcessRunner::processStopped, this, [this]() {
        m_running = false;
        m_stopAction->setEnabled(false);

        if (m_bottomPanel && m_bottomPanel->runTerminal()) {
            m_bottomPanel->runTerminal()->setInputEnabled(false);
            m_bottomPanel->runTerminal()->setRunning(false);
            m_bottomPanel->runTerminal()->enableTextSelection(true);
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
    Q_UNUSED(filePath)
    Q_UNUSED(ext)
    return true;
}

CompileRunManager::ResolvedFile CompileRunManager::resolveIdeFilePath()
{
    if (!m_tabManager || !m_bottomPanel)
        return {};

    auto *oj = m_tabManager->findOpenJudgeWidget();
    if (oj && oj->isIdeMode()) {
        oj->saveIdeCodeToCache();
        QString filePath = oj->ideCacheFilePath();
        if (filePath.isEmpty())
            return {};
        return {filePath, QFileInfo(filePath).suffix().toLower(), true};
    }

    auto *editor = m_tabManager->currentEditor();
    if (!editor)
        return {};

    QString filePath = editor->currentFilePath();
    if (filePath.isEmpty() || editor->isModified()) {
        QString root = m_explorer ? m_explorer->rootPath() : QString();
        filePath = saveEditorToTempFile(editor, root);
        if (filePath.isEmpty())
            return {};
    }

    return {filePath, QFileInfo(filePath).suffix().toLower(), false};
}

void CompileRunManager::compile()
{
    auto rf = resolveIdeFilePath();
    if (!rf.isValid())
        return;

    if (!rf.isIde) {
        auto *editor = m_tabManager->currentEditor();
        if (!editor || !editor->isCodeEdit())
            return;
        if (!processCodeFile(rf.filePath, rf.ext))
            return;
    }

    if (!m_bottomPanel || !m_bottomPanel->terminalPanel())
        return;

    showOutputPanel();
    auto *terminal = m_bottomPanel->terminalPanel();
    const QString cwd = QFileInfo(rf.filePath).absolutePath();

    if (rf.ext == QStringLiteral("py") || rf.ext == QStringLiteral("pyw")) {
        terminal->openCommandTerminal(tr("Python"), psWriteError(tr("Python has no compile step.")),
                                      cwd, QStringLiteral("run:python"));
        return;
    }

    const CompilerInfo compiler = CompilerUtils::defaultCompiler();
    if (!compiler.available)
        return;

    const QStringList args = CompilerUtils::getCompileArgs(compiler.id, rf.filePath,
                                                           CompilerUtils::getOutputPath(rf.filePath));
    terminal->openCommandTerminal(QFileInfo(rf.filePath).completeBaseName(),
                                  psInvoke(compiler.compilerPath, args), cwd, cppReuseKey(rf.filePath));
}

void CompileRunManager::run()
{
    auto rf = resolveIdeFilePath();
    if (!rf.isValid())
        return;

    const bool isPython = (rf.ext == QStringLiteral("py") || rf.ext == QStringLiteral("pyw"));
    if (!m_bottomPanel || !m_bottomPanel->terminalPanel())
        return;

    showOutputPanel();
    auto *terminal = m_bottomPanel->terminalPanel();
    const QString cwd = QFileInfo(rf.filePath).absolutePath();

    if (isPython) {
        const CompilerInfo python = CompilerUtils::findPython();
        if (!python.available)
            return;
        terminal->openCommandTerminal(tr("Python"), psInvoke(python.compilerPath, {rf.filePath}),
                                      cwd, QStringLiteral("run:python"));
        return;
    }

    const QString executable = CompilerUtils::getOutputPath(rf.filePath);
    if (QFileInfo::exists(executable)) {
        terminal->openCommandTerminal(QFileInfo(rf.filePath).completeBaseName(),
                                      psInvoke(executable), cwd, cppReuseKey(rf.filePath));
    } else {
        compileAndRun();
    }
}

void CompileRunManager::compileAndRun()
{
    auto rf = resolveIdeFilePath();
    if (!rf.isValid())
        return;

    if (!rf.isIde) {
        auto *editor = m_tabManager->currentEditor();
        if (!editor || !editor->isCodeEdit())
            return;
    }

    if (!m_bottomPanel || !m_bottomPanel->terminalPanel())
        return;

    showOutputPanel();
    auto *terminal = m_bottomPanel->terminalPanel();
    const QString cwd = QFileInfo(rf.filePath).absolutePath();

    if (rf.ext == QStringLiteral("py") || rf.ext == QStringLiteral("pyw")) {
        const CompilerInfo python = CompilerUtils::findPython();
        if (!python.available)
            return;
        terminal->openCommandTerminal(tr("Python"), psInvoke(python.compilerPath, {rf.filePath}),
                                      cwd, QStringLiteral("run:python"));
        return;
    }

    const CompilerInfo compiler = CompilerUtils::defaultCompiler();
    if (!compiler.available)
        return;

    const QString outputFile = CompilerUtils::getOutputPath(rf.filePath);
    const QStringList args = CompilerUtils::getCompileArgs(compiler.id, rf.filePath, outputFile);
    const QString compileCmd = psInvoke(compiler.compilerPath, args);
    const QString runCmd = psInvoke(outputFile);
    const QString command = compileCmd + QStringLiteral("; if ($LASTEXITCODE -eq 0) { ") + runCmd + QStringLiteral(" }");
    terminal->openCommandTerminal(QFileInfo(rf.filePath).completeBaseName(),
                                  command, cwd, cppReuseKey(rf.filePath));
}

void CompileRunManager::stop()
{
    m_manualStop = true;
    m_processRunner->stop();
}

void CompileRunManager::toggleDiagnostics()
{
    if (!m_bottomPanel || !m_tabManager)
        return;

    auto *oj = m_tabManager->findOpenJudgeWidget();
    if (oj && oj->isIdeMode()) {
        if (!m_bottomPanel->isVisible()
            || m_bottomPanel->currentTab() != BottomPanel::DiagnosticsTab) {
            showOutputPanel();
            m_bottomPanel->showDiagnosticsTab();
            if (auto *ce = oj->ideCodeEditor())
                m_bottomPanel->setDiagnostics(ce->diagnostics());
        } else {
            m_bottomPanel->hide();
        }
        return;
    }

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

void CompileRunManager::showOutputPanel()
{
    if (!m_bottomPanel || !m_rightSplitter)
        return;

    const bool shouldResize = !m_bottomPanel->property("bottomPanelSizedOnce").toBool();
    m_bottomPanel->setVisible(true);
    m_bottomPanel->showTerminalTab();

    QPointer<QSplitter> splitter = m_rightSplitter;
    QPointer<BottomPanel> bottomPanel = m_bottomPanel;
    QTimer::singleShot(0, this, [splitter, bottomPanel, shouldResize]() {
        if (!shouldResize)
            return;
        if (!splitter)
            return;
        const int totalHeight = splitter->height();
        if (totalHeight > 0) {
            const int panelHeight = totalHeight / 3;
            splitter->setSizes({totalHeight - panelHeight, panelHeight});
            if (bottomPanel)
                bottomPanel->setProperty("bottomPanelSizedOnce", true);
        }
    });
}

void CompileRunManager::updateActions()
{
    if (!m_tabManager)
        return;

    auto *editor = m_tabManager->currentEditor();
    auto *oj = m_tabManager->findOpenJudgeWidget();
    const bool isCode = (editor && editor->isCodeEdit()) || (oj && oj->isIdeMode());
    const bool running = m_running;

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
