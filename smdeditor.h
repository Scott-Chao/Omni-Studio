#ifndef SMDEDITOR_H
#define SMDEDITOR_H

#include <QWidget>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>
#include <QList>
#include <QTimer>
#include <QPointer>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>

#include "smdcell.h"
#include "smdformat.h"

class ProcessRunner;
class SmdOutputWidget;
class SmdLspManager;
class SmdDiagnosticsPanel;

class SmdEditor : public QWidget
{
    Q_OBJECT

public:
    explicit SmdEditor(QWidget *parent = nullptr);
    ~SmdEditor();

    bool loadFile(const QString &filePath);
    bool saveFile();
    QString toPlainText() const;
    void setPlainText(const QString &text);
    bool isModified() const;
    void setModified(bool modified);
    QString currentFilePath() const { return m_filePath; }
    void setFilePath(const QString &path) { m_filePath = path; }

    void applyZoom(qreal factor, int baseFontSize);
    void setEditorFont(const QString &family, int size);
    void reloadColors();
    void refreshStyle();

    QString toPlainTextContentOnly() const;

    // Cell access
    int cellCount() const { return m_cells.size(); }
    int activeCellIndex() const { return m_activeCellIndex; }
    void setActiveCell(int index);
    SmdCell *cellAt(int index) const;
    SmdLspManager *lspManager() const { return m_lspManager; }
    int activeCellCursorLine() const;
    int activeCellCursorColumn() const;
    void setActiveCellCursor(int line, int column);
    QList<SmdFormat::Cell> exportCells() const;
    void reloadShortcuts();
    void toggleDiagnosticsPanel();

signals:
    void modificationChanged(bool modified);
    void fileLoaded(const QString &filePath);
    void fileSaved(const QString &filePath);
    void contentChanged();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // Mode
    void enterCommandMode();
    void enterEditMode();

    // Cells
    SmdCell *addCell(int index, SmdCell::CellType type, const QString &content = QString());
    void removeCell(int index);
    void removeInsertScrollPad();
    void insertCellAbove();
    void insertCellBelow();

    // Execution
    void executeCurrentCell();
    void executeMarkdownCell(SmdCell *cell);
    void executeCodeCell(SmdCell *cell);
    void executePythonCell(SmdCell *cell);
    void jumpToNextCell();
    int cppGroupForCell(int cellIndex) const;
    void onCellRenderFinished();
    void handleProcessStop();
    void splitCellAtCursor();

    // Persistent Python execution (Jupyter-like)
    void startPythonExecProcess();
    void stopPythonExecProcess();
    void onPyExecReadyRead();
    void onPyExecFinished(int exitCode, QProcess::ExitStatus status);
    void onPyExecError(QProcess::ProcessError error);

    // Diagnostics panel (public, also used by MainWindow shortcut)

    // Language selector
    void showLanguageSelector(int cellIndex, bool isNewCell = false, int originalCellIndex = -1);

    // Connections
    void connectCellSignals(SmdCell *cell, int index);

    QSplitter *m_splitter;
    QScrollArea *m_scrollArea;
    QWidget *m_cellContainer;
    QVBoxLayout *m_cellLayout;
    int m_savedScrollPos = 0;
    bool m_clickSuppressScroll = false;
    QSpacerItem *m_insertScrollPad = nullptr;

    QList<SmdCell*> m_cells;
    QList<SmdOutputWidget*> m_outputWidgets;
    int m_activeCellIndex = -1;
    bool m_commandMode = false;

    // Auto-render state
    QList<SmdCell*> m_autoRenderQueue;
    int m_autoRenderIndex = 0;
    QTimer *m_autoRenderTimer = nullptr;

    // Auto-render on file open
    void startAutoRender();

    // Execution callbacks
    void onProcessOutput(const QString &text, bool isStderr);
    void onProcessFinished(int exitCode);

    ProcessRunner *m_processRunner;
    QPointer<SmdCell> m_executingCell;
    int m_pendingRenderJumpIndex = -1;
    bool m_userTerminated = false;
    bool m_jumpAfterExecute = false;
    QString m_executingTempFile;
    bool m_executingCompileOnly = false;
    int m_executeCounter = 0;
    QMetaObject::Connection m_execOutputConn;
    QMetaObject::Connection m_execCompileConn;
    QMetaObject::Connection m_execRunConn;

    QString m_filePath;
    QString m_originalContent;

    qreal m_zoomFactor = 1.0;
    int m_baseFontSize = 14;

    SmdLspManager *m_lspManager = nullptr;
    SmdDiagnosticsPanel *m_diagnosticsPanel = nullptr;

    // Configurable shortcuts
    QKeySequence m_cellExecute;
    QKeySequence m_cellExecuteJump;
    QKeySequence m_cellLanguage;
    QKeySequence m_cellTerminate;
    QKeySequence m_cellClearOutput;
    QKeySequence m_cellSplit;
    QKeySequence m_toggleDiagnostics;
    QKeySequence m_cellInsertAbove;
    QKeySequence m_cellInsertBelow;
    QKeySequence m_cellDelete;

    // Persistent Python execution process (Jupyter-like)
    QProcess *m_pyExecProcess = nullptr;
    QByteArray m_pyExecBuffer;
    QString m_pyExecScriptPath;
};

#endif // SMDEDITOR_H
