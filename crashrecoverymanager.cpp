#include "crashrecoverymanager.h"
#include "configmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDateTime>

CrashRecoveryManager::CrashRecoveryManager(QObject *parent)
    : QObject(parent)
{
}

QString CrashRecoveryManager::recoveryDirectoryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + QStringLiteral("/SM-Recovery");
}

void CrashRecoveryManager::cleanStaleRecoveryFiles()
{
    QDir dir(recoveryDirectoryPath());
    if (!dir.exists())
        return;

    int maxAgeHours = ConfigManager::instance().autoSaveRecoveryMaxAgeHours();
    qint64 cutoff = QDateTime::currentSecsSinceEpoch() - (maxAgeHours * 3600);

    const QStringList entries = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        QString filePath = dir.absoluteFilePath(entry);
        QFileInfo info(filePath);
        if (info.lastModified().toSecsSinceEpoch() < cutoff) {
            QFile::remove(filePath);
        }
    }

    if (dir.entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
        dir.removeRecursively();
    }
}

void CrashRecoveryManager::clearRecoveryDirectory()
{
    QDir dir(recoveryDirectoryPath());
    if (dir.exists())
        dir.removeRecursively();
}

bool CrashRecoveryManager::hasRecoveryFiles() const
{
    QDir dir(recoveryDirectoryPath());
    if (!dir.exists())
        return false;
    return !dir.entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty();
}
