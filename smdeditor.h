#ifndef SMDEDITOR_H
#define SMDEDITOR_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QList>
#include <QTimer>

#include "smdcell.h"
#include "smdformat.h"

class ProcessRunner;
class SmdOutputWidget;

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

    QString toPlainTextContentOnly() const;

    // Cell access
    int cellCount() const { return m_cells.size(); }
    int activeCellIndex() const { return m_activeCellIndex; }
    void setActiveCell(int index);
    SmdCell *cellAt(int index) const;
    int activeCellCursorLine() const;
    int activeCellCursorColumn() const;
    void setActiveCellCursor(int line, int column);
    QList<SmdFormat::Cell> exportCells() const;

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
    void insertCellAbove();
    void insertCellBelow();

    // Execution
    void executeCurrentCell();
    void executeMarkdownCell(SmdCell *cell);
    void executeCodeCell(SmdCell *cell);
    void jumpToNextCell();
    void onCellRenderFinished();
    void handleProcessStop();

    // Language selector
    void showLanguageSelector(int cellIndex, bool isNewCell = false, int originalCellIndex = -1);

    // Connections
    void connectCellSignals(SmdCell *cell, int index);

    QScrollArea *m_scrollArea;
    QWidget *m_cellContainer;
    QVBoxLayout *m_cellLayout;
    int m_savedScrollPos = 0;

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
    int m_executingCellIndex = -1;
    int m_pendingRenderJumpIndex = -1;
    bool m_userTerminated = false;
    QString m_executingTempFile;
    int m_executeCounter = 0;
    QMetaObject::Connection m_execOutputConn;
    QMetaObject::Connection m_execCompileConn;
    QMetaObject::Connection m_execRunConn;

    QString m_filePath;
    QString m_originalContent;
};

#endif // SMDEDITOR_H
