#include "cppsyntaxhighlighter.h"
#include "configmanager.h"
#include "thememanager.h"
#include "cppkeywords.h"

CppSyntaxHighlighter::CppSyntaxHighlighter(QTextDocument *parent)
    : BracketHighlighter(parent)
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

    // --- Pointer / reference operator format (blue, same as keywords) ---
    m_operatorFormat.setForeground(cfg.syntaxKeywords());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral("\\b(?:const\\s+)?(?:int|char|float|double|bool|void|short|long|auto|wchar_t)\\s*([*&]+)\\b"));
        rule.format = m_operatorFormat;
        rule.captureGroup = 2;
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

        // Highlight * and & operators that follow type tokens (clangd-driven, handles all types)
        for (const SemanticToken &token : it.value()) {
            if (token.type != QStringLiteral("type") &&
                token.type != QStringLiteral("class") &&
                token.type != QStringLiteral("struct") &&
                token.type != QStringLiteral("enum") &&
                token.type != QStringLiteral("typeParameter"))
                continue;

            int pos = token.startChar + token.length;
            if (pos >= text.length())
                continue;

            // Skip whitespace, template closing '>', and cv-qualifiers between type and operator.
            // Handles: "int*", "int *", "QMap<A,B> &", "QMap<A,B>*", "int const *", etc.
            for (;;) {
                while (pos < text.length() && text[pos].isSpace())
                    pos++;
                if (pos >= text.length())
                    break;
                if (text[pos] == QLatin1Char('*') || text[pos] == QLatin1Char('&'))
                    break;
                // Skip template closing brackets (single or nested >>)
                if (text[pos] == QLatin1Char('>')) {
                    pos++;
                    continue;
                }
                // Skip cv-qualifiers
                if (text.mid(pos, 5) == QLatin1String("const") &&
                    (pos + 5 >= text.length() || !text[pos + 5].isLetterOrNumber())) {
                    pos += 5;
                    continue;
                }
                if (text.mid(pos, 8) == QLatin1String("volatile") &&
                    (pos + 8 >= text.length() || !text[pos + 8].isLetterOrNumber())) {
                    pos += 8;
                    continue;
                }
                break;
            }

            if (pos >= text.length())
                continue;
            if (text[pos] != QLatin1Char('*') && text[pos] != QLatin1Char('&'))
                continue;

            // Don't overwrite already-formatted positions (string, comment, or regex-fallback)
            if (format(pos).hasProperty(QTextFormat::ForegroundBrush))
                continue;

            int start = pos;
            while (pos < text.length() && (text[pos] == QLatin1Char('*') || text[pos] == QLatin1Char('&')))
                pos++;
            setFormat(start, pos - start, m_operatorFormat);
        }
    }

    highlightBrackets(text,
                      cfg.syntaxComments(), cfg.syntaxStrings(),
                      cfg.syntaxPreprocessor(), /*checkPreprocessor=*/true);
}
