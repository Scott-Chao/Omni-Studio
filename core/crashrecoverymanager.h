#ifndef CRASHRECOVERYMANAGER_H
#define CRASHRECOVERYMANAGER_H

#include <QObject>
#include <QString>

class CrashRecoveryManager : public QObject
{
    Q_OBJECT
public:
    explicit CrashRecoveryManager(QObject *parent = nullptr);

    // Remove stale recovery files older than configured max age
    void cleanStaleRecoveryFiles();

    // Remove the entire recovery directory
    void clearRecoveryDirectory();

    // Check if recovery directory has any files
    bool hasRecoveryFiles() const;

    // Get the recovery directory path
    static QString recoveryDirectoryPath();

signals:
    void recoveryFilesCleaned();
};

#endif // CRASHRECOVERYMANAGER_H
