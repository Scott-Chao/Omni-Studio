#include "brackethighlighter.h"
#include <QTextBlock>

void BracketHighlighter::setSemanticTokens(const QList<SemanticToken> &tokens)
{
    m_semanticTokens.clear();
    for (const auto &token : tokens) {
        m_semanticTokens[token.line].append(token);
    }
    rehighlight();
}

void BracketHighlighter::clearSemanticTokens()
{
    m_semanticTokens.clear();
    rehighlight();
}

void BracketHighlighter::highlightBrackets(const QString &text,
                                            const QColor &commentFg,
                                            const QColor &stringFg,
                                            const QColor &preprocessorFg,
                                            bool checkPreprocessor)
{
    struct BracketInfo {
        int pos;
        QChar ch;
    };
    QVector<BracketInfo> bracketStack;

    // Decode bracket stack from previous block state
    int prevState = previousBlockState();
    if (prevState < 0) prevState = 0;
    int prevDepth = (prevState >> 3) & 31;
    for (int i = 0; i < prevDepth && i < 12; ++i) {
        int type = (prevState >> (8 + i * 2)) & 3;
        QChar ch = (type == 1) ? QLatin1Char('[') : (type == 2) ? QLatin1Char('{') : QLatin1Char('(');
        bracketStack.append({-1, ch});
    }

    auto isInSpecialFormat = [&](int pos) -> bool {
        QTextCharFormat fmt = format(pos);
        if (fmt.hasProperty(QTextFormat::ForegroundBrush)) {
            QColor fg = fmt.foreground().color();
            if (fg == commentFg || fg == stringFg)
                return true;
            if (checkPreprocessor && fg == preprocessorFg)
                return true;
        }
        return false;
    };

    for (int i = 0; i < text.length(); ++i) {
        const QChar ch = text.at(i);

        if (ch != QLatin1Char('(') && ch != QLatin1Char(')') &&
            ch != QLatin1Char('[') && ch != QLatin1Char(']') &&
            ch != QLatin1Char('{') && ch != QLatin1Char('}'))
            continue;

        if (isInSpecialFormat(i))
            continue;

        if (ch == QLatin1Char('(') || ch == QLatin1Char('[') || ch == QLatin1Char('{')) {
            bracketStack.append({i, ch});
            const int depth = bracketStack.size() - 1;
            QTextCharFormat fmt;
            fmt.setForeground(m_bracketColors[depth % 3]);
            fmt.setFontWeight(QFont::Bold);
            setFormat(i, 1, fmt);
        } else {
            QChar expected;
            if (ch == QLatin1Char(')'))
                expected = QLatin1Char('(');
            else if (ch == QLatin1Char(']'))
                expected = QLatin1Char('[');
            else
                expected = QLatin1Char('{');

            int matchIdx = -1;
            for (int j = bracketStack.size() - 1; j >= 0; --j) {
                if (bracketStack[j].ch == expected) {
                    matchIdx = j;
                    break;
                }
            }

            if (matchIdx >= 0) {
                for (int j = matchIdx + 1; j < bracketStack.size(); ++j) {
                    if (bracketStack[j].pos >= 0) {
                        QTextCharFormat errFmt;
                        errFmt.setForeground(m_unpairedBracketColor);
                        errFmt.setFontWeight(QFont::Bold);
                        setFormat(bracketStack[j].pos, 1, errFmt);
                    }
                }
                BracketInfo opener = bracketStack[matchIdx];
                bracketStack.resize(matchIdx);
                const int depth = matchIdx;
                QTextCharFormat fmt;
                fmt.setForeground(m_bracketColors[depth % 3]);
                fmt.setFontWeight(QFont::Bold);
                if (opener.pos >= 0)
                    setFormat(opener.pos, 1, fmt);
                setFormat(i, 1, fmt);
            } else {
                QTextCharFormat fmt;
                fmt.setForeground(m_unpairedBracketColor);
                fmt.setFontWeight(QFont::Bold);
                setFormat(i, 1, fmt);
            }
        }
    }

    // Recolor unmatched opening brackets from the current line as red.
    // Scan forward through subsequent blocks; any opening bracket from the
    // current line that never finds its closer in future lines is marked red.
    if (!bracketStack.isEmpty()) {
        struct Pending {
            QChar ch;
            int pos; // >= 0 = bracket is on the current line at this position
        };
        QVector<Pending> pending;
        for (const auto &b : bracketStack)
            pending.append({b.ch, b.pos});

        QTextBlock nextBlock = currentBlock().next();
        while (nextBlock.isValid() && !pending.isEmpty()) {
            QString nextText = nextBlock.text();
            for (int i = 0; i < nextText.length() && !pending.isEmpty(); ++i) {
                QChar ch = nextText.at(i);
                if (ch == QLatin1Char('(') || ch == QLatin1Char('[') || ch == QLatin1Char('{')) {
                    pending.append({ch, -1});
                } else if (ch == QLatin1Char(')') || ch == QLatin1Char(']') || ch == QLatin1Char('}')) {
                    QChar expected;
                    if (ch == QLatin1Char(')')) expected = QLatin1Char('(');
                    else if (ch == QLatin1Char(']')) expected = QLatin1Char('[');
                    else expected = QLatin1Char('{');
                    // Search backwards for the nearest opener of the same type.
                    // A plain LIFO stack (pending.last()) would fail when an
                    // unmatched bracket of a different type sits on top — e.g.
                    // unmatched '(' blocks a '}' from reaching the '{' below.
                    for (int j = pending.size() - 1; j >= 0; --j) {
                        if (pending[j].ch == expected) {
                            pending.removeAt(j);
                            break;
                        }
                    }
                }
            }
            nextBlock = nextBlock.next();
        }

        // Any remaining brackets that belong to the current line are unpaired
        for (const auto &p : pending) {
            if (p.pos >= 0) {
                QTextCharFormat errFmt;
                errFmt.setForeground(m_unpairedBracketColor);
                errFmt.setFontWeight(QFont::Bold);
                setFormat(p.pos, 1, errFmt);
            }
        }
    }

    // Encode final bracket stack into block state (bits 3+), preserving comment state (bits 0-2)
    int originalState = currentBlockState();
    if (originalState < 0) originalState = 0;
    originalState &= 7;

    int state = originalState;
    int depth = qMin(bracketStack.size(), 12);
    state |= (depth << 3);
    for (int i = 0; i < depth; ++i) {
        int type = 0;
        if (bracketStack[i].ch == QLatin1Char('[')) type = 1;
        else if (bracketStack[i].ch == QLatin1Char('{')) type = 2;
        state |= (type << (8 + i * 2));
    }
    setCurrentBlockState(state);
}
