#include "configmanager.h"
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
        qDebug() << "[ConfigManager] config.json not found at" << filePath << "- using built-in defaults";
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
    qDebug() << "[ConfigManager] Loaded config from" << filePath;
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
// Build default config - complete embedded JSON with current defaults
// ==================================================================
QJsonObject ConfigManager::buildDefaultConfig()
{
    QJsonObject root;

    // ---- editor ----
    QJsonObject editor;
    editor["base_font_size"] = 14;
    editor["indent_width"] = 4;
    editor["markdown_indent_width"] = 2;
    editor["content_check_timer_ms"] = 300;
    QJsonObject editorFont;
    editorFont["family"] = "Consolas";
    editorFont["size"] = 12;
    editor["font"] = editorFont;
    QJsonObject zoom;
    zoom["min"] = 0.5;
    zoom["max"] = 3.0;
    zoom["step"] = 0.1;
    zoom["default"] = 1.0;
    editor["zoom"] = zoom;
    QJsonObject lineNumber;
    lineNumber["right_padding"] = 4;
    editor["line_number_area"] = lineNumber;
    QJsonObject fontLimits;
    fontLimits["min_point_size"] = 1;
    fontLimits["max_point_size"] = 72;
    editor["font_limits"] = fontLimits;
    root["editor"] = editor;

    // ---- output_panel ----
    QJsonObject outputPanel;
    QJsonObject opFont;
    opFont["family"] = "Consolas";
    opFont["size"] = 10;
    outputPanel["font"] = opFont;
    outputPanel["max_blocks"] = 10000;
    outputPanel["min_height"] = 100;
    outputPanel["default_height_ratio"] = 0.333;
    outputPanel["paste_timer_ms"] = 20;
    outputPanel["input_enable_delay_ms"] = 50;
    root["output_panel"] = outputPanel;

    // ---- preview ----
    QJsonObject preview;
    preview["split_debounce_ms"] = 500;
    preview["split_preview_ratio"] = 50;
    root["preview"] = preview;

    // ---- search_panel ----
    QJsonObject searchPanel;
    searchPanel["debounce_ms"] = 300;
    searchPanel["max_per_file"] = 20;
    searchPanel["max_total_results"] = 500;
    searchPanel["snippet_max_length"] = 120;
    root["search_panel"] = searchPanel;

    // ---- splitter ----
    QJsonObject splitter;
    splitter["main_default_ratio"] = 4;
    splitter["editor_stretch"] = 2;
    splitter["output_stretch"] = 1;
    root["splitter"] = splitter;

    // ---- main_window ----
    QJsonObject mainWindow;
    mainWindow["default_width"] = 1200;
    mainWindow["default_height"] = 800;
    mainWindow["default_x"] = 100;
    mainWindow["default_y"] = 100;
    root["main_window"] = mainWindow;

    // ---- submission_result_panel ----
    QJsonObject submissionPanel;
    submissionPanel["height_ratio"] = 0.333;
    submissionPanel["status_min_height"] = 60;
    submissionPanel["status_max_height"] = 80;
    submissionPanel["ce_edit_min_height"] = 60;
    submissionPanel["ce_edit_max_height"] = 150;
    submissionPanel["hide_button_width"] = 60;
    root["submission_result_panel"] = submissionPanel;

    // ---- process ----
    QJsonObject process;
    process["kill_wait_ms"] = 3000;
    root["process"] = process;

    // ---- compiler ----
    QJsonObject compiler;
    QJsonArray gxxFlags;
    gxxFlags << "-std=c++17" << "-Wall" << "-Wextra";
    compiler["gxx_flags"] = gxxFlags;
    QJsonArray msvcFlags;
    msvcFlags << "/std:c++17" << "/W4" << "/EHsc";
    compiler["msvc_flags"] = msvcFlags;
    compiler["executable_extension"] = ".exe";
    compiler["temp_file_prefix"] = "temp_";
    compiler["temp_file_suffix"] = ".cpp";
    compiler["code_block_prefix"] = "mdblock_";
    root["compiler"] = compiler;

    // ---- tools ----
    QJsonObject tools;
    QJsonObject clangd;
    clangd["path"] = QString();
    clangd["args"] = "--fallback-style=Google";
    tools["clangd"] = clangd;
    QJsonObject python;
    python["path"] = QString();
    tools["python"] = python;
    root["tools"] = tools;

    // ---- settings_panel ----
    QJsonObject settingsPanel;
    settingsPanel["width"] = 680;
    settingsPanel["height"] = 480;
    settingsPanel["min_width"] = 400;
    settingsPanel["min_height"] = 300;
    QJsonObject zoomSlider;
    zoomSlider["groove_color"] = "#555555";
    zoomSlider["groove_height"] = 4;
    zoomSlider["groove_radius"] = 2;
    zoomSlider["handle_color"] = "#0078d4";
    zoomSlider["handle_hover_color"] = "#1a8ad4";
    zoomSlider["handle_width"] = 14;
    zoomSlider["handle_radius"] = 7;
    settingsPanel["zoom_slider"] = zoomSlider;
    QJsonObject zoomSpinbox;
    zoomSpinbox["width"] = 50;
    settingsPanel["zoom_spinbox"] = zoomSpinbox;
    root["settings_panel"] = settingsPanel;

    // ---- judge ----
    QJsonObject judge;
    judge["time_limit_ms"] = 1000;
    judge["memory_limit_kb"] = 65536;
    judge["memory_poll_ms"] = 100;
    judge["input_file_pattern"] = "*.in";
    root["judge"] = judge;

    // ---- judge_panel ----
    QJsonObject judgePanel;
    judgePanel["table_min_height"] = 120;
    judgePanel["detail_min_height"] = 80;
    judgePanel["browse_button_width"] = 80;
    root["judge_panel"] = judgePanel;

    // ---- open_judge ----
    QJsonObject openJudge;
    openJudge["base_url"] = "http://cxsjsx.openjudge.cn";
    openJudge["user_agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    openJudge["transfer_timeout_ms"] = 15000;
    openJudge["problem_detail_timeout_ms"] = 30000;
    openJudge["poll_interval_ms"] = 2000;
    openJudge["max_poll_attempts"] = 15;
    openJudge["login_dialog_delay_ms"] = 200;
    openJudge["debug_log_file"] = "crawler_debug.log";
    QJsonObject langMap;
    langMap[".c"] = 0;
    langMap[".cpp"] = 1;
    langMap[".cc"] = 1;
    langMap[".cxx"] = 1;
    langMap[".py"] = 6;
    langMap[".pyw"] = 6;
    openJudge["submission_language_map"] = langMap;
    root["open_judge"] = openJudge;

    // ---- open_judge_window ----
    QJsonObject ojWindow;
    ojWindow["width"] = 900;
    ojWindow["height"] = 650;
    ojWindow["section_list_width"] = 100;
    ojWindow["select_button_min_width"] = 140;
    root["open_judge_window"] = ojWindow;

    // ---- login_dialog ----
    QJsonObject loginDialog;
    loginDialog["width"] = 360;
    loginDialog["height"] = 260;
    loginDialog["title_font_size"] = 11;
    root["login_dialog"] = loginDialog;

    // ---- panels ----
    QJsonObject panels;
    panels["backlinks_min_width"] = 200;
    panels["tag_min_width"] = 200;
    panels["outline_min_width"] = 200;
    root["panels"] = panels;

    // ---- history ----
    QJsonObject history;
    history["max_entries"] = 50;
    root["history"] = history;

    // ---- extensions ----
    QJsonObject extensions;
    QStringList textExts = {
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
    QJsonArray textExtsArr;
    for (const QString &e : textExts)
        textExtsArr.append(e);
    extensions["text_files"] = textExtsArr;
    QJsonArray tagScan;
    tagScan << "md" << "markdown";
    extensions["tag_scan"] = tagScan;
    QJsonArray judgeInput;
    judgeInput << "in";
    extensions["judge_input"] = judgeInput;
    root["extensions"] = extensions;

    // ---- auto_save ----
    QJsonObject autoSave;
    autoSave["enabled"] = true;
    autoSave["interval_ms"] = 30000;
    autoSave["recovery_dir_max_age_hours"] = 72;
    root["auto_save"] = autoSave;

    // ---- webengine ----
    QJsonObject webengine;
    webengine["remote_debugging_port"] = "9222";
    root["webengine"] = webengine;

    // ---- translation ----
    QJsonObject translation;
    translation["prefix"] = "smart-markdown_";
    translation["path"] = ":/i18n/";
    root["translation"] = translation;

    // ---- appearance.colors ----
    QJsonObject colors;

    QJsonObject editorCol;
    editorCol["background"] = "#1E1E1E";
    editorCol["foreground"] = "#D4D4D4";
    editorCol["selection"] = "#264F78";
    colors["editor"] = editorCol;

    QJsonObject lineNumCol;
    lineNumCol["background"] = "#252525";
    lineNumCol["foreground"] = "#858585";
    colors["line_number"] = lineNumCol;

    QJsonObject currLine;
    currLine["highlight"] = "#2A2D2E";
    colors["current_line"] = currLine;

    QJsonObject outputCol;
    outputCol["background"] = "#1E1E1E";
    outputCol["foreground"] = "#D4D4D4";
    outputCol["selection"] = "#264F78";
    outputCol["stderr"] = "#F48771";
    outputCol["error_status"] = "#F48771";
    outputCol["success_status"] = "#6A9955";
    colors["output_panel"] = outputCol;

    QJsonObject searchCol;
    searchCol["highlight_background"] = "#FFD700";
    searchCol["highlight_foreground"] = "#000000";
    colors["search"] = searchCol;

    QJsonObject previewCol;
    previewCol["container_background"] = "#2d2d2d";
    previewCol["webengine_background"] = "#2d2d2d";
    colors["preview"] = previewCol;

    QJsonObject syntax;
    syntax["keywords"] = "#569CD6";
    syntax["preprocessor"] = "#C586C0";
    syntax["types"] = "#4EC9B0";
    syntax["numbers"] = "#B5CEA8";
    syntax["strings"] = "#CE9178";
    syntax["comments"] = "#6A9955";
    syntax["python_decorators"] = "#C586C0";
    syntax["python_self_cls"] = "#DCDCDC";
    colors["syntax_highlight"] = syntax;

    QJsonObject judgeColors;
    judgeColors["ac"] = "#52C41A";
    judgeColors["wa"] = "#E74C3C";
    judgeColors["tle"] = "#3498DB";
    judgeColors["mle"] = "#9B59B6";
    judgeColors["re"] = "#F39C12";
    judgeColors["pe"] = "#E67E22";
    judgeColors["ole"] = "#FF6B6B";
    judgeColors["ce"] = "#F39C12";
    colors["judge_status"] = judgeColors;

    QJsonObject appearance;
    appearance["colors"] = colors;
    root["appearance"] = appearance;

    // ---- ai ----
    QJsonObject ai;
    ai["provider_type"] = QStringLiteral("OpenAI");
    ai["endpoint"] = QStringLiteral("https://api.deepseek.com/v1");
    ai["model"] = QStringLiteral("deepseek-v4-flash");
    ai["max_tokens"] = 4096;
    ai["system_prompt"] = QString();
    root["ai"] = ai;

    // ---- shortcuts ----
    QJsonObject shortcuts;
    shortcuts["new_file"] = "Ctrl+N";
    shortcuts["save"] = "Ctrl+S";
    shortcuts["save_as"] = "Ctrl+Shift+S";
    shortcuts["toggle_preview"] = "Ctrl+Shift+P";
    shortcuts["toggle_split_preview"] = "Ctrl+P";
    shortcuts["toggle_history"] = "Ctrl+H";
    shortcuts["toggle_backlinks"] = "Ctrl+Shift+B";
    shortcuts["toggle_tags"] = "Ctrl+Shift+T";
    shortcuts["toggle_outline"] = "Ctrl+Shift+O";
    shortcuts["toggle_search"] = "Ctrl+Shift+F";
    shortcuts["toggle_judge"] = "Ctrl+Shift+J";
    shortcuts["toggle_ai"] = "Ctrl+Shift+A";
    shortcuts["toggle_settings"] = "Ctrl+,";
    shortcuts["compile_and_run"] = "F5";
    shortcuts["compile_only"] = "F6";
    shortcuts["run_only"] = "F7";
    shortcuts["stop_process"] = "Ctrl+Break";
    shortcuts["zoom_in"] = "Ctrl+=";
    shortcuts["zoom_out"] = "Ctrl+-";
    shortcuts["zoom_reset"] = "Ctrl+0";
    shortcuts["toggle_comment"] = "Ctrl+/";
    shortcuts["stop_in_output"] = "Ctrl+C";
    shortcuts["paste_in_output"] = "Ctrl+V";
    shortcuts["delete_file"] = "Delete";
    shortcuts["export_pdf"] = "Ctrl+E";
    shortcuts["convert_md_smd"] = "Ctrl+T";

    // CodeEditor widget shortcuts
    shortcuts["completion_trigger"] = "Ctrl+I";
    shortcuts["indent_right"] = "Ctrl+]";
    shortcuts["indent_left"] = "Ctrl+[";

    // SmdEditor cell shortcuts
    shortcuts["cell_execute"] = "Ctrl+Return";
    shortcuts["cell_execute_jump"] = "Shift+Return";
    shortcuts["cell_language"] = "Ctrl+K";
    shortcuts["cell_terminate"] = "Ctrl+C";
    shortcuts["cell_clear_output"] = "Ctrl+Shift+Z";
    shortcuts["cell_split"] = "Ctrl+Shift+-";
    shortcuts["toggle_diagnostics"] = "Ctrl+D";

    // SmdEditor command-mode shortcuts
    shortcuts["cell_insert_above"] = "A";
    shortcuts["cell_insert_below"] = "B";
    shortcuts["cell_delete"] = "Delete";
    root["shortcuts"] = shortcuts;

    return root;
}

// ==================================================================
// Typed accessors
// ==================================================================

// ---- Editor ----
int ConfigManager::editorBaseFontSize() const { return intValue("editor.base_font_size", 14); }
int ConfigManager::editorIndentWidth() const { return intValue("editor.indent_width", 4); }
int ConfigManager::editorMarkdownIndentWidth() const { return intValue("editor.markdown_indent_width", 2); }
int ConfigManager::editorFileTreeItemHeight() const { return intValue("editor.file_tree_item_height", 28); }
int ConfigManager::editorLineNumberRightPadding() const { return intValue("editor.line_number_area.right_padding", 4); }
QString ConfigManager::editorFontFamily() const { return stringValue("editor.font.family", "Consolas"); }
int ConfigManager::editorFontSize() const { return intValue("editor.font.size", 12); }
double ConfigManager::zoomMin() const { return doubleValue("editor.zoom.min", 0.5); }
double ConfigManager::zoomMax() const { return doubleValue("editor.zoom.max", 3.0); }
double ConfigManager::zoomStep() const { return doubleValue("editor.zoom.step", 0.1); }
double ConfigManager::zoomDefault() const { return doubleValue("editor.zoom.default", 1.0); }
int ConfigManager::editorContentCheckTimerMs() const { return intValue("editor.content_check_timer_ms", 300); }
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
QColor ConfigManager::editorBackground() const { return colorValue("appearance.colors.editor.background", QColor("#1E1E1E")); }
QColor ConfigManager::editorForeground() const { return colorValue("appearance.colors.editor.foreground", QColor("#D4D4D4")); }
QColor ConfigManager::editorSelection() const { return colorValue("appearance.colors.editor.selection", QColor("#264F78")); }
QColor ConfigManager::lineNumberBackground() const { return colorValue("appearance.colors.line_number.background", QColor("#252525")); }
QColor ConfigManager::lineNumberForeground() const { return colorValue("appearance.colors.line_number.foreground", QColor("#858585")); }
QColor ConfigManager::currentLineHighlight() const { return colorValue("appearance.colors.current_line.highlight", QColor("#2A2D2E")); }
QColor ConfigManager::outputPanelBackground() const { return colorValue("appearance.colors.output_panel.background", QColor("#1E1E1E")); }
QColor ConfigManager::outputPanelForeground() const { return colorValue("appearance.colors.output_panel.foreground", QColor("#D4D4D4")); }
QColor ConfigManager::outputPanelSelection() const { return colorValue("appearance.colors.output_panel.selection", QColor("#264F78")); }
QColor ConfigManager::outputStderr() const { return colorValue("appearance.colors.output_panel.stderr", QColor("#F48771")); }
QColor ConfigManager::outputErrorStatus() const { return colorValue("appearance.colors.output_panel.error_status", QColor("#F48771")); }
QColor ConfigManager::outputSuccessStatus() const { return colorValue("appearance.colors.output_panel.success_status", QColor("#6A9955")); }
QColor ConfigManager::searchHighlightBackground() const { return colorValue("appearance.colors.search.highlight_background", QColor("#FFD700")); }
QColor ConfigManager::searchHighlightForeground() const { return colorValue("appearance.colors.search.highlight_foreground", QColor("#000000")); }
QColor ConfigManager::previewContainerBackground() const { return colorValue("appearance.colors.preview.container_background", QColor("#2d2d2d")); }
QColor ConfigManager::previewWebEngineBackground() const { return colorValue("appearance.colors.preview.webengine_background", QColor("#2d2d2d")); }

// ---- Syntax Highlighting ----
QColor ConfigManager::syntaxKeywords() const { return colorValue("appearance.colors.syntax_highlight.keywords", QColor("#569CD6")); }
QColor ConfigManager::syntaxPreprocessor() const { return colorValue("appearance.colors.syntax_highlight.preprocessor", QColor("#C586C0")); }
QColor ConfigManager::syntaxTypes() const { return colorValue("appearance.colors.syntax_highlight.types", QColor("#4EC9B0")); }
QColor ConfigManager::syntaxNumbers() const { return colorValue("appearance.colors.syntax_highlight.numbers", QColor("#B5CEA8")); }
QColor ConfigManager::syntaxStrings() const { return colorValue("appearance.colors.syntax_highlight.strings", QColor("#CE9178")); }
QColor ConfigManager::syntaxComments() const { return colorValue("appearance.colors.syntax_highlight.comments", QColor("#6A9955")); }
QColor ConfigManager::syntaxPythonDecorators() const { return colorValue("appearance.colors.syntax_highlight.python_decorators", QColor("#C586C0")); }
QColor ConfigManager::syntaxPythonSelfCls() const { return colorValue("appearance.colors.syntax_highlight.python_self_cls", QColor("#DCDCDC")); }

// ---- Judge Status Colors ----
QColor ConfigManager::judgeColorAc() const { return colorValue("appearance.colors.judge_status.ac", QColor("#52C41A")); }
QColor ConfigManager::judgeColorWa() const { return colorValue("appearance.colors.judge_status.wa", QColor("#E74C3C")); }
QColor ConfigManager::judgeColorTle() const { return colorValue("appearance.colors.judge_status.tle", QColor("#3498DB")); }
QColor ConfigManager::judgeColorMle() const { return colorValue("appearance.colors.judge_status.mle", QColor("#9B59B6")); }
QColor ConfigManager::judgeColorRe() const { return colorValue("appearance.colors.judge_status.re", QColor("#F39C12")); }
QColor ConfigManager::judgeColorPe() const { return colorValue("appearance.colors.judge_status.pe", QColor("#E67E22")); }
QColor ConfigManager::judgeColorOle() const { return colorValue("appearance.colors.judge_status.ole", QColor("#FF6B6B")); }
QColor ConfigManager::judgeColorCe() const { return colorValue("appearance.colors.judge_status.ce", QColor("#F39C12")); }

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
