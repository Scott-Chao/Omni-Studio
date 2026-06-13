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
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>

namespace {

bool isPureModifierKey(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
    case Qt::Key_AltGr:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
        return true;
    default:
        return false;
    }
}

int mouseButtonCode(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton:   return 0;
    case Qt::MiddleButton: return 1;
    case Qt::RightButton:  return 2;
    default:               return 3;
    }
}

} // namespace

TerminalView::TerminalView(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("terminalView"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(false);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    m_lines << QString();

    const QString configuredFamily = ConfigManager::instance().outputPanelFontFamily();
    const QStringList families = QFontDatabase::families();
    QString fontFamily = configuredFamily;
    const QStringList terminalFonts = {
        QStringLiteral("Cascadia Mono"),
        QStringLiteral("Cascadia Code"),
        QStringLiteral("Consolas"),
        QStringLiteral("Courier New")
    };
    for (const QString &candidate : terminalFonts) {
        if (families.contains(candidate)) {
            fontFamily = candidate;
            break;
        }
    }

    QFont mono(fontFamily, ConfigManager::instance().outputPanelFontSize());
    mono.setStyleHint(QFont::Monospace);
    mono.setFixedPitch(true);
    mono.setKerning(false);
    setFont(mono);

    m_scrollBar = new QScrollBar(Qt::Vertical, this);
    m_scrollBar->setSingleStep(1);
    m_scrollBar->setVisible(false);
    connect(m_scrollBar, &QScrollBar::valueChanged,
            this, qOverload<>(&QWidget::update));

    auto &tm = ThemeManager::instance();
    setStyleSheet(QStringLiteral(
        "TerminalView#terminalView {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "}"
        "TerminalView#terminalView QScrollBar:vertical {"
        "  background: %1;"
        "}")
        .arg(tm.color("output.background").name(),
             tm.color("output.foreground").name()));

    updateMetrics();
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
        renderBuffer();
    });
    connect(m_session, &TerminalSession::exited, this, [this](int exitCode) {
        insertTerminalText(QStringLiteral("\r\n[terminal exited: %1]\r\n").arg(exitCode));
        renderBuffer();
    });
}

void TerminalView::appendTerminalData(const QByteArray &data)
{
    const QString text = m_decoder.decode(data);
    for (QChar ch : text)
        handleChar(ch);
    renderBuffer();
}

int TerminalView::contentWidth() const
{
    return qMax(1, width() - m_scrollBar->sizeHint().width());
}

int TerminalView::contentHeight() const
{
    return qMax(1, height());
}

int TerminalView::terminalColumns() const
{
    const int availableWidth = qMax(1, contentWidth());
    return qMax(1, availableWidth / m_cellWidth);
}

int TerminalView::terminalRows() const
{
    const int availableHeight = qMax(1, contentHeight());
    return qMax(1, availableHeight / m_cellHeight);
}

void TerminalView::syncTerminalSize()
{
    updateMetrics();
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
    } else {
        ensureCursorLine();
        ensureScreenBuffer();
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

QVariant TerminalView::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:         return true;
    case Qt::ImReadOnly:        return false;
    case Qt::ImCursorRectangle: return cursorRect();
    case Qt::ImSurroundingText: return QString();
    case Qt::ImCurrentSelection:return selectedText();
    default:                    return QWidget::inputMethodQuery(query);
    }
}

bool TerminalView::event(QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride
        || event->type() == QEvent::KeyPress
        || event->type() == QEvent::KeyRelease) {
        auto *keyEvent = static_cast<QKeyEvent*>(event);
        if (isPureModifierKey(keyEvent)) {
            event->ignore();
            return false;
        }
    }
    return QWidget::event(event);
}

void TerminalView::inputMethodEvent(QInputMethodEvent *event)
{
    const QString commit = event->commitString();
    if (!commit.isEmpty())
        sendText(commit);
    event->accept();
}

void TerminalView::keyPressEvent(QKeyEvent *event)
{
    if (isPureModifierKey(event)) {
        event->ignore();
        return;
    }

    const bool ctrlCShortcut =
        event->key() == Qt::Key_C
        && (event->modifiers() & Qt::ControlModifier)
        && !(event->modifiers() & Qt::AltModifier)
        && !(event->modifiers() & Qt::MetaModifier);
    if (ctrlCShortcut && hasSelection()) {
        copySelection();
        return;
    }

    const bool pasteShortcut =
        (event->key() == Qt::Key_V && (event->modifiers() & Qt::ControlModifier))
        || event->matches(QKeySequence::Paste);
    if (pasteShortcut) {
        pasteClipboard();
        return;
    }

    QByteArray sequence = keySequenceForEvent(event);
    if (!sequence.isEmpty()) {
        clearSelection();
        emit inputGenerated(sequence);
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            scheduleTerminalSizeSync();
        return;
    }

    event->ignore();
}

void TerminalView::keyReleaseEvent(QKeyEvent *event)
{
    if (isPureModifierKey(event)) {
        event->ignore();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void TerminalView::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    const QPoint gridPos = gridPositionFromPoint(event->pos());
    if (m_mouseTracking || m_mouseButtonTracking || m_mouseAnyTracking) {
        sendMouseEvent(mouseButtonCode(event->button()), gridPos);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_mouseSelecting = true;
        m_selectionAnchor = gridPos;
        m_selectionEnd = gridPos;
        m_hasSelection = false;
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void TerminalView::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint gridPos = gridPositionFromPoint(event->pos());
    if (m_mouseAnyTracking || (m_mouseButtonTracking && event->buttons())) {
        Qt::MouseButton btn = event->buttons() & Qt::LeftButton   ? Qt::LeftButton
                            : event->buttons() & Qt::MiddleButton ? Qt::MiddleButton
                            : event->buttons() & Qt::RightButton  ? Qt::RightButton
                            : Qt::NoButton;
        sendMouseEvent(32 + mouseButtonCode(btn), gridPos);
        event->accept();
        return;
    }

    if (m_mouseSelecting) {
        m_selectionEnd = gridPos;
        m_hasSelection = m_selectionAnchor != m_selectionEnd;
        update();
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void TerminalView::mouseReleaseEvent(QMouseEvent *event)
{
    const QPoint gridPos = gridPositionFromPoint(event->pos());
    if (m_mouseTracking || m_mouseButtonTracking || m_mouseAnyTracking) {
        if (m_sgrMouseMode)
            emit inputGenerated(QStringLiteral("\x1b[<3;%1;%2m")
                                    .arg(gridPos.x() + 1)
                                    .arg(gridPos.y() - firstVisibleRow() + 1)
                                    .toUtf8());
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_mouseSelecting) {
        m_mouseSelecting = false;
        m_selectionEnd = gridPos;
        m_hasSelection = m_selectionAnchor != m_selectionEnd;
        update();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void TerminalView::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta != 0)
        m_scrollBar->setValue(m_scrollBar->value() - delta / 120 * m_scrollBar->singleStep());
    event->accept();
}

void TerminalView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Scrollbar always sits at the right edge; contentWidth() always
    // reserves space for it regardless of visibility.
    const int scrollW = m_scrollBar->sizeHint().width();
    m_scrollBar->setGeometry(width() - scrollW, 0, scrollW, height());
    updateMetrics();
    emitSizeIfChanged();
    QTimer::singleShot(0, this, &TerminalView::syncTerminalSize);
}

void TerminalView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(hasSelection());
    QAction *pasteAction = menu.addAction(tr("Paste"));
    QAction *selected = menu.exec(event->globalPos());
    if (selected == copyAction)
        copySelection();
    else if (selected == pasteAction)
        pasteClipboard();
}

void TerminalView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    paintTerminal(painter);
}

// ─── terminal emulation (unchanged from QAbstractScrollArea version) ────────

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
        if (m_cursorRow >= rows - 1) {
            scrollUpOneLine();
        } else {
            ++m_cursorRow;
        }
        m_cursorColumn = 0;
        ensureCursorLine();
        trimScrollback();
        return;
    }

    const int width = cellDisplayWidth(cell);
    const int columns = terminalColumns();
    if (m_cursorColumn >= columns) {
        if (m_cursorRow >= terminalRows() - 1) {
            scrollUpOneLine();
        } else {
            ++m_cursorRow;
        }
        m_cursorColumn = 0;
        ensureCursorLine();
    }

    const int writeRow = absoluteCursorRow();
    QString &writeLine = m_lines[writeRow];
    QVector<CellStyle> &writeStyles = m_lineStyles[writeRow];
    const int currentCells = cellCount(writeLine);
    if (currentCells < m_cursorColumn)
        writeLine += QString(m_cursorColumn - currentCells, QLatin1Char(' '));
    while (writeStyles.size() < m_cursorColumn)
        writeStyles.append(CellStyle());
    const int stringIndex = cellToStringIndex(writeLine, m_cursorColumn);
    if (m_cursorColumn < cellCount(writeLine)) {
        const int replaceLength = cellCharLengthAt(writeLine, stringIndex);
        const int replacedWidth = cellDisplayWidthAt(writeLine, stringIndex);
        writeLine.replace(stringIndex, replaceLength, cell);
        if (m_cursorColumn < writeStyles.size()) {
            const int removeCount = qMin(replacedWidth, writeStyles.size() - m_cursorColumn);
            writeStyles.remove(m_cursorColumn, removeCount);
        }
        for (int i = 0; i < width; ++i)
            writeStyles.insert(qMin(m_cursorColumn + i, writeStyles.size()), m_currentStyle);
    } else {
        writeLine += cell;
        for (int i = 0; i < width; ++i)
            writeStyles.append(m_currentStyle);
    }
    m_cursorColumn += width;
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
            if (m_pendingCarriageReturn)
                m_pendingCarriageReturn = false;
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
        } else if (ch == QLatin1Char('M')) {
            if (m_cursorRow > 0)
                --m_cursorRow;
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
    case 'J': eraseInDisplay(values.isEmpty() ? 0 : values.first()); break;
    case 'K': eraseInLine(values.isEmpty() ? 0 : values.first());   break;
    case 'H': case 'f':
        m_cursorRow = qMax(0, (values.size() >= 1 ? values[0] : 1) - 1);
        m_cursorColumn = qMax(0, (values.size() >= 2 ? values[1] : 1) - 1);
        ensureCursorLine();
        break;
    case 'A': m_cursorRow = qMax(0, m_cursorRow - csiParamOrDefault(params, 1)); break;
    case 'B': m_cursorRow += csiParamOrDefault(params, 1); ensureCursorLine();    break;
    case 'C': m_cursorColumn += csiParamOrDefault(params, 1);                      break;
    case 'D': m_cursorColumn = qMax(0, m_cursorColumn - csiParamOrDefault(params, 1)); break;
    case 'G': m_cursorColumn = qMax(0, csiParamOrDefault(params, 1) - 1);         break;
    case 'P': {
        ensureCursorLine();
        const int row = absoluteCursorRow();
        QString &line = m_lines[row];
        QVector<CellStyle> &styles = m_lineStyles[row];
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
        const int row = absoluteCursorRow();
        QString &line = m_lines[row];
        QVector<CellStyle> &styles = m_lineStyles[row];
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
        const int row = absoluteCursorRow();
        QString &line = m_lines[row];
        QVector<CellStyle> &styles = m_lineStyles[row];
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
    case 'E': m_cursorRow += csiParamOrDefault(params, 1); m_cursorColumn = 0; ensureCursorLine(); break;
    case 'F': m_cursorRow = qMax(0, m_cursorRow - csiParamOrDefault(params, 1)); m_cursorColumn = 0; break;
    case 'd': m_cursorRow = qMax(0, csiParamOrDefault(params, 1) - 1); ensureCursorLine(); break;
    case 'e': m_cursorRow += csiParamOrDefault(params, 1); ensureCursorLine(); break;
    case 'L': {
        int count = csiParamOrDefault(params, 1);
        ensureCursorLine();
        const int row = absoluteCursorRow();
        const int screenBottom = screenTopRow() + terminalRows();
        for (int i = 0; i < count; ++i) {
            m_lines.insert(row, QString());
            m_lineStyles.insert(row, QVector<CellStyle>());
            if (m_lines.size() > screenBottom) {
                m_lines.removeAt(screenBottom);
                if (screenBottom < m_lineStyles.size())
                    m_lineStyles.removeAt(screenBottom);
            }
        }
        break;
    }
    case 'M': {
        int count = csiParamOrDefault(params, 1);
        ensureCursorLine();
        const int row = absoluteCursorRow();
        const int screenBottom = screenTopRow() + terminalRows();
        while (count-- > 0 && row < screenBottom && row < m_lines.size()) {
            m_lines.removeAt(row);
            if (row < m_lineStyles.size())
                m_lineStyles.removeAt(row);
            m_lines.insert(qMin(screenBottom - 1, m_lines.size()), QString());
            m_lineStyles.insert(qMin(screenBottom - 1, m_lineStyles.size()), QVector<CellStyle>());
        }
        ensureCursorLine();
        break;
    }
    case 's': m_savedCursorRow = m_cursorRow; m_savedCursorColumn = m_cursorColumn; break;
    case 'u':
        m_cursorRow = m_savedCursorRow;
        m_cursorColumn = m_savedCursorColumn;
        ensureCursorLine();
        break;
    case 'm': handleSgr(params); break;
    case 'h':
        if (params.contains(QStringLiteral("1049"))
            || params.contains(QStringLiteral("1047"))
            || params.contains(QStringLiteral("47"))) {
            if (!m_alternateScreen) {
                m_savedNormalLines = m_lines;
                m_savedNormalLineStyles = m_lineStyles;
                m_savedNormalScreenTopRow = m_screenTopRow;
                m_savedNormalCursorRow = m_cursorRow;
                m_savedNormalCursorColumn = m_cursorColumn;
                m_hasSavedNormalScreen = true;
            }
            m_alternateScreen = true;
            m_scrollBar->setVisible(false);
            clearSelection();
            resetScreenBuffer();
            m_currentStyle = CellStyle();
            scheduleTerminalSizeSync();
        }
        if (params.contains(QStringLiteral("1000"))) m_mouseTracking = true;
        if (params.contains(QStringLiteral("1002"))) m_mouseButtonTracking = true;
        if (params.contains(QStringLiteral("1003"))) m_mouseAnyTracking = true;
        if (params.contains(QStringLiteral("1006"))) m_sgrMouseMode = true;
        break;
    case 'l':
        if (params.contains(QStringLiteral("1049"))
            || params.contains(QStringLiteral("1047"))
            || params.contains(QStringLiteral("47"))) {
            m_alternateScreen = false;
            clearSelection();
            if (m_hasSavedNormalScreen) {
                m_lines = m_savedNormalLines;
                m_lineStyles = m_savedNormalLineStyles;
                m_screenTopRow = m_savedNormalScreenTopRow;
                m_cursorRow = m_savedNormalCursorRow;
                m_cursorColumn = m_savedNormalCursorColumn;
                m_savedNormalLines.clear();
                m_savedNormalLineStyles.clear();
                m_hasSavedNormalScreen = false;
                ensureCursorLine();
            } else {
                resetScreenBuffer();
            }
            m_currentStyle = CellStyle();
            scheduleTerminalSizeSync();
        }
        if (params.contains(QStringLiteral("1000"))) m_mouseTracking = false;
        if (params.contains(QStringLiteral("1002"))) m_mouseButtonTracking = false;
        if (params.contains(QStringLiteral("1003"))) m_mouseAnyTracking = false;
        if (params.contains(QStringLiteral("1006"))) m_sgrMouseMode = false;
        break;
    case '?': break;
    default: break;
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
    if (values.isEmpty()) return defaultValue;
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
    if (cleaned.isEmpty()) return values;

    const QStringList parts = cleaned.split(QLatin1Char(';'));
    for (const QString &part : parts) {
        if (part.isEmpty()) { values << 0; continue; }
        bool ok = false;
        int value = part.toInt(&ok);
        values << (ok ? value : 0);
    }
    return values;
}

QList<int> TerminalView::sgrParams(const QString &params) const
{
    if (params.isEmpty()) return {0};
    QString cleaned = params;
    cleaned.replace(QLatin1Char(':'), QLatin1Char(';'));
    return csiParams(cleaned);
}

void TerminalView::ensureCursorLine()
{
    if (m_lines.isEmpty())
        m_lines << QString();

    clampCursorToScreen();
    ensureScreenBuffer();
}

void TerminalView::ensureScreenBuffer()
{
    if (m_lines.isEmpty())
        m_lines << QString();

    const int requiredRows = screenTopRow() + terminalRows();
    while (m_lines.size() < requiredRows)
        m_lines << QString();

    ensureLineStorage();
}

void TerminalView::ensureLineStorage()
{
    while (m_lineStyles.size() < m_lines.size())
        m_lineStyles.append(QVector<CellStyle>());
    while (m_lineStyles.size() > m_lines.size())
        m_lineStyles.removeLast();
}

void TerminalView::clampCursorToScreen()
{
    m_cursorRow = qBound(0, m_cursorRow, qMax(0, terminalRows() - 1));
    m_cursorColumn = qMax(0, m_cursorColumn);
}

int TerminalView::screenTopRow() const
{
    return m_alternateScreen ? 0 : m_screenTopRow;
}

int TerminalView::absoluteCursorRow() const
{
    return screenTopRow() + m_cursorRow;
}

int TerminalView::cellToStringIndex(const QString &line, int cellColumn) const
{
    int index = 0, column = 0;
    while (index < line.length() && column < cellColumn) {
        const int width = cellDisplayWidthAt(line, index);
        if (column + width > cellColumn) break;
        index += cellCharLengthAt(line, index);
        column += width;
    }
    return index;
}

int TerminalView::cellCount(const QString &line) const
{
    int count = 0, index = 0;
    while (index < line.length()) {
        count += cellDisplayWidthAt(line, index);
        index += cellCharLengthAt(line, index);
    }
    return count;
}

int TerminalView::cellCharLengthAt(const QString &line, int stringIndex) const
{
    if (stringIndex + 1 < line.length()
        && line.at(stringIndex).isHighSurrogate()
        && line.at(stringIndex + 1).isLowSurrogate())
        return 2;
    return 1;
}

int TerminalView::cellDisplayWidth(const QString &cell) const
{
    if (cell.isEmpty()) return 0;
    const uint cp = cell.at(0).isHighSurrogate() && cell.size() > 1
        ? QChar::surrogateToUcs4(cell.at(0), cell.at(1))
        : cell.at(0).unicode();
    return isWideCodepoint(cp) ? 2 : 1;
}

int TerminalView::cellDisplayWidthAt(const QString &line, int stringIndex) const
{
    return cellDisplayWidth(line.mid(stringIndex, cellCharLengthAt(line, stringIndex)));
}

// East Asian Width — returns true for codepoints occupying 2 terminal columns.
bool TerminalView::isWideCodepoint(uint cp) const
{
    if (cp >= 0x0300 && cp <= 0x036f) return false;
    if (cp >= 0x1ab0 && cp <= 0x1aff) return false;
    if (cp >= 0x1dc0 && cp <= 0x1dff) return false;
    if (cp >= 0x20d0 && cp <= 0x20ff) return false;
    if (cp >= 0xfe00 && cp <= 0xfe0f) return false;

    if (cp >= 0x2500 && cp <= 0x259f) return false;
    if (cp >= 0x25a0 && cp <= 0x25ff) return false;
    if (cp >= 0x2800 && cp <= 0x28ff) return false;

    if (cp == 0x2329 || cp == 0x232a) return true;
    if (cp >= 0x2e80 && cp <= 0xa4cf && cp != 0x303f) return true;
    if (cp >= 0xac00 && cp <= 0xd7a3) return true;
    if (cp >= 0xf900 && cp <= 0xfaff) return true;
    if (cp >= 0xfe10 && cp <= 0xfe19) return true;
    if (cp >= 0xfe30 && cp <= 0xfe6f) return true;
    if (cp >= 0xff00 && cp <= 0xff60) return true;
    if (cp >= 0xffe0 && cp <= 0xffe6) return true;
    if (cp >= 0x20000 && cp <= 0x3fffd) return true;

    if (cp >= 0x1f300 && cp <= 0x1f64f) return true;
    if (cp >= 0x1f900 && cp <= 0x1f9ff) return true;
    if (cp >= 0x1fa70 && cp <= 0x1faff) return true;
    return false;
}

bool TerminalView::isContinuationCell(const QString &line, int cellColumn) const
{
    int index = 0, column = 0;
    while (index < line.length()) {
        const int width = cellDisplayWidthAt(line, index);
        if (cellColumn > column && cellColumn < column + width)
            return true;
        if (cellColumn == column)
            return false;
        column += width;
        index += cellCharLengthAt(line, index);
    }
    return false;
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

    const int excess = m_lines.size() - maxLines;
    const int removeCount = qMin(excess, m_screenTopRow);
    if (removeCount <= 0)
        return;

    m_lines.erase(m_lines.begin(), m_lines.begin() + removeCount);
    m_lineStyles.erase(m_lineStyles.begin(), m_lineStyles.begin() + qMin(removeCount, m_lineStyles.size()));
    m_screenTopRow = qMax(0, m_screenTopRow - removeCount);
    if (m_hasSelection) {
        m_selectionAnchor.ry() = qMax(0, m_selectionAnchor.y() - removeCount);
        m_selectionEnd.ry() = qMax(0, m_selectionEnd.y() - removeCount);
    }
}

void TerminalView::resetScreenBuffer()
{
    const int rows = terminalRows();
    m_lines.clear();
    m_lineStyles.clear();
    m_screenTopRow = 0;
    for (int i = 0; i < rows; ++i)
        m_lines << QString();
    ensureLineStorage();
    m_cursorRow = 0;
    m_cursorColumn = 0;
    m_savedCursorRow = 0;
    m_savedCursorColumn = 0;
    if (!m_alternateScreen)
        m_scrollBar->setValue(m_screenTopRow);
}

void TerminalView::scrollUpOneLine()
{
    const int rows = terminalRows();
    ensureLineStorage();

    if (m_alternateScreen) {
        if (!m_lines.isEmpty()) {
            m_lines.removeFirst();
            if (!m_lineStyles.isEmpty())
                m_lineStyles.removeFirst();
        }
        m_lines << QString();
        m_lineStyles.append(QVector<CellStyle>());
        m_cursorRow = qMax(0, rows - 1);
        return;
    }

    ++m_screenTopRow;
    const int requiredRows = m_screenTopRow + rows;
    while (m_lines.size() < requiredRows) {
        m_lines << QString();
        m_lineStyles.append(QVector<CellStyle>());
    }
    m_cursorRow = qMax(0, rows - 1);
    trimScrollback();
}

void TerminalView::renderBuffer()
{
    trimScrollback();
    updateScrollBar(true);
    update();
}

QColor TerminalView::ansiColor(int index) const
{
    static const QColor basic[] = {
        QColor(0,0,0),       QColor(205,49,49),   QColor(13,188,121),
        QColor(229,229,16),  QColor(36,114,200),  QColor(188,63,188),
        QColor(17,168,205),  QColor(229,229,229), QColor(102,102,102),
        QColor(241,76,76),   QColor(35,209,139),  QColor(245,245,67),
        QColor(59,142,234),  QColor(214,112,214), QColor(41,184,219),
        QColor(255,255,255),
    };
    if (index >= 0 && index < 16) return basic[index];

    if (index >= 16 && index <= 231) {
        int v = index - 16;
        const int b = v % 6; v /= 6;
        const int g = v % 6;
        const int r = v / 6;
        auto c = [](int x) { return x == 0 ? 0 : 55 + x * 40; };
        return QColor(c(r), c(g), c(b));
    }
    if (index >= 232 && index <= 255) {
        int lvl = 8 + (index - 232) * 10;
        return QColor(lvl, lvl, lvl);
    }
    return QColor();
}

int TerminalView::terminalCellWidth() const
{
    QFontMetrics fm(font());
    const int advance = fm.horizontalAdvance(QStringLiteral("AAAAAAAAAA"));
    return qMax(1, (advance + 5) / 10); // ceiling divide — avoids fractional-pixel drift
}

int TerminalView::terminalCellHeight() const
{
    QFontMetrics fm(font());
    return qMax(1, fm.lineSpacing());
}

QColor TerminalView::foregroundForStyle(const CellStyle &style) const
{
    auto &tm = ThemeManager::instance();
    QColor fg = style.hasForeground ? style.foreground : tm.color("output.foreground");
    QColor bg = style.hasBackground ? style.background : tm.color("output.background");
    if (style.inverse) std::swap(fg, bg);
    return fg;
}

QColor TerminalView::backgroundForStyle(const CellStyle &style) const
{
    auto &tm = ThemeManager::instance();
    QColor fg = style.hasForeground ? style.foreground : tm.color("output.foreground");
    QColor bg = style.hasBackground ? style.background : tm.color("output.background");
    if (style.inverse) std::swap(fg, bg);
    return bg;
}

int TerminalView::displayLineLength(const QString &line) const
{
    int length = qMin(cellCount(line), terminalColumns());
    while (length > 0) {
        const int idx = cellToStringIndex(line, length - 1);
        if (idx >= line.length() || line.at(idx) != QLatin1Char(' '))
            break;
        --length;
    }
    return length;
}

void TerminalView::eraseInLine(int mode)
{
    ensureCursorLine();
    const int row = absoluteCursorRow();
    QString &line = m_lines[row];
    QVector<CellStyle> &styles = m_lineStyles[row];
    if (mode == 2) {
        line.clear();
        styles.clear();
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
        if (m_alternateScreen || mode == 3) {
            resetScreenBuffer();
        } else {
            const int firstRow = screenTopRow();
            const int lastRow = screenTopRow() + terminalRows() - 1;
            for (int row = firstRow; row <= lastRow && row < m_lines.size(); ++row) {
                m_lines[row].clear();
                if (row < m_lineStyles.size())
                    m_lineStyles[row].clear();
            }
            m_cursorRow = 0;
            m_cursorColumn = 0;
        }
        m_currentStyle = CellStyle();
        return;
    }
    if (mode == 0) {
        eraseInLine(0);
        const int firstRow = absoluteCursorRow() + 1;
        const int lastRow = screenTopRow() + terminalRows() - 1;
        for (int row = firstRow; row <= lastRow && row < m_lines.size(); ++row) {
            m_lines[row].clear();
            if (row < m_lineStyles.size())
                m_lineStyles[row].clear();
        }
    } else if (mode == 1) {
        for (int row = screenTopRow(); row < absoluteCursorRow() && row < m_lines.size(); ++row) {
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
    if (text.isEmpty()) return;
    clearSelection();
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\r"));
    text.replace(QLatin1Char('\n'), QLatin1Char('\r'));
    sendText(text);
}

QByteArray TerminalView::keySequenceForEvent(QKeyEvent *event) const
{
    const Qt::KeyboardModifiers mods = event->modifiers();

    if (mods & Qt::ControlModifier) {
        const int key = event->key();
        if (key >= Qt::Key_A && key <= Qt::Key_Z)
            return QByteArray(1, char(key - Qt::Key_A + 1));
    }

    switch (event->key()) {
    case Qt::Key_Return:    case Qt::Key_Enter:     return QByteArray("\r", 1);
    case Qt::Key_Backspace:                         return QByteArray("\x7f", 1);
    case Qt::Key_Tab:                               return QByteArray("\t", 1);
    case Qt::Key_Escape:                            return QByteArray("\x1b", 1);
    case Qt::Key_Left:                              return QByteArray("\x1b[D", 3);
    case Qt::Key_Right:                             return QByteArray("\x1b[C", 3);
    case Qt::Key_Up:                                return QByteArray("\x1b[A", 3);
    case Qt::Key_Down:                              return QByteArray("\x1b[B", 3);
    case Qt::Key_Home:                              return QByteArray("\x1b[H", 3);
    case Qt::Key_End:                               return QByteArray("\x1b[F", 3);
    case Qt::Key_Delete:                            return QByteArray("\x1b[3~", 4);
    case Qt::Key_PageUp:                            return QByteArray("\x1b[5~", 4);
    case Qt::Key_PageDown:                          return QByteArray("\x1b[6~", 4);
    default: break;
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

void TerminalView::updateMetrics()
{
    m_cellWidth = terminalCellWidth();
    m_cellHeight = terminalCellHeight();
}

void TerminalView::updateScrollBar(bool followCursor)
{
    if (m_alternateScreen) {
        m_scrollBar->hide();
        return;
    }

    const int rows = terminalRows();
    const int maxValue = qMax(0, m_screenTopRow);
    const bool wasAtBottom = m_scrollBar->value() >= m_scrollBar->maximum();

    m_scrollBar->blockSignals(true);
    m_scrollBar->setRange(0, maxValue);
    m_scrollBar->setPageStep(rows);
    m_scrollBar->show();
    m_scrollBar->blockSignals(false);

    if (followCursor && wasAtBottom)
        m_scrollBar->setValue(maxValue);
}

int TerminalView::firstVisibleRow() const
{
    return m_alternateScreen ? 0 : m_scrollBar->value();
}

int TerminalView::bufferRowFromViewportY(int y) const
{
    return qBound(0, firstVisibleRow() + y / m_cellHeight, qMax(0, m_lines.size() - 1));
}

int TerminalView::columnFromViewportX(int x) const
{
    return qBound(0, x / m_cellWidth, qMax(0, terminalColumns() - 1));
}

QPoint TerminalView::gridPositionFromPoint(const QPoint &point) const
{
    return QPoint(columnFromViewportX(point.x()), bufferRowFromViewportY(point.y()));
}

void TerminalView::clearSelection()
{
    if (!m_hasSelection) return;
    m_hasSelection = false;
    update();
}

bool TerminalView::hasSelection() const
{
    return m_hasSelection && m_selectionAnchor != m_selectionEnd;
}

bool TerminalView::isCellSelected(int row, int column) const
{
    if (!hasSelection()) return false;

    QPoint start = m_selectionAnchor;
    QPoint end = m_selectionEnd;
    if (start.y() > end.y() || (start.y() == end.y() && start.x() > end.x()))
        std::swap(start, end);

    if (row < start.y() || row > end.y()) return false;
    if (start.y() == end.y()) return column >= start.x() && column < end.x();
    if (row == start.y()) return column >= start.x();
    if (row == end.y()) return column < end.x();
    return true;
}

QString TerminalView::selectedText() const
{
    if (!hasSelection()) return QString();

    QPoint start = m_selectionAnchor;
    QPoint end = m_selectionEnd;
    if (start.y() > end.y() || (start.y() == end.y() && start.x() > end.x()))
        std::swap(start, end);

    QStringList rows;
    for (int row = start.y(); row <= end.y(); ++row) {
        const QString line = m_lines.value(row);
        const int from = row == start.y() ? start.x() : 0;
        const int to = row == end.y() ? end.x() : terminalColumns();
        QString text = lineLeftCells(line, qMin(to, cellCount(line)))
                           .mid(cellToStringIndex(line, from));
        if (to > cellCount(line))
            text += QString(to - qMax(from, cellCount(line)), QLatin1Char(' '));
        rows << text;
    }
    return rows.join(QLatin1Char('\n'));
}

QString TerminalView::cellTextAt(int row, int column) const
{
    const QString line = m_lines.value(row);
    if (isContinuationCell(line, column))
        return QString();
    const int index = cellToStringIndex(line, column);
    if (index >= line.length())
        return QString(QLatin1Char(' '));
    return line.mid(index, cellCharLengthAt(line, index));
}

TerminalView::CellStyle TerminalView::cellStyleAt(int row, int column) const
{
    if (row >= 0 && row < m_lineStyles.size()) {
        const QVector<CellStyle> &styles = m_lineStyles[row];
        if (column >= 0 && column < styles.size())
            return styles[column];
    }
    return CellStyle();
}

QRect TerminalView::cellRect(int row, int column) const
{
    return QRect(column * m_cellWidth,
                 (row - firstVisibleRow()) * m_cellHeight,
                 m_cellWidth,
                 m_cellHeight);
}

QRect TerminalView::cursorRect() const
{
    return cellRect(absoluteCursorRow(), qBound(0, m_cursorColumn, terminalColumns() - 1));
}

void TerminalView::copySelection()
{
    const QString text = selectedText();
    if (!text.isEmpty())
        QApplication::clipboard()->setText(text);
}

void TerminalView::paintTerminal(QPainter &painter)
{
    painter.setFont(font());
    auto &tm = ThemeManager::instance();

    const QColor defaultBg = tm.color("output.background");
    const QColor selBg = tm.color("output.selectionBackground");
    const QRect contentRect(0, 0, contentWidth(), contentHeight());

    // 1. Background
    painter.fillRect(contentRect, defaultBg);

    const int firstRow = firstVisibleRow();
    const int rows = terminalRows();
    const int columns = terminalColumns();

    // 2. Per-cell backgrounds (selection + ANSI backgrounds)
    for (int screenRow = 0; screenRow < rows; ++screenRow) {
        const int row = firstRow + screenRow;
        if (row >= m_lines.size()) break;
        for (int col = 0; col < columns; ++col) {
            const CellStyle style = cellStyleAt(row, col);
            if (isCellSelected(row, col)) {
                painter.fillRect(cellRect(row, col), selBg);
            } else if (style.hasBackground || style.inverse) {
                painter.fillRect(cellRect(row, col), backgroundForStyle(style));
            }
        }
    }

    // 3. Text — draw every cell individually at its exact grid position.
    //    Single code path for selected and unselected cells guarantees
    //    pixel-identical spacing in both states.
    const int ascent = QFontMetrics(font()).ascent();
    for (int screenRow = 0; screenRow < rows; ++screenRow) {
        const int row = firstRow + screenRow;
        if (row >= m_lines.size()) break;

        const QString &line = m_lines[row];
        for (int col = 0; col < columns; ++col) {
            if (isContinuationCell(line, col))
                continue;

            const QString text = cellTextAt(row, col);
            if (text.isEmpty() || text == QLatin1String(" "))
                continue;

            const CellStyle style = cellStyleAt(row, col);
            QFont rf = font();
            rf.setBold(style.bold);
            rf.setItalic(style.italic);
            rf.setUnderline(style.underline);
            painter.setFont(rf);
            painter.setPen(foregroundForStyle(style));

            const QRect r = cellRect(row, col);
            painter.save();
            painter.setClipRect(r.adjusted(0, 0, m_cellWidth * qMax(1, cellDisplayWidth(text)) - 1, 0));
            painter.drawText(r.left(), r.top() + ascent, text);
            painter.restore();
        }
    }

    // 4. Cursor
    if (!hasSelection()) {
        const QRect r = cursorRect();
        if (contentRect.intersects(r)) {
            painter.fillRect(r.adjusted(0, 0, -1, -1), tm.color("output.foreground"));
            painter.setPen(tm.color("output.background"));
            const QString text = cellTextAt(absoluteCursorRow(), m_cursorColumn);
            if (!text.isEmpty()) {
                painter.save();
                painter.setClipRect(r.adjusted(0, 0, m_cellWidth * qMax(1, cellDisplayWidth(text)) - 1, 0));
                painter.drawText(r.left(), r.top() + ascent, text);
                painter.restore();
            }
        }
    }
}

void TerminalView::sendMouseEvent(int buttonCode, const QPoint &gridPos)
{
    const int x = gridPos.x() + 1;
    const int y = gridPos.y() - firstVisibleRow() + 1;
    if (m_sgrMouseMode) {
        emit inputGenerated(QStringLiteral("\x1b[<%1;%2;%3M")
                                .arg(buttonCode).arg(x).arg(y).toUtf8());
        return;
    }
    if (x <= 0 || y <= 0 || x > 223 || y > 223 || buttonCode > 223)
        return;
    QByteArray sequence("\x1b[M", 3);
    sequence.append(char(32 + buttonCode));
    sequence.append(char(32 + x));
    sequence.append(char(32 + y));
    emit inputGenerated(sequence);
}
