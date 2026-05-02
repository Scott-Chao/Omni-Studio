#include "settingsmanager.h"
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

static const QString KEY_GEOMETRY    = "window/geometry";
static const QString KEY_SPLITTER    = "window/splitterState";
static const QString KEY_LAST_FOLDER = "file/lastFolderPath";

SettingsManager::SettingsManager(const QString &fileName)
{
    if (fileName.isEmpty()) {
        // 默认将配置文件保存在应用程序所在目录的 config.ini
        QString appDir = QCoreApplication::applicationDirPath();
        QString iniPath = appDir + "/config.ini";
        m_settings = new QSettings(iniPath, QSettings::IniFormat);
    } else {
        m_settings = new QSettings(fileName, QSettings::IniFormat);
    }
}

SettingsManager::~SettingsManager()
{
    delete m_settings;
}

void SettingsManager::setWindowGeometry(const QByteArray &geometry)
{
    // 设置窗口几何信息
    m_settings->setValue(KEY_GEOMETRY, geometry);
}

QByteArray SettingsManager::windowGeometry() const
{
    // 返回窗口几何信息
    return m_settings->value(KEY_GEOMETRY).toByteArray();
}

void SettingsManager::setSplitterState(const QByteArray &state)
{
    // 设置分隔条位置
    m_settings->setValue(KEY_SPLITTER, state);
}

QByteArray SettingsManager::splitterState() const
{
    // 返回分隔条位置
    return m_settings->value(KEY_SPLITTER).toByteArray();
}

void SettingsManager::setLastFolderPath(const QString &path)
{
    // 设置上次打开的文件夹路径
    m_settings->setValue(KEY_LAST_FOLDER, path);
}

QString SettingsManager::lastFolderPath(const QString &defaultPath) const
{
    // 返回上次打开的文件夹路径
    QString path = m_settings->value(KEY_LAST_FOLDER).toString();
    if (path.isEmpty() || !QDir(path).exists()) {
        if (defaultPath.isEmpty())
            return QDir::homePath();
        return defaultPath;
    }
    return path;
}

void SettingsManager::clear()
{
    // 清空设置
    m_settings->clear();
}
