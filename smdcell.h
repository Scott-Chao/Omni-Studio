#ifndef SMDCELL_H
#define SMDCELL_H

#include <QFrame>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QWebEngineView>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QList>

class CodeEditor;
class RenderPixmapWidget;

class SmdCell : public QFrame
{
    Q_OBJECT

public:
    enum CellType { Markdown, Cpp, Python };

    explicit SmdCell(CellType type = Markdown, const QString &content = QString(),
                     QWidget *parent = nullptr);

    CellType cellType() const { return m_type; }
    void setCellType(CellType type);
    QString content() const;
    void setContent(const QString &text);
    bool isModified() const;

    void setCommandMode(bool cmd);
    bool isCommandMode() const { return m_commandMode; }
    void setActive(bool active);

    // MD rendering
    bool isRendered() const { return m_rendered; }
    void setRendered(bool rendered);
    // Set rendered flag without triggering render pipeline (for load-time sync)
    void setRenderedState(bool rendered);

    QWidget *editorWidget() const;
    QWidget *renderImageWidget() const;
    void setEditorFocus();

    // Cursor position
    int cursorLine() const;
    int cursorColumn() const;
    void setCursorPosition(int line, int column);

    void applyZoom(qreal factor, int baseFontSize);
    void checkReRender();
    void updateEditorHeight();

    static CellType typeFromLangId(const QString &langId);
    static QString langIdFromType(CellType type);

signals:
    void executeRequested();
    void cellTypeChanged();
    void focusEntered();
    void focusLeft();
    void contentChanged();
    void renderFinished();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUi(CellType type);
    void setupMarkdownEditor();
    void setupCodeEditor(const QString &langId);
    void updateTypeLabel();
    void updateBorderStyle();
    void ensureRenderView();
    void applyRenderHeight(int contentH);
    void onRenderLoadFinished(bool ok);
    void startGrabPolling();
    void pollGrabReady();
    void performGrab();
    void cleanupRenderView();
    void startRenderPipeline(bool isInitialRender);
    void scheduleReRender();
    void performReRender();

    CellType m_type;
    bool m_commandMode = false;
    bool m_active = false;
    bool m_rendered = false;
    bool m_grabbing = false;  // suppress focusEntered during performGrab

    QWidget *m_headerBar = nullptr;
    QLabel *m_typeLabel = nullptr;
    QLabel *m_executeHint = nullptr;
    QStackedWidget *m_editorStack = nullptr;   // page 0 = editor, page 1 = static QLabel
    QPlainTextEdit *m_markdownEditor = nullptr;
    CodeEditor *m_codeEditor = nullptr;
    QWebEngineView *m_renderView = nullptr;      // direct child, off-stack, overlays editor
    RenderPixmapWidget *m_renderImage = nullptr;   // pixmap replacement (no native HWND, no sizeHint)

    // Adaptive grab state
    QWidget *m_renderOverlay = nullptr;           // native overlay hides QWebEngineView during render
    QTimer *m_grabTimer = nullptr;
    int m_pollCount = 0;
    QList<int> m_polledHeights;
    static constexpr int kMaxPollCount = 25;      // 5s at 200ms interval

    // Re-render on resize
    QTimer *m_renderDebounceTimer = nullptr;
    int m_lastRenderWidth = 0;
    qreal m_zoomFactor = 1.0;

    QString m_languageId;
};

#endif // SMDCELL_H
