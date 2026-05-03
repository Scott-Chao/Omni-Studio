#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QByteArray>
#include <QString>
#include <QVariant>

class QSettings;

class SettingsManager
{
public:
    explicit SettingsManager(const QString &fileName = QString());
    ~SettingsManager();

    // 窗口状态
    void setWindowGeometry(const QByteArray &geometry);
    QByteArray windowGeometry() const;

    // 分隔条位置
    void setSplitterState(const QByteArray &state);
    QByteArray splitterState() const;

    // 上一次打开目录
    void setLastFolderPath(const QString &path);
    QString lastFolderPath(const QString &defaultPath = QString()) const;

    // 上一次另存为目录
    void setLastSaveAsFolderPath(const QString &path);
    QString lastSaveAsFolderPath(const QString &defaultPath = QString()) const;

    // 清除所有设置
    void clear();

private:
    QSettings *m_settings;
};

#endif // SETTINGSMANAGER_H
