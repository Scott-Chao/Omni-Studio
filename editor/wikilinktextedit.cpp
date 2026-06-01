#include "wikilinktextedit.h"
#include "indenthelper.h"

#include <QKeyEvent>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QTextBlock>

// Scan backward from cursor position to find '#' at a word boundary.
// Returns position of '#' or -1 if not found.
static int findHashTag(const QString &blockText, int cursorPosInBlock)
{
    for (int i = cursorPosInBlock - 1; i >= 0; --i) {
        QChar c = blockText[i];
        if (c == QLatin1Char('#')) {
            if (i == 0 || blockText[i - 1].isSpace()
                || blockText[i - 1] == QLatin1Char('(')
                || blockText[i - 1] == QLatin1Char('[')
                || blockText[i - 1] == QLatin1Char(','))
            {
                // Ensure no space between # and cursor (tag must be contiguous)
                bool hasSpace = false;
                for (int j = i + 1; j < cursorPosInBlock; ++j) {
                    if (blockText[j].isSpace()) { hasSpace = true; break; }
                }
                if (!hasSpace)
                    return i;
            }
            return -1;
        }
        if (c.isSpace()) return -1;
    }
    return -1;
}

WikiLinkTextEdit::WikiLinkTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    m_model = new QStringListModel(this);
    m_tagModel = new QStringListModel(this);
    m_completer = new QCompleter(this);
    m_completer->setModel(m_model);
    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchStartsWith);

    QFontMetrics fm(font());
    setTabStopDistance(fm.horizontalAdvance(QLatin1Char(' ')) * m_indentWidth);

    connect(this, &QTextEdit::cursorPositionChanged,
            this, &WikiLinkTextEdit::updateCompleter);
}

void WikiLinkTextEdit::setFileNames(const QStringList &names)
{
    m_model->setStringList(names);
}

void WikiLinkTextEdit::setTagNames(const QStringList &names)
{
    m_tagModel->setStringList(names);
}

void WikiLinkTextEdit::keyPressEvent(QKeyEvent *event)
{
    bool completerVisible = m_completer && m_completer->popup()->isVisible();

    if (completerVisible) {
        switch (event->key()) {
        case Qt::Key_Tab:
        {
            QAbstractItemView *pv = m_completer->popup();
            QModelIndex idx = pv->currentIndex();
            if (!idx.isValid())
                idx = pv->model()->index(0, 0);
            if (idx.isValid()) {
                QString completion = idx.data().toString();
                if (m_inTagMode)
                    insertTagCompletion(completion);
                else
                    insertCompletion(completion);
            }
            pv->hide();
            return;
        }
        case Qt::Key_Escape:
            m_completer->popup()->hide();
            return;
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            event->ignore();
            return;
        case Qt::Key_Enter:
        case Qt::Key_Return:
            m_completer->popup()->hide();
            // fall through to auto-indent below
            break;
        default:
            break;
        }
    }

    // Tab → spaces-based indent (only when completer is not visible)
    if (event->key() == Qt::Key_Tab && !completerVisible) {
        handleTabKey();
        return;
    }

    // Backspace in leading whitespace → smart dedent
    if (event->key() == Qt::Key_Backspace && !textCursor().hasSelection()) {
        if (handleBackspaceIndent())
            return;
    }

    // Enter → auto-indent (preserve leading whitespace)
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && !completerVisible) {
        handleAutoIndentOnReturn();
        return;
    }

    QTextEdit::keyPressEvent(event);

    // cursorPositionChanged will fire updateCompleter automatically.
    // But arrow keys and similar non-text keys don't trigger it in all cases,
    // so also call directly for those.
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right
        || event->key() == Qt::Key_Up || event->key() == Qt::Key_Down
        || event->key() == Qt::Key_Home || event->key() == Qt::Key_End)
    {
        updateCompleter();
    }
}

void WikiLinkTextEdit::updateCompleter()
{
    if (!m_completer)
        return;

    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    QString blockText = block.text();
    int cursorPosInBlock = cursor.positionInBlock();

    // --- Check for #tag ---
    int hashPos = findHashTag(blockText, cursorPosInBlock);

    if (hashPos >= 0) {
        m_inTagMode = true;
        m_completer->setModel(m_tagModel);
        QString prefix = blockText.mid(hashPos + 1, cursorPosInBlock - hashPos - 1);
        m_completer->setCompletionPrefix(prefix);
        if (m_completer->completionCount() > 0) {
            QRect cr = cursorRect();
            cr.setWidth(m_completer->popup()->sizeHintForColumn(0)
                        + m_completer->popup()->verticalScrollBar()->sizeHint().width() + 20);
            m_completer->complete(cr);
            m_completer->popup()->setCurrentIndex(
                m_completer->completionModel()->index(0, 0));
        } else {
            m_completer->popup()->hide();
        }
        return;
    }

    // --- Check for [[wikilink (existing logic) ---
    m_inTagMode = false;
    m_completer->setModel(m_model);

    int openBracket = blockText.lastIndexOf(QStringLiteral("[["), cursorPosInBlock);
    if (openBracket < 0) {
        m_completer->popup()->hide();
        return;
    }

    int closeBracket = blockText.indexOf(QStringLiteral("]]"), openBracket + 2);
    if (closeBracket >= 0 && closeBracket < cursorPosInBlock) {
        m_completer->popup()->hide();
        return;
    }

    QString prefix = blockText.mid(openBracket + 2, cursorPosInBlock - openBracket - 2);
    m_completer->setCompletionPrefix(prefix);

    if (m_completer->completionCount() > 0) {
        QRect cr = cursorRect();
        cr.setWidth(m_completer->popup()->sizeHintForColumn(0)
                    + m_completer->popup()->verticalScrollBar()->sizeHint().width() + 20);
        m_completer->complete(cr);
        m_completer->popup()->setCurrentIndex(
            m_completer->completionModel()->index(0, 0));
    } else {
        m_completer->popup()->hide();
    }
}

void WikiLinkTextEdit::insertCompletion(const QString &completion)
{
    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    QString blockText = block.text();
    int cursorPosInBlock = cursor.positionInBlock();
    int blockStartPos = block.position();

    int openBracket = blockText.lastIndexOf(QStringLiteral("[["), cursorPosInBlock);
    if (openBracket < 0)
        return;

    int closeBracket = blockText.indexOf(QStringLiteral("]]"), openBracket + 2);
    if (closeBracket < 0)
        closeBracket = cursorPosInBlock;

    int selStart = blockStartPos + openBracket + 2;
    int selEnd = blockStartPos + closeBracket;

    cursor.setPosition(selStart);
    cursor.setPosition(selEnd, QTextCursor::KeepAnchor);
    cursor.insertText(completion + QStringLiteral("]]"));

    cursor.setPosition(selStart + completion.length() + 2);
    setTextCursor(cursor);
}

void WikiLinkTextEdit::insertTagCompletion(const QString &completion)
{
    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    QString blockText = block.text();
    int cursorPosInBlock = cursor.positionInBlock();
    int blockStartPos = block.position();

    int hashPos = findHashTag(blockText, cursorPosInBlock);
    if (hashPos < 0)
        return;

    int selStart = blockStartPos + hashPos + 1;
    int selEnd = blockStartPos + cursorPosInBlock;

    cursor.setPosition(selStart);
    cursor.setPosition(selEnd, QTextCursor::KeepAnchor);
    cursor.insertText(completion);

    cursor.setPosition(selStart + completion.length());
    setTextCursor(cursor);
}

void WikiLinkTextEdit::setIndentWidth(int width)
{
    m_indentWidth = width;
    QFontMetrics fm(font());
    setTabStopDistance(fm.horizontalAdvance(QLatin1Char(' ')) * m_indentWidth);
}

QString WikiLinkTextEdit::indentString() const
{
    return IndentUtils::indentString(m_indentWidth);
}

bool WikiLinkTextEdit::handleTabKey()
{
    QTextCursor cursor = textCursor();
    return IndentUtils::handleTabKey(cursor, m_indentWidth);
}

bool WikiLinkTextEdit::handleBackspaceIndent()
{
    QTextCursor cursor = textCursor();
    return IndentUtils::handleBackspaceIndent(cursor, m_indentWidth);
}

void WikiLinkTextEdit::handleAutoIndentOnReturn()
{
    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    QString blockText = block.text();

    // Extract leading whitespace from current line
    int i = 0;
    while (i < blockText.length() && (blockText.at(i) == QLatin1Char(' ') ||
                                      blockText.at(i) == QLatin1Char('\t'))) {
        ++i;
    }
    QString indent = blockText.left(i);

    cursor.beginEditBlock();
    cursor.insertText(QStringLiteral("\n") + indent);
    cursor.endEditBlock();
    setTextCursor(cursor);
}
