#ifndef LANGUAGEUTILS_H
#define LANGUAGEUTILS_H

#include <QString>
#include <QStringList>
#include <QSet>
#include <QMap>
#include <functional>

class QSyntaxHighlighter;
class QTextDocument;

struct LanguageInfo {
    QString displayName;
    QSet<QString> extensions;
    std::function<QSyntaxHighlighter*(QTextDocument*)> factory;
};

namespace LanguageUtils {

const QMap<QString, LanguageInfo> &languageMap();

inline QString languageForExtension(const QString &ext)
{
    const QString lower = ext.toLower();
    const QMap<QString, LanguageInfo> &map = languageMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        if (it.value().extensions.contains(lower))
            return it.key();
    }
    return {};
}

inline bool isCodeFile(const QString &ext)
{
    return !languageForExtension(ext).isEmpty();
}

inline QStringList codeExtensions()
{
    QStringList result;
    const QMap<QString, LanguageInfo> &map = languageMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        for (const QString &ext : it.value().extensions)
            result.append(ext);
    }
    return result;
}

QSyntaxHighlighter *createHighlighter(const QString &langId, QTextDocument *doc);

} // namespace LanguageUtils

#endif // LANGUAGEUTILS_H
