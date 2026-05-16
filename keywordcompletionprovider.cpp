#include "keywordcompletionprovider.h"
#include <QDebug>
#include <QRegularExpression>

KeywordCompletionProvider::KeywordCompletionProvider(const QString &languageId, QObject *parent)
    : CompletionProvider(parent)
    , m_languageId(languageId)
{
    qDebug() << "KeywordCompletionProvider: created for" << languageId;
}

void KeywordCompletionProvider::requestCompletion(const QString &text, int cursorPos)
{
    // Find current word prefix (the identifier being typed before cursor)
    int start = cursorPos;
    while (start > 0) {
        QChar c = text.at(start - 1);
        if (c != QLatin1Char('_') && !c.isLetterOrNumber())
            break;
        --start;
    }
    QString prefix = text.mid(start, cursorPos - start);

    QList<CompletionItem> items;

    // 1. Keywords
    for (const QString &kw : keywordsForLanguage()) {
        if (kw.startsWith(prefix, Qt::CaseInsensitive)) {
            CompletionItem ci;
            ci.name = kw;
            ci.type = QStringLiteral("keyword");
            items.append(ci);
        }
    }

    // 2. Words from document (excluding the current prefix position)
    for (const QString &word : extractWords(text)) {
        if (word == prefix)
            continue;
        if (word.startsWith(prefix, Qt::CaseInsensitive) && word.length() > prefix.length()) {
            // Deduplicate against keywords
            bool isKeyword = false;
            for (const QString &kw : keywordsForLanguage()) {
                if (word == kw) { isKeyword = true; break; }
            }
            if (isKeyword)
                continue;

            CompletionItem ci;
            ci.name = word;
            ci.type = QStringLiteral("Text");
            items.append(ci);
        }
    }

    // 3. Sort: exact prefix match first, then alphabetical
    std::sort(items.begin(), items.end(),
        [&prefix](const CompletionItem &a, const CompletionItem &b) {
            bool aExact = a.name.compare(prefix, Qt::CaseInsensitive) == 0;
            bool bExact = b.name.compare(prefix, Qt::CaseInsensitive) == 0;
            if (aExact != bExact) return aExact;
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });

    // Limit to top 50 to avoid overwhelming the popup
    if (items.size() > 50)
        items = items.mid(0, 50);

    qDebug() << "KeywordCompletionProvider:" << items.size() << "items for prefix" << prefix;
    emit completionReady(items);
}

void KeywordCompletionProvider::requestHover(const QString &text, int cursorPos)
{
    Q_UNUSED(text);
    Q_UNUSED(cursorPos);
    emit hoverReady({});
}

void KeywordCompletionProvider::requestSignatureHelp(const QString &text, int cursorPos)
{
    Q_UNUSED(text);
    Q_UNUSED(cursorPos);
    emit signatureHelpReady({}, 0);
}

// ---- Keyword lists ----

QStringList KeywordCompletionProvider::keywordsForLanguage() const
{
    if (m_languageId == QStringLiteral("cpp") || m_languageId == QStringLiteral("c")) {
        return {
            QStringLiteral("alignas"), QStringLiteral("alignof"), QStringLiteral("auto"),
            QStringLiteral("bool"), QStringLiteral("break"), QStringLiteral("case"),
            QStringLiteral("catch"), QStringLiteral("char"), QStringLiteral("class"),
            QStringLiteral("const"), QStringLiteral("constexpr"), QStringLiteral("continue"),
            QStringLiteral("decltype"), QStringLiteral("default"), QStringLiteral("delete"),
            QStringLiteral("do"), QStringLiteral("double"), QStringLiteral("else"),
            QStringLiteral("enum"), QStringLiteral("explicit"), QStringLiteral("extern"),
            QStringLiteral("false"), QStringLiteral("float"), QStringLiteral("for"),
            QStringLiteral("friend"), QStringLiteral("goto"), QStringLiteral("if"),
            QStringLiteral("inline"), QStringLiteral("int"), QStringLiteral("long"),
            QStringLiteral("mutable"), QStringLiteral("namespace"), QStringLiteral("new"),
            QStringLiteral("noexcept"), QStringLiteral("nullptr"), QStringLiteral("operator"),
            QStringLiteral("override"), QStringLiteral("private"), QStringLiteral("protected"),
            QStringLiteral("public"), QStringLiteral("return"), QStringLiteral("short"),
            QStringLiteral("signed"), QStringLiteral("sizeof"), QStringLiteral("static"),
            QStringLiteral("static_cast"), QStringLiteral("struct"), QStringLiteral("switch"),
            QStringLiteral("template"), QStringLiteral("this"), QStringLiteral("throw"),
            QStringLiteral("true"), QStringLiteral("try"), QStringLiteral("typedef"),
            QStringLiteral("typename"), QStringLiteral("union"), QStringLiteral("unsigned"),
            QStringLiteral("using"), QStringLiteral("virtual"), QStringLiteral("void"),
            QStringLiteral("volatile"), QStringLiteral("while"),

            // Common C++ standard types
            QStringLiteral("int8_t"), QStringLiteral("int16_t"), QStringLiteral("int32_t"),
            QStringLiteral("int64_t"), QStringLiteral("uint8_t"), QStringLiteral("uint16_t"),
            QStringLiteral("uint32_t"), QStringLiteral("uint64_t"), QStringLiteral("size_t"),
            QStringLiteral("string"), QStringLiteral("vector"), QStringLiteral("map"),
            QStringLiteral("set"), QStringLiteral("list"), QStringLiteral("pair"),
            QStringLiteral("optional"), QStringLiteral("variant"), QStringLiteral("unique_ptr"),
            QStringLiteral("shared_ptr"), QStringLiteral("string_view"), QStringLiteral("span"),
            QStringLiteral("array"), QStringLiteral("tuple"), QStringLiteral("function"),
            QStringLiteral("cout"), QStringLiteral("cin"), QStringLiteral("endl"),

            // Common Qt types (user project uses Qt)
            QStringLiteral("QString"), QStringLiteral("QWidget"), QStringLiteral("QObject"),
            QStringLiteral("QVariant"), QStringLiteral("QList"), QStringLiteral("QVector"),
            QStringLiteral("QMap"), QStringLiteral("QSet"), QStringLiteral("QHash"),
            QStringLiteral("QStringList"), QStringLiteral("QSharedPointer"), QStringLiteral("QScopedPointer"),
            QStringLiteral("QDebug"), QStringLiteral("QFile"), QStringLiteral("QDir"),
            QStringLiteral("QTimer"), QStringLiteral("QProcess"), QStringLiteral("QThread"),
        };
    }

    if (m_languageId == QStringLiteral("python")) {
        return {
            QStringLiteral("False"), QStringLiteral("None"), QStringLiteral("True"),
            QStringLiteral("and"), QStringLiteral("as"), QStringLiteral("assert"),
            QStringLiteral("async"), QStringLiteral("await"), QStringLiteral("break"),
            QStringLiteral("class"), QStringLiteral("continue"), QStringLiteral("def"),
            QStringLiteral("del"), QStringLiteral("elif"), QStringLiteral("else"),
            QStringLiteral("except"), QStringLiteral("finally"), QStringLiteral("for"),
            QStringLiteral("from"), QStringLiteral("global"), QStringLiteral("if"),
            QStringLiteral("import"), QStringLiteral("in"), QStringLiteral("is"),
            QStringLiteral("lambda"), QStringLiteral("nonlocal"), QStringLiteral("not"),
            QStringLiteral("or"), QStringLiteral("pass"), QStringLiteral("raise"),
            QStringLiteral("return"), QStringLiteral("try"), QStringLiteral("while"),
            QStringLiteral("with"), QStringLiteral("yield"),

            // Common builtins
            QStringLiteral("print"), QStringLiteral("len"), QStringLiteral("range"),
            QStringLiteral("int"), QStringLiteral("float"), QStringLiteral("str"),
            QStringLiteral("list"), QStringLiteral("dict"), QStringLiteral("tuple"),
            QStringLiteral("set"), QStringLiteral("bool"), QStringLiteral("bytes"),
            QStringLiteral("open"), QStringLiteral("isinstance"), QStringLiteral("enumerate"),
            QStringLiteral("zip"), QStringLiteral("map"), QStringLiteral("filter"),
            QStringLiteral("sorted"), QStringLiteral("reversed"), QStringLiteral("type"),
            QStringLiteral("super"), QStringLiteral("self"), QStringLiteral("cls"),
            QStringLiteral("ValueError"), QStringLiteral("TypeError"), QStringLiteral("KeyError"),
            QStringLiteral("IndexError"), QStringLiteral("AttributeError"), QStringLiteral("ImportError"),
            QStringLiteral("Exception"), QStringLiteral("BaseException"),
        };
    }

    return {};
}

QStringList KeywordCompletionProvider::extractWords(const QString &text) const
{
    static const QRegularExpression wordRe(QStringLiteral("\\b([A-Za-z_]\\w*)\\b"));
    QSet<QString> words;
    auto it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        QString word = m.captured(1);
        // Skip single chars and numeric literals
        if (word.length() >= 2)
            words.insert(word);
    }
    QStringList sorted = words.values();
    std::sort(sorted.begin(), sorted.end(),
              [](const QString &a, const QString &b) {
                  return a.compare(b, Qt::CaseInsensitive) < 0;
              });
    return sorted;
}
