#ifndef UTILITIES_H
#define UTILITIES_H

#include <QString>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QJsonDocument>
#include <QProcess>
#include "configmanager.h"

// ── String utilities ────────────────────────────────────────────────────────

namespace StringUtils {

// Normalize line-endings and lone surrogates for Python consumption.
inline QString sanitizeForPython(const QString &s)
{
    QString out = s;
    out.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    out.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    for (int i = 0; i < out.size(); ++i) {
        QChar ch = out.at(i);
        if (ch.isLowSurrogate() && (i == 0 || !out.at(i - 1).isHighSurrogate()))
            out[i] = QChar(QChar::ReplacementCharacter);
        else if (ch.isHighSurrogate()
                 && (i + 1 >= out.size() || !out.at(i + 1).isLowSurrogate()))
            out[i] = QChar(QChar::ReplacementCharacter);
    }
    out = QString::fromUtf8(out.toUtf8());
    return out;
}

// Convert LSP CompletionItemKind integer code to human-readable string.
inline QString completionKindToString(int kind)
{
    switch (kind) {
    case 1:  return QStringLiteral("Text");
    case 2:  return QStringLiteral("Method");
    case 3:  return QStringLiteral("Function");
    case 4:  return QStringLiteral("Constructor");
    case 5:  return QStringLiteral("Field");
    case 6:  return QStringLiteral("Variable");
    case 7:  return QStringLiteral("Class");
    case 8:  return QStringLiteral("Interface");
    case 9:  return QStringLiteral("Module");
    case 10: return QStringLiteral("Property");
    case 11: return QStringLiteral("Unit");
    case 12: return QStringLiteral("Value");
    case 13: return QStringLiteral("Enum");
    case 14: return QStringLiteral("Keyword");
    case 15: return QStringLiteral("Snippet");
    case 18: return QStringLiteral("Reference");
    case 20: return QStringLiteral("EnumMember");
    case 21: return QStringLiteral("Constant");
    case 22: return QStringLiteral("Struct");
    case 23: return QStringLiteral("Event");
    case 24: return QStringLiteral("Operator");
    case 25: return QStringLiteral("TypeParameter");
    default: return QStringLiteral("Text");
    }
}

} // namespace StringUtils

// ── File utilities ──────────────────────────────────────────────────────────

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

inline QJsonDocument readJsonFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll());
}

inline bool writeJsonFile(const QString &path, const QJsonDocument &doc)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

} // namespace TextFileUtils

// ── Process utilities ───────────────────────────────────────────────────────

namespace ProcessUtils {

// Safely stop and schedule deletion of a QProcess.
inline void cleanup(QProcess *&process)
{
    if (!process)
        return;
    process->disconnect();
    if (process->state() != QProcess::NotRunning)
        process->kill();
    process->deleteLater();
    process = nullptr;
}

} // namespace ProcessUtils

#endif // UTILITIES_H
