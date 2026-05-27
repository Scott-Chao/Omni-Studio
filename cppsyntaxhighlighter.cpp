#include "cppsyntaxhighlighter.h"
#include "configmanager.h"
#include "thememanager.h"
#include "cppkeywords.h"
#include <QTextLayout>

CppSyntaxHighlighter::CppSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    initFormats();

    // Rebuild rules and re-highlight when the theme changes
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        initFormats();
        rehighlight();
    });
}

void CppSyntaxHighlighter::initFormats()
{
    const auto &cfg = ConfigManager::instance();

    // Reset rule list — all rules will be re-created with current theme colors
    m_rules.clear();

    // --- Function call heuristic (regex fallback, before semantic tokens kick in) ---
    // Applied first so keyword/type rules can override it for built-in keywords/type constructors
    m_functionFormat.setForeground(cfg.syntaxFunctions());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b(\\w+)(?=\\s*\\()"));
        rule.format = m_functionFormat;
        rule.captureGroup = 1;
        m_rules.append(rule);
    }

    // --- Type format (teal) ---
    m_typeFormat.setForeground(cfg.syntaxTypes());

    // --- :: scope resolution (highlight namespace/class qualifier) ---
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b(\\w+)(?=\\s*::)"));
        rule.format = m_typeFormat;
        rule.captureGroup = 1;
        m_rules.append(rule);
    }

    // --- Keyword format ---
    m_keywordFormat.setForeground(cfg.syntaxKeywords());
    m_keywordFormat.setFontWeight(QFont::Bold);

    for (const QString &kw : cppKeywords()) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw));
        rule.format = m_keywordFormat;
        m_rules.append(rule);
    }

    // --- Control-flow keyword format (purple in dark, overrides regular keyword blue) ---
    m_controlKeywordFormat.setForeground(cfg.syntaxControlKeywords());
    m_controlKeywordFormat.setFontWeight(QFont::Bold);
    for (const QString &kw : cppControlKeywords()) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw));
        rule.format = m_controlKeywordFormat;
        m_rules.append(rule);
    }

    // --- Primitive type format (blue, keyword color but no bold) ---
    m_primitiveTypeFormat.setForeground(cfg.syntaxKeywords());
    for (const QString &t : cppPrimitiveTypes()) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(t));
        rule.format = m_primitiveTypeFormat;
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

    for (const QString &t : cppCommonTypes()) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(t));
        rule.format = m_typeFormat;
        m_rules.append(rule);
    }

    // --- Class/struct/enum declaration names ---
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b(?:class|struct|enum(?:\\s+class)?)\\s+(\\w+)"));
        rule.format = m_typeFormat;
        rule.captureGroup = 1;
        m_rules.append(rule);
    }

    // --- Number format (green) ---
    m_numberFormat.setForeground(cfg.syntaxNumbers());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral("\\b0[xX][0-9a-fA-F]+[']?[0-9a-fA-F]*\\b"
                           "|\\b0[bB][01]+[']?[01]*\\b"
                           "|\\b[0-9]+[']?[0-9]*(?:\\.[0-9]+[']?[0-9]*)?(?:[eE][+-]?[0-9]+)?(?:f|F|l|L|u|U|ll|LL|ull|ULL)?\\b"));
        rule.format = m_numberFormat;
        m_rules.append(rule);
    }

    // --- #include header path ---
    m_includeHeaderFormat.setForeground(cfg.syntaxStrings());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("#include\\s+(<[^>]+>)"));
        rule.format = m_includeHeaderFormat;
        rule.captureGroup = 1;
        m_rules.append(rule);
    }
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("#include\\s+\"([^\"]+)\""));
        rule.format = m_includeHeaderFormat;
        rule.captureGroup = 1;
        m_rules.append(rule);
    }

    // --- String / char / raw string format ---
    m_stringFormat.setForeground(cfg.syntaxStrings());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral(R"("(?:[^"\\]|\\.)*")"));
        rule.format = m_stringFormat;
        m_rules.append(rule);
    }
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral(R"('(?:[^'\\]|\\.)'|'(?:\\.)')"));
        rule.format = m_stringFormat;
        m_rules.append(rule);
    }
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral(R"(R"([^(]*)\([^)]*\)\1")"));
        rule.format = m_stringFormat;
        m_rules.append(rule);
    }

    // --- Parameter format (also used for variables/properties from LSP) ---
    m_parameterFormat.setForeground(cfg.syntaxParameters());

    // --- Comment formats ---
    m_singleLineCommentFormat.setForeground(cfg.syntaxComments());
    m_multiLineCommentFormat.setForeground(cfg.syntaxComments());
    m_commentStartExpr = QRegularExpression(QStringLiteral("/\\*"));
    m_commentEndExpr = QRegularExpression(QStringLiteral("\\*/"));

    m_bracketColors[0] = QColor("#FFD700");
    m_bracketColors[1] = QColor("#DA70D6");
    m_bracketColors[2] = QColor("#179FFF");
    m_unpairedBracketColor = QColor("#FF0000");
}

void CppSyntaxHighlighter::setSemanticTokens(const QList<SemanticToken> &tokens)
{
    m_semanticTokens.clear();
    for (const auto &token : tokens) {
        m_semanticTokens[token.line].append(token);
    }
    rehighlight();
}

void CppSyntaxHighlighter::clearSemanticTokens()
{
    m_semanticTokens.clear();
    rehighlight();
}

QTextCharFormat CppSyntaxHighlighter::formatForTokenType(const QString &type) const
{
    // Map LSP semantic token types to formats.
    // Types already handled by regex rules (keywords, types, strings, comments, numbers)
    // are skipped — we only override with formats that add new information.
    if (type == QStringLiteral("function") || type == QStringLiteral("method")) {
        return m_functionFormat;
    }
    if (type == QStringLiteral("parameter")) {
        return m_parameterFormat;
    }
    // For types like class/struct/enum/type/typeParameter — reuse typeFormat
    if (type == QStringLiteral("class") || type == QStringLiteral("struct")
        || type == QStringLiteral("enum") || type == QStringLiteral("enumMember")
        || type == QStringLiteral("type") || type == QStringLiteral("typeParameter")) {
        return m_typeFormat;
    }
    // macro → preprocessor color
    if (type == QStringLiteral("macro")) {
        return m_preprocessorFormat;
    }
    if (type == QStringLiteral("variable") || type == QStringLiteral("property")) {
        return m_parameterFormat;
    }
    // namespace — keep default (no highlight) to avoid visual noise
    return QTextCharFormat();
}

void CppSyntaxHighlighter::highlightBlock(const QString &text)
{
    // Apply regex rules first
    for (const HighlightingRule &rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            int start, len;
            if (rule.captureGroup > 0) {
                start = match.capturedStart(rule.captureGroup);
                len = match.capturedLength(rule.captureGroup);
                if (start < 0) continue;
            } else {
                start = match.capturedStart();
                len = match.capturedLength();
            }
            setFormat(start, len, rule.format);
        }
    }

    // Combined single-line & multi-line comment handling — string-aware
    // Clear only comment state bits, preserve bracket state from previous block
    setCurrentBlockState(previousBlockState() & ~7);

    int searchFrom = 0;
    if ((previousBlockState() & 7) == 1) {
        int endIdx = text.indexOf(QStringLiteral("*/"));
        if (endIdx == -1) {
            setFormat(0, text.length(), m_multiLineCommentFormat);
            // Preserve bracket state, set comment state = 1
            setCurrentBlockState((previousBlockState() & ~7) | 1);
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
                        setCurrentBlockState((currentBlockState() & ~7) | 1);
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

    // Apply semantic tokens on top of regex rules.
    // Only apply where no format has been set yet (to preserve keyword/comment/string highlights).
    int blockNumber = currentBlock().blockNumber();
    auto it = m_semanticTokens.find(blockNumber);
    if (it != m_semanticTokens.end()) {
        for (const SemanticToken &token : it.value()) {
            QTextCharFormat fmt = formatForTokenType(token.type);
            if (!fmt.isValid() || fmt == QTextCharFormat())
                continue;

            // Only apply where there's no existing format (except the default)
            // Check a midpoint character to detect if a comment/string/other format is already there
            int midPoint = token.startChar + token.length / 2;
            if (midPoint < text.length()) {
                QTextCharFormat existing = format(midPoint);
                // If the position already has a foreground set (not the default text color),
                // skip to avoid overriding keywords, strings, comments, etc.
                if (existing.hasProperty(QTextFormat::ForegroundBrush))
                    continue;
            }

            setFormat(token.startChar, token.length, fmt);
        }
    }

    highlightBrackets(text);
}

void CppSyntaxHighlighter::highlightBrackets(const QString &text)
{
    struct BracketInfo {
        int pos;   // -1 for brackets carried from previous blocks
        QChar ch;
    };
    QVector<BracketInfo> bracketStack;

    // Decode bracket stack from previous block state
    int prevState = previousBlockState();
    int prevDepth = (prevState >> 3) & 31;
    for (int i = 0; i < prevDepth && i < 12; ++i) {
        int type = (prevState >> (8 + i * 2)) & 3;
        QChar ch = (type == 1) ? QLatin1Char('[') : (type == 2) ? QLatin1Char('{') : QLatin1Char('(');
        bracketStack.append({-1, ch});
    }

    // Build a set of positions that fall inside string/comment format ranges
    QVector<QTextLayout::FormatRange> fmtRanges = currentBlock().layout()->formats();
    const auto &cfg = ConfigManager::instance();
    const QColor commentFg = cfg.syntaxComments();
    const QColor stringFg = cfg.syntaxStrings();
    const QColor preprocessorFg = cfg.syntaxPreprocessor();
    auto isInSpecialFormat = [&](int pos) -> bool {
        for (const auto &r : fmtRanges) {
            if (pos >= r.start && pos < r.start + r.length) {
                QColor fg = r.format.foreground().color();
                if (fg == commentFg || fg == stringFg || fg == preprocessorFg)
                    return true;
                break;
            }
        }
        return false;
    };

    for (int i = 0; i < text.length(); ++i) {
        const QChar ch = text.at(i);

        if (ch != QLatin1Char('(') && ch != QLatin1Char(')') &&
            ch != QLatin1Char('[') && ch != QLatin1Char(']') &&
            ch != QLatin1Char('{') && ch != QLatin1Char('}'))
            continue;

        // Skip brackets inside strings, comments, or preprocessor directives
        if (isInSpecialFormat(i))
            continue;

        if (ch == QLatin1Char('(') || ch == QLatin1Char('[') || ch == QLatin1Char('{')) {
            bracketStack.append({i, ch});
            // Color with tentative depth color (will be recolor later if matched on same line)
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

            if (!bracketStack.isEmpty() && bracketStack.last().ch == expected) {
                const int depth = bracketStack.size() - 1;
                QTextCharFormat fmt;
                fmt.setForeground(m_bracketColors[depth % 3]);
                fmt.setFontWeight(QFont::Bold);
                // Recolor opener if on the same line (pos >= 0)
                if (bracketStack.last().pos >= 0)
                    setFormat(bracketStack.last().pos, 1, fmt);
                setFormat(i, 1, fmt);
                bracketStack.removeLast();
            } else {
                QTextCharFormat fmt;
                fmt.setForeground(m_unpairedBracketColor);
                fmt.setFontWeight(QFont::Bold);
                setFormat(i, 1, fmt);
            }
        }
    }

    // Encode final bracket stack into block state (bits 3+), preserving comment state (bits 0-2)
    int commentState = currentBlockState();
    if (commentState < 0) commentState = 0;
    commentState &= 7;

    int state = commentState;
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
