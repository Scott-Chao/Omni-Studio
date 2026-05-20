#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

inline void debugLog(const QString &msg)
{
    // Write to temp dir — always writable on all platforms
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                  + QStringLiteral("/smd-debug");
    QDir().mkpath(dir);
    QFile f(dir + QStringLiteral("/log.txt"));
    f.open(QIODevice::Append | QIODevice::Text);
    QTextStream s(&f);
    s << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"))
      << " [" << msg << "]\n";
}

#endif // DEBUGLOG_H
