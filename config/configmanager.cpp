#include "configmanager.h"
#include "core/thememanager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

ConfigManager &ConfigManager::instance()
{
    static ConfigManager inst;
    return inst;
}

ConfigManager::ConfigManager()
{
    m_root = buildDefaultConfig();
}

bool ConfigManager::load(const QString &path)
{
    QString filePath = path;
    if (filePath.isEmpty()) {
        // Walk up from the executable directory, checking each ancestor for config.json
        // This handles: release/ (1 level deep), build/.../ (N levels deep), or CWD
        QStringList checked;
        filePath = QDir::currentPath() + "/config.json";
        if (QFileInfo::exists(filePath)) {
            checked.append(filePath);
            // found it
        } else {
            checked.append(filePath);
            QDir dir(QCoreApplication::applicationDirPath());
            filePath.clear();
            do {
                QString candidate = dir.absoluteFilePath("config.json");
                checked.append(candidate);
                if (QFileInfo::exists(candidate)) {
                    filePath = candidate;
                    break;
                }
            } while (dir.cdUp());
            if (filePath.isEmpty()) {
                filePath = checked.last();
            }
        }
    }

    QFile file(filePath);
    if (!file.exists()) {
        m_root = buildDefaultConfig();
        m_loaded = false;
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[ConfigManager] Cannot open config.json at" << filePath;
        m_root = buildDefaultConfig();
        m_loaded = false;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "[ConfigManager] JSON parse error:" << error.errorString();
        m_root = buildDefaultConfig();
        m_loaded = false;
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "[ConfigManager] JSON root is not an object";
        m_root = buildDefaultConfig();
        m_loaded = false;
        return false;
    }

    m_root = doc.object();
    m_filePath = filePath;
    m_loaded = true;
    return true;
}

bool ConfigManager::reload()
{
    if (m_filePath.isEmpty())
        return load();
    return load(m_filePath);
}

// ---- File-scope helpers ----

static QJsonValue traverseJsonPath(const QJsonObject &root, const QString &jsonPath)
{
    QStringList parts = jsonPath.split('.', Qt::SkipEmptyParts);
    QJsonValue current(root);
    for (const QString &part : parts) {
        if (!current.isObject())
            return QJsonValue::Undefined;
        current = current.toObject().value(part);
        if (current.isUndefined())
            return QJsonValue::Undefined;
    }
    return current;
}

QJsonValue ConfigManager::resolvePath(const QString &jsonPath) const
{
    return traverseJsonPath(m_root, jsonPath);
}

int ConfigManager::intValue(const QString &jsonPath, int defaultValue) const
{
    QJsonValue v = resolvePath(jsonPath);
    if (v.isUndefined() || !v.isDouble())
        return defaultValue;
    return (int)v.toDouble();
}

double ConfigManager::doubleValue(const QString &jsonPath, double defaultValue) const
{
    QJsonValue v = resolvePath(jsonPath);
    if (v.isUndefined() || !v.isDouble())
        return defaultValue;
    return v.toDouble();
}

QString ConfigManager::stringValue(const QString &jsonPath, const QString &defaultValue) const
{
    QJsonValue v = resolvePath(jsonPath);
    if (v.isUndefined() || !v.isString())
        return defaultValue;
    return v.toString();
}

QStringList ConfigManager::stringListValue(const QString &jsonPath, const QStringList &defaultValue) const
{
    QJsonValue v = resolvePath(jsonPath);
    if (v.isUndefined() || !v.isArray())
        return defaultValue;
    QJsonArray arr = v.toArray();
    QStringList result;
    for (const QJsonValue &el : arr) {
        if (el.isString())
            result.append(el.toString());
    }
    return result;
}

bool ConfigManager::boolValue(const QString &jsonPath, bool defaultValue) const
{
    QJsonValue v = resolvePath(jsonPath);
    if (v.isUndefined() || !v.isBool())
        return defaultValue;
    return v.toBool();
}

QColor ConfigManager::colorValue(const QString &jsonPath, const QColor &defaultValue) const
{
    QString str = stringValue(jsonPath, QString());
    if (str.isEmpty())
        return defaultValue;
    QColor c(str);
    if (!c.isValid())
        return defaultValue;
    return c;
}

QMap<QString, int> ConfigManager::mapStringIntValue(const QString &jsonPath) const
{
    QMap<QString, int> result;
    QJsonValue v = resolvePath(jsonPath);
    if (v.isUndefined() || !v.isObject())
        return result;
    QJsonObject obj = v.toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it.value().isDouble())
            result.insert(it.key(), (int)it.value().toDouble());
    }
    return result;
}

// ==================================================================
// Build default config - loaded from embedded JSON resource
// ==================================================================
QJsonObject ConfigManager::buildDefaultConfig()
{
    QFile f(QStringLiteral(":/config/default_config.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[ConfigManager] Cannot load default_config.json resource";
        return {};
    }
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "[ConfigManager] Invalid default_config.json resource";
        return {};
    }
    return doc.object();
}

// ==================================================================
// Typed accessors
// ==================================================================

// ---- Editor ----
int ConfigManager::editorBaseFontSize() const { return intValue("editor.base_font_size", 14); }
int ConfigManager::editorIndentWidth() const { return intValue("editor.indent_width", 4); }
int ConfigManager::editorMarkdownIndentWidth() const { return intValue("editor.markdown_indent_width", 2); }
int ConfigManager::editorFileTreeItemHeight() const { return intValue("editor.file_tree_item_height", 28); }
int ConfigManager::tabHeight() const { return intValue("appearance.tab_height", 26); }
int ConfigManager::editorLineNumberRightPadding() const { return intValue("editor.line_number_area.right_padding", 4); }
QString ConfigManager::editorFontFamily() const { return stringValue("editor.font.family", "Consolas"); }
int ConfigManager::editorFontSize() const { return intValue("editor.font.size", 12); }
double ConfigManager::zoomMin() const { return doubleValue("editor.zoom.min", 0.5); }
double ConfigManager::zoomMax() const { return doubleValue("editor.zoom.max", 3.0); }
double ConfigManager::zoomStep() const { return doubleValue("editor.zoom.step", 0.1); }
double ConfigManager::zoomDefault() const { return doubleValue("editor.zoom.default", 1.0); }
int ConfigManager::editorContentCheckTimerMs() const { return intValue("editor.content_check_timer_ms", 300); }
bool ConfigManager::editorCompletionParenEnabled() const { return boolValue("editor.completion_paren", true); }
int ConfigManager::fontMinPointSize() const { return intValue("editor.font_limits.min_point_size", 1); }
int ConfigManager::fontMaxPointSize() const { return intValue("editor.font_limits.max_point_size", 72); }

// ---- Output Panel ----
QString ConfigManager::outputPanelFontFamily() const { return stringValue("output_panel.font.family", "Consolas"); }
int ConfigManager::outputPanelFontSize() const { return intValue("output_panel.font.size", 10); }
int ConfigManager::outputPanelMaxBlocks() const { return intValue("output_panel.max_blocks", 10000); }
int ConfigManager::outputPanelMinHeight() const { return intValue("output_panel.min_height", 100); }
double ConfigManager::outputPanelDefaultHeightRatio() const { return doubleValue("output_panel.default_height_ratio", 0.333); }
int ConfigManager::outputPanelPasteTimerMs() const { return intValue("output_panel.paste_timer_ms", 20); }
int ConfigManager::outputPanelInputEnableDelayMs() const { return intValue("output_panel.input_enable_delay_ms", 50); }

// ---- Preview ----
int ConfigManager::previewSplitDebounceMs() const { return intValue("preview.split_debounce_ms", 500); }
int ConfigManager::previewSplitPreviewRatio() const { return intValue("preview.split_preview_ratio", 50); }

// ---- Search Panel ----
int ConfigManager::searchDebounceMs() const { return intValue("search_panel.debounce_ms", 300); }
int ConfigManager::searchMaxPerFile() const { return intValue("search_panel.max_per_file", 20); }
int ConfigManager::searchMaxTotalResults() const { return intValue("search_panel.max_total_results", 500); }
int ConfigManager::searchSnippetMaxLength() const { return intValue("search_panel.snippet_max_length", 120); }

// ---- Splitter ----
int ConfigManager::mainSplitterDefaultRatio() const { return intValue("splitter.main_default_ratio", 4); }
int ConfigManager::rightSplitterEditorStretch() const { return intValue("splitter.editor_stretch", 2); }
int ConfigManager::rightSplitterOutputStretch() const { return intValue("splitter.output_stretch", 1); }

// ---- Main Window ----
int ConfigManager::mainWindowDefaultWidth() const { return intValue("main_window.default_width", 1200); }
int ConfigManager::mainWindowDefaultHeight() const { return intValue("main_window.default_height", 800); }
int ConfigManager::mainWindowDefaultX() const { return intValue("main_window.default_x", 100); }
int ConfigManager::mainWindowDefaultY() const { return intValue("main_window.default_y", 100); }

// ---- Submission Result Panel ----
double ConfigManager::submissionResultHeightRatio() const { return doubleValue("submission_result_panel.height_ratio", 0.333); }
int ConfigManager::submitResultStatusMinHeight() const { return intValue("submission_result_panel.status_min_height", 60); }
int ConfigManager::submitResultStatusMaxHeight() const { return intValue("submission_result_panel.status_max_height", 80); }
int ConfigManager::submitResultCeEditMinHeight() const { return intValue("submission_result_panel.ce_edit_min_height", 60); }
int ConfigManager::submitResultCeEditMaxHeight() const { return intValue("submission_result_panel.ce_edit_max_height", 150); }
int ConfigManager::submitResultHideButtonWidth() const { return intValue("submission_result_panel.hide_button_width", 60); }

// ---- Process ----
int ConfigManager::processKillWaitMs() const { return intValue("process.kill_wait_ms", 3000); }

// ---- OpenJudge ----
QString ConfigManager::openJudgeBaseUrl() const { return stringValue("open_judge.base_url", "http://cxsjsx.openjudge.cn"); }
QString ConfigManager::openJudgeUserAgent() const { return stringValue("open_judge.user_agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"); }
int ConfigManager::openJudgeTransferTimeoutMs() const { return intValue("open_judge.transfer_timeout_ms", 15000); }
int ConfigManager::openJudgeProblemDetailTimeoutMs() const { return intValue("open_judge.problem_detail_timeout_ms", 30000); }
int ConfigManager::openJudgePollIntervalMs() const { return intValue("open_judge.poll_interval_ms", 2000); }
int ConfigManager::openJudgeMaxPollAttempts() const { return intValue("open_judge.max_poll_attempts", 15); }
int ConfigManager::openJudgeLoginDialogDelayMs() const { return intValue("open_judge.login_dialog_delay_ms", 200); }
QString ConfigManager::openJudgeDebugLogFile() const { return stringValue("open_judge.debug_log_file", "crawler_debug.log"); }
QMap<QString, int> ConfigManager::openJudgeSubmissionLanguageMap() const { return mapStringIntValue("open_judge.submission_language_map"); }

// ---- Compiler ----
QStringList ConfigManager::compilerGxxFlags() const { return stringListValue("compiler.gxx_flags", {"-std=c++17", "-Wall", "-Wextra"}); }
QStringList ConfigManager::compilerMsvcFlags() const { return stringListValue("compiler.msvc_flags", {"/std:c++17", "/W4", "/EHsc"}); }
QString ConfigManager::compilerExecutableExtension() const { return stringValue("compiler.executable_extension", ".exe"); }
QString ConfigManager::compilerTempFilePrefix() const { return stringValue("compiler.temp_file_prefix", "temp_"); }
QString ConfigManager::compilerTempFileSuffix() const { return stringValue("compiler.temp_file_suffix", ".cpp"); }
QString ConfigManager::compilerCodeBlockPrefix() const { return stringValue("compiler.code_block_prefix", "mdblock_"); }

// ---- Judge ----
int ConfigManager::judgeTimeLimitMs() const { return intValue("judge.time_limit_ms", 1000); }
int ConfigManager::judgeMemoryLimitKb() const { return intValue("judge.memory_limit_kb", 65536); }
int ConfigManager::judgeMemoryPollMs() const { return intValue("judge.memory_poll_ms", 100); }
QString ConfigManager::judgeInputFilePattern() const { return stringValue("judge.input_file_pattern", "*.in"); }

// ---- Judge Panel ----
int ConfigManager::judgeTableMinHeight() const { return intValue("judge_panel.table_min_height", 120); }
int ConfigManager::judgeDetailMinHeight() const { return intValue("judge_panel.detail_min_height", 80); }
int ConfigManager::judgeBrowseButtonWidth() const { return intValue("judge_panel.browse_button_width", 80); }

// ---- OpenJudge Window ----
int ConfigManager::openJudgeWindowWidth() const { return intValue("open_judge_window.width", 900); }
int ConfigManager::openJudgeWindowHeight() const { return intValue("open_judge_window.height", 650); }
int ConfigManager::openJudgeSectionListWidth() const { return intValue("open_judge_window.section_list_width", 100); }
int ConfigManager::openJudgeSelectButtonMinWidth() const { return intValue("open_judge_window.select_button_min_width", 140); }

// ---- Login Dialog ----
int ConfigManager::loginDialogWidth() const { return intValue("login_dialog.width", 360); }
int ConfigManager::loginDialogHeight() const { return intValue("login_dialog.height", 260); }
int ConfigManager::loginDialogTitleFontSize() const { return intValue("login_dialog.title_font_size", 11); }

// ---- Panels ----
int ConfigManager::backlinksPanelMinWidth() const { return intValue("panels.backlinks_min_width", 200); }
int ConfigManager::tagPanelMinWidth() const { return intValue("panels.tag_min_width", 200); }
int ConfigManager::outlinePanelMinWidth() const { return intValue("panels.outline_min_width", 200); }

// ---- History ----
int ConfigManager::historyMaxEntries() const { return intValue("history.max_entries", 50); }

// ---- Extensions ----
QStringList ConfigManager::textFileExtensions() const
{
    QStringList def = {
        "md", "markdown", "txt",
        "c", "cpp", "cxx", "cc", "h", "hpp", "hxx", "hh",
        "cs", "java", "py", "pyw", "pyx", "js", "jsx", "ts", "tsx", "mjs",
        "rs", "go", "rb", "php", "swift", "kt", "kts",
        "html", "htm", "css", "scss", "sass", "less",
        "xml", "svg", "json", "yaml", "yml", "toml", "ini", "cfg", "conf",
        "rst", "tex", "log", "csv", "tsv",
        "sql", "graphql", "proto",
        "in", "out",
        "sh", "bash", "zsh", "fish", "ps1", "bat", "cmd",
        "cmake", "mak", "mk",
        "pro", "pri", "qml", "qrc", "ui",
        "diff", "patch"
    };
    return stringListValue("extensions.text_files", def);
}

// ---- Auto Save ----
bool ConfigManager::autoSaveEnabled() const { return boolValue("auto_save.enabled", true); }
int ConfigManager::autoSaveIntervalMs() const { return intValue("auto_save.interval_ms", 30000); }
int ConfigManager::autoSaveRecoveryMaxAgeHours() const { return intValue("auto_save.recovery_dir_max_age_hours", 72); }

QStringList ConfigManager::tagScanExtensions() const
{
    return stringListValue("extensions.tag_scan", {"md", "markdown"});
}

QStringList ConfigManager::judgeInputExtensions() const
{
    return stringListValue("extensions.judge_input", {"in"});
}

// ---- WebEngine ----
QString ConfigManager::webEngineDebuggingPort() const { return stringValue("webengine.remote_debugging_port", "9222"); }

// ---- Translation ----
QString ConfigManager::translationPrefix() const { return stringValue("translation.prefix", "smart-markdown_"); }
QString ConfigManager::translationPath() const { return stringValue("translation.path", ":/i18n/"); }

// ---- Colors ----
// Resolve via ThemeManager theme JSON first, falling back to per-theme
// hardcoded defaults when the JSON token is missing (stale QRC, etc.).

namespace {

inline bool isLight()
{ return ThemeManager::instance().currentThemeType() == ThemeManager::Light; }

QColor syntaxThemeOr(const char *token, const QColor &dark, const QColor &light)
{
    QColor c = ThemeManager::instance().color(QString::fromLatin1(token));
    if (c.isValid()) return c;
    return isLight() ? light : dark;
}

} // anonymous namespace

QColor ConfigManager::editorBackground() const
    { return syntaxThemeOr("editor.background", QColor("#121314"), QColor("#FFFFFF")); }
QColor ConfigManager::editorForeground() const
    { return syntaxThemeOr("editor.foreground", QColor("#BBBEBF"), QColor("#000000")); }
QColor ConfigManager::editorSelection() const
    { return syntaxThemeOr("editor.selectionBackground", QColor("#276782dd"), QColor("#BBDEFB")); }
QColor ConfigManager::lineNumberBackground() const
    { return syntaxThemeOr("editorLineNumber.background", QColor("#121314"), QColor("#F3F3F3")); }
QColor ConfigManager::lineNumberForeground() const
    { return syntaxThemeOr("editorLineNumber.foreground", QColor("#858889"), QColor("#237893")); }
QColor ConfigManager::currentLineHighlight() const
    { return syntaxThemeOr("editor.lineHighlightBackground", QColor("#242526"), QColor("#F5F5F5")); }
QColor ConfigManager::outputPanelBackground() const
    { return syntaxThemeOr("output.background", QColor("#191A1B"), QColor("#FFFFFF")); }
QColor ConfigManager::outputPanelForeground() const
    { return syntaxThemeOr("output.foreground", QColor("#bfbfbf"), QColor("#000000")); }
QColor ConfigManager::outputPanelSelection() const
    { return syntaxThemeOr("output.selectionBackground", QColor("#3994BC26"), QColor("#ADD6FF")); }
QColor ConfigManager::outputStderr() const
    { return syntaxThemeOr("output.stderr", QColor("#f48771"), QColor("#A31515")); }
QColor ConfigManager::outputErrorStatus() const
    { return syntaxThemeOr("output.errorStatus", QColor("#f48771"), QColor("#A31515")); }
QColor ConfigManager::outputSuccessStatus() const
    { return syntaxThemeOr("output.successStatus", QColor("#73c991"), QColor("#008000")); }
QColor ConfigManager::searchHighlightBackground() const
    { return syntaxThemeOr("search.highlightBackground", QColor("#27678280"), QColor("#FFFF00")); }
QColor ConfigManager::searchHighlightForeground() const
    { return syntaxThemeOr("search.highlightForeground", QColor("#bfbfbf"), QColor("#000000")); }
QColor ConfigManager::previewContainerBackground() const
    { return syntaxThemeOr("preview.containerBackground", QColor("#242526"), QColor("#F5F5F5")); }
QColor ConfigManager::previewWebEngineBackground() const
    { return syntaxThemeOr("preview.webEngineBackground", QColor("#121314"), QColor("#FFFFFF")); }

QColor ConfigManager::syntaxKeywords() const
    { return syntaxThemeOr("syntax.keywords", QColor("#569CD6"), QColor("#0000FF")); }
QColor ConfigManager::syntaxControlKeywords() const
    { return syntaxThemeOr("syntax.controlKeywords", QColor("#D192CC"), QColor("#AF00DB")); }
QColor ConfigManager::syntaxPreprocessor() const
    { return syntaxThemeOr("syntax.preprocessor", QColor("#D192CC"), QColor("#AF00DB")); }
QColor ConfigManager::syntaxTypes() const
    { return syntaxThemeOr("syntax.types", QColor("#4EC9B0"), QColor("#267F99")); }
QColor ConfigManager::syntaxNumbers() const
    { return syntaxThemeOr("syntax.numbers", QColor("#B5CEA8"), QColor("#098658")); }
QColor ConfigManager::syntaxStrings() const
    { return syntaxThemeOr("syntax.strings", QColor("#CE9178"), QColor("#A31515")); }
QColor ConfigManager::syntaxComments() const
    { return syntaxThemeOr("syntax.comments", QColor("#6A9955"), QColor("#008000")); }
QColor ConfigManager::syntaxPythonDecorators() const
    { return syntaxThemeOr("syntax.pythonDecorators", QColor("#D192CC"), QColor("#AF00DB")); }
QColor ConfigManager::syntaxPythonSelfCls() const
    { return syntaxThemeOr("syntax.pythonSelfCls", QColor("#DCDCDC"), QColor("#0000FF")); }
QColor ConfigManager::syntaxFunctions() const
    { return syntaxThemeOr("syntax.functions", QColor("#DCDCAA"), QColor("#795E26")); }
QColor ConfigManager::syntaxParameters() const
    { return syntaxThemeOr("syntax.parameters", QColor("#9CDCFE"), QColor("#001080")); }
QColor ConfigManager::syntaxBracket(int depth) const
{
    const char *tokens[] = { "syntax.brackets0", "syntax.brackets1", "syntax.brackets2" };
    const QColor darkDefaults[] = { QColor("#FFD700"), QColor("#DA70D6"), QColor("#179FFF") };
    const QColor lightDefaults[] = { QColor("#0431FA"), QColor("#AF00DB"), QColor("#267F99") };
    int i = qBound(0, depth, 2);
    return syntaxThemeOr(tokens[i], darkDefaults[i], lightDefaults[i]);
}
QColor ConfigManager::syntaxUnpairedBracket() const
    { return syntaxThemeOr("syntax.unpairedBracket", QColor("#FF0000"), QColor("#FF0000")); }

// ---- Judge Status Colors ----
QColor ConfigManager::judgeColorAc() const
    { return syntaxThemeOr("judge.ac", QColor("#73c991"), QColor("#587c0c")); }
QColor ConfigManager::judgeColorWa() const
    { return syntaxThemeOr("judge.wa", QColor("#f48771"), QColor("#ad0707")); }
QColor ConfigManager::judgeColorTle() const
    { return syntaxThemeOr("judge.tle", QColor("#48A0C7"), QColor("#0069CC")); }
QColor ConfigManager::judgeColorMle() const
    { return syntaxThemeOr("judge.mle", QColor("#d2a8ff"), QColor("#652D90")); }
QColor ConfigManager::judgeColorRe() const
    { return syntaxThemeOr("judge.re", QColor("#e5ba7d"), QColor("#d18616")); }
QColor ConfigManager::judgeColorPe() const
    { return syntaxThemeOr("judge.pe", QColor("#e5ba7d"), QColor("#667309")); }
QColor ConfigManager::judgeColorOle() const
    { return syntaxThemeOr("judge.ole", QColor("#ff7b72"), QColor("#ad0707")); }
QColor ConfigManager::judgeColorCe() const
    { return syntaxThemeOr("judge.ce", QColor("#e5ba7d"), QColor("#d18616")); }

// ---- Shortcuts ----
QString ConfigManager::shortcut(const QString &actionName, const QString &defaultValue) const
{
    return stringValue("shortcuts." + actionName, defaultValue);
}

// ---- AI ----
QString ConfigManager::aiProviderType() const { return stringValue("ai.provider_type", "OpenAI"); }
QString ConfigManager::aiEndpoint() const { return stringValue("ai.endpoint", "https://api.deepseek.com/v1"); }
QString ConfigManager::aiModel() const { return stringValue("ai.model", "deepseek-v4-flash"); }
int ConfigManager::aiMaxTokens() const { return intValue("ai.max_tokens", 4096); }
double ConfigManager::aiTemperature() const { return doubleValue("ai.temperature", 0.7); }
QString ConfigManager::aiSystemPrompt() const { return stringValue("ai.system_prompt", ""); }

// ---- Tools ----
QString ConfigManager::toolClangdPath() const { return stringValue("tools.clangd.path", ""); }
QString ConfigManager::toolClangdArgs() const { return stringValue("tools.clangd.args", "--fallback-style=Google"); }
QString ConfigManager::toolPythonPath() const { return stringValue("tools.python.path", ""); }

// ---- Settings Panel ----
int ConfigManager::settingsPanelWidth() const { return intValue("settings_panel.width", 500); }
int ConfigManager::settingsPanelHeight() const { return intValue("settings_panel.height", 400); }
int ConfigManager::settingsPanelMinWidth() const { return intValue("settings_panel.min_width", 300); }
int ConfigManager::settingsPanelMinHeight() const { return intValue("settings_panel.min_height", 200); }
QString ConfigManager::settingsPanelZoomSliderGrooveColor() const { return stringValue("settings_panel.zoom_slider.groove_color", "#555555"); }
int ConfigManager::settingsPanelZoomSliderGrooveHeight() const { return intValue("settings_panel.zoom_slider.groove_height", 4); }
int ConfigManager::settingsPanelZoomSliderGrooveRadius() const { return intValue("settings_panel.zoom_slider.groove_radius", 2); }
QString ConfigManager::settingsPanelZoomSliderHandleColor() const { return stringValue("settings_panel.zoom_slider.handle_color", "#0078d4"); }
QString ConfigManager::settingsPanelZoomSliderHandleHoverColor() const { return stringValue("settings_panel.zoom_slider.handle_hover_color", "#1a8ad4"); }
int ConfigManager::settingsPanelZoomSliderHandleWidth() const { return intValue("settings_panel.zoom_slider.handle_width", 14); }
int ConfigManager::settingsPanelZoomSliderHandleRadius() const { return intValue("settings_panel.zoom_slider.handle_radius", 7); }
int ConfigManager::settingsPanelZoomSpinboxWidth() const { return intValue("settings_panel.zoom_spinbox.width", 60); }
