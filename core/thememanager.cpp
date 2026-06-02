#include "thememanager.h"
#include "config/settingsmanager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QDebug>
#include <QRegularExpression>
#include <QWidget>

ThemeManager &ThemeManager::instance()
{
    static ThemeManager inst;
    return inst;
}

ThemeManager::ThemeManager()
{
    initBuiltinThemes();

    // Load first built-in theme as default
    if (!m_builtinThemes.isEmpty()) {
        loadTheme(m_builtinThemes.first().name);
    }
}

void ThemeManager::initBuiltinThemes()
{
    m_builtinThemes = {
        { QStringLiteral("2026 Dark"),  Dark,
          QStringLiteral(":/themes/dark-vscode.json") },
        { QStringLiteral("2026 Light"), Light,
          QStringLiteral(":/themes/light-vscode.json") },
        { QStringLiteral("Custom"),     Dark,  QString() },
    };
}

bool ThemeManager::loadTheme(const QString &name)
{
    // Guard against re-entrant calls (e.g. triggered by a slot connected to
    // themeChanged() that somehow calls loadTheme() again).
    if (m_loadingTheme) {
        qWarning() << "[ThemeManager] Re-entrant loadTheme(\"" << name
                   << "\") blocked — already loading \"" << m_currentTheme.name << "\"";
        return false;
    }
    m_loadingTheme = true;

    for (const auto &entry : m_builtinThemes) {
        if (entry.name == name) {
            ThemeData data;

            if (name == QStringLiteral("Custom")) {
                // Custom theme: load dark as base, apply saved overrides
                QString darkPath;
                for (const auto &e : m_builtinThemes) {
                    if (e.type == Dark && !e.filePath.isEmpty()) {
                        darkPath = e.filePath;
                        break;
                    }
                }
                if (darkPath.isEmpty() || !loadThemeFromFile(darkPath, data)) {
                    m_loadingTheme = false;
                    return false;
                }
                data.name = QStringLiteral("Custom");
                data.type = Dark;
            } else {
                if (!loadThemeFromFile(entry.filePath, data)) {
                    m_loadingTheme = false;
                    return false;
                }
            }

            m_currentTheme = data;
            m_overrides.clear();

            if (name == QStringLiteral("Custom"))
                loadOverridesFromSettings();

            applyPalette();
            loadQss();
            emit themeChanged();
            m_loadingTheme = false;
            return true;
        }
    }

    qWarning() << "[ThemeManager] Unknown theme:" << name;
    m_loadingTheme = false;
    return false;
}

bool ThemeManager::loadThemeFromFile(const QString &path, ThemeData &out)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[ThemeManager] Cannot open theme file:" << path;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "[ThemeManager] JSON parse error in" << path << ":" << error.errorString();
        return false;
    }

    QJsonObject root = doc.object();
    out.name = root.value(QStringLiteral("name")).toString();
    QString typeStr = root.value(QStringLiteral("type")).toString().toLower();
    out.type = (typeStr == QStringLiteral("light")) ? Light : Dark;

    QJsonObject colorsObj = root.value(QStringLiteral("colors")).toObject();
    out.colors.clear();
    for (auto it = colorsObj.begin(); it != colorsObj.end(); ++it) {
        QString colorStr = it.value().toString();
        // VS Code uses #RRGGBBAA format; Qt expects #AARRGGBB
        if (colorStr.length() == 9 && colorStr.startsWith(QLatin1Char('#'))) {
            colorStr = QLatin1Char('#') + colorStr.mid(7, 2) + colorStr.mid(1, 6);
        }
        QColor c(colorStr);
        if (c.isValid()) {
            out.colors.insert(it.key(), c);
        } else {
            qWarning() << "[ThemeManager] Invalid color for token" << it.key()
                       << ":" << it.value().toString();
        }
    }

    return true;
}

QColor ThemeManager::color(const QString &token) const
{
    // 1. User overrides only apply in Custom theme mode
    if (m_currentTheme.name == QStringLiteral("Custom")) {
        auto overrideIt = m_overrides.find(token);
        if (overrideIt != m_overrides.end())
            return overrideIt.value();
    }

    // 2. Check current theme
    auto themeIt = m_currentTheme.colors.find(token);
    if (themeIt != m_currentTheme.colors.end())
        return themeIt.value();

    // 3. Fallback
    return QColor();
}

void ThemeManager::setOverride(const QString &token, const QColor &color)
{
    m_overrides[token] = color;
}

void ThemeManager::clearOverrides()
{
    m_overrides.clear();
}

void ThemeManager::loadOverridesFromSettings()
{
    m_overrides.clear();
    auto &sm = SettingsManager::instance();

    // Map of settings keys → theme tokens (must match s_colorTokenMap in
    // SettingsChangeHandler for consistency between save and load).
    static const QMap<QString, QString> s_colorTokenMap = {
        {QStringLiteral("appearance.colors.editor.background"),        QStringLiteral("editor.background")},
        {QStringLiteral("appearance.colors.editor.foreground"),        QStringLiteral("editor.foreground")},
        {QStringLiteral("appearance.colors.editor.selection"),         QStringLiteral("editor.selectionBackground")},
        {QStringLiteral("appearance.colors.current_line.highlight"),   QStringLiteral("editor.lineHighlightBackground")},
        {QStringLiteral("appearance.colors.line_number.background"),   QStringLiteral("editorLineNumber.background")},
        {QStringLiteral("appearance.colors.line_number.foreground"),   QStringLiteral("editorLineNumber.foreground")},
        {QStringLiteral("appearance.colors.syntax_highlight.keywords"),         QStringLiteral("syntax.keywords")},
        {QStringLiteral("appearance.colors.syntax_highlight.controlKeywords"),  QStringLiteral("syntax.controlKeywords")},
        {QStringLiteral("appearance.colors.syntax_highlight.preprocessor"),     QStringLiteral("syntax.preprocessor")},
        {QStringLiteral("appearance.colors.syntax_highlight.types"),            QStringLiteral("syntax.types")},
        {QStringLiteral("appearance.colors.syntax_highlight.numbers"),          QStringLiteral("syntax.numbers")},
        {QStringLiteral("appearance.colors.syntax_highlight.strings"),          QStringLiteral("syntax.strings")},
        {QStringLiteral("appearance.colors.syntax_highlight.comments"),         QStringLiteral("syntax.comments")},
        {QStringLiteral("appearance.colors.syntax_highlight.functions"),        QStringLiteral("syntax.functions")},
        {QStringLiteral("appearance.colors.syntax_highlight.parameters"),       QStringLiteral("syntax.parameters")},
        {QStringLiteral("appearance.colors.syntax_highlight.python_decorators"), QStringLiteral("syntax.pythonDecorators")},
        {QStringLiteral("appearance.colors.syntax_highlight.python_self_cls"),  QStringLiteral("syntax.pythonSelfCls")},
        {QStringLiteral("appearance.colors.syntax_highlight.brackets0"),        QStringLiteral("syntax.brackets0")},
        {QStringLiteral("appearance.colors.syntax_highlight.brackets1"),        QStringLiteral("syntax.brackets1")},
        {QStringLiteral("appearance.colors.syntax_highlight.brackets2"),        QStringLiteral("syntax.brackets2")},
        {QStringLiteral("appearance.colors.syntax_highlight.unpairedBracket"),  QStringLiteral("syntax.unpairedBracket")},
        {QStringLiteral("appearance.colors.output_panel.background"),  QStringLiteral("output.background")},
        {QStringLiteral("appearance.colors.output_panel.foreground"),  QStringLiteral("output.foreground")},
        {QStringLiteral("appearance.colors.output_panel.selection"),   QStringLiteral("output.selectionBackground")},
        {QStringLiteral("appearance.colors.output_panel.stderr"),      QStringLiteral("output.stderr")},
        {QStringLiteral("appearance.colors.search.highlight_background"), QStringLiteral("search.highlightBackground")},
        {QStringLiteral("appearance.colors.search.highlight_foreground"), QStringLiteral("search.highlightForeground")},
        {QStringLiteral("appearance.colors.preview.container_background"), QStringLiteral("preview.containerBackground")},
        {QStringLiteral("appearance.colors.preview.webengine_background"), QStringLiteral("preview.webEngineBackground")},
        {QStringLiteral("appearance.colors.judge_status.ac"),  QStringLiteral("judge.ac")},
        {QStringLiteral("appearance.colors.judge_status.wa"),  QStringLiteral("judge.wa")},
        {QStringLiteral("appearance.colors.judge_status.tle"), QStringLiteral("judge.tle")},
        {QStringLiteral("appearance.colors.judge_status.mle"), QStringLiteral("judge.mle")},
        {QStringLiteral("appearance.colors.judge_status.re"),  QStringLiteral("judge.re")},
        {QStringLiteral("appearance.colors.judge_status.pe"),  QStringLiteral("judge.pe")},
        {QStringLiteral("appearance.colors.judge_status.ole"), QStringLiteral("judge.ole")},
        {QStringLiteral("appearance.colors.judge_status.ce"),  QStringLiteral("judge.ce")},
    };

    for (auto it = s_colorTokenMap.constBegin(); it != s_colorTokenMap.constEnd(); ++it) {
        QVariant ov = sm.settingOverride(it.key());
        if (ov.isValid()) {
            QColor c(ov.toString());
            if (c.isValid())
                m_overrides[it.value()] = c;
        }
    }
}

QStringList ThemeManager::availableThemes() const
{
    QStringList names;
    for (const auto &entry : m_builtinThemes)
        names.append(entry.name);
    return names;
}

QString ThemeManager::currentThemeName() const
{
    return m_currentTheme.name;
}

ThemeManager::ThemeType ThemeManager::currentThemeType() const
{
    return m_currentTheme.type;
}

void ThemeManager::loadQss()
{
    QFile file(QStringLiteral(":/styles/global.qss"));
    if (!file.exists())
        return;

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[ThemeManager] Cannot open global.qss";
        return;
    }

    QString qss = QString::fromUtf8(file.readAll());
    file.close();

    // Replace non-color variables before the color token pass
    qss.replace("%%tab.height%%", QString::number(m_tabHeight));

    // Replace %%token.name%% placeholders with resolved colors
    static const QRegularExpression tokenRx(QStringLiteral("%%([a-zA-Z0-9.]+)%%"));
    QString result;
    int lastPos = 0;
    auto it = tokenRx.globalMatch(qss);
    while (it.hasNext()) {
        auto match = it.next();
        result += qss.mid(lastPos, match.capturedStart() - lastPos);
        QString token = match.captured(1);
        QColor c = color(token);
        if (c.isValid()) {
            if (c.alpha() < 255)
                result += QStringLiteral("rgba(%1,%2,%3,%4)")
                    .arg(c.red()).arg(c.green()).arg(c.blue())
                    .arg(c.alphaF(), 0, 'f', 2);
            else
                result += c.name();
        } else {
            result += QStringLiteral("#000000");
        }
        lastPos = match.capturedEnd();
    }
    result += qss.mid(lastPos);

    if (m_qssTarget)
        m_qssTarget->setStyleSheet(result);
    else
        qApp->setStyleSheet(result);
}

void ThemeManager::setTabHeight(int h)
{
    m_tabHeight = h;
    loadQss();
}

void ThemeManager::setStyleSheetTarget(QWidget *w)
{
    m_qssTarget = w;
}

void ThemeManager::applyPalette()
{
    QPalette pal;

    // Core application colors
    pal.setColor(QPalette::Window, color(QStringLiteral("workbench.background")));
    pal.setColor(QPalette::WindowText, color(QStringLiteral("workbench.foreground")));

    // Editor / input area colors
    pal.setColor(QPalette::Base, color(QStringLiteral("editor.background")));
    pal.setColor(QPalette::Text, color(QStringLiteral("editor.foreground")));

    // Button colors
    pal.setColor(QPalette::Button, color(QStringLiteral("button.background")));
    pal.setColor(QPalette::ButtonText, color(QStringLiteral("button.foreground")));

    // Selection colors — use editor.selectionBackground for text selection
    // (list.activeBackground is for tree/list item backgrounds, too low-alpha for text)
    pal.setColor(QPalette::Highlight, color(QStringLiteral("editor.selectionBackground")));
    pal.setColor(QPalette::HighlightedText, color(QStringLiteral("editor.foreground")));

    // Tooltip colors (fallback to menu if tooltip tokens absent)
    pal.setColor(QPalette::ToolTipBase, color(QStringLiteral("menu.background")));
    pal.setColor(QPalette::ToolTipText, color(QStringLiteral("menu.foreground")));

    // Link colors
    pal.setColor(QPalette::Link, color(QStringLiteral("textLink.foreground")));
    pal.setColor(QPalette::LinkVisited, color(QStringLiteral("textLink.activeForeground")));

    // Placeholder text
    pal.setColor(QPalette::PlaceholderText, color(QStringLiteral("editorLineNumber.foreground")));

    // ---- Fusion 3D border / shading roles (derive from workbench background) ----
    {
        QColor bg = color(QStringLiteral("workbench.background"));
        if (currentThemeType() == Dark) {
            pal.setColor(QPalette::Light,       bg.lighter(130));
            pal.setColor(QPalette::Midlight,    bg.lighter(115));
            pal.setColor(QPalette::Mid,         bg.lighter(105));
            pal.setColor(QPalette::Dark,        bg.darker(105));
            pal.setColor(QPalette::Shadow,      bg.darker(110));
        } else {
            pal.setColor(QPalette::Light,       bg.lighter(101));
            pal.setColor(QPalette::Midlight,    bg);
            pal.setColor(QPalette::Mid,         bg);
            pal.setColor(QPalette::Dark,        bg.darker(101));
            pal.setColor(QPalette::Shadow,      bg.darker(103));
        }
    }

    // ---- Alternating row color (subtle variation of editor background) ----
    {
        QColor alt = color(QStringLiteral("editor.background"));
        if (currentThemeType() == Dark)
            pal.setColor(QPalette::AlternateBase, alt.lighter(108));
        else
            pal.setColor(QPalette::AlternateBase, alt.darker(103));
    }

    // ---- Bright text (error/emphasis) ----
    pal.setColor(QPalette::BrightText, color(QStringLiteral("diagnostics.error")));

    // Disabled state
    QColor dimmed = color(QStringLiteral("tab.inactiveForeground"));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, dimmed);
    pal.setColor(QPalette::Disabled, QPalette::Text, dimmed);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, dimmed);

    QApplication::setPalette(pal);
}
