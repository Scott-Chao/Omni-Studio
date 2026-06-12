#ifndef TERMINALVIEW_H
#define TERMINALVIEW_H

#include <QPlainTextEdit>
#include <QByteArray>
#include <QStringDecoder>
#include <QStringList>

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

    TerminalSession *m_session = nullptr;
    ParserState m_state = ParserState::Normal;
    QString m_csi;
    QStringDecoder m_decoder { QStringDecoder::Utf8 };

    int m_lastColumns = 0;
    int m_lastRows = 0;
    bool m_pendingCarriageReturn = false;
    QStringList m_lines;
    int m_cursorRow = 0;
    int m_cursorColumn = 0;
    int m_savedCursorRow = 0;
    int m_savedCursorColumn = 0;
    bool m_alternateScreen = false;

    void insertTerminalText(const QString &text);
    void insertTerminalChar(QChar ch);
    void handleChar(QChar ch);
    void handleCsi(const QString &sequence);
    int csiParamOrDefault(const QString &params, int defaultValue) const;
    QList<int> csiParams(const QString &params) const;
    void ensureCursorLine();
    void trimScrollback();
    void resetScreenBuffer();
    void scrollUpOneLine();
    void renderBuffer();
    QStringList displayLines() const;
    QString trimDisplayLine(QString line) const;
    void eraseInLine(int mode);
    void eraseInDisplay(int mode);
    void sendText(const QString &text);
    void pasteClipboard();
    QByteArray keySequenceForEvent(QKeyEvent *event) const;
    void emitSizeIfChanged();
};

#endif // TERMINALVIEW_H
