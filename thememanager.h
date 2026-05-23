#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QColor>
#include <QPalette>
#include <QMap>
#include <QStringList>

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum ThemeType { Dark, Light };

    static ThemeManager &instance();

    bool loadTheme(const QString &name);
    QColor color(const QString &token) const;
    void setOverride(const QString &token, const QColor &color);
    void clearOverrides();

    QStringList availableThemes() const;
    QString currentThemeName() const;
    ThemeType currentThemeType() const;

    void loadQss();

    void applyPalette();

signals:
    void themeChanged();

private:
    ThemeManager();
    ~ThemeManager() = default;
    ThemeManager(const ThemeManager &) = delete;
    ThemeManager &operator=(const ThemeManager &) = delete;

    struct ThemeData {
        QString name;
        ThemeType type = Dark;
        QMap<QString, QColor> colors;
    };

    struct ThemeEntry {
        QString name;
        ThemeType type = Dark;
        QString filePath;
    };

    ThemeData m_currentTheme;
    QMap<QString, QColor> m_overrides;
    QList<ThemeEntry> m_builtinThemes;
    bool m_loadingTheme = false;

    bool loadThemeFromFile(const QString &path, ThemeData &out);
    void initBuiltinThemes();
};

#endif // THEMEMANAGER_H
