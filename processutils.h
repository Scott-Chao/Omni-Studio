#ifndef PROCESSUTILS_H
#define PROCESSUTILS_H

#include <QProcess>

namespace ProcessUtils {

// Safely stop and schedule deletion of a QProcess.
// Disconnects signals first, kills if running, then schedules deleteLater.
// Sets the pointer to nullptr after scheduling deletion.
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

#endif // PROCESSUTILS_H
