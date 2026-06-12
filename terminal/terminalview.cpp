#include "terminalview.h"

#include "terminalsession.h"
#include "config/configmanager.h"
#include "core/thememanager.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <QTextBlock>

TerminalView::TerminalView(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setObjectName(QStringLiteral("terminalView"));
    setFrameShape(QFrame::NoFrame);
    setUndoRedoEnabled(false);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setWordWrapMode(QTextOption::NoWrap);
    setMaximumBlockCount(20000);
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setCursorWidth(8);

    QFont mono(ConfigManager::instance().outputPanelFontFamily(),
               ConfigManager::instance().outputPanelFontSize());
    mono.setStyleHint(QFont::Monospace);
    setFont(mono);

    auto &tm = ThemeManager::instance();
    setStyleSheet(QStringLiteral(
        "QPlainTextEdit#terminalView {"
        "  background: %1;"
        "  color: %2;"
        "  selection-background-color: %3;"
        "  border: none;"
        "}")
        .arg(tm.color("output.background").name(),
             tm.color("output.foreground").name(),
             tm.color("output.selectionBackground").name()));
}

TerminalView::~TerminalView()
{
    if (m_session)
        m_session->stop();
}

void TerminalView::attachSession(TerminalSession *session)
{
    m_session = session;
    if (!m_session)
        return;

    connect(this, &TerminalView::inputGenerated,
            m_session, &TerminalSession::writeInput);
    connect(this, &TerminalView::terminalResized,
            m_session, &TerminalSession::resizeTerminal);
    connect(m_session, &TerminalSession::outputReady,
            this, &TerminalView::appendTerminalData);
    connect(m_session, &TerminalSession::errorOccurred, this, [this](const QString &message) {
        insertTerminalText(QStringLiteral("\r\n[terminal] %1\r\n").arg(message));
    });
    connect(m_session, &TerminalSession::exited, this, [this](int exitCode) {
        insertTerminalText(QStringLiteral("\r\n[terminal exited: %1]\r\n").arg(exitCode));
    });
}

void TerminalView::appendTerminalData(const QByteArray &data)
{
    const QString text = m_decoder.decode(data);
    for (QChar ch : text)
        handleChar(ch);

    ensureCursorVisible();
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void TerminalView::keyPressEvent(QKeyEvent *event)
{
    const bool copyShortcut =
        event->key() == Qt::Key_C
        && (event->modifiers() & Qt::ControlModifier)
        && (event->modifiers() & Qt::ShiftModifier);
    if (copyShortcut && textCursor().hasSelection()) {
        copy();
        return;
    }

    const bool pasteShortcut =
        (event->key() == Qt::Key_V && (event->modifiers() & Qt::ControlModifier))
        || (event->matches(QKeySequence::Paste));
    if (pasteShortcut) {
        pasteClipboard();
        return;
    }

    QByteArray sequence = keySequenceForEvent(event);
    if (!sequence.isEmpty()) {
        emit inputGenerated(sequence);
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void TerminalView::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    emitSizeIfChanged();
}

void TerminalView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(textCursor().hasSelection());
    QAction *pasteAction = menu.addAction(tr("Paste"));
    QAction *selected = menu.exec(event->globalPos());
    if (selected == copyAction)
        copy();
    else if (selected == pasteAction)
        pasteClipboard();
}

void TerminalView::insertTerminalText(const QString &text)
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    setTextCursor(cursor);
}

void TerminalView::handleChar(QChar ch)
{
    switch (m_state) {
    case ParserState::Normal:
        if (ch == QChar(0x1b)) {
            m_state = ParserState::Escape;
        } else if (ch == QLatin1Char('\r')) {
            QTextCursor cursor = textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.movePosition(QTextCursor::StartOfBlock);
            setTextCursor(cursor);
        } else if (ch == QLatin1Char('\b')) {
            QTextCursor cursor = textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.deletePreviousChar();
            setTextCursor(cursor);
        } else if (ch == QLatin1Char('\a')) {
            QApplication::beep();
        } else if (ch == QLatin1Char('\n')) {
            insertTerminalText(QStringLiteral("\n"));
        } else if (!ch.isNull()) {
            insertTerminalText(QString(ch));
        }
        break;
    case ParserState::Escape:
        if (ch == QLatin1Char('[')) {
            m_csi.clear();
            m_state = ParserState::Csi;
        } else if (ch == QLatin1Char(']')) {
            m_state = ParserState::Osc;
        } else if (ch == QLatin1Char('c')) {
            clear();
            m_state = ParserState::Normal;
        } else {
            m_state = ParserState::Normal;
        }
        break;
    case ParserState::Csi:
        m_csi += ch;
        if (ch.unicode() >= 0x40 && ch.unicode() <= 0x7e) {
            handleCsi(m_csi);
            m_csi.clear();
            m_state = ParserState::Normal;
        }
        break;
    case ParserState::Osc:
        if (ch == QLatin1Char('\a'))
            m_state = ParserState::Normal;
        else if (ch == QChar(0x1b))
            m_state = ParserState::OscEscape;
        break;
    case ParserState::OscEscape:
        m_state = (ch == QLatin1Char('\\')) ? ParserState::Normal : ParserState::Osc;
        break;
    }
}

void TerminalView::handleCsi(const QString &sequence)
{
    if (sequence.isEmpty())
        return;

    const QChar command = sequence.back();
    const QString params = sequence.left(sequence.size() - 1);
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);

    switch (command.unicode()) {
    case 'J':
        if (params.isEmpty() || params == QStringLiteral("2") || params == QStringLiteral("3"))
            clear();
        break;
    case 'K':
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        setTextCursor(cursor);
        break;
    case 'H':
    case 'f':
        if (params.isEmpty() || params == QStringLiteral(";") || params == QStringLiteral("1;1")) {
            cursor.movePosition(QTextCursor::Start);
            setTextCursor(cursor);
        }
        break;
    case 'A':
        cursor.movePosition(QTextCursor::Up);
        setTextCursor(cursor);
        break;
    case 'B':
        cursor.movePosition(QTextCursor::Down);
        setTextCursor(cursor);
        break;
    case 'C':
        cursor.movePosition(QTextCursor::Right);
        setTextCursor(cursor);
        break;
    case 'D':
        cursor.movePosition(QTextCursor::Left);
        setTextCursor(cursor);
        break;
    case 'm':
    case 'h':
    case 'l':
    case '?':
        break;
    default:
        break;
    }
}

void TerminalView::sendText(const QString &text)
{
    if (!text.isEmpty())
        emit inputGenerated(text.toUtf8());
}

void TerminalView::pasteClipboard()
{
    QString text = QApplication::clipboard()->text();
    if (text.isEmpty())
        return;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\r"));
    text.replace(QLatin1Char('\n'), QLatin1Char('\r'));
    sendText(text);
}

QByteArray TerminalView::keySequenceForEvent(QKeyEvent *event) const
{
    const Qt::KeyboardModifiers mods = event->modifiers();

    if (mods & Qt::ControlModifier) {
        const int key = event->key();
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            char code = char(key - Qt::Key_A + 1);
            return QByteArray(1, code);
        }
    }

    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return QByteArray("\r", 1);
    case Qt::Key_Backspace:
        return QByteArray("\x7f", 1);
    case Qt::Key_Tab:
        return QByteArray("\t", 1);
    case Qt::Key_Escape:
        return QByteArray("\x1b", 1);
    case Qt::Key_Left:
        return QByteArray("\x1b[D", 3);
    case Qt::Key_Right:
        return QByteArray("\x1b[C", 3);
    case Qt::Key_Up:
        return QByteArray("\x1b[A", 3);
    case Qt::Key_Down:
        return QByteArray("\x1b[B", 3);
    case Qt::Key_Home:
        return QByteArray("\x1b[H", 3);
    case Qt::Key_End:
        return QByteArray("\x1b[F", 3);
    case Qt::Key_Delete:
        return QByteArray("\x1b[3~", 4);
    case Qt::Key_PageUp:
        return QByteArray("\x1b[5~", 4);
    case Qt::Key_PageDown:
        return QByteArray("\x1b[6~", 4);
    default:
        break;
    }

    QString text = event->text();
    if (!text.isEmpty() && !(mods & Qt::ControlModifier))
        return text.toUtf8();

    return {};
}

void TerminalView::emitSizeIfChanged()
{
    QFontMetrics fm(font());
    const int charWidth = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
    const int charHeight = qMax(1, fm.lineSpacing());
    int columns = qMax(1, viewport()->width() / charWidth);
    int rows = qMax(1, viewport()->height() / charHeight);

    if (columns == m_lastColumns && rows == m_lastRows)
        return;

    m_lastColumns = columns;
    m_lastRows = rows;
    emit terminalResized(columns, rows);
}
