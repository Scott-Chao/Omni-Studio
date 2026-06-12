#ifndef TERMINALVIEW_H
#define TERMINALVIEW_H

#include <QPlainTextEdit>
#include <QByteArray>
#include <QColor>
#include <QStringDecoder>
#include <QStringList>
#include <QTextCharFormat>
#include <QVector>

class TerminalSession;

class TerminalView : public QPlainTextEdit
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

signals:
    void inputGenerated(const QByteArray &data);
    void terminalResized(int columns, int rows);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

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
    CellStyle m_currentStyle;
    int m_cursorRow = 0;
    int m_cursorColumn = 0;
    int m_savedCursorRow = 0;
    int m_savedCursorColumn = 0;
    bool m_alternateScreen = false;

    void insertTerminalText(const QString &text);
    void insertTerminalCell(const QString &cell);
    void handleChar(QChar ch);
    void handleCsi(const QString &sequence);
    void handleSgr(const QString &params);
    int csiParamOrDefault(const QString &params, int defaultValue) const;
    QList<int> csiParams(const QString &params) const;
    QList<int> sgrParams(const QString &params) const;
    void ensureCursorLine();
    void ensureLineStorage();
    int cellToStringIndex(const QString &line, int cellColumn) const;
    int cellCount(const QString &line) const;
    int cellCharLengthAt(const QString &line, int stringIndex) const;
    QString lineLeftCells(const QString &line, int cellCount) const;
    void trimScrollback();
    void resetScreenBuffer();
    void scrollUpOneLine();
    void renderBuffer();
    void applyTextFormats(const QStringList &visibleLines);
    QTextCharFormat textFormatForStyle(const CellStyle &style) const;
    QColor ansiColor(int index) const;
    qreal terminalCellWidth() const;
    QColor foregroundForStyle(const CellStyle &style) const;
    QColor backgroundForStyle(const CellStyle &style) const;
    QStringList displayLines() const;
    QString trimDisplayLine(QString line) const;
    int displayLineLength(const QString &line) const;
    void eraseInLine(int mode);
    void eraseInDisplay(int mode);
    void sendText(const QString &text);
    void pasteClipboard();
    QByteArray keySequenceForEvent(QKeyEvent *event) const;
    void emitSizeIfChanged();
};

#endif // TERMINALVIEW_H
