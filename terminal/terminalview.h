#ifndef TERMINALVIEW_H
#define TERMINALVIEW_H

#include <QByteArray>
#include <QColor>
#include <QPoint>
#include <QStringDecoder>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QWidget>

class QScrollBar;
class TerminalSession;

class TerminalView : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalView(QWidget *parent = nullptr);
    ~TerminalView() override;

    void attachSession(TerminalSession *session);
    TerminalSession *session() const { return m_session; }
    void appendTerminalData(const QByteArray &data);
    int terminalColumns() const;
    int terminalRows() const;
    void syncTerminalSize();
    void scheduleTerminalSizeSync();
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

signals:
    void inputGenerated(const QByteArray &data);
    void terminalResized(int columns, int rows);

protected:
    bool event(QEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    enum class ParserState { Normal, Escape, Csi, Osc, OscEscape };

    struct CellStyle {
        QColor foreground;
        QColor background;
        bool hasForeground = false;
        bool hasBackground = false;
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool inverse = false;

        bool operator==(const CellStyle &other) const
        {
            return foreground == other.foreground
                && background == other.background
                && hasForeground == other.hasForeground
                && hasBackground == other.hasBackground
                && bold == other.bold
                && italic == other.italic
                && underline == other.underline
                && inverse == other.inverse;
        }
    };

    QScrollBar *m_scrollBar = nullptr;
    TerminalSession *m_session = nullptr;
    ParserState m_state = ParserState::Normal;
    QString m_csi;
    QStringDecoder m_decoder { QStringDecoder::Utf8 };
    QChar m_pendingHighSurrogate;

    int m_lastColumns = 0;
    int m_lastRows = 0;
    bool m_pendingCarriageReturn = false;
    QStringList m_lines;
    QVector<QVector<CellStyle>> m_lineStyles;
    QStringList m_savedNormalLines;
    QVector<QVector<CellStyle>> m_savedNormalLineStyles;
    CellStyle m_currentStyle;
    int m_screenTopRow = 0;
    int m_savedNormalScreenTopRow = 0;
    int m_cursorRow = 0;
    int m_cursorColumn = 0;
    int m_savedNormalCursorRow = 0;
    int m_savedNormalCursorColumn = 0;
    int m_savedCursorRow = 0;
    int m_savedCursorColumn = 0;
    bool m_alternateScreen = false;
    bool m_hasSavedNormalScreen = false;
    bool m_mouseSelecting = false;
    bool m_mouseTracking = false;
    bool m_mouseButtonTracking = false;
    bool m_mouseAnyTracking = false;
    bool m_sgrMouseMode = false;
    bool m_cursorVisible = true;
    bool m_cursorHiddenDuringOutput = false;
    bool m_outputCursorHideSuppressed = false;
    int m_outputCursorHideGeneration = 0;
    bool m_hasSelection = false;
    bool m_selectionAutoScrollActive = false;
    QPoint m_selectionAnchor;
    QPoint m_selectionEnd;
    QPoint m_lastMousePos;
    int m_cellHeight = 1;
    int m_cellWidth = 1;

    // Layout helpers
    int contentWidth() const;
    int contentHeight() const;

    void insertTerminalText(const QString &text);
    void insertTerminalCell(const QString &cell);
    void handleChar(QChar ch);
    void handleCsi(const QString &sequence);
    void handleSgr(const QString &params);
    int csiParamOrDefault(const QString &params, int defaultValue) const;
    QList<int> csiParams(const QString &params) const;
    QList<int> sgrParams(const QString &params) const;
    void ensureCursorLine();
    void ensureScreenBuffer();
    void ensureLineStorage();
    void clampCursorToScreen();
    int screenTopRow() const;
    int absoluteCursorRow() const;
    int cellToStringIndex(const QString &line, int cellColumn) const;
    int cellCount(const QString &line) const;
    int cellCharLengthAt(const QString &line, int stringIndex) const;
    int cellDisplayWidth(const QString &cell) const;
    int cellDisplayWidthAt(const QString &line, int stringIndex) const;
    bool isWideCodepoint(uint codepoint) const;
    bool isContinuationCell(const QString &line, int cellColumn) const;
    QString lineLeftCells(const QString &line, int cellCount) const;
    void trimScrollback();
    void resetScreenBuffer();
    void scrollUpOneLine();
    void renderBuffer();
    QColor ansiColor(int index) const;
    int terminalCellWidth() const;
    int terminalCellHeight() const;
    QColor foregroundForStyle(const CellStyle &style) const;
    QColor backgroundForStyle(const CellStyle &style) const;
    int displayLineLength(const QString &line) const;
    void eraseInLine(int mode);
    void eraseInDisplay(int mode);
    void sendText(const QString &text);
    void markUserInput();
    void pasteClipboard();
    QByteArray keySequenceForEvent(QKeyEvent *event) const;
    void emitSizeIfChanged();
    void updateMetrics();
    void updateScrollBar(bool followCursor);
    int firstVisibleRow() const;
    int bufferRowFromViewportY(int y) const;
    int columnFromViewportX(int x) const;
    QPoint gridPositionFromPoint(const QPoint &point) const;
    void updateSelectionFromPoint(const QPoint &point);
    void updateSelectionAutoScroll(const QPoint &point);
    void stopSelectionAutoScroll();
    void selectionAutoScrollTick();
    void clearSelection();
    bool hasSelection() const;
    bool isCellSelected(int row, int column) const;
    QString selectedText() const;
    QString cellTextAt(int row, int column) const;
    CellStyle cellStyleAt(int row, int column) const;
    QRect cellRect(int row, int column) const;
    QRect cursorRect() const;
    void copySelection();
    void paintTerminal(QPainter &painter);
    void sendMouseEvent(int buttonCode, const QPoint &gridPos);
};

#endif // TERMINALVIEW_H
