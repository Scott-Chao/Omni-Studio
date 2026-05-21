#include "thememanager.h"
#include "settingsmanager.h"
#include "configmanager.h"

#include <QTime>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ThemeManager &ThemeManager::instance()
{
    static ThemeManager inst;
    return inst;
}

ThemeManager::ThemeManager()
    : QObject(nullptr)
{
    buildPalettes();
    // Start in Dark until loadFromSettings() is called by MainWindow
    m_activeTheme = Dark;
    m_requestedTheme = System;
}

void ThemeManager::setTheme(Theme theme)
{
    m_requestedTheme = theme;
    if (theme == System) {
        theme = detectSystemTheme();
    }
    if (m_activeTheme != theme) {
        m_activeTheme = theme;
        emit themeChanged(m_activeTheme);
    }
    startAutoRefresh();
}

void ThemeManager::refreshSystemTheme()
{
    if (m_requestedTheme != System)
        return;
    Theme detected = detectSystemTheme();
    if (m_activeTheme != detected) {
        m_activeTheme = detected;
        emit themeChanged(m_activeTheme);
    }
}

bool ThemeManager::applyCurrentTheme()
{
    // Re-emit themeChanged so all widgets reload even if theme didn't change
    emit themeChanged(m_activeTheme);
    return true;
}

ThemeManager::Theme ThemeManager::detectSystemTheme() const
{
#ifdef Q_OS_WIN
    HKEY hKey;
    LONG ret = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0,
        KEY_READ,
        &hKey);
    if (ret == ERROR_SUCCESS) {
        DWORD value = 0;
        DWORD size = sizeof(value);
        ret = RegGetValueW(hKey, nullptr, L"AppsUseLightTheme",
                           RRF_RT_DWORD, nullptr, &value, &size);
        RegCloseKey(hKey);
        if (ret == ERROR_SUCCESS)
            return (value == 1) ? Light : Dark;
    }
#endif
    // Fallback: 6:00–18:00 → Light, otherwise → Dark
    QTime now = QTime::currentTime();
    return (now >= QTime(6, 0) && now < QTime(18, 0)) ? Light : Dark;
}

void ThemeManager::startAutoRefresh()
{
    if (!m_refreshTimer) {
        m_refreshTimer = new QTimer(this);
        m_refreshTimer->setInterval(300000); // 5 minutes
        connect(m_refreshTimer, &QTimer::timeout, this, &ThemeManager::refreshSystemTheme);
    }
    if (m_requestedTheme == System)
        m_refreshTimer->start();
    else
        m_refreshTimer->stop();
}

QColor ThemeManager::color(const QString &key) const
{
    const auto &palette = (m_activeTheme == Light) ? m_light : m_dark;
    auto it = palette.find(key);
    if (it != palette.end())
        return it.value();
    return QColor("#FF00FF"); // magenta = missing key, easily spotted during dev
}

QString ThemeManager::hex(const QString &key) const
{
    return color(key).name();
}

// ============================================================
// Palette definitions
// ============================================================

void ThemeManager::buildPalettes()
{
    // ---- Editor ----
    m_dark["editor.background"]            = QColor("#1E1E1E");
    m_dark["editor.foreground"]            = QColor("#D4D4D4");
    m_dark["editor.selection"]             = QColor("#264F78");
    m_dark["editor.line_highlight"]        = QColor("#2A2D2E");
    m_dark["editor.line_number_bg"]        = QColor("#252525");
    m_dark["editor.line_number_fg"]        = QColor("#858585");

    m_light["editor.background"]           = QColor("#FFFFFF");
    m_light["editor.foreground"]           = QColor("#333333");
    m_light["editor.selection"]            = QColor("#ADD6FF");
    m_light["editor.line_highlight"]       = QColor("#F5F5F5");
    m_light["editor.line_number_bg"]       = QColor("#F3F3F3");
    m_light["editor.line_number_fg"]       = QColor("#666666");

    // ---- Output Panel ----
    m_dark["output.background"]            = QColor("#1E1E1E");
    m_dark["output.foreground"]            = QColor("#D4D4D4");
    m_dark["output.selection"]             = QColor("#264F78");
    m_dark["output.stderr"]                = QColor("#F48771");
    m_dark["output.error_status"]          = QColor("#F48771");
    m_dark["output.success_status"]        = QColor("#6A9955");

    m_light["output.background"]           = QColor("#FFFFFF");
    m_light["output.foreground"]           = QColor("#333333");
    m_light["output.selection"]            = QColor("#ADD6FF");
    m_light["output.stderr"]               = QColor("#C41A16");
    m_light["output.error_status"]         = QColor("#C41A16");
    m_light["output.success_status"]       = QColor("#008000");

    // ---- Search ----
    m_dark["search.highlight_bg"]          = QColor("#FFD700");
    m_dark["search.highlight_fg"]          = QColor("#000000");

    m_light["search.highlight_bg"]         = QColor("#FFFF00");
    m_light["search.highlight_fg"]         = QColor("#000000");

    // ---- Preview ----
    m_dark["preview.container_bg"]         = QColor("#2d2d2d");
    m_dark["preview.webengine_bg"]         = QColor("#2d2d2d");

    m_light["preview.container_bg"]        = QColor("#FFFFFF");
    m_light["preview.webengine_bg"]        = QColor("#FFFFFF");

    // ---- Panels (sidebars, left/right panels) ----
    m_dark["panel.background"]             = QColor("#2b2b2b");
    m_dark["panel.foreground"]             = QColor("#cccccc");
    m_dark["panel.border"]                 = QColor("#3c3c3c");
    m_dark["panel.title_bg"]               = QColor("#333333");
    m_dark["panel.hover"]                  = QColor("#2a2a2a");

    m_light["panel.background"]            = QColor("#F3F3F3");
    m_light["panel.foreground"]            = QColor("#333333");
    m_light["panel.border"]                = QColor("#E0E0E0");
    m_light["panel.title_bg"]              = QColor("#E8E8E8");
    m_light["panel.hover"]                 = QColor("#E8E8E8");

    // ---- ActivityBar ----
    m_dark["activitybar.background"]       = QColor("#333337");
    m_dark["activitybar.button_hover"]     = QColor("#2d2d2d");
    m_dark["activitybar.active_border"]    = QColor("#0078D4");

    m_light["activitybar.background"]      = QColor("#F3F3F3");
    m_light["activitybar.button_hover"]    = QColor("#E8E8E8");
    m_light["activitybar.active_border"]   = QColor("#0078D4");

    // ---- Scrollbar ----
    m_dark["scrollbar.track"]              = QColor("#1E1E1E");
    m_dark["scrollbar.handle"]             = QColor("#555555");

    m_light["scrollbar.track"]             = QColor("#F0F0F0");
    m_light["scrollbar.handle"]            = QColor("#C1C1C1");

    // ---- Input / LineEdit / ComboBox ----
    m_dark["input.background"]             = QColor("#3c3c3c");
    m_dark["input.foreground"]             = QColor("#cccccc");
    m_dark["input.border"]                 = QColor("#555555");
    m_dark["input.focus_border"]           = QColor("#0078D4");

    m_light["input.background"]            = QColor("#FFFFFF");
    m_light["input.foreground"]            = QColor("#333333");
    m_light["input.border"]                = QColor("#CECECE");
    m_light["input.focus_border"]          = QColor("#0078D4");

    // ---- Button ----
    m_dark["button.background"]            = QColor("#3c3c3c");
    m_dark["button.foreground"]            = QColor("#cccccc");
    m_dark["button.hover_bg"]              = QColor("#094771");
    m_dark["button.hover_border"]          = QColor("#0078D4");

    m_light["button.background"]           = QColor("#E8E8E8");
    m_light["button.foreground"]           = QColor("#333333");
    m_light["button.hover_bg"]             = QColor("#D0D0D0");
    m_light["button.hover_border"]         = QColor("#0078D4");

    // ---- Tab ----
    m_dark["tab.background"]               = QColor("#2d2d2d");
    m_dark["tab.active_bg"]                = QColor("#1E1E1E");
    m_dark["tab.hover_bg"]                 = QColor("#4a4a4a");
    m_dark["tab.border"]                   = QColor("#3c3c3c");

    m_light["tab.background"]              = QColor("#ECECEC");
    m_light["tab.active_bg"]               = QColor("#FFFFFF");
    m_light["tab.hover_bg"]                = QColor("#E0E0E0");
    m_light["tab.border"]                  = QColor("#E0E0E0");

    // ---- Overlay ----
    m_dark["overlay.background"]           = QColor(0, 0, 0, 128);
    m_light["overlay.background"]          = QColor(0, 0, 0, 48);

    // ---- TreeView / File Explorer ----
    m_dark["treeview.background"]          = QColor("#252526");
    m_dark["treeview.foreground"]          = QColor("#cccccc");
    m_dark["treeview.hover"]               = QColor("#2a2d2e");

    m_light["treeview.background"]         = QColor("#F3F3F3");
    m_light["treeview.foreground"]         = QColor("#333333");
    m_light["treeview.hover"]              = QColor("#E8E8E8");

    // ---- Separator ----
    m_dark["separator"]                    = QColor("#3c3c3c");
    m_light["separator"]                   = QColor("#E0E0E0");

    // ---- Labels / Text ----
    m_dark["label.foreground"]             = QColor("#cccccc");
    m_dark["label.secondary"]              = QColor("#999999");
    m_dark["label.title"]                  = QColor("#e0e0e0");

    m_light["label.foreground"]            = QColor("#555555");
    m_light["label.secondary"]             = QColor("#888888");
    m_light["label.title"]                 = QColor("#333333");

    // ---- Accent colors ----
    m_dark["accent.primary"]               = QColor("#0078D4");
    m_dark["accent.error"]                 = QColor("#F44747");
    m_dark["accent.warning"]               = QColor("#CCA700");
    m_dark["accent.success"]               = QColor("#6A9955");
    m_dark["accent.info"]                  = QColor("#2196F3");

    m_light["accent.primary"]              = QColor("#0078D4");
    m_light["accent.error"]                = QColor("#E74C3C");
    m_light["accent.warning"]              = QColor("#CA8500");
    m_light["accent.success"]              = QColor("#008000");
    m_light["accent.info"]                 = QColor("#2196F3");

    // ---- Close button ----
    m_dark["close.hover_bg"]               = QColor("#c42b1c");
    m_light["close.hover_bg"]              = QColor("#E81123");

    // ---- Slider ----
    m_dark["slider.groove"]                = QColor("#3c3c3c");
    m_dark["slider.handle"]                = QColor("#cccccc");

    m_light["slider.groove"]               = QColor("#D0D0D0");
    m_light["slider.handle"]               = QColor("#666666");

    // ---- Toggle switch ----
    m_dark["toggle.active"]                = QColor("#2196F3");
    m_dark["toggle.inactive"]              = QColor("#999999");
    m_light["toggle.active"]               = QColor("#0078D4");
    m_light["toggle.inactive"]             = QColor("#CCCCCC");

    // ---- Cell (SMD / Completion popup) ----
    m_dark["cell.background"]              = QColor("#252526");
    m_dark["cell.badge_md"]                = QColor("#569CD6");
    m_dark["cell.badge_cpp"]               = QColor("#4EC9B0");
    m_dark["cell.badge_python"]            = QColor("#DCDCAA");
    m_dark["cell.border_edit"]             = QColor("#0078D4");
    m_dark["cell.border_command"]          = QColor("#C586C0");

    m_light["cell.background"]             = QColor("#F8F8F8");
    m_light["cell.badge_md"]               = QColor("#569CD6");
    m_light["cell.badge_cpp"]              = QColor("#267F99");
    m_light["cell.badge_python"]           = QColor("#DCDCAA");
    m_light["cell.border_edit"]            = QColor("#0078D4");
    m_light["cell.border_command"]         = QColor("#C586C0");

    // ---- Chat (AI panel) ----
    m_dark["chat.user_bubble"]             = QColor("#3c3c3c");
    m_dark["chat.assistant_bubble"]        = QColor("#2d2d2d");
    m_dark["chat.background"]              = QColor("#1E1E1E");

    m_light["chat.user_bubble"]            = QColor("#E8E8E8");
    m_light["chat.assistant_bubble"]       = QColor("#F3F3F3");
    m_light["chat.background"]             = QColor("#FFFFFF");

    // ---- Title bar ----
    m_dark["titlebar.background"]          = QColor("#2d2d2d");
    m_dark["titlebar.foreground"]          = QColor("#cccccc");
    m_dark["titlebar.button_hover"]        = QColor("#4a4a4a");

    m_light["titlebar.background"]         = QColor("#ECECEC");
    m_light["titlebar.foreground"]         = QColor("#333333");
    m_light["titlebar.button_hover"]       = QColor("#E0E0E0");

    // ---- Menu ----
    m_dark["menu.background"]              = QColor("#2d2d2d");
    m_dark["menu.foreground"]              = QColor("#cccccc");
    m_dark["menu.hover"]                   = QColor("#094771");

    m_light["menu.background"]             = QColor("#FFFFFF");
    m_light["menu.foreground"]             = QColor("#333333");
    m_light["menu.hover"]                  = QColor("#E8E8E8");

    // ---- Status bar / Bottom Panel ----
    m_dark["statusbar.background"]         = QColor("#007ACC");
    m_dark["statusbar.foreground"]         = QColor("#FFFFFF");

    m_light["statusbar.background"]        = QColor("#0078D4");
    m_light["statusbar.foreground"]        = QColor("#FFFFFF");

    // ---- Bottom Panel tabs ----
    m_dark["bottom.tab_active"]            = QColor("#1E1E1E");
    m_dark["bottom.tab_inactive"]          = QColor("#2d2d2d");
    m_light["bottom.tab_active"]           = QColor("#FFFFFF");
    m_light["bottom.tab_inactive"]         = QColor("#ECECEC");
}
