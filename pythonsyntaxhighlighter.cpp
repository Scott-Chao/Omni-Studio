#include "pythonsyntaxhighlighter.h"
#include "pykeywords.h"
#include "configmanager.h"

PythonSyntaxHighlighter::PythonSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    const auto &cfg = ConfigManager::instance();

    // --- Comment format (dim green, applied in highlightBlock via string-aware scanner) ---
    m_commentFormat.setForeground(cfg.syntaxComments());

    // --- Decorator format (purple) ---
    m_decoratorFormat.setForeground(cfg.syntaxPythonDecorators());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("^\\s*@[\\w.]+"));
        rule.format = m_decoratorFormat;
        m_rules.append(rule);
    }

    // --- Self/cls format (yellow) ---
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

    // --- Keyword format (blue, bold) ---
    m_keywordFormat.setForeground(cfg.syntaxKeywords());
    m_keywordFormat.setFontWeight(QFont::Bold);

    for (const QString &kw : pyKeywords()) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw));
        rule.format = m_keywordFormat;
        m_rules.append(rule);
    }

    // --- Constants format (blue, bold) ---
    // True, False, None already included in pyKeywords()

    // --- Builtin format (teal) ---
    m_builtinFormat.setForeground(cfg.syntaxTypes());
    for (const QString &b : pyBuiltins()) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(b));
        rule.format = m_builtinFormat;
        m_rules.append(rule);
    }

    // --- Function call format (gold) ---
    m_functionFormat.setForeground(cfg.syntaxFunctions());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b(\\w+)(?=\\s*\\()"));
        rule.format = m_functionFormat;
        m_rules.append(rule);
    }

    // --- Number format (green) ---
    m_numberFormat.setForeground(cfg.syntaxNumbers());
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral(
            "\\b0[xX][0-9a-fA-F](?:[0-9a-fA-F_]*[0-9a-fA-F])?\\b|"   // hex
            "\\b0[bB][01](?:[01_]*[01])?\\b|"                          // binary
            "\\b0[oO][0-7](?:[0-7_]*[0-7])?\\b|"                       // octal
            "\\b\\d[\\d_]*(?:\\.\\d[\\d_]*)?(?:[eE][+-]?\\d[\\d_]*(?:\\.\\d[\\d_]*)?)?[jJ]?\\b"));  // int/float/complex
        rule.format = m_numberFormat;
        m_rules.append(rule);
    }

    // --- String format (orange-brown) ---
    m_stringFormat.setForeground(cfg.syntaxStrings());

    // Double-quoted strings with optional prefix: f, r, b, u, fr, rf, br, rb
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"((?:[furbFURB]{1,2})?"(?:[^"\\]|\\.)*")"));
        rule.format = m_stringFormat;
        m_rules.append(rule);
    }
    // Single-quoted strings with optional prefix
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"((?:[furbFURB]{1,2})?'(?:[^'\\]|\\.)*')"));
        rule.format = m_stringFormat;
        m_rules.append(rule);
    }

    // Triple-quote patterns for block-state tracking
    m_tripleFormat.setForeground(cfg.syntaxComments());
    m_tripleDoubleStart = QRegularExpression(QStringLiteral("\"\"\""));
    m_tripleDoubleEnd   = QRegularExpression(QStringLiteral("\"\"\""));
    m_tripleSingleStart = QRegularExpression(QStringLiteral("'''"));
    m_tripleSingleEnd   = QRegularExpression(QStringLiteral("'''"));
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
                    // Skip triple-quoted strings (handled separately below)
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
                if (text[i] == u'\\') {
                    ++i; // skip escaped character
                    continue;
                }
                if (text[i] == stringChar)
                    inString = false;
            }
        }
    }

    // Multi-line triple-quoted string handling — string-aware: skip triple quotes inside single-line strings
    setCurrentBlockState(0);

    int searchFrom = 0;
    int prevState = previousBlockState();

    if (prevState == 1 || prevState == 2) {
        // Continuing a triple-quoted string from the previous block
        int state = prevState;
        QString closing = (state == 1) ? QStringLiteral("\"\"\"") : QStringLiteral("'''");
        int endIdx = text.indexOf(closing);
        if (endIdx == -1) {
            setFormat(0, text.length(), m_tripleFormat);
            setCurrentBlockState(state);
            return;
        }
        setFormat(0, endIdx + 3, m_tripleFormat);
        searchFrom = endIdx + 3;
    }

    // Search for new triple-quoted strings in this block — skip positions inside single-line strings
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
                    setCurrentBlockState(tripleState);
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
            if (text[i] == u'\\') {
                ++i;
                continue;
            }
            if (text[i] == stringChar)
                inString = false;
        }
    }
}
