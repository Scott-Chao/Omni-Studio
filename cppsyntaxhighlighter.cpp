#include "cppsyntaxhighlighter.h"
#include "configmanager.h"
#include "cppkeywords.h"

CppSyntaxHighlighter::CppSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    const auto &cfg = ConfigManager::instance();

    // --- Keyword format (blue) ---
    m_keywordFormat.setForeground(cfg.syntaxKeywords());
    m_keywordFormat.setFontWeight(QFont::Bold);

    for (const QString &kw : cppKeywords()) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw));
        rule.format = m_keywordFormat;
        m_rules.append(rule);
    }

    // --- Preprocessor format (purple) ---
    m_preprocessorFormat.setForeground(cfg.syntaxPreprocessor());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("^\\s*#\\s*\\w+"));
        rule.format = m_preprocessorFormat;
        m_rules.append(rule);
    }

    // --- Type format (teal) ---
    m_typeFormat.setForeground(cfg.syntaxTypes());
    for (const QString &t : cppCommonTypes()) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(t));
        rule.format = m_typeFormat;
        m_rules.append(rule);
    }

    // --- Number format (green) ---
    m_numberFormat.setForeground(cfg.syntaxNumbers());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral("\\b0[xX][0-9a-fA-F]+[']?[0-9a-fA-F]*\\b"  // hex
                           "|\\b0[bB][01]+[']?[01]*\\b"                  // binary
                           "|\\b[0-9]+[']?[0-9]*(?:\\.[0-9]+[']?[0-9]*)?(?:[eE][+-]?[0-9]+)?(?:f|F|l|L|u|U|ll|LL|ull|ULL)?\\b"));
        rule.format = m_numberFormat;
        m_rules.append(rule);
    }

    // --- String format (orange-brown) ---
    m_stringFormat.setForeground(cfg.syntaxStrings());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"("(?:[^"\\]|\\.)*")"));
        rule.format = m_stringFormat;
        m_rules.append(rule);
    }
    // Char literals
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"('(?:[^'\\]|\\.)'|'(?:\\.)')"));
        rule.format = m_stringFormat;
        m_rules.append(rule);
    }
    // Raw string literals
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"(R"([^(]*)\([^)]*\)\1")"));
        rule.format = m_stringFormat;
        m_rules.append(rule);
    }

    // --- Single-line comment format (dim green, applied in highlightBlock via string-aware scanner) ---
    m_singleLineCommentFormat.setForeground(cfg.syntaxComments());

    // --- Multi-line comment (block state tracking) ---
    m_multiLineCommentFormat.setForeground(cfg.syntaxComments());
    m_commentStartExpr = QRegularExpression(QStringLiteral("/\\*"));
    m_commentEndExpr = QRegularExpression(QStringLiteral("\\*/"));
}

void CppSyntaxHighlighter::highlightBlock(const QString &text)
{
    // Apply single-line rules first
    for (const HighlightingRule &rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Combined single-line & multi-line comment handling — string-aware
    setCurrentBlockState(0);

    int searchFrom = 0;
    if (previousBlockState() == 1) {
        int endIdx = text.indexOf(QStringLiteral("*/"));
        if (endIdx == -1) {
            setFormat(0, text.length(), m_multiLineCommentFormat);
            setCurrentBlockState(1);
            return;
        }
        setFormat(0, endIdx + 2, m_multiLineCommentFormat);
        searchFrom = endIdx + 2;
    }

    bool inString = false;
    bool inChar = false;
    for (int i = searchFrom; i < text.length(); ++i) {
        if (inString) {
            if (text[i] == u'\\') { ++i; continue; }
            if (text[i] == u'"') inString = false;
        } else if (inChar) {
            if (text[i] == u'\\') { ++i; continue; }
            if (text[i] == u'\'') inChar = false;
        } else {
            if (text[i] == u'"') {
                inString = true;
            } else if (text[i] == u'\'') {
                inChar = true;
            } else if (text[i] == u'/' && i + 1 < text.length()) {
                if (text[i+1] == u'/') {
                    setFormat(i, text.length() - i, m_singleLineCommentFormat);
                    break;
                } else if (text[i+1] == u'*') {
                    int endIdx = text.indexOf(QStringLiteral("*/"), i + 2);
                    if (endIdx == -1) {
                        setCurrentBlockState(1);
                        setFormat(i, text.length() - i, m_multiLineCommentFormat);
                        break;
                    } else {
                        setFormat(i, endIdx - i + 2, m_multiLineCommentFormat);
                        i = endIdx + 1;
                    }
                }
            }
        }
    }

    // Apply preprocessor format last so it overrides keyword/type inside # lines
    // (already in m_rules, but re-apply to ensure it paints over)
}
