#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

inline void debugLog(const QString &msg, const QString &filePath = QString())
{
    QString path = filePath;
    if (path.isEmpty()) {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + QStringLiteral("/smd-debug");
        QDir().mkpath(dir);
        path = dir + QStringLiteral("/log.txt");
    }
    QFile f(path);
    f.open(QIODevice::Append | QIODevice::Text);
    QTextStream s(&f);
    s << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"))
      << " [" << msg << "]\n";
}

inline void clearLog(const QString &filePath = QString())
{
    QString path = filePath;
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
               + QStringLiteral("/smd-debug/log.txt");
    }
    QFile f(path);
    f.open(QIODevice::WriteOnly | QIODevice::Text);
}

#endif // DEBUGLOG_H
