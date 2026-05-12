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

    // 历史记录
    void setRecentFiles(const QStringList &files);
    QStringList recentFiles() const;

    // 清除所有设置
    void clear();

    // 编辑器默认缩放
    qreal editorDefaultZoom() const;
    void setEditorDefaultZoom(qreal zoom);

    // OpenJudge 自动登录
    void setOpenJudgeAutoLogin(bool enabled);
    bool openJudgeAutoLogin() const;
    void setOpenJudgeCredentials(const QString &username, const QString &password);
    QPair<QString, QString> openJudgeCredentials() const; // <username, password>
    void clearOpenJudgeCredentials();

    // 崩溃恢复 - 恢复文件元数据
    // 每个条目存储为 "recoveryPath|originalPath"，originalPath 为空表示未命名文件
    void setRecoveryFiles(const QList<QPair<QString, QString>> &files);
    QList<QPair<QString, QString>> recoveryFiles() const;
    void clearRecoveryFiles();

private:
    QSettings *m_settings;
};

#endif // SETTINGSMANAGER_H
