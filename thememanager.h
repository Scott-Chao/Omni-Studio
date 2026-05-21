#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QColor>
#include <QMap>
#include <QString>
#include <QTimer>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum Theme { Dark = 0, Light = 1, System = 2 };
    Q_ENUM(Theme)

    static ThemeManager &instance();

    // Current effective theme (resolved: System → Dark/Light)
    Theme currentTheme() const { return m_activeTheme; }
    // Requested mode (could be System)
    Theme requestedTheme() const { return m_requestedTheme; }

    // Switch theme mode. If System, detects and applies automatically.
    void setTheme(Theme theme);

    // Re-detect when mode=System (e.g., on app focus or timer)
    void refreshSystemTheme();

    // Look up a color by semantic key (e.g., "editor.background", "panel.border")
    QColor color(const QString &key) const;
    QString hex(const QString &key) const;  // "#RRGGBB"

    // Convenience: re-apply UI from previously loaded palette
    // Returns true if the theme actually changed
    bool applyCurrentTheme();

signals:
    void themeChanged(ThemeManager::Theme newTheme);

private:
    ThemeManager();
    ~ThemeManager() = default;
    ThemeManager(const ThemeManager &) = delete;
    ThemeManager &operator=(const ThemeManager &) = delete;

    Theme detectSystemTheme() const;  // Windows registry → fallback to time
    void buildPalettes();
    void startAutoRefresh();

    // Palette maps
    QMap<QString, QColor> m_dark;
    QMap<QString, QColor> m_light;

    Theme m_requestedTheme = System;
    Theme m_activeTheme = Dark;

    QTimer *m_refreshTimer = nullptr;
};

#endif // THEMEMANAGER_H
