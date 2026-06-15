#include "codeblockrunner.h"
#include "editor/tabmanager.h"
#include "editor/editorwidget.h"
#include "panels/bottompanel.h"
#include "panels/runterminalpanel.h"
#include "compilerunmanager.h"
#include "processrunner.h"
#include "compilererrorparser.h"
#include "core/languageutils.h"
#include "config/configmanager.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QCoreApplication>

CodeBlockRunner::CodeBlockRunner(TabManager *tabManager, BottomPanel *bottomPanel,
                                 CompileRunManager *compileRunMgr, QObject *parent)
    : QObject(parent)
    , m_tabManager(tabManager)
    , m_bottomPanel(bottomPanel)
    , m_compileRunMgr(compileRunMgr)
{
    connectRunnerSignals();
}

CodeBlockRunner::~CodeBlockRunner() = default;

void CodeBlockRunner::connectRunnerSignals()
{
    if (!m_compileRunMgr)
        return;

    // Buffer stderr from ProcessRunner during code block execution
    ProcessRunner *pr = m_compileRunMgr->processRunner();
    if (pr) {
        connect(pr, &ProcessRunner::outputReceived, this,
                [this](const QString &text, bool isStderr) {
            if (m_isRunningCodeBlock && isStderr)
                m_mdStderrBuffer += text;
        });
    }

    // Forward compile/run finished to diagnostics parser
    connect(m_compileRunMgr, &CompileRunManager::compileFinished, this,
            [this](bool) {
        if (m_isRunningCodeBlock)
            parseAndShowBlockDiagnostics();
    });

    connect(m_compileRunMgr, &CompileRunManager::runFinished, this,
            [this](int) {
        if (m_isRunningCodeBlock)
            parseAndShowBlockDiagnostics();
    });

    // Clean up code block state when process stops
    connect(m_compileRunMgr, &CompileRunManager::processStopped, this,
            [this](bool manual) {
        if (m_isRunningCodeBlock) {
            m_isRunningCodeBlock = false;
            m_mdStderrBuffer.clear();
        }
        m_processManuallyStopped = manual;
    });
}

void CodeBlockRunner::runCodeBlock(const QString &language, const QString &code, int blockIndex)
{
    if (!m_tabManager || !m_bottomPanel)
        return;

    const QString normalizedLang = LanguageUtils::normalizeCodeFenceLanguage(language);

    EditorWidget *editor = m_tabManager->currentEditor();
    m_currentMdFilePath = editor ? editor->currentFilePath() : QString();
    m_currentBlockIndexMd = blockIndex;
    m_currentBlockLanguage = normalizedLang;
    m_isRunningCodeBlock = true;
    m_mdStderrBuffer.clear();

    // Clear previous diagnostics for this block
    if (!m_currentMdFilePath.isEmpty()) {
        m_mdDiagnostics[m_currentMdFilePath].remove(blockIndex);
        m_lastRunBlockIndexMd[m_currentMdFilePath] = blockIndex;
    }
    if (editor)
        editor->clearBlockDiagnostics();
    m_bottomPanel->clearDiagnostics();

    auto showPanel = [this]() {
        if (m_compileRunMgr) {
            m_compileRunMgr->showOutputPanel();
        } else {
            m_bottomPanel->setVisible(true);
            m_bottomPanel->showTerminalTab();
        }
    };

    if (normalizedLang.isEmpty()) {
        m_isRunningCodeBlock = false;
        showPanel();
        m_bottomPanel->runTerminal()->clearOutput();
        m_bottomPanel->runTerminal()->appendOutput(
            tr("涓嶆敮鎸佺殑璇█: %1\n褰撳墠鏀寔: python, cpp\n").arg(language), true);
        m_bottomPanel->runTerminal()->setStatus(tr("閿欒"), true);
        return;
    }

    const QString filePath = saveCodeBlockToTempFile(normalizedLang, code);
    if (filePath.isEmpty()) {
        m_isRunningCodeBlock = false;
        showPanel();
        m_bottomPanel->runTerminal()->clearOutput();
        m_bottomPanel->runTerminal()->appendOutput(
            tr("閿欒: 鏃犳硶鍒涘缓涓存椂鏂囦欢銆俓n"), true);
        m_bottomPanel->runTerminal()->setStatus(tr("閿欒"), true);
        return;
    }

    showPanel();
    m_bottomPanel->runTerminal()->clearOutput();

    ProcessRunner *pr = m_compileRunMgr ? m_compileRunMgr->processRunner() : nullptr;
    if (!pr) return;

    if (normalizedLang == QStringLiteral("python")) {
        m_bottomPanel->runTerminal()->appendOutput(
            QStringLiteral("--- ") + tr("杩愯 Python 浠ｇ爜鍧?---\n"), false);
        m_bottomPanel->runTerminal()->setStatus(tr("杩愯涓?.."));
        pr->startRunPython(filePath);
    } else if (normalizedLang == QStringLiteral("cpp")) {
        m_bottomPanel->runTerminal()->appendOutput(
            QStringLiteral("--- ") + tr("缂栬瘧杩愯 C++ 浠ｇ爜鍧?---\n"), false);
        m_bottomPanel->runTerminal()->setStatus(tr("缂栬瘧涓?.."));
        pr->startCompileAndRun(filePath);
    }
}

bool CodeBlockRunner::loadDiagnosticsForCurrentTab()
{
    if (!m_tabManager || !m_bottomPanel)
        return false;

    auto *editor = m_tabManager->currentEditor();
    if (!editor) return false;

    QString filePath = editor->currentFilePath();

    int lastBlock = m_lastRunBlockIndexMd.value(filePath, -1);
    if (lastBlock >= 0 && m_mdDiagnostics.contains(filePath)
        && m_mdDiagnostics[filePath].contains(lastBlock)) {
        QList<SmdDiagnostic> diags = m_mdDiagnostics[filePath][lastBlock];
        m_bottomPanel->setDiagnostics(diags);

        QMap<int, QList<SmdDiagnostic>> blockMap;
        blockMap[lastBlock] = diags;
        editor->applyBlockDiagnostics(blockMap);
        return true;
    }

    editor->clearBlockDiagnostics();
    return false;
}

QString CodeBlockRunner::saveCodeBlockToTempFile(const QString &language, const QString &code)
{
    QString ext;
    if (language == QStringLiteral("python")) {
        ext = QStringLiteral("py");
    } else if (language == QStringLiteral("cpp")) {
        ext = QStringLiteral("cpp");
    } else {
        return {};
    }

    const QString tempPath = QDir::tempPath()
        + QStringLiteral("/") + ConfigManager::instance().compilerCodeBlockPrefix()
        + QString::number(QCoreApplication::applicationPid())
        + QStringLiteral("_")
        + QString::number(m_codeBlockCounter++)
        + QStringLiteral(".")
        + ext;

    QFile file(tempPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};

    QTextStream out(&file);
    out << code;
    file.close();
    return tempPath;
}

void CodeBlockRunner::parseAndShowBlockDiagnostics()
{
    m_isRunningCodeBlock = false;

    if (m_processManuallyStopped) {
        m_processManuallyStopped = false;
        return;
    }

    auto *editor = m_tabManager->currentEditor();
    if (!editor || editor->currentFilePath() != m_currentMdFilePath)
        return;

    QList<SmdDiagnostic> diags;
    if (m_currentBlockLanguage == QStringLiteral("cpp")) {
        diags = CompilerErrorParser::parseCompileErrors(m_mdStderrBuffer, m_currentBlockIndexMd);
    } else if (m_currentBlockLanguage == QStringLiteral("python")) {
        diags = CompilerErrorParser::parsePythonTraceback(m_mdStderrBuffer, m_currentBlockIndexMd);
    }

    m_mdDiagnostics[m_currentMdFilePath][m_currentBlockIndexMd] = diags;
    m_bottomPanel->setDiagnostics(diags);

    QMap<int, QList<SmdDiagnostic>> blockMap;
    blockMap[m_currentBlockIndexMd] = diags;
    editor->applyBlockDiagnostics(blockMap);
}
