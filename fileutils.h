#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QString>
#include <QStringList>
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

} // namespace TextFileUtils

#endif // FILEUTILS_H
