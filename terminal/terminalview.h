#ifndef TERMINALVIEW_H
#define TERMINALVIEW_H

#include <QPlainTextEdit>
#include <QByteArray>
#include <QStringDecoder>

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

    void insertTerminalText(const QString &text);
    void handleChar(QChar ch);
    void handleCsi(const QString &sequence);
    void sendText(const QString &text);
    void pasteClipboard();
    QByteArray keySequenceForEvent(QKeyEvent *event) const;
    void emitSizeIfChanged();
};

#endif // TERMINALVIEW_H
