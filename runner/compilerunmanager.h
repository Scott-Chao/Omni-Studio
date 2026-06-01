#ifndef COMPILERUNMANAGER_H
#define COMPILERUNMANAGER_H

#include <QObject>
#include <QAction>

class ProcessRunner;
class BottomPanel;
class TabManager;
class FileExplorerWidget;
class EditorWidget;
class QSplitter;
class QMenu;

// Aggregate dependency object — reduces constructor parameter count
struct CompileRunDependencies {
    TabManager *tabManager = nullptr;
    BottomPanel *bottomPanel = nullptr;
    FileExplorerWidget *explorer = nullptr;
    QSplitter *rightSplitter = nullptr;
};

class CompileRunManager : public QObject
{
    Q_OBJECT
public:
    CompileRunManager(const CompileRunDependencies &deps, QObject *parent = nullptr);
    ~CompileRunManager() override;

    // Actions for toolbar/menu setup
    QAction *compileAction() const { return m_compileAction; }
    QAction *runAction() const { return m_runAction; }
    QAction *compileRunAction() const { return m_compileRunAction; }
    QAction *stopAction() const { return m_stopAction; }
    QAction *runToolAction() const { return m_runToolAction; }
    QMenu *runMenu() const { return m_runMenu; }

    ProcessRunner *processRunner() const { return m_processRunner; }
    bool isRunning() const { return m_running; }
    bool isManualStop() const { return m_manualStop; }
    void updateActions();
    void showOutputPanel();

    // Temp file helper (used by Judge code too)
    static QString saveEditorToTempFile(EditorWidget *editor, const QString &rootPath);

public slots:
    void compile();
    void run();
    void compileAndRun();
    void stop();
    void toggleDiagnostics();

signals:
    void processStarted(bool acceptingInput);
    void processStopped(bool manual);
    void compileFinished(bool success);
    void runFinished(int exitCode);

private:
    void setupActions();
    void connectProcessRunner();
    bool processCodeFile(const QString &filePath, const QString &ext);

    // Resolve file path for compile/run: handles both IDE and normal modes,
    // returns whether a valid path was resolved. Extracted from the duplicated
    // preamble of compile() / run() / compileAndRun().
    struct ResolvedFile {
        QString filePath;
        QString ext;
        bool isIde = false;
        bool isValid() const { return !filePath.isEmpty(); }
    };
    ResolvedFile resolveIdeFilePath();

    // Dependencies (raw pointers — lifetime managed by Qt parent hierarchy)
    TabManager *m_tabManager = nullptr;
    BottomPanel *m_bottomPanel = nullptr;
    FileExplorerWidget *m_explorer = nullptr;
    QSplitter *m_rightSplitter = nullptr;

    // Owned
    ProcessRunner *m_processRunner = nullptr;
    bool m_running = false;
    bool m_manualStop = false;

    // Actions
    QAction *m_compileAction = nullptr;
    QAction *m_runAction = nullptr;
    QAction *m_compileRunAction = nullptr;
    QAction *m_stopAction = nullptr;
    QAction *m_runToolAction = nullptr;
    QMenu *m_runMenu = nullptr;
};

#endif // COMPILERUNMANAGER_H
