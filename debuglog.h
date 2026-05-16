#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QThread>

static inline void debugLog(const QString &msg)
{
    QFile file(QCoreApplication::applicationDirPath()
               + QStringLiteral("/smd_debug.log"));
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        {
            QTextStream ts(&file);
            ts << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"))
               << QStringLiteral(" [%1] ").arg(reinterpret_cast<quintptr>(QThread::currentThread()), 0, 16)
               << msg << QStringLiteral("\n");
            ts.flush();
        } // ts destructor runs here, before file.close()
        file.close();
    }
}

#endif // DEBUGLOG_H
