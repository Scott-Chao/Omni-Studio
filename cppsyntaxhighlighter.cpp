#include "cppsyntaxhighlighter.h"
#include "configmanager.h"

CppSyntaxHighlighter::CppSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    const auto &cfg = ConfigManager::instance();

    // --- Keyword format (blue) ---
    m_keywordFormat.setForeground(cfg.syntaxKeywords());
    m_keywordFormat.setFontWeight(QFont::Bold);

    const QStringList keywords = {
        QStringLiteral("alignas"), QStringLiteral("alignof"), QStringLiteral("and"),
        QStringLiteral("and_eq"), QStringLiteral("asm"), QStringLiteral("auto"),
        QStringLiteral("bitand"), QStringLiteral("bitor"), QStringLiteral("break"),
        QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"),
        QStringLiteral("compl"), QStringLiteral("concept"), QStringLiteral("const"),
        QStringLiteral("consteval"), QStringLiteral("constexpr"), QStringLiteral("constinit"),
        QStringLiteral("const_cast"), QStringLiteral("continue"), QStringLiteral("co_await"),
        QStringLiteral("co_return"), QStringLiteral("co_yield"), QStringLiteral("decltype"),
        QStringLiteral("default"), QStringLiteral("delete"), QStringLiteral("do"),
        QStringLiteral("dynamic_cast"), QStringLiteral("else"), QStringLiteral("enum"),
        QStringLiteral("explicit"), QStringLiteral("export"), QStringLiteral("extern"),
        QStringLiteral("false"), QStringLiteral("final"), QStringLiteral("for"),
        QStringLiteral("friend"), QStringLiteral("goto"), QStringLiteral("if"),
        QStringLiteral("inline"), QStringLiteral("mutable"), QStringLiteral("namespace"),
        QStringLiteral("new"), QStringLiteral("noexcept"), QStringLiteral("not"),
        QStringLiteral("not_eq"), QStringLiteral("nullptr"), QStringLiteral("operator"),
        QStringLiteral("or"), QStringLiteral("or_eq"), QStringLiteral("override"),
        QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("public"),
        QStringLiteral("register"), QStringLiteral("reinterpret_cast"), QStringLiteral("requires"),
        QStringLiteral("return"), QStringLiteral("signed"), QStringLiteral("sizeof"),
        QStringLiteral("static"), QStringLiteral("static_assert"), QStringLiteral("static_cast"),
        QStringLiteral("struct"), QStringLiteral("switch"), QStringLiteral("template"),
        QStringLiteral("this"), QStringLiteral("thread_local"), QStringLiteral("throw"),
        QStringLiteral("true"), QStringLiteral("try"), QStringLiteral("typedef"),
        QStringLiteral("typeid"), QStringLiteral("typename"), QStringLiteral("union"),
        QStringLiteral("unsigned"), QStringLiteral("using"), QStringLiteral("virtual"),
        QStringLiteral("void"), QStringLiteral("volatile"), QStringLiteral("while"),
        QStringLiteral("xor"), QStringLiteral("xor_eq")
    };

    for (const QString &kw : keywords) {
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
    const QStringList types = {
        QStringLiteral("bool"), QStringLiteral("char"), QStringLiteral("char16_t"),
        QStringLiteral("char32_t"), QStringLiteral("char8_t"), QStringLiteral("double"),
        QStringLiteral("float"), QStringLiteral("int"), QStringLiteral("long"),
        QStringLiteral("short"), QStringLiteral("size_t"), QStringLiteral("ssize_t"),
        QStringLiteral("ptrdiff_t"), QStringLiteral("int8_t"), QStringLiteral("int16_t"),
        QStringLiteral("int32_t"), QStringLiteral("int64_t"), QStringLiteral("uint8_t"),
        QStringLiteral("uint16_t"), QStringLiteral("uint32_t"), QStringLiteral("uint64_t"),
        QStringLiteral("wchar_t"), QStringLiteral("std"), QStringLiteral("string"),
        QStringLiteral("wstring"), QStringLiteral("u16string"), QStringLiteral("u32string"),
        QStringLiteral("vector"), QStringLiteral("map"), QStringLiteral("set"),
        QStringLiteral("list"), QStringLiteral("deque"), QStringLiteral("queue"),
        QStringLiteral("stack"), QStringLiteral("array"), QStringLiteral("tuple"),
        QStringLiteral("pair"), QStringLiteral("optional"), QStringLiteral("variant"),
        QStringLiteral("unique_ptr"), QStringLiteral("shared_ptr"), QStringLiteral("weak_ptr"),
        QStringLiteral("function"), QStringLiteral("string_view"), QStringLiteral("span"),
        QStringLiteral("initializer_list"), QStringLiteral("mutex"), QStringLiteral("lock_guard"),
        QStringLiteral("unique_lock"), QStringLiteral("shared_lock"), QStringLiteral("condition_variable"),
        QStringLiteral("promise"), QStringLiteral("future"), QStringLiteral("atomic"),
        QStringLiteral("thread"), QStringLiteral("jthread"), QStringLiteral("filesystem"),
        QStringLiteral("path"), QStringLiteral("error_code"), QStringLiteral("error_category"),
        QStringLiteral("istream"), QStringLiteral("ostream"), QStringLiteral("iostream"),
        QStringLiteral("fstream"), QStringLiteral("sstream"), QStringLiteral("stringstream"),
        QStringLiteral("ifstream"), QStringLiteral("ofstream"), QStringLiteral("QString"),
        QStringLiteral("QWidget"), QStringLiteral("QObject"), QStringLiteral("QVariant"),
        QStringLiteral("QList"), QStringLiteral("QVector"), QStringLiteral("QMap"),
        QStringLiteral("QSet"), QStringLiteral("QHash"), QStringLiteral("QPair"),
        QStringLiteral("QSharedPointer"), QStringLiteral("QScopedPointer")
    };
    for (const QString &t : types) {
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
