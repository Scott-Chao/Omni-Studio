#include "codeeditor.h"
#include "completionmanager.h"
#include "languageutils.h"
#include "configmanager.h"
#include "settingsmanager.h"
#include <QPainter>
#include <QTextBlock>
#include <QKeyEvent>
#include <QSyntaxHighlighter>

// ============================================================
// LineNumberArea
// ============================================================

LineNumberArea::LineNumberArea(CodeEditor *editor)
    : QWidget(editor)
    , m_codeEditor(editor)
{
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(m_codeEditor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    m_codeEditor->lineNumberAreaPaintEvent(event);
}

// ============================================================
// CodeEditor
// ============================================================

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    const auto &cfg = ConfigManager::instance();
    auto &sm = SettingsManager::instance();

    // Dark code editor theme (with override support)
    setStyleSheet(QString(
        "QPlainTextEdit { background-color: %1; color: %2; "
        "selection-background-color: %3; }")
        .arg(sm.value("appearance.colors.editor.background", cfg.editorBackground().name()).toString())
        .arg(sm.value("appearance.colors.editor.foreground", cfg.editorForeground().name()).toString())
        .arg(sm.value("appearance.colors.editor.selection", cfg.editorSelection().name()).toString()));

    QString fontFamily = sm.value("editor.font.family", cfg.editorFontFamily()).toString();
    int fontSize = sm.value("editor.font.size", cfg.editorFontSize()).toInt();
    QFont font(fontFamily, fontSize);
    font.setStyleHint(QFont::Monospace);
    setFont(font);
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * m_indentWidth);

    // Cache paint-time colors to avoid QSettings reads on every repaint
    m_cachedLnBg = QColor(sm.value("appearance.colors.line_number.background", cfg.lineNumberBackground().name()).toString());
    m_cachedLnFg = QColor(sm.value("appearance.colors.line_number.foreground", cfg.lineNumberForeground().name()).toString());
    m_cachedCurrentLine = QColor(sm.value("appearance.colors.current_line.highlight", cfg.currentLineHighlight().name()).toString());

    setLineWrapMode(QPlainTextEdit::NoWrap);

    m_lineNumberArea = new LineNumberArea(this);
    m_completionManager = new CompletionManager(this);

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

void CodeEditor::setIndentWidth(int width)
{
    m_indentWidth = width;
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * m_indentWidth);
}

void CodeEditor::reloadColors()
{
    const auto &cfg = ConfigManager::instance();
    auto &sm = SettingsManager::instance();

    setStyleSheet(QString(
        "QPlainTextEdit { background-color: %1; color: %2; "
        "selection-background-color: %3; }")
        .arg(sm.value("appearance.colors.editor.background", cfg.editorBackground().name()).toString())
        .arg(sm.value("appearance.colors.editor.foreground", cfg.editorForeground().name()).toString())
        .arg(sm.value("appearance.colors.editor.selection", cfg.editorSelection().name()).toString()));

    m_cachedLnBg = QColor(sm.value("appearance.colors.line_number.background", cfg.lineNumberBackground().name()).toString());
    m_cachedLnFg = QColor(sm.value("appearance.colors.line_number.foreground", cfg.lineNumberForeground().name()).toString());
    m_cachedCurrentLine = QColor(sm.value("appearance.colors.current_line.highlight", cfg.currentLineHighlight().name()).toString());

    highlightCurrentLine();
    m_lineNumberArea->update();
}

void CodeEditor::setLanguage(const QString &langId)
{
    if (m_highlighter) {
        delete m_highlighter;
        m_highlighter = nullptr;
    }
    m_languageId = langId;
    m_highlighter = LanguageUtils::createHighlighter(langId, document());
    m_completionManager->setLanguage(langId);
}

// ---- Line number area ----

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(
        QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::refreshLineNumberArea()
{
    updateLineNumberAreaWidth(0);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(
        QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
    m_lineNumberArea->update();
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    const auto &cfg = ConfigManager::instance();
    QPainter painter(m_lineNumberArea);
    painter.setFont(font());
    painter.fillRect(event->rect(), m_cachedLnBg);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(m_cachedLnFg);
            painter.drawText(0, top, m_lineNumberArea->width() - cfg.editorLineNumberRightPadding(),
                             fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

// ---- Current line highlight ----

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections = m_searchHighlights;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(m_cachedCurrentLine);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

// ---- Key handling ----

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    // Ctrl+/ — toggle line comment
    if (event->key() == Qt::Key_Slash && (event->modifiers() & Qt::ControlModifier)) {
        handleToggleComment();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        handleAutoIndent();
        return;

    case Qt::Key_Tab:
        if (handleTabKey(event))
            return;
        break;

    case Qt::Key_Backspace:
        if (handleBackspaceIndent(event))
            return;
        if (handleBackspacePairRemoval(event))
            return;
        break;

    default:
        break;
    }

    // Bracket completion: opening characters
    QString text = event->text();
    if (!text.isEmpty() && !event->matches(QKeySequence::Paste)) {
        if (text == QStringLiteral("{") || text == QStringLiteral("(") ||
            text == QStringLiteral("[") || text == QStringLiteral("\"") ||
            text == QStringLiteral("'"))
        {
            if (handleBracketCompletion(event))
                return;
        }
        // Closing bracket skip-over
        if (text == QStringLiteral("}") || text == QStringLiteral(")") ||
            text == QStringLiteral("]") || text == QStringLiteral("\"") ||
            text == QStringLiteral("'"))
        {
            if (handleClosingBracketSkip(event))
                return;
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}

// ---- Auto-indent ----

void CodeEditor::handleAutoIndent()
{
    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    QString blockText = block.text();
    int posInBlock = cursor.positionInBlock();

    // Extract leading whitespace from current line
    int i = 0;
    while (i < blockText.length() && (blockText.at(i) == QLatin1Char(' ') ||
                                      blockText.at(i) == QLatin1Char('\t'))) {
        ++i;
    }
    QString indent = blockText.left(i);

    // Split {} into three lines when cursor is between them
    if (posInBlock > 0 && posInBlock < blockText.length() &&
        blockText.at(posInBlock - 1) == QLatin1Char('{') &&
        blockText.at(posInBlock) == QLatin1Char('}'))
    {
        QString innerIndent = indent + indentString();
        cursor.beginEditBlock();
        cursor.insertText(QStringLiteral("\n") + innerIndent + QStringLiteral("\n") + indent);
        // Move cursor back to the middle line, at the end of innerIndent
        for (int k = 0; k < indent.length() + 1; ++k)
            cursor.movePosition(QTextCursor::Left);
        cursor.endEditBlock();
        setTextCursor(cursor);
        return;
    }

    // Only add one more indent level if the text before the cursor ends with
    // { (C-style) or : (Python), not just anywhere on the line.
    QString trimmedBefore = blockText.left(posInBlock).trimmed();
    bool pythonColon = (m_languageId == QStringLiteral("python") && trimmedBefore.endsWith(QLatin1Char(':')));
    if (trimmedBefore.endsWith(QLatin1Char('{')) || pythonColon) {
        indent += indentString();
    }
    // If the cursor is before the current indent text, use cursor position for indent
    if (posInBlock < i) {
        indent = blockText.left(posInBlock);
    }

    cursor.beginEditBlock();
    cursor.insertText(QStringLiteral("\n") + indent);
    cursor.endEditBlock();
    setTextCursor(cursor);
}

// ---- Bracket completion ----

bool CodeEditor::handleBracketCompletion(QKeyEvent *event)
{
    if (isCursorInStringOrComment())
        return false;

    QString opening = event->text();
    QChar openChar = opening.at(0);
    QChar closeChar;
    if (openChar == QLatin1Char('{'))      closeChar = QLatin1Char('}');
    else if (openChar == QLatin1Char('(')) closeChar = QLatin1Char(')');
    else if (openChar == QLatin1Char('[')) closeChar = QLatin1Char(']');
    else if (openChar == QLatin1Char('"')) closeChar = QLatin1Char('"');
    else if (openChar == QLatin1Char('\'')) closeChar = QLatin1Char('\'');
    else return false;

    QTextCursor cursor = textCursor();

    if (cursor.hasSelection()) {
        // Wrap selection
        QString sel = cursor.selectedText();
        cursor.beginEditBlock();
        int start = cursor.selectionStart();
        int end = cursor.selectionEnd();
        cursor.clearSelection();
        cursor.setPosition(start);
        cursor.insertText(opening);
        cursor.setPosition(end + 1);
        cursor.insertText(QString(closeChar));
        cursor.endEditBlock();
    } else {
        cursor.beginEditBlock();
        cursor.insertText(opening + QString(closeChar));
        cursor.movePosition(QTextCursor::PreviousCharacter);
        cursor.endEditBlock();
        setTextCursor(cursor);
    }
    return true;
}

// ---- Closing bracket skip ----

bool CodeEditor::handleClosingBracketSkip(QKeyEvent *event)
{
    QString closing = event->text();
    QTextCursor cursor = textCursor();

    if (cursor.hasSelection())
        return false;

    if (!cursor.atBlockEnd()) {
        QChar nextChar = document()->characterAt(cursor.position());
        if (nextChar == closing.at(0)) {
            cursor.movePosition(QTextCursor::Right);
            setTextCursor(cursor);
            return true;
        }
    }
    return false;
}

// ---- Backspace pair removal ----

bool CodeEditor::handleBackspaceIndent(QKeyEvent *event)
{
    Q_UNUSED(event);
    QTextCursor cursor = textCursor();

    if (cursor.hasSelection())
        return false;

    int pos = cursor.position();
    if (pos == 0)
        return false;

    QTextBlock block = cursor.block();
    int posInBlock = cursor.positionInBlock();
    if (posInBlock == 0)
        return false;

    // Only in leading whitespace
    QString textBeforeCursor = block.text().left(posInBlock);
    if (!textBeforeCursor.trimmed().isEmpty())
        return false;

    if (block.text().at(posInBlock - 1) == QLatin1Char('\t')) {
        cursor.deletePreviousChar();
        return true;
    }

    // Delete spaces back to previous tab stop
    int spaceCount = posInBlock % m_indentWidth;
    if (spaceCount == 0)
        spaceCount = m_indentWidth;

    cursor.beginEditBlock();
    for (int j = 0; j < spaceCount; ++j)
        cursor.deletePreviousChar();
    cursor.endEditBlock();
    return true;
}

bool CodeEditor::handleBackspacePairRemoval(QKeyEvent *event)
{
    Q_UNUSED(event);
    QTextCursor cursor = textCursor();

    if (cursor.hasSelection())
        return false;

    int pos = cursor.position();
    if (pos == 0)
        return false;

    QChar leftChar = document()->characterAt(pos - 1);
    QChar expectedRight;

    if (leftChar == QLatin1Char('{'))      expectedRight = QLatin1Char('}');
    else if (leftChar == QLatin1Char('(')) expectedRight = QLatin1Char(')');
    else if (leftChar == QLatin1Char('[')) expectedRight = QLatin1Char(']');
    else if (leftChar == QLatin1Char('"')) expectedRight = QLatin1Char('"');
    else if (leftChar == QLatin1Char('\'')) expectedRight = QLatin1Char('\'');
    else return false;

    if (pos < document()->characterCount() - 1) {
        QChar rightChar = document()->characterAt(pos);
        if (rightChar == expectedRight) {
            cursor.beginEditBlock();
            cursor.deletePreviousChar();
            cursor.deleteChar();
            cursor.endEditBlock();
            return true;
        }
    }
    return false;
}

// ---- Tab handling ----

bool CodeEditor::handleTabKey(QKeyEvent *event)
{
    Q_UNUSED(event);

    QTextCursor cursor = textCursor();

    if (cursor.hasSelection()) {
        // Indent selected lines
        QTextDocument *doc = document();
        int startBlock = doc->findBlock(cursor.selectionStart()).blockNumber();
        int endBlock = doc->findBlock(cursor.selectionEnd()).blockNumber();

        cursor.beginEditBlock();
        for (int i = startBlock; i <= endBlock; ++i) {
            QTextBlock block = doc->findBlockByNumber(i);
            QTextCursor blockCursor(block);
            blockCursor.insertText(indentString());
        }
        cursor.endEditBlock();
    } else {
        cursor.insertText(indentString());
    }
    return true;
}

// ---- Toggle comment ----

QString CodeEditor::commentPrefix() const
{
    if (m_languageId == QStringLiteral("python"))
        return QStringLiteral("#");
    return QStringLiteral("//");
}

void CodeEditor::handleToggleComment()
{
    QTextCursor cursor = textCursor();
    QTextDocument *doc = document();

    int startBlock = doc->findBlock(cursor.selectionStart()).blockNumber();
    int endBlock = doc->findBlock(cursor.selectionEnd()).blockNumber();

    // When the selection ends exactly at column 0 of a subsequent block,
    // exclude that trailing unselected line.
    if (cursor.selectionEnd() > cursor.selectionStart() &&
        doc->findBlock(cursor.selectionEnd()).position() == cursor.selectionEnd() &&
        startBlock != endBlock)
    {
        --endBlock;
    }

    QString prefix = commentPrefix();

    // Check whether all non-blank lines are already commented (prefix at col 0)
    bool allCommented = true;
    for (int i = startBlock; i <= endBlock; ++i) {
        QTextBlock block = doc->findBlockByNumber(i);
        if (block.text().trimmed().isEmpty())
            continue;
        if (!block.text().startsWith(prefix)) {
            allCommented = false;
            break;
        }
    }

    // ----- Save original selection as (blockNumber, offsetInBlock) -----
    QTextBlock origStartBlock = doc->findBlock(cursor.selectionStart());
    QTextBlock origEndBlock = doc->findBlock(cursor.selectionEnd());
    int origStartBlockNum = origStartBlock.blockNumber();
    int origEndBlockNum = origEndBlock.blockNumber();
    int origStartOffset = cursor.selectionStart() - origStartBlock.position();
    int origEndOffset = cursor.selectionEnd() - origEndBlock.position();

    // ----- Pre-compute per-block text shift -----
    // comment  → shift = +prefix+space  (added at column 0)
    // uncomment → shift = -(prefix+space) or -(prefix) removed from column 0
    QMap<int, int> shiftMap;
    for (int i = startBlock; i <= endBlock; ++i) {
        QTextBlock block = doc->findBlockByNumber(i);
        QString text = block.text();
        if (text.trimmed().isEmpty())
            continue;

        int shift = 0;
        if (allCommented) {
            if (text.startsWith(prefix)) {
                int removeLen = prefix.length();
                if (removeLen < text.length() && text.at(removeLen) == QLatin1Char(' '))
                    ++removeLen;
                shift = -removeLen;
            }
        } else {
            shift = prefix.length() + 1;
        }
        if (shift != 0)
            shiftMap[i] = shift;
    }

    // ----- Apply edits -----
    cursor.beginEditBlock();

    for (int i = startBlock; i <= endBlock; ++i) {
        if (!shiftMap.contains(i))
            continue;
        QTextBlock block = doc->findBlockByNumber(i);
        QTextCursor lineCursor(block);

        if (allCommented) {
            // Remove prefix (and optional trailing space) from column 0
            QString text = block.text();
            int idx = text.indexOf(prefix, 0);
            if (idx < 0)
                continue;
            int removeLen = prefix.length();
            if (idx + removeLen < text.length() && text.at(idx + removeLen) == QLatin1Char(' '))
                ++removeLen;
            lineCursor.setPosition(block.position() + idx);
            lineCursor.setPosition(block.position() + idx + removeLen, QTextCursor::KeepAnchor);
            lineCursor.removeSelectedText();
        } else {
            // Insert prefix + space at column 0
            lineCursor.setPosition(block.position());
            lineCursor.insertText(prefix + QStringLiteral(" "));
        }
    }

    cursor.endEditBlock();

    // ----- Restore selection (adjust offsets for modified blocks) -----
    QTextCursor newCursor = textCursor();
    QTextBlock newStartBlock = doc->findBlockByNumber(origStartBlockNum);
    QTextBlock newEndBlock = doc->findBlockByNumber(origEndBlockNum);

    int newStartOff = shiftMap.contains(origStartBlockNum)
        ? qMax(0, origStartOffset + shiftMap.value(origStartBlockNum))
        : origStartOffset;
    int newEndOff = shiftMap.contains(origEndBlockNum)
        ? qMax(0, origEndOffset + shiftMap.value(origEndBlockNum))
        : origEndOffset;

    int newStart = newStartBlock.position() + qMin(newStartOff, newStartBlock.length() - 1);
    int newEnd   = newEndBlock.position()   + qMin(newEndOff,   newEndBlock.length() - 1);

    newCursor.setPosition(newStart);
    newCursor.setPosition(newEnd, QTextCursor::KeepAnchor);
    setTextCursor(newCursor);
}

// ---- Helpers ----

QString CodeEditor::indentString() const
{
    return QString(m_indentWidth, QLatin1Char(' '));
}

bool CodeEditor::isCursorInStringOrComment() const
{
    if (!m_highlighter)
        return false;

    QTextCursor cursor = textCursor();
    int pos = cursor.position();
    if (pos <= 0)
        return false;

    // Check the format at the cursor position.
    // If the cursor is at the end, check the character just before.
    int checkPos = (pos == document()->characterCount() - 1) ? pos - 1 : pos;
    if (checkPos < 0)
        return false;

    QTextBlock block = document()->findBlock(checkPos);
    // Use previousBlockState + highlightBlock to get format
    // Instead, check the actual format list at this position
    const auto &cfg = ConfigManager::instance();
    QVector<QTextLayout::FormatRange> formats = block.layout()->formats();
    int offsetInBlock = checkPos - block.position();
    for (const auto &fmt : formats) {
        if (offsetInBlock >= fmt.start && offsetInBlock < fmt.start + fmt.length) {
            QColor fg = fmt.format.foreground().color();
            if (fg == cfg.syntaxComments() || fg == cfg.syntaxStrings())
                return true;
            break;
        }
    }
    return false;
}

void CodeEditor::setSearchHighlights(const QString &searchText)
{
    m_searchHighlights.clear();
    QTextDocument *doc = document();

    QTextCursor searchCursor(doc);
    while (true) {
        QTextCursor found = doc->find(searchText, searchCursor);
        if (found.isNull())
            break;

        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(ConfigManager::instance().searchHighlightBackground());
        sel.format.setForeground(ConfigManager::instance().searchHighlightForeground());
        sel.cursor = found;
        m_searchHighlights.append(sel);

        searchCursor = found;
        searchCursor.movePosition(QTextCursor::EndOfWord);
    }

    // Merge with current line highlight
    highlightCurrentLine();
}

void CodeEditor::clearSearchHighlights()
{
    m_searchHighlights.clear();
    highlightCurrentLine();
}

void CodeEditor::clearCurrentLineHighlight()
{
    setExtraSelections(m_searchHighlights);
}

void CodeEditor::refreshCurrentLineHighlight()
{
    highlightCurrentLine();
}
