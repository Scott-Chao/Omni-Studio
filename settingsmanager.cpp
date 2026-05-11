#include "settingsmanager.h"
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

static const QString KEY_GEOMETRY    = "window/geometry";
static const QString KEY_SPLITTER    = "window/splitterState";
static const QString KEY_LAST_FOLDER = "file/lastFolderPath";
static const QString KEY_LAST_SAVE_AS_FOLDER = "LastSaveAsFolder";

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

void SettingsManager::setLastSaveAsFolderPath(const QString &path)
{
    // 设置上次另存为路径
    m_settings->setValue(KEY_LAST_SAVE_AS_FOLDER, path);
}

QString SettingsManager::lastSaveAsFolderPath(const QString &defaultPath) const
{
    // 返回上次另存为路径
    return m_settings->value(KEY_LAST_SAVE_AS_FOLDER, defaultPath).toString();
}

void SettingsManager::setRecentFiles(const QStringList &files) {
    // 只保留前 MaxRecentFiles 条
    QStringList trimmed = files.mid(0, MaxRecentFiles);
    m_settings->setValue("History/recentFiles", trimmed);
}

QStringList SettingsManager::recentFiles() const {
    return m_settings->value("History/recentFiles").toStringList();
}

void SettingsManager::clear()
{
    // 清空设置
    m_settings->clear();
}

static const QString KEY_OJ_AUTO_LOGIN  = "OpenJudge/autoLogin";
static const QString KEY_OJ_USERNAME    = "OpenJudge/username";
static const QString KEY_OJ_PASSWORD    = "OpenJudge/password";

static QString obfuscate(const QString &text)
{
    return QString::fromLatin1(text.toUtf8().toBase64());
}

static QString deobfuscate(const QString &encoded)
{
    return QString::fromUtf8(QByteArray::fromBase64(encoded.toLatin1()));
}

void SettingsManager::setOpenJudgeAutoLogin(bool enabled)
{
    m_settings->setValue(KEY_OJ_AUTO_LOGIN, enabled);
}

bool SettingsManager::openJudgeAutoLogin() const
{
    return m_settings->value(KEY_OJ_AUTO_LOGIN, false).toBool();
}

void SettingsManager::setOpenJudgeCredentials(const QString &username, const QString &password)
{
    m_settings->setValue(KEY_OJ_USERNAME, username);
    m_settings->setValue(KEY_OJ_PASSWORD, obfuscate(password));
}

QPair<QString, QString> SettingsManager::openJudgeCredentials() const
{
    QString username = m_settings->value(KEY_OJ_USERNAME).toString();
    QString password = deobfuscate(m_settings->value(KEY_OJ_PASSWORD).toString());
    return {username, password};
}

void SettingsManager::clearOpenJudgeCredentials()
{
    m_settings->remove(KEY_OJ_USERNAME);
    m_settings->remove(KEY_OJ_PASSWORD);
}
