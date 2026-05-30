#ifndef CODEBLOCKRUNNER_H
#define CODEBLOCKRUNNER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QList>
#include "smddiagnostic.h"

class TabManager;
class BottomPanel;
class CompileRunManager;
class ProcessRunner;

class CodeBlockRunner : public QObject
{
    Q_OBJECT
public:
    CodeBlockRunner(TabManager *tabManager, BottomPanel *bottomPanel,
                    CompileRunManager *compileRunMgr, QObject *parent = nullptr);
    ~CodeBlockRunner() override;

    void runCodeBlock(const QString &language, const QString &code, int blockIndex);
    bool loadDiagnosticsForCurrentTab();

private:
    void connectRunnerSignals();
    QString saveCodeBlockToTempFile(const QString &language, const QString &code);
    void parseAndShowBlockDiagnostics();

    TabManager *m_tabManager = nullptr;
    BottomPanel *m_bottomPanel = nullptr;
    CompileRunManager *m_compileRunMgr = nullptr;

    // Diagnostics cache
    QMap<QString, QMap<int, QList<SmdDiagnostic>>> m_mdDiagnostics;
    QMap<QString, int> m_lastRunBlockIndexMd;

    // Current execution state
    bool m_isRunningCodeBlock = false;
    bool m_processManuallyStopped = false;
    int m_currentBlockIndexMd = -1;
    QString m_currentMdFilePath;
    QString m_currentBlockLanguage;
    QString m_mdStderrBuffer;

    int m_codeBlockCounter = 0;
};

#endif // CODEBLOCKRUNNER_H
