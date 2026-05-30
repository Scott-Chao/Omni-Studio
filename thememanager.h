#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QColor>
#include <QPalette>
#include <QMap>
#include <QStringList>

class QWidget;

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum ThemeType { Dark, Light };

    static ThemeManager &instance();

    bool loadTheme(const QString &name);
    QColor color(const QString &token) const;

    // Generate a CSS property: value string from a theme color key.
    // Example: colorStyle("background-color", "editor.background") → "background-color: #1e1e1e;"
    QString colorStyle(const QString &property, const QString &colorKey) const
    {
        return QStringLiteral("%1: %2;").arg(property, color(colorKey).name());
    }

    void setOverride(const QString &token, const QColor &color);
    void clearOverrides();

    QStringList availableThemes() const;
    QString currentThemeName() const;
    ThemeType currentThemeType() const;

    void loadQss();
    void setStyleSheetTarget(QWidget *w);
    void setTabHeight(int h);

    void applyPalette();

    // Convenience: connect themeChanged to a member function slot.
    // Usage: ThemeManager::watchTheme(this, &MyWidget::refreshStyle);
    template<typename Receiver>
    static void watchTheme(Receiver *receiver, void (Receiver::*slot)())
    {
        QObject::connect(&instance(), &ThemeManager::themeChanged, receiver, slot);
    }

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
    int m_tabHeight = 26;
    QWidget *m_qssTarget = nullptr;

    bool loadThemeFromFile(const QString &path, ThemeData &out);
    void initBuiltinThemes();
};

#endif // THEMEMANAGER_H
