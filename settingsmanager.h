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

    void setSplitterState(const QByteArray &state);
    QByteArray splitterState() const;

    // 文件浏览
    void setLastFolderPath(const QString &path);
    QString lastFolderPath(const QString &defaultPath = QString()) const;

    // 清除所有设置
    void clear();

private:
    QSettings *m_settings;
};

#endif // SETTINGSMANAGER_H
