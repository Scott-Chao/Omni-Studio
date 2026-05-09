#include "pythonsyntaxhighlighter.h"

PythonSyntaxHighlighter::PythonSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // --- Comment format (dim green) ---
    m_commentFormat.setForeground(QColor(0x6A, 0x99, 0x55));
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("#[^\n]*"));
        rule.format = m_commentFormat;
        m_rules.append(rule);
    }

    // --- Decorator format (purple) ---
    m_decoratorFormat.setForeground(QColor(0xC5, 0x86, 0xC0));
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("^\\s*@[\\w.]+"));
        rule.format = m_decoratorFormat;
        m_rules.append(rule);
    }

    // --- Self/cls format (yellow) ---
    m_selfFormat.setForeground(QColor(0xDC, 0xDC, 0xAA));
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
    m_keywordFormat.setForeground(QColor(0x56, 0x9C, 0xD6));
    m_keywordFormat.setFontWeight(QFont::Bold);

    const QStringList keywords = {
        QStringLiteral("def"), QStringLiteral("class"), QStringLiteral("if"),
        QStringLiteral("elif"), QStringLiteral("else"), QStringLiteral("for"),
        QStringLiteral("while"), QStringLiteral("import"), QStringLiteral("from"),
        QStringLiteral("as"), QStringLiteral("return"), QStringLiteral("yield"),
        QStringLiteral("try"), QStringLiteral("except"), QStringLiteral("finally"),
        QStringLiteral("raise"), QStringLiteral("with"), QStringLiteral("pass"),
        QStringLiteral("break"), QStringLiteral("continue"), QStringLiteral("lambda"),
        QStringLiteral("del"), QStringLiteral("global"), QStringLiteral("nonlocal"),
        QStringLiteral("assert"), QStringLiteral("async"), QStringLiteral("await"),
        QStringLiteral("match"), QStringLiteral("case"), QStringLiteral("and"),
        QStringLiteral("or"), QStringLiteral("not"), QStringLiteral("is"),
        QStringLiteral("in")
    };
    for (const QString &kw : keywords) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw));
        rule.format = m_keywordFormat;
        m_rules.append(rule);
    }

    // --- Constants format (blue, bold) ---
    // True, False, None get same color as keywords
    const QStringList constants = {
        QStringLiteral("True"), QStringLiteral("False"), QStringLiteral("None")
    };
    for (const QString &c : constants) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(c));
        rule.format = m_keywordFormat;
        m_rules.append(rule);
    }

    // --- Builtin format (teal) ---
    m_builtinFormat.setForeground(QColor(0x4E, 0xC9, 0xB0));
    const QStringList builtins = {
        // Types
        QStringLiteral("int"), QStringLiteral("float"), QStringLiteral("str"),
        QStringLiteral("list"), QStringLiteral("dict"), QStringLiteral("tuple"),
        QStringLiteral("set"), QStringLiteral("bool"), QStringLiteral("bytes"),
        QStringLiteral("bytearray"), QStringLiteral("complex"), QStringLiteral("frozenset"),
        QStringLiteral("range"), QStringLiteral("slice"), QStringLiteral("type"),
        QStringLiteral("super"), QStringLiteral("object"), QStringLiteral("property"),
        QStringLiteral("staticmethod"), QStringLiteral("classmethod"),
        // Functions
        QStringLiteral("enumerate"), QStringLiteral("zip"), QStringLiteral("map"),
        QStringLiteral("filter"), QStringLiteral("len"), QStringLiteral("print"),
        QStringLiteral("open"), QStringLiteral("isinstance"), QStringLiteral("hasattr"),
        QStringLiteral("getattr"), QStringLiteral("setattr"), QStringLiteral("sorted"),
        QStringLiteral("reversed"), QStringLiteral("iter"), QStringLiteral("next"),
        QStringLiteral("any"), QStringLiteral("all"), QStringLiteral("sum"),
        QStringLiteral("min"), QStringLiteral("max"), QStringLiteral("abs"),
        QStringLiteral("round"), QStringLiteral("ord"), QStringLiteral("chr"),
        QStringLiteral("repr"), QStringLiteral("input"), QStringLiteral("format"),
        QStringLiteral("id"), QStringLiteral("dir"), QStringLiteral("vars"),
        QStringLiteral("callable"), QStringLiteral("issubclass"), QStringLiteral("eval"),
        QStringLiteral("exec"), QStringLiteral("compile"), QStringLiteral("locals"),
        QStringLiteral("globals"), QStringLiteral("hash"),
        // Exceptions
        QStringLiteral("ValueError"), QStringLiteral("TypeError"),
        QStringLiteral("KeyError"), QStringLiteral("IndexError"),
        QStringLiteral("AttributeError"), QStringLiteral("ImportError"),
        QStringLiteral("ModuleNotFoundError"), QStringLiteral("NameError"),
        QStringLiteral("FileNotFoundError"), QStringLiteral("ZeroDivisionError"),
        QStringLiteral("StopIteration"), QStringLiteral("RuntimeError"),
        QStringLiteral("OSError"), QStringLiteral("IOError"),
        QStringLiteral("Exception"), QStringLiteral("BaseException"),
        QStringLiteral("Warning"), QStringLiteral("UserWarning"),
        QStringLiteral("DeprecationWarning")
    };
    for (const QString &b : builtins) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(b));
        rule.format = m_builtinFormat;
        m_rules.append(rule);
    }

    // --- Number format (green) ---
    m_numberFormat.setForeground(QColor(0xB5, 0xCE, 0xA8));
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
    m_stringFormat.setForeground(QColor(0xCE, 0x91, 0x78));

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
    m_tripleFormat.setForeground(QColor(0x6A, 0x99, 0x55));
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

    // Multi-line triple-quoted string handling
    setCurrentBlockState(0);

    int searchFrom = 0;
    int prevState = previousBlockState();

    if (prevState == 1 || prevState == 2) {
        // Continuing a triple-quoted string from the previous block
        int state = prevState; // 1 = """, 2 = '''
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

    // Search for new triple-quoted strings in this block
    while (true) {
        int dPos = text.indexOf(QStringLiteral("\"\"\""), searchFrom);
        int sPos = text.indexOf(QStringLiteral("'''"), searchFrom);

        int tripleStart;
        int tripleState;
        if (dPos >= 0 && (sPos < 0 || dPos < sPos)) {
            tripleStart = dPos;
            tripleState = 1;
        } else if (sPos >= 0) {
            tripleStart = sPos;
            tripleState = 2;
        } else {
            break;
        }

        QString closing = (tripleState == 1) ? QStringLiteral("\"\"\"") : QStringLiteral("'''");
        int endIdx = text.indexOf(closing, tripleStart + 3);

        if (endIdx == -1) {
            setFormat(tripleStart, text.length() - tripleStart, m_tripleFormat);
            setCurrentBlockState(tripleState);
            break;
        }
        setFormat(tripleStart, endIdx - tripleStart + 3, m_tripleFormat);
        searchFrom = endIdx + 3;
    }
}
