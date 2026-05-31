#include "pythonsyntaxhighlighter.h"
#include "keywords.h"
#include "../configmanager.h"
#include "../thememanager.h"

PythonSyntaxHighlighter::PythonSyntaxHighlighter(QTextDocument *parent)
    : BracketHighlighter(parent)
{
    initFormats();

    // Rebuild rules and re-highlight when the theme changes
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        initFormats();
        rehighlight();
    });
}

void PythonSyntaxHighlighter::initFormats()
{
    const auto &cfg = ConfigManager::instance();
    m_rules.clear();

    // --- Comment format (applied in highlightBlock via string-aware scanner) ---
    m_commentFormat.setForeground(cfg.syntaxComments());

    // --- Decorator format (purple) ---
    m_decoratorFormat.setForeground(cfg.syntaxPythonDecorators());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("^\\s*@[\\w.]+"));
        rule.format = m_decoratorFormat;
        m_rules.append(rule);
    }

    // --- Self/cls format ---
    m_selfFormat.setForeground(cfg.syntaxPythonSelfCls());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\bself\\b"));
        rule.format = m_selfFormat;
        m_rules.append(rule);
    }
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\bcls\\b"));
        rule.format = m_selfFormat;
        m_rules.append(rule);
    }

    // --- Keyword format ---
    m_keywordFormat.setForeground(cfg.syntaxKeywords());

    for (const QString &kw : pyKeywords()) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw));
        rule.format = m_keywordFormat;
        m_rules.append(rule);
    }

    // --- Control-flow keyword format (overrides regular keyword color) ---
    m_controlKeywordFormat.setForeground(cfg.syntaxControlKeywords());
    {
        const QStringList ctrl = {
            QStringLiteral("if"), QStringLiteral("elif"), QStringLiteral("else"),
            QStringLiteral("for"), QStringLiteral("while"), QStringLiteral("try"),
            QStringLiteral("except"), QStringLiteral("finally"), QStringLiteral("with"),
            QStringLiteral("return"), QStringLiteral("yield"), QStringLiteral("break"),
            QStringLiteral("continue"), QStringLiteral("raise"), QStringLiteral("assert"),
            QStringLiteral("pass"), QStringLiteral("match"), QStringLiteral("case"),
        };
        for (const QString &kw : ctrl) {
            HighlightingRule rule;
            rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw));
            rule.format = m_controlKeywordFormat;
            m_rules.append(rule);
        }
    }

    // --- Builtin format ---
    m_builtinFormat.setForeground(cfg.syntaxTypes());
    for (const QString &b : pyBuiltins()) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(b));
        rule.format = m_builtinFormat;
        m_rules.append(rule);
    }

    // --- Function call format ---
    m_functionFormat.setForeground(cfg.syntaxFunctions());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b(\\w+)(?=\\s*\\()"));
        rule.format = m_functionFormat;
        m_rules.append(rule);
    }

    // --- Parameter/variable/property format (used by semantic tokens overlay) ---
    m_parameterFormat.setForeground(cfg.syntaxParameters());

    // --- Number format ---
    m_numberFormat.setForeground(cfg.syntaxNumbers());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral(
            "\\b0[xX][0-9a-fA-F](?:[0-9a-fA-F_]*[0-9a-fA-F])?\\b|"
            "\\b0[bB][01](?:[01_]*[01])?\\b|"
            "\\b0[oO][0-7](?:[0-7_]*[0-7])?\\b|"
            "\\b\\d[\\d_]*(?:\\.\\d[\\d_]*)?(?:[eE][+-]?\\d[\\d_]*(?:\\.\\d[\\d_]*)?)?[jJ]?\\b"));
        rule.format = m_numberFormat;
        m_rules.append(rule);
    }

    // --- String format ---
    m_stringFormat.setForeground(cfg.syntaxStrings());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"((?:[furbFURB]{1,2})?"(?:[^"\\]|\\.)*")"));
        rule.format = m_stringFormat;
        m_rules.append(rule);
    }
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"((?:[furbFURB]{1,2})?'(?:[^'\\]|\\.)*')"));
        rule.format = m_stringFormat;
        m_rules.append(rule);
    }

    // Triple-quote patterns for block-state tracking
    m_tripleFormat.setForeground(cfg.syntaxStrings());
    m_tripleDoubleStart = QRegularExpression(QStringLiteral("\"\"\""));
    m_tripleDoubleEnd   = QRegularExpression(QStringLiteral("\"\"\""));
    m_tripleSingleStart = QRegularExpression(QStringLiteral("'''"));
    m_tripleSingleEnd   = QRegularExpression(QStringLiteral("'''"));
}

QTextCharFormat PythonSyntaxHighlighter::formatForTokenType(const QString &type) const
{
    if (type == QStringLiteral("function") || type == QStringLiteral("method")) {
        return m_functionFormat;
    }
    if (type == QStringLiteral("parameter")) {
        return m_parameterFormat;
    }
    if (type == QStringLiteral("class") || type == QStringLiteral("type")
        || type == QStringLiteral("module")) {
        return m_builtinFormat;
    }
    if (type == QStringLiteral("variable") || type == QStringLiteral("property")) {
        return m_parameterFormat;
    }
    // namespace, module — keep default to avoid visual noise
    return QTextCharFormat();
}

void PythonSyntaxHighlighter::highlightBlock(const QString &text)
{
    // Apply single-line rules (strings, comments, keywords, etc.)
    for (const HighlightingRule &rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Apply comment format (# to end of line) — string-aware: skip # inside quoted strings
    {
        bool inString = false;
        QChar stringChar;
        for (int i = 0; i < text.length(); ++i) {
            if (!inString) {
                if (text[i] == u'\'' || text[i] == u'"') {
                    if (i + 2 < text.length() && text[i] == text[i+1] && text[i] == text[i+2]) {
                        i += 2;
                        continue;
                    }
                    inString = true;
                    stringChar = text[i];
                } else if (text[i] == u'#') {
                    setFormat(i, text.length() - i, m_commentFormat);
                    break;
                }
            } else {
                if (text[i] == u'\\') { ++i; continue; }
                if (text[i] == stringChar)
                    inString = false;
            }
        }
    }

    // Multi-line triple-quoted string handling
    // Clear only triple-string state bits, preserve bracket state from previous block
    int prevState = previousBlockState();
    if (prevState < 0) prevState = 0;
    setCurrentBlockState(prevState & ~7);

    int searchFrom = 0;
    int prevTripleState = prevState & 7;

    if (prevTripleState == 1 || prevTripleState == 2) {
        int state = prevTripleState;
        QString closing = (state == 1) ? QStringLiteral("\"\"\"") : QStringLiteral("'''");
        int endIdx = text.indexOf(closing);
        if (endIdx == -1) {
            setFormat(0, text.length(), m_tripleFormat);
            // Preserve bracket state, set triple-string state
            setCurrentBlockState((prevState & ~7) | state);
            return;
        }
        setFormat(0, endIdx + 3, m_tripleFormat);
        searchFrom = endIdx + 3;
    }

    bool inString = false;
    QChar stringChar;
    for (int i = searchFrom; i < text.length(); ++i) {
        if (!inString) {
            if (i + 2 < text.length() && text[i] == text[i+1] && text[i] == text[i+2]
                && (text[i] == u'"' || text[i] == u'\'')) {
                int tripleState = (text[i] == u'"') ? 1 : 2;
                QString closeStr = (text[i] == u'"') ? QStringLiteral("\"\"\"") : QStringLiteral("'''");
                int endIdx = text.indexOf(closeStr, i + 3);
                if (endIdx == -1) {
                    setFormat(i, text.length() - i, m_tripleFormat);
                    // Preserve bracket state, set triple-string state
                    setCurrentBlockState((currentBlockState() & ~7) | tripleState);
                    break;
                } else {
                    setFormat(i, endIdx - i + 3, m_tripleFormat);
                    i = endIdx + 2;
                    continue;
                }
            }
            if (text[i] == u'\'' || text[i] == u'"') {
                inString = true;
                stringChar = text[i];
            }
        } else {
            if (text[i] == u'\\') { ++i; continue; }
            if (text[i] == stringChar)
                inString = false;
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

            int midPoint = token.startChar + token.length / 2;
            if (midPoint < text.length()) {
                QTextCharFormat existing = format(midPoint);
                if (existing.hasProperty(QTextFormat::ForegroundBrush))
                    continue;
            }

            setFormat(token.startChar, token.length, fmt);
        }
    }

    const auto &cfg = ConfigManager::instance();
    highlightBrackets(text,
                      cfg.syntaxComments(), cfg.syntaxStrings());
}