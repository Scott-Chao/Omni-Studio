#ifndef INDENTHELPER_H
#define INDENTHELPER_H

#include <QString>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextDocument>

// ── Shared tab/indent/backspace logic ──────────────────────────────────
// Consolidated from CodeEditor and WikiLinkTextEdit which had identical
// implementations of handleTabKey, handleBackspaceIndent, and indentString.

namespace IndentUtils {

inline QString indentString(int width)
{
    return QString(width, QLatin1Char(' '));
}

// Insert indent at cursor (or indent selected lines).
// Returns true if handled.
inline bool handleTabKey(QTextCursor &cursor, int indentWidth)
{
    if (cursor.hasSelection()) {
        QTextDocument *doc = cursor.document();
        int startBlock = doc->findBlock(cursor.selectionStart()).blockNumber();
        int endBlock = doc->findBlock(cursor.selectionEnd()).blockNumber();

        cursor.beginEditBlock();
        for (int i = startBlock; i <= endBlock; ++i) {
            QTextBlock block = doc->findBlockByNumber(i);
            QTextCursor blockCursor(block);
            blockCursor.insertText(indentString(indentWidth));
        }
        cursor.endEditBlock();
    } else {
        cursor.insertText(indentString(indentWidth));
    }
    return true;
}

// Smart backspace in leading whitespace: delete back to previous tab stop.
// Returns true if handled (leading whitespace case), false otherwise.
// Does NOT handle literal \t characters — callers should check that first.
inline bool handleBackspaceIndent(QTextCursor &cursor, int indentWidth)
{
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

    // Delete spaces back to previous tab stop
    int spaceCount = posInBlock % indentWidth;
    if (spaceCount == 0)
        spaceCount = indentWidth;

    cursor.beginEditBlock();
    for (int j = 0; j < spaceCount; ++j)
        cursor.deletePreviousChar();
    cursor.endEditBlock();
    return true;
}

} // namespace IndentUtils

#endif // INDENTHELPER_H
