#include "settingsmanager.h"
#include "configmanager.h"
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QJsonArray>

static const QString KEY_GEOMETRY    = "window/geometry";
static const QString KEY_SPLITTER    = "window/splitterState";
static const QString KEY_LAST_FOLDER = "file/lastFolderPath";
static const QString KEY_LAST_SAVE_AS_FOLDER = "LastSaveAsFolder";
static const QString KEY_OVERRIDES = "settings_overrides";
static const QString KEY_DEFAULT_ZOOM   = "editor/defaultZoom";

SettingsManager *SettingsManager::s_instance = nullptr;

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
    s_instance = this;

    // 预加载已持久化的覆盖值到内存 map
    m_settings->beginGroup(KEY_OVERRIDES);
    const QStringList keys = m_settings->childKeys();
    for (const QString &key : keys)
        m_overrideMap[key] = m_settings->value(key);
    m_settings->endGroup();

    m_cachedDefaultZoom = m_settings->value(KEY_DEFAULT_ZOOM, 1.0).toDouble();
}

SettingsManager::~SettingsManager()
{
    delete m_settings;
    if (s_instance == this)
        s_instance = nullptr;
}

SettingsManager &SettingsManager::instance()
{
    return *s_instance;
}

QVariant SettingsManager::value(const QString &key, const QVariant &defaultValue) const
{
    // 1. 检查用户覆盖值
    QVariant override = settingOverride(key);
    if (override.isValid())
        return override;

    // 2. 回退到 ConfigManager 的静态默认值
    QJsonValue jsonVal = ConfigManager::instance().resolvePath(key);
    if (jsonVal.isUndefined() || jsonVal.isNull())
        return defaultValue;

    switch (jsonVal.type()) {
    case QJsonValue::String: return jsonVal.toString();
    case QJsonValue::Double: return jsonVal.toDouble();
    case QJsonValue::Bool:   return jsonVal.toBool();
    case QJsonValue::Array:  return jsonVal.toArray().toVariantList();
    case QJsonValue::Object: return jsonVal.toObject().toVariantMap();
    default: return defaultValue;
    }
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
    int maxEntries = ConfigManager::instance().historyMaxEntries();
    QStringList trimmed = files.mid(0, maxEntries);
    m_settings->setValue("History/recentFiles", trimmed);
}

QStringList SettingsManager::recentFiles() const {
    return m_settings->value("History/recentFiles").toStringList();
}

void SettingsManager::setSettingOverride(const QString &key, const QVariant &value)
{
    m_overrideMap[key] = value;
}

QVariant SettingsManager::settingOverride(const QString &key, const QVariant &defaultValue) const
{
    auto it = m_overrideMap.find(key);
    if (it != m_overrideMap.end())
        return it.value();
    return defaultValue;
}

void SettingsManager::removeSettingOverride(const QString &key)
{
    m_overrideMap.remove(key);
}

QStringList SettingsManager::allOverrideKeys() const
{
    return m_overrideMap.keys();
}

void SettingsManager::clear()
{
    m_overrideMap.clear();
    m_settings->clear();
}

void SettingsManager::flushOverrides()
{
    // 将所有 pending override 写入 QSettings
    m_settings->beginGroup(KEY_OVERRIDES);
    m_settings->remove(QString()); // 清空旧值
    for (auto it = m_overrideMap.constBegin(); it != m_overrideMap.constEnd(); ++it)
        m_settings->setValue(it.key(), it.value());
    m_settings->endGroup();

    // 写 editor/defaultZoom
    m_settings->setValue(KEY_DEFAULT_ZOOM, m_cachedDefaultZoom);

    m_settings->sync();
}

qreal SettingsManager::editorDefaultZoom() const
{
    return m_cachedDefaultZoom;
}

void SettingsManager::setEditorDefaultZoom(qreal zoom)
{
    m_cachedDefaultZoom = zoom;
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

// ---- Auto-Save / Crash Recovery ----
static const QString KEY_RECOVERY_FILES = "AutoSave/recoveryFiles";
static const QString RECOVERY_SEPARATOR = QStringLiteral("|");

void SettingsManager::setRecoveryFiles(const QList<QPair<QString, QString>> &files)
{
    QStringList encoded;
    for (const auto &pair : files) {
        encoded.append(pair.first + RECOVERY_SEPARATOR + pair.second);
    }
    m_settings->setValue(KEY_RECOVERY_FILES, encoded);
}

QList<QPair<QString, QString>> SettingsManager::recoveryFiles() const
{
    QStringList encoded = m_settings->value(KEY_RECOVERY_FILES).toStringList();
    QList<QPair<QString, QString>> result;
    for (const QString &entry : encoded) {
        int sepIdx = entry.indexOf(RECOVERY_SEPARATOR);
        if (sepIdx >= 0) {
            QString recoveryPath = entry.left(sepIdx);
            QString originalPath = entry.mid(sepIdx + 1);
            result.append({recoveryPath, originalPath});
        }
    }
    return result;
}

void SettingsManager::clearRecoveryFiles()
{
    m_settings->remove(KEY_RECOVERY_FILES);
}
