#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QColor>
#include <QMap>
#include <QVariant>
#include <QJsonValue>

class ConfigManager
{
public:
    static ConfigManager &instance();

    // Load config.json from the executable directory.
    // If the file does not exist, all accessors return built-in defaults.
    // Call once at application startup before any widget construction.
    bool load(const QString &path = QString());

    // Reload config.json at runtime
    bool reload();

    // ---- Raw access ----
    QJsonValue resolvePath(const QString &jsonPath) const;
    int intValue(const QString &jsonPath, int defaultValue) const;
    double doubleValue(const QString &jsonPath, double defaultValue) const;
    QString stringValue(const QString &jsonPath, const QString &defaultValue) const;
    QStringList stringListValue(const QString &jsonPath, const QStringList &defaultValue) const;
    bool boolValue(const QString &jsonPath, bool defaultValue) const;
    QColor colorValue(const QString &jsonPath, const QColor &defaultValue) const;
    QMap<QString, int> mapStringIntValue(const QString &jsonPath) const;

    // ---- Editor ----
    int editorBaseFontSize() const;
    int editorIndentWidth() const;
    int editorMarkdownIndentWidth() const;
    int editorFileTreeItemHeight() const;
    int editorLineNumberRightPadding() const;
    QString editorFontFamily() const;
    int editorFontSize() const;
    double zoomMin() const;
    double zoomMax() const;
    double zoomStep() const;
    double zoomDefault() const;
    int editorContentCheckTimerMs() const;
    int fontMinPointSize() const;
    int fontMaxPointSize() const;

    // ---- Output Panel ----
    QString outputPanelFontFamily() const;
    int outputPanelFontSize() const;
    int outputPanelMaxBlocks() const;
    int outputPanelMinHeight() const;
    double outputPanelDefaultHeightRatio() const;
    int outputPanelPasteTimerMs() const;
    int outputPanelInputEnableDelayMs() const;

    // ---- Preview ----
    int previewSplitDebounceMs() const;
    int previewSplitPreviewRatio() const;

    // ---- Search Panel ----
    int searchDebounceMs() const;
    int searchMaxPerFile() const;
    int searchMaxTotalResults() const;
    int searchSnippetMaxLength() const;

    // ---- Splitter ----
    int mainSplitterDefaultRatio() const;
    int rightSplitterEditorStretch() const;
    int rightSplitterOutputStretch() const;

    // ---- Main Window ----
    int mainWindowDefaultWidth() const;
    int mainWindowDefaultHeight() const;
    int mainWindowDefaultX() const;
    int mainWindowDefaultY() const;

    // ---- Submission Result Panel ----
    double submissionResultHeightRatio() const;

    // ---- Process ----
    int processKillWaitMs() const;

    // ---- OpenJudge ----
    QString openJudgeBaseUrl() const;
    QString openJudgeUserAgent() const;
    int openJudgeTransferTimeoutMs() const;
    int openJudgeProblemDetailTimeoutMs() const;
    int openJudgePollIntervalMs() const;
    int openJudgeMaxPollAttempts() const;
    int openJudgeLoginDialogDelayMs() const;
    QString openJudgeDebugLogFile() const;
    QMap<QString, int> openJudgeSubmissionLanguageMap() const;

    // ---- Compiler ----
    QStringList compilerGxxFlags() const;
    QStringList compilerMsvcFlags() const;
    QString compilerExecutableExtension() const;
    QString compilerTempFilePrefix() const;
    QString compilerTempFileSuffix() const;
    QString compilerCodeBlockPrefix() const;

    // ---- Judge ----
    int judgeTimeLimitMs() const;
    int judgeMemoryLimitKb() const;
    int judgeMemoryPollMs() const;
    QString judgeInputFilePattern() const;

    // ---- Judge Panel ----
    int judgeTableMinHeight() const;
    int judgeDetailMinHeight() const;
    int judgeBrowseButtonWidth() const;

    // ---- OpenJudge Window ----
    int openJudgeWindowWidth() const;
    int openJudgeWindowHeight() const;
    int openJudgeSectionListWidth() const;
    int openJudgeSelectButtonMinWidth() const;

    // ---- Login Dialog ----
    int loginDialogWidth() const;
    int loginDialogHeight() const;
    int loginDialogTitleFontSize() const;

    // ---- Submission Result Panel sizing ----
    int submitResultStatusMinHeight() const;
    int submitResultStatusMaxHeight() const;
    int submitResultCeEditMinHeight() const;
    int submitResultCeEditMaxHeight() const;
    int submitResultHideButtonWidth() const;

    // ---- Panels ----
    int backlinksPanelMinWidth() const;
    int tagPanelMinWidth() const;
    int outlinePanelMinWidth() const;

    // ---- History ----
    int historyMaxEntries() const;

    // ---- Auto Save ----
    bool autoSaveEnabled() const;
    int autoSaveIntervalMs() const;
    int autoSaveRecoveryMaxAgeHours() const;

    // ---- Extensions ----
    QStringList textFileExtensions() const;
    QStringList tagScanExtensions() const;
    QStringList judgeInputExtensions() const;

    // ---- WebEngine Debugging ----
    QString webEngineDebuggingPort() const;

    // ---- Translation ----
    QString translationPrefix() const;
    QString translationPath() const;

    // ---- Colors ----
    QColor editorBackground() const;
    QColor editorForeground() const;
    QColor editorSelection() const;
    QColor lineNumberBackground() const;
    QColor lineNumberForeground() const;
    QColor currentLineHighlight() const;
    QColor outputPanelBackground() const;
    QColor outputPanelForeground() const;
    QColor outputPanelSelection() const;
    QColor outputStderr() const;
    QColor outputErrorStatus() const;
    QColor outputSuccessStatus() const;
    QColor searchHighlightBackground() const;
    QColor searchHighlightForeground() const;
    QColor previewContainerBackground() const;
    QColor previewWebEngineBackground() const;

    // ---- Syntax Highlighting ----
    QColor syntaxKeywords() const;
    QColor syntaxPreprocessor() const;
    QColor syntaxTypes() const;
    QColor syntaxNumbers() const;
    QColor syntaxStrings() const;
    QColor syntaxComments() const;
    QColor syntaxPythonDecorators() const;
    QColor syntaxPythonSelfCls() const;

    // ---- Judge Status Colors ----
    QColor judgeColorAc() const;
    QColor judgeColorWa() const;
    QColor judgeColorTle() const;
    QColor judgeColorMle() const;
    QColor judgeColorRe() const;
    QColor judgeColorPe() const;
    QColor judgeColorOle() const;
    QColor judgeColorCe() const;

    // ---- Shortcuts ----
    QString shortcut(const QString &actionName, const QString &defaultValue) const;

    // ---- AI ----
    QString aiProviderType() const;
    QString aiEndpoint() const;
    QString aiModel() const;
    int aiMaxTokens() const;
    double aiTemperature() const;
    QString aiSystemPrompt() const;

    // ---- Tools ----
    QString toolClangdPath() const;
    QString toolClangdArgs() const;
    QString toolPythonPath() const;

    // ---- Settings Panel ----
    int settingsPanelWidth() const;
    int settingsPanelHeight() const;
    int settingsPanelMinWidth() const;
    int settingsPanelMinHeight() const;
    QString settingsPanelZoomSliderGrooveColor() const;
    int settingsPanelZoomSliderGrooveHeight() const;
    int settingsPanelZoomSliderGrooveRadius() const;
    QString settingsPanelZoomSliderHandleColor() const;
    QString settingsPanelZoomSliderHandleHoverColor() const;
    int settingsPanelZoomSliderHandleWidth() const;
    int settingsPanelZoomSliderHandleRadius() const;
    int settingsPanelZoomSpinboxWidth() const;

private:
    ConfigManager();
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager &) = delete;
    ConfigManager &operator=(const ConfigManager &) = delete;

    QJsonObject m_root;
    QString m_filePath;
    bool m_loaded = false;

    // Build the complete default config as a QJsonObject
    static QJsonObject buildDefaultConfig();
};

#endif // CONFIGMANAGER_H
