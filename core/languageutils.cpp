#include "core/languageutils.h"
#include "editor/cppsyntaxhighlighter.h"
#include "editor/pythonsyntaxhighlighter.h"

namespace LanguageUtils {

const QMap<QString, LanguageInfo> &languageMap()
{
    static const QMap<QString, LanguageInfo> map = {
        {QStringLiteral("cpp"), {
            QStringLiteral("C/C++"),
            {QStringLiteral("cpp"), QStringLiteral("hpp"), QStringLiteral("cxx"),
             QStringLiteral("cc"), QStringLiteral("c"), QStringLiteral("h"),
             QStringLiteral("hxx"), QStringLiteral("hh")},
            [](QTextDocument *doc) -> QSyntaxHighlighter* {
                return new CppSyntaxHighlighter(doc);
            }
        }},
        {QStringLiteral("python"), {
            QStringLiteral("Python"),
            {QStringLiteral("py"), QStringLiteral("pyw"), QStringLiteral("pyx")},
            [](QTextDocument *doc) -> QSyntaxHighlighter* {
                return new PythonSyntaxHighlighter(doc);
            }
        }},
    };
    return map;
}

QSyntaxHighlighter *createHighlighter(const QString &langId, QTextDocument *doc)
{
    const QMap<QString, LanguageInfo> &map = languageMap();
    auto it = map.find(langId);
    if (it != map.cend() && it.value().factory)
        return it.value().factory(doc);
    return nullptr;
}

} // namespace LanguageUtils
