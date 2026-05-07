#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QString>
#include <QStringList>

namespace TextFileUtils {

inline QStringList textExtensions()
{
    static const QStringList exts = {
        "md", "markdown", "txt",
        "c", "cpp", "cxx", "cc", "h", "hpp", "hxx", "hh",
        "cs", "java", "py", "pyw", "js", "jsx", "ts", "tsx", "mjs",
        "rs", "go", "rb", "php", "swift", "kt", "kts",
        "html", "htm", "css", "scss", "sass", "less",
        "xml", "svg", "json", "yaml", "yml", "toml", "ini", "cfg", "conf",
        "rst", "tex", "log", "csv", "tsv",
        "sql", "graphql", "proto",
        "sh", "bash", "zsh", "fish", "ps1", "bat", "cmd",
        "cmake", "mak", "mk",
        "pro", "pri", "qml", "qrc", "ui",
        "diff", "patch"
    };
    return exts;
}

inline QStringList scanNameFilters()
{
    static const QStringList filters = []() {
        QStringList list;
        for (const QString &ext : textExtensions())
            list << "*." + ext;
        return list;
    }();
    return filters;
}

inline bool isTextExtension(const QString &suffix)
{
    return textExtensions().contains(suffix.toLower());
}

} // namespace TextFileUtils

#endif // FILEUTILS_H
