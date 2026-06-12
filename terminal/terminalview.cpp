#include "terminalview.h"

#include "terminalsession.h"
#include "config/configmanager.h"
#include "core/thememanager.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTimer>

#include <algorithm>

TerminalView::TerminalView(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setObjectName(QStringLiteral("terminalView"));
    setFrameShape(QFrame::NoFrame);
    setUndoRedoEnabled(false);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setWordWrapMode(QTextOption::NoWrap);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMaximumBlockCount(20000);
    document()->setDocumentMargin(0);
    setReadOnly(true);
    setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_lines << QString();

    const QString configuredFamily = ConfigManager::instance().outputPanelFontFamily();
    const QStringList availableFamilies = QFontDatabase::families();
    const QString fontFamily = availableFamilies.contains(QStringLiteral("Cascadia Mono"))
        ? QStringLiteral("Cascadia Mono")
        : configuredFamily;
    QFont mono(fontFamily, ConfigManager::instance().outputPanelFontSize());
    mono.setStyleHint(QFont::Monospace);
    mono.setFixedPitch(true);
    mono.setKerning(false);
    setFont(mono);
    setCursorWidth(qMax(1, qRound(terminalCellWidth())));

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
    const qreal charWidth = terminalCellWidth();
    constexpr int widthPaddingPx = 1;
    const int availableWidth = qMax(1, viewport()->width() - widthPaddingPx);
    return qMax(1, int(availableWidth / charWidth));
}

int TerminalView::terminalRows() const
{
    QFontMetrics fm(font());
    const int charHeight = qMax(1, fm.lineSpacing());
    constexpr int heightPaddingPx = 1;
    const int availableHeight = qMax(1, viewport()->height() - heightPaddingPx);
    return qMax(1, availableHeight / charHeight);
}

void TerminalView::syncTerminalSize()
{
    if (m_alternateScreen) {
        const int rows = terminalRows();
        while (m_lines.size() > rows) {
            m_lines.removeLast();
            if (!m_lineStyles.isEmpty())
                m_lineStyles.removeLast();
        }
        while (m_lines.size() < rows) {
            m_lines << QString();
            m_lineStyles.append(QVector<CellStyle>());
        }
        m_cursorRow = qBound(0, m_cursorRow, qMax(0, rows - 1));
        ensureLineStorage();
    }
    m_lastColumns = 0;
    m_lastRows = 0;
    emitSizeIfChanged();
    renderBuffer();
}

void TerminalView::scheduleTerminalSizeSync()
{
    QTimer::singleShot(0, this, &TerminalView::syncTerminalSize);
    QTimer::singleShot(80, this, &TerminalView::syncTerminalSize);
    QTimer::singleShot(250, this, &TerminalView::syncTerminalSize);
    QTimer::singleShot(600, this, &TerminalView::syncTerminalSize);
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
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            scheduleTerminalSizeSync();
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void TerminalView::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    emitSizeIfChanged();
    QTimer::singleShot(0, this, &TerminalView::syncTerminalSize);
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
        handleChar(ch);
}

void TerminalView::insertTerminalCell(const QString &cell)
{
    if (cell != QLatin1String("\n"))
        m_pendingCarriageReturn = false;

    ensureCursorLine();

    if (cell == QLatin1String("\n")) {
        const int rows = terminalRows();
        if (m_alternateScreen && m_cursorRow >= rows - 1) {
            scrollUpOneLine();
        } else {
            ++m_cursorRow;
        }
        m_cursorColumn = 0;
        ensureCursorLine();
        trimScrollback();
        return;
    }

    QString &line = m_lines[m_cursorRow];
    QVector<CellStyle> &styles = m_lineStyles[m_cursorRow];
    const int currentCells = cellCount(line);
    if (currentCells < m_cursorColumn)
        line += QString(m_cursorColumn - currentCells, QLatin1Char(' '));
    while (styles.size() < m_cursorColumn)
        styles.append(CellStyle());
    const int stringIndex = cellToStringIndex(line, m_cursorColumn);
    if (m_cursorColumn < cellCount(line)) {
        line.replace(stringIndex, cellCharLengthAt(line, stringIndex), cell);
        if (m_cursorColumn < styles.size())
            styles[m_cursorColumn] = m_currentStyle;
        else
            styles.append(m_currentStyle);
    } else {
        line += cell;
        styles.append(m_currentStyle);
    }
    ++m_cursorColumn;
}

void TerminalView::handleChar(QChar ch)
{
    switch (m_state) {
    case ParserState::Normal:
        if (!m_pendingHighSurrogate.isNull()) {
            if (ch.isLowSurrogate()) {
                QString cell;
                cell.append(m_pendingHighSurrogate);
                cell.append(ch);
                m_pendingHighSurrogate = QChar();
                insertTerminalCell(cell);
                break;
            }
            insertTerminalCell(QString(m_pendingHighSurrogate));
            m_pendingHighSurrogate = QChar();
        }

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
            insertTerminalCell(QString(ch));
        } else if (!ch.isNull()) {
            if (ch.isHighSurrogate())
                m_pendingHighSurrogate = ch;
            else
                insertTerminalCell(QString(ch));
        }
        break;
    case ParserState::Escape:
        if (ch == QLatin1Char('[')) {
            m_csi.clear();
            m_state = ParserState::Csi;
        } else if (ch == QLatin1Char(']')) {
            m_state = ParserState::Osc;
        } else if (ch == QLatin1Char('c')) {
            resetScreenBuffer();
            m_currentStyle = CellStyle();
            m_state = ParserState::Normal;
        } else if (ch == QLatin1Char('7')) {
            m_savedCursorRow = m_cursorRow;
            m_savedCursorColumn = m_cursorColumn;
            m_state = ParserState::Normal;
        } else if (ch == QLatin1Char('8')) {
            m_cursorRow = m_savedCursorRow;
            m_cursorColumn = m_savedCursorColumn;
            ensureCursorLine();
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
        QVector<CellStyle> &styles = m_lineStyles[m_cursorRow];
        int count = csiParamOrDefault(params, 1);
        if (m_cursorColumn < cellCount(line)) {
            const int start = cellToStringIndex(line, m_cursorColumn);
            const int end = cellToStringIndex(line, m_cursorColumn + count);
            line.remove(start, end - start);
        }
        if (m_cursorColumn < styles.size())
            styles.remove(m_cursorColumn, qMin(count, styles.size() - m_cursorColumn));
        break;
    }
    case '@': {
        ensureCursorLine();
        QString &line = m_lines[m_cursorRow];
        QVector<CellStyle> &styles = m_lineStyles[m_cursorRow];
        int count = csiParamOrDefault(params, 1);
        const int currentCells = cellCount(line);
        if (currentCells < m_cursorColumn)
            line += QString(m_cursorColumn - currentCells, QLatin1Char(' '));
        while (styles.size() < m_cursorColumn)
            styles.append(CellStyle());
        line.insert(cellToStringIndex(line, m_cursorColumn), QString(count, QLatin1Char(' ')));
        for (int i = 0; i < count; ++i)
            styles.insert(m_cursorColumn, CellStyle());
        break;
    }
    case 'X': {
        ensureCursorLine();
        QString &line = m_lines[m_cursorRow];
        QVector<CellStyle> &styles = m_lineStyles[m_cursorRow];
        int count = csiParamOrDefault(params, 1);
        const int currentCells = cellCount(line);
        if (currentCells < m_cursorColumn + count)
            line += QString(m_cursorColumn + count - currentCells, QLatin1Char(' '));
        while (styles.size() < m_cursorColumn + count)
            styles.append(CellStyle());
        const int start = cellToStringIndex(line, m_cursorColumn);
        const int end = cellToStringIndex(line, m_cursorColumn + count);
        line.replace(start, end - start, QString(count, QLatin1Char(' ')));
        for (int i = 0; i < count && m_cursorColumn + i < styles.size(); ++i)
            styles[m_cursorColumn + i] = CellStyle();
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
    case 'd':
        m_cursorRow = qMax(0, csiParamOrDefault(params, 1) - 1);
        ensureCursorLine();
        break;
    case 'e':
        m_cursorRow += csiParamOrDefault(params, 1);
        ensureCursorLine();
        break;
    case 'L': {
        int count = csiParamOrDefault(params, 1);
        ensureCursorLine();
        for (int i = 0; i < count; ++i) {
            m_lines.insert(m_cursorRow, QString());
            m_lineStyles.insert(m_cursorRow, QVector<CellStyle>());
        }
        break;
    }
    case 'M': {
        int count = csiParamOrDefault(params, 1);
        ensureCursorLine();
        while (count-- > 0 && m_cursorRow < m_lines.size()) {
            m_lines.removeAt(m_cursorRow);
            if (m_cursorRow < m_lineStyles.size())
                m_lineStyles.removeAt(m_cursorRow);
        }
        ensureCursorLine();
        break;
    }
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
        handleSgr(params);
        break;
    case 'h':
        if (params.contains(QStringLiteral("1049"))
            || params.contains(QStringLiteral("1047"))
            || params.contains(QStringLiteral("47"))) {
            m_alternateScreen = true;
            setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            resetScreenBuffer();
            scheduleTerminalSizeSync();
        }
        break;
    case 'l':
        if (params.contains(QStringLiteral("1049"))
            || params.contains(QStringLiteral("1047"))
            || params.contains(QStringLiteral("47"))) {
            m_alternateScreen = false;
            setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            resetScreenBuffer();
            scheduleTerminalSizeSync();
        }
        break;
    case '?':
        break;
    default:
        break;
    }
}

void TerminalView::handleSgr(const QString &params)
{
    const QList<int> values = sgrParams(params);
    for (int i = 0; i < values.size(); ++i) {
        const int value = values[i];
        if (value == 0) {
            m_currentStyle = CellStyle();
        } else if (value == 1) {
            m_currentStyle.bold = true;
        } else if (value == 3) {
            m_currentStyle.italic = true;
        } else if (value == 4) {
            m_currentStyle.underline = true;
        } else if (value == 7) {
            m_currentStyle.inverse = true;
        } else if (value == 22) {
            m_currentStyle.bold = false;
        } else if (value == 23) {
            m_currentStyle.italic = false;
        } else if (value == 24) {
            m_currentStyle.underline = false;
        } else if (value == 27) {
            m_currentStyle.inverse = false;
        } else if (value == 39) {
            m_currentStyle.foreground = QColor();
            m_currentStyle.hasForeground = false;
        } else if (value == 49) {
            m_currentStyle.background = QColor();
            m_currentStyle.hasBackground = false;
        } else if (value >= 30 && value <= 37) {
            m_currentStyle.foreground = ansiColor(value - 30);
            m_currentStyle.hasForeground = true;
        } else if (value >= 40 && value <= 47) {
            m_currentStyle.background = ansiColor(value - 40);
            m_currentStyle.hasBackground = true;
        } else if (value >= 90 && value <= 97) {
            m_currentStyle.foreground = ansiColor(value - 90 + 8);
            m_currentStyle.hasForeground = true;
        } else if (value >= 100 && value <= 107) {
            m_currentStyle.background = ansiColor(value - 100 + 8);
            m_currentStyle.hasBackground = true;
        } else if ((value == 38 || value == 48) && i + 1 < values.size()) {
            const bool foreground = value == 38;
            const int mode = values[++i];
            QColor color;
            if (mode == 5 && i + 1 < values.size()) {
                color = ansiColor(values[++i]);
            } else if (mode == 2 && i + 3 < values.size()) {
                const int red = qBound(0, values[++i], 255);
                const int green = qBound(0, values[++i], 255);
                const int blue = qBound(0, values[++i], 255);
                color = QColor(red, green, blue);
            }

            if (color.isValid()) {
                if (foreground) {
                    m_currentStyle.foreground = color;
                    m_currentStyle.hasForeground = true;
                } else {
                    m_currentStyle.background = color;
                    m_currentStyle.hasBackground = true;
                }
            }
        }
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

QList<int> TerminalView::sgrParams(const QString &params) const
{
    if (params.isEmpty())
        return {0};

    QString cleaned = params;
    cleaned.replace(QLatin1Char(':'), QLatin1Char(';'));
    return csiParams(cleaned);
}

void TerminalView::ensureCursorLine()
{
    if (m_lines.isEmpty())
        m_lines << QString();

    if (m_alternateScreen)
        m_cursorRow = qBound(0, m_cursorRow, qMax(0, terminalRows() - 1));

    while (m_cursorRow >= m_lines.size())
        m_lines << QString();
    if (m_cursorRow < 0)
        m_cursorRow = 0;
    ensureLineStorage();
}

void TerminalView::ensureLineStorage()
{
    while (m_lineStyles.size() < m_lines.size())
        m_lineStyles.append(QVector<CellStyle>());
    while (m_lineStyles.size() > m_lines.size())
        m_lineStyles.removeLast();
}

int TerminalView::cellToStringIndex(const QString &line, int cellColumn) const
{
    int index = 0;
    int column = 0;
    while (index < line.length() && column < cellColumn) {
        index += cellCharLengthAt(line, index);
        ++column;
    }
    return index;
}

int TerminalView::cellCount(const QString &line) const
{
    int count = 0;
    int index = 0;
    while (index < line.length()) {
        index += cellCharLengthAt(line, index);
        ++count;
    }
    return count;
}

int TerminalView::cellCharLengthAt(const QString &line, int stringIndex) const
{
    if (stringIndex + 1 < line.length()
        && line.at(stringIndex).isHighSurrogate()
        && line.at(stringIndex + 1).isLowSurrogate()) {
        return 2;
    }
    return 1;
}

QString TerminalView::lineLeftCells(const QString &line, int cellCount) const
{
    return line.left(cellToStringIndex(line, cellCount));
}

void TerminalView::trimScrollback()
{
    if (m_alternateScreen) {
        const int rows = terminalRows();
        while (m_lines.size() > rows) {
            m_lines.removeLast();
            if (!m_lineStyles.isEmpty())
                m_lineStyles.removeLast();
        }
        while (m_lines.size() < rows) {
            m_lines << QString();
            m_lineStyles.append(QVector<CellStyle>());
        }
        return;
    }

    constexpr int maxLines = 20000;
    if (m_lines.size() <= maxLines)
        return;

    int removeCount = m_lines.size() - maxLines;
    m_lines.erase(m_lines.begin(), m_lines.begin() + removeCount);
    m_lineStyles.erase(m_lineStyles.begin(), m_lineStyles.begin() + qMin(removeCount, m_lineStyles.size()));
    m_cursorRow = qMax(0, m_cursorRow - removeCount);
    m_savedCursorRow = qMax(0, m_savedCursorRow - removeCount);
}

void TerminalView::resetScreenBuffer()
{
    const int rows = m_alternateScreen ? terminalRows() : 1;
    m_lines.clear();
    m_lineStyles.clear();
    for (int i = 0; i < rows; ++i)
        m_lines << QString();
    ensureLineStorage();
    m_cursorRow = 0;
    m_cursorColumn = 0;
    m_savedCursorRow = 0;
    m_savedCursorColumn = 0;
}

void TerminalView::scrollUpOneLine()
{
    ensureCursorLine();
    if (!m_lines.isEmpty()) {
        m_lines.removeFirst();
        if (!m_lineStyles.isEmpty())
            m_lineStyles.removeFirst();
    }
    m_lines << QString();
    m_lineStyles.append(QVector<CellStyle>());
    m_cursorRow = qMax(0, terminalRows() - 1);
}

void TerminalView::renderBuffer()
{
    trimScrollback();
    QSignalBlocker blocker(this);
    const QStringList visibleLines = displayLines();
    const QString text = visibleLines.join(QLatin1Char('\n'));
    const bool textChanged = toPlainText() != text;
    if (textChanged)
        setPlainText(text);
    applyTextFormats(visibleLines);

    QTextBlock block = document()->findBlockByNumber(m_cursorRow);
    QTextCursor cursor(document());
    if (block.isValid()) {
        const int column = qMin(m_cursorColumn, terminalColumns() - 1);
        cursor.setPosition(block.position() + cellToStringIndex(block.text(), column));
    } else {
        cursor.movePosition(QTextCursor::End);
    }
    setTextCursor(cursor);
    ensureCursorVisible();
    horizontalScrollBar()->setValue(0);
    if (m_alternateScreen)
        verticalScrollBar()->setValue(0);
}

void TerminalView::applyTextFormats(const QStringList &visibleLines)
{
    QTextCursor cursor(document());
    cursor.beginEditBlock();

    const QTextCharFormat defaultFormat;
    cursor.select(QTextCursor::Document);
    cursor.setCharFormat(defaultFormat);

    const int lineCount = qMin(visibleLines.size(), m_lineStyles.size());
    for (int row = 0; row < lineCount; ++row) {
        const int visibleLength = displayLineLength(visibleLines[row]);
        if (visibleLength <= 0)
            continue;

        const QVector<CellStyle> &styles = m_lineStyles[row];
        int column = 0;
        while (column < visibleLength && column < styles.size()) {
            const CellStyle style = styles[column];
            int runEnd = column + 1;
            while (runEnd < visibleLength
                   && runEnd < styles.size()
                   && styles[runEnd] == style) {
                ++runEnd;
            }

            const bool hasExplicitStyle = style.hasForeground
                || style.hasBackground
                || style.bold
                || style.italic
                || style.underline
                || style.inverse;
            if (hasExplicitStyle) {
                QTextBlock block = document()->findBlockByNumber(row);
                if (block.isValid()) {
                    const int start = cellToStringIndex(block.text(), column);
                    const int end = cellToStringIndex(block.text(), runEnd);
                    QTextCursor runCursor(block);
                    runCursor.setPosition(block.position() + start);
                    runCursor.setPosition(block.position() + end, QTextCursor::KeepAnchor);
                    runCursor.setCharFormat(textFormatForStyle(style));
                }
            }

            column = runEnd;
        }
    }

    cursor.endEditBlock();
}

QTextCharFormat TerminalView::textFormatForStyle(const CellStyle &style) const
{
    QTextCharFormat format;
    const QColor foreground = foregroundForStyle(style);
    const QColor background = backgroundForStyle(style);

    format.setForeground(foreground);
    if (style.hasBackground || style.inverse)
        format.setBackground(background);
    format.setFontWeight(style.bold ? QFont::Bold : QFont::Normal);
    format.setFontItalic(style.italic);
    format.setFontUnderline(style.underline);
    return format;
}

QColor TerminalView::ansiColor(int index) const
{
    static const QColor basicColors[] = {
        QColor(0, 0, 0),
        QColor(205, 49, 49),
        QColor(13, 188, 121),
        QColor(229, 229, 16),
        QColor(36, 114, 200),
        QColor(188, 63, 188),
        QColor(17, 168, 205),
        QColor(229, 229, 229),
        QColor(102, 102, 102),
        QColor(241, 76, 76),
        QColor(35, 209, 139),
        QColor(245, 245, 67),
        QColor(59, 142, 234),
        QColor(214, 112, 214),
        QColor(41, 184, 219),
        QColor(255, 255, 255),
    };

    if (index >= 0 && index < int(sizeof(basicColors) / sizeof(basicColors[0])))
        return basicColors[index];

    if (index >= 16 && index <= 231) {
        int value = index - 16;
        const int blue = value % 6;
        value /= 6;
        const int green = value % 6;
        const int red = value / 6;
        auto component = [](int color) {
            return color == 0 ? 0 : 55 + color * 40;
        };
        return QColor(component(red), component(green), component(blue));
    }

    if (index >= 232 && index <= 255) {
        const int level = 8 + (index - 232) * 10;
        return QColor(level, level, level);
    }

    return QColor();
}

qreal TerminalView::terminalCellWidth() const
{
    QFontMetricsF fm(font());
    qreal charWidth = fm.horizontalAdvance(QLatin1Char(' '));
    if (charWidth <= 1.0)
        charWidth = fm.horizontalAdvance(QLatin1Char('0'));
    return qMax<qreal>(1.0, charWidth);
}

QColor TerminalView::foregroundForStyle(const CellStyle &style) const
{
    auto &tm = ThemeManager::instance();
    QColor foreground = style.hasForeground ? style.foreground : tm.color("output.foreground");
    QColor background = style.hasBackground ? style.background : tm.color("output.background");
    if (style.inverse)
        std::swap(foreground, background);
    return foreground;
}

QColor TerminalView::backgroundForStyle(const CellStyle &style) const
{
    auto &tm = ThemeManager::instance();
    QColor foreground = style.hasForeground ? style.foreground : tm.color("output.foreground");
    QColor background = style.hasBackground ? style.background : tm.color("output.background");
    if (style.inverse)
        std::swap(foreground, background);
    return background;
}

QStringList TerminalView::displayLines() const
{
    QStringList result;
    result.reserve(m_lines.size());
    for (const QString &line : m_lines)
        result << trimDisplayLine(line);

    if (!m_alternateScreen) {
        const int minSize = qMax(1, m_cursorRow + 1);
        while (result.size() > minSize && result.last().isEmpty())
            result.removeLast();
    }

    return result;
}

QString TerminalView::trimDisplayLine(QString line) const
{
    return lineLeftCells(line, displayLineLength(line));
}

int TerminalView::displayLineLength(const QString &line) const
{
    int length = qMin(cellCount(line), terminalColumns());
    while (length > 0) {
        const int index = cellToStringIndex(line, length - 1);
        if (index >= line.length() || line.at(index) != QLatin1Char(' '))
            break;
        --length;
    }
    return length;
}

void TerminalView::eraseInLine(int mode)
{
    ensureCursorLine();
    QString &line = m_lines[m_cursorRow];
    QVector<CellStyle> &styles = m_lineStyles[m_cursorRow];
    if (mode == 2) {
        line.clear();
        styles.clear();
        m_cursorColumn = 0;
    } else if (mode == 1) {
        int count = qMin(m_cursorColumn + 1, cellCount(line));
        if (count > 0) {
            const int end = cellToStringIndex(line, count);
            line.replace(0, end, QString(count, QLatin1Char(' ')));
            for (int i = 0; i < count && i < styles.size(); ++i)
                styles[i] = CellStyle();
        }
    } else {
        if (m_cursorColumn < cellCount(line)) {
            line.truncate(cellToStringIndex(line, m_cursorColumn));
            styles.resize(qMin(styles.size(), m_cursorColumn));
        }
    }
}

void TerminalView::eraseInDisplay(int mode)
{
    if (mode == 2 || mode == 3) {
        resetScreenBuffer();
        return;
    }

    if (mode == 0) {
        eraseInLine(0);
        while (m_lines.size() > m_cursorRow + 1) {
            m_lines.removeLast();
            if (!m_lineStyles.isEmpty())
                m_lineStyles.removeLast();
        }
    } else if (mode == 1) {
        for (int row = 0; row < m_cursorRow && row < m_lines.size(); ++row) {
            m_lines[row].clear();
            if (row < m_lineStyles.size())
                m_lineStyles[row].clear();
        }
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
    int columns = terminalColumns();
    int rows = terminalRows();

    if (columns == m_lastColumns && rows == m_lastRows)
        return;

    m_lastColumns = columns;
    m_lastRows = rows;
    emit terminalResized(columns, rows);
}
