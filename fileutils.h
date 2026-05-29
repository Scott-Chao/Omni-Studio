#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QString>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include "configmanager.h"

namespace TextFileUtils {

inline QStringList textExtensions()
{
    return ConfigManager::instance().textFileExtensions();
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

inline QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream in(&file);
    return in.readAll();
}

inline bool writeTextFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&file);
    out << content;
    return true;
}

inline bool isSafeRootPath(const QString &rootPath)
{
    return !rootPath.isEmpty()
        && !QDir(rootPath).isRoot()
        && rootPath != QDir::homePath();
}

} // namespace TextFileUtils

#endif // FILEUTILS_H
