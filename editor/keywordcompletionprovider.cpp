#include "keywordcompletionprovider.h"
#include "../lsp/keywords.h"
#include <QRegularExpression>

KeywordCompletionProvider::KeywordCompletionProvider(const QString &languageId, QObject *parent)
    : CompletionProvider(parent)
    , m_languageId(languageId)
{
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
        return cppKeywords() + cppCommonTypes();
    }

    if (m_languageId == QStringLiteral("python")) {
        return pyKeywords() + pyBuiltins() + QStringList{
            QStringLiteral("self"), QStringLiteral("cls"),
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
