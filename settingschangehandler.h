#ifndef SETTINGSCHANGEHANDLER_H
#define SETTINGSCHANGEHANDLER_H

#include <QObject>
#include <QPointer>
#include <QMap>

class TabManager;
class SettingsManager;
class QAction;

class SettingsChangeHandler : public QObject
{
    Q_OBJECT
public:
    SettingsChangeHandler(TabManager *tabManager, SettingsManager *settings,
                          QObject *parent = nullptr);
    ~SettingsChangeHandler() override;

    void handleDefaultZoom(qreal zoom);
    void handleEditorSetting(const QString &key, const QVariant &value);
    void handleAppearanceSetting(const QString &key, const QVariant &value);
    void handleOutputPanelSetting(const QString &key, const QVariant &value);
    void handlePreviewSetting(const QString &key, const QVariant &value);
    void handleSearchSetting(const QString &key, const QVariant &value);
    void handleAiSetting(const QString &key, const QVariant &value);
    void handleToolSetting(const QString &key, const QVariant &value);
    void handleShortcutChanged(const QString &actionKey, const QString &keySequenceText,
                               const QMap<QString, QAction*> &shortcutActions);
    void handleResetToDefaults(const QMap<QString, QAction*> &shortcutActions);

signals:
    void equalWidthTabRequested(bool enabled);
    void zoomLabelUpdateRequested();
    void resetComplete();

private:
    void applyToAllEditors(const std::function<void(class EditorWidget*)> &fn);

    QPointer<TabManager> m_tabManager;
    QPointer<SettingsManager> m_settings;
};

#endif // SETTINGSCHANGEHANDLER_H
