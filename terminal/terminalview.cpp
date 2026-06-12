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
#include <QSignalBlocker>
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
    setReadOnly(true);
    setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    setCursorWidth(8);
    m_lines << QString();

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

    renderBuffer();
}

int TerminalView::terminalColumns() const
{
    QFontMetrics fm(font());
    const int charWidth = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
    return qMax(1, viewport()->width() / charWidth);
}

int TerminalView::terminalRows() const
{
    QFontMetrics fm(font());
    const int charHeight = qMax(1, fm.lineSpacing());
    return qMax(1, viewport()->height() / charHeight);
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
    for (QChar ch : text)
        insertTerminalChar(ch);
}

void TerminalView::insertTerminalChar(QChar ch)
{
    if (ch != QLatin1Char('\n'))
        m_pendingCarriageReturn = false;

    ensureCursorLine();

    if (ch == QLatin1Char('\n')) {
        ++m_cursorRow;
        m_cursorColumn = 0;
        ensureCursorLine();
        trimScrollback();
        return;
    }

    QString &line = m_lines[m_cursorRow];
    if (line.length() < m_cursorColumn)
        line += QString(m_cursorColumn - line.length(), QLatin1Char(' '));
    if (m_cursorColumn < line.length())
        line[m_cursorColumn] = ch;
    else
        line += ch;
    ++m_cursorColumn;
}

void TerminalView::handleChar(QChar ch)
{
    switch (m_state) {
    case ParserState::Normal:
        if (ch == QChar(0x1b)) {
            m_pendingCarriageReturn = false;
            m_state = ParserState::Escape;
        } else if (ch == QLatin1Char('\r')) {
            m_cursorColumn = 0;
            m_pendingCarriageReturn = true;
        } else if (ch == QLatin1Char('\b')) {
            if (m_cursorColumn > 0)
                --m_cursorColumn;
            m_pendingCarriageReturn = false;
        } else if (ch == QLatin1Char('\a')) {
            m_pendingCarriageReturn = false;
            QApplication::beep();
        } else if (ch == QLatin1Char('\n')) {
            if (m_pendingCarriageReturn) {
                QTextCursor cursor = textCursor();
                cursor.movePosition(QTextCursor::EndOfBlock);
                setTextCursor(cursor);
                m_pendingCarriageReturn = false;
            }
            insertTerminalChar(ch);
        } else if (!ch.isNull()) {
            insertTerminalChar(ch);
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
    const QList<int> values = csiParams(params);

    switch (command.unicode()) {
    case 'J':
        eraseInDisplay(values.isEmpty() ? 0 : values.first());
        break;
    case 'K':
        eraseInLine(values.isEmpty() ? 0 : values.first());
        break;
    case 'H':
    case 'f':
        m_cursorRow = qMax(0, (values.size() >= 1 ? values[0] : 1) - 1);
        m_cursorColumn = qMax(0, (values.size() >= 2 ? values[1] : 1) - 1);
        ensureCursorLine();
        break;
    case 'A':
        m_cursorRow = qMax(0, m_cursorRow - csiParamOrDefault(params, 1));
        break;
    case 'B':
        m_cursorRow += csiParamOrDefault(params, 1);
        ensureCursorLine();
        break;
    case 'C':
        m_cursorColumn += csiParamOrDefault(params, 1);
        break;
    case 'D':
        m_cursorColumn = qMax(0, m_cursorColumn - csiParamOrDefault(params, 1));
        break;
    case 'G':
        m_cursorColumn = qMax(0, csiParamOrDefault(params, 1) - 1);
        break;
    case 'P': {
        ensureCursorLine();
        QString &line = m_lines[m_cursorRow];
        int count = csiParamOrDefault(params, 1);
        if (m_cursorColumn < line.length())
            line.remove(m_cursorColumn, count);
        break;
    }
    case '@': {
        ensureCursorLine();
        QString &line = m_lines[m_cursorRow];
        int count = csiParamOrDefault(params, 1);
        if (line.length() < m_cursorColumn)
            line += QString(m_cursorColumn - line.length(), QLatin1Char(' '));
        line.insert(m_cursorColumn, QString(count, QLatin1Char(' ')));
        break;
    }
    case 'X': {
        ensureCursorLine();
        QString &line = m_lines[m_cursorRow];
        int count = csiParamOrDefault(params, 1);
        if (line.length() < m_cursorColumn + count)
            line += QString(m_cursorColumn + count - line.length(), QLatin1Char(' '));
        line.replace(m_cursorColumn, count, QString(count, QLatin1Char(' ')));
        break;
    }
    case 'E':
        m_cursorRow += csiParamOrDefault(params, 1);
        m_cursorColumn = 0;
        ensureCursorLine();
        break;
    case 'F':
        m_cursorRow = qMax(0, m_cursorRow - csiParamOrDefault(params, 1));
        m_cursorColumn = 0;
        break;
    case 's':
        m_savedCursorRow = m_cursorRow;
        m_savedCursorColumn = m_cursorColumn;
        break;
    case 'u':
        m_cursorRow = m_savedCursorRow;
        m_cursorColumn = m_savedCursorColumn;
        ensureCursorLine();
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

int TerminalView::csiParamOrDefault(const QString &params, int defaultValue) const
{
    QList<int> values = csiParams(params);
    if (values.isEmpty())
        return defaultValue;
    int value = values.first();
    return value > 0 ? value : defaultValue;
}

QList<int> TerminalView::csiParams(const QString &params) const
{
    QList<int> values;
    QString cleaned = params;
    cleaned.remove(QLatin1Char('?'));
    cleaned.remove(QLatin1Char('>'));
    cleaned.remove(QLatin1Char('='));
    if (cleaned.isEmpty())
        return values;

    const QStringList parts = cleaned.split(QLatin1Char(';'));
    for (const QString &part : parts) {
        if (part.isEmpty()) {
            values << 0;
            continue;
        }
        bool ok = false;
        int value = part.toInt(&ok);
        values << (ok ? value : 0);
    }
    return values;
}

void TerminalView::ensureCursorLine()
{
    if (m_lines.isEmpty())
        m_lines << QString();
    while (m_cursorRow >= m_lines.size())
        m_lines << QString();
    if (m_cursorRow < 0)
        m_cursorRow = 0;
}

void TerminalView::trimScrollback()
{
    constexpr int maxLines = 20000;
    if (m_lines.size() <= maxLines)
        return;

    int removeCount = m_lines.size() - maxLines;
    m_lines.erase(m_lines.begin(), m_lines.begin() + removeCount);
    m_cursorRow = qMax(0, m_cursorRow - removeCount);
    m_savedCursorRow = qMax(0, m_savedCursorRow - removeCount);
}

void TerminalView::renderBuffer()
{
    QSignalBlocker blocker(this);
    const QString text = m_lines.join(QLatin1Char('\n'));
    if (toPlainText() != text)
        setPlainText(text);

    QTextBlock block = document()->findBlockByNumber(m_cursorRow);
    QTextCursor cursor(document());
    if (block.isValid()) {
        int column = qMin(m_cursorColumn, block.text().length());
        cursor.setPosition(block.position() + column);
    } else {
        cursor.movePosition(QTextCursor::End);
    }
    setTextCursor(cursor);
    ensureCursorVisible();
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void TerminalView::eraseInLine(int mode)
{
    ensureCursorLine();
    QString &line = m_lines[m_cursorRow];
    if (mode == 2) {
        line.clear();
        m_cursorColumn = 0;
    } else if (mode == 1) {
        int count = qMin(m_cursorColumn + 1, line.length());
        if (count > 0)
            line.replace(0, count, QString(count, QLatin1Char(' ')));
    } else {
        if (m_cursorColumn < line.length())
            line.truncate(m_cursorColumn);
    }
}

void TerminalView::eraseInDisplay(int mode)
{
    if (mode == 2 || mode == 3) {
        m_lines = { QString() };
        m_cursorRow = 0;
        m_cursorColumn = 0;
        return;
    }

    if (mode == 0) {
        eraseInLine(0);
        while (m_lines.size() > m_cursorRow + 1)
            m_lines.removeLast();
    } else if (mode == 1) {
        for (int row = 0; row < m_cursorRow && row < m_lines.size(); ++row)
            m_lines[row].clear();
        eraseInLine(1);
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
        return QByteArray("\x08", 1);
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
    int columns = terminalColumns();
    int rows = terminalRows();

    if (columns == m_lastColumns && rows == m_lastRows)
        return;

    m_lastColumns = columns;
    m_lastRows = rows;
    emit terminalResized(columns, rows);
}
