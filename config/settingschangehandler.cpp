#include "settingschangehandler.h"
#include "editor/tabmanager.h"
#include "editor/editorwidget.h"
#include "editor/codeeditor.h"
#include "smd/smdeditor.h"
#include "settingsmanager.h"
#include "configmanager.h"
#include "core/thememanager.h"

#include <QAction>
#include <QKeySequence>
#include <QFont>

SettingsChangeHandler::SettingsChangeHandler(TabManager *tabManager,
                                             SettingsManager *settings,
                                             QObject *parent)
    : QObject(parent)
    , m_tabManager(tabManager)
    , m_settings(settings)
{
}

SettingsChangeHandler::~SettingsChangeHandler() = default;

void SettingsChangeHandler::applyToAllEditors(const std::function<void(EditorWidget*)> &fn)
{
    if (!m_tabManager) return;
    for (int i = 0; i < m_tabManager->count(); ++i) {
        if (auto *editor = qobject_cast<EditorWidget*>(m_tabManager->widget(i)))
            fn(editor);
    }
}

void SettingsChangeHandler::handleDefaultZoom(qreal zoom)
{
    if (!m_settings) return;
    m_settings->setEditorDefaultZoom(zoom);
    applyToAllEditors([zoom](EditorWidget *editor) {
        editor->setZoomFactor(zoom);
    });
    emit zoomLabelUpdateRequested();
}

void SettingsChangeHandler::handleEditorSetting(const QString &key, const QVariant &value)
{
    if (!m_settings) return;
    m_settings->setSettingOverride(key, value);

    if (key == "editor.indent_width") {
        int width = value.toInt();
        applyToAllEditors([width](EditorWidget *editor) {
            editor->setCodeIndentWidth(width);
        });
    } else if (key == "editor.markdown_indent_width") {
        int width = value.toInt();
        applyToAllEditors([width](EditorWidget *editor) {
            editor->setMarkdownIndentWidth(width);
        });
    } else if (key == "editor.font.family") {
        QString family = value.toString();
        int size = m_settings->settingOverride("editor.font.size",
                     ConfigManager::instance().editorFontSize()).toInt();
        applyToAllEditors([family, size](EditorWidget *editor) {
            editor->setEditorFont(family, size);
        });
    } else if (key == "editor.font.size") {
        int size = value.toInt();
        QString family = m_settings->settingOverride("editor.font.family",
                           ConfigManager::instance().editorFontFamily()).toString();
        applyToAllEditors([family, size](EditorWidget *editor) {
            editor->setEditorFont(family, size);
        });
    } else if (key == "auto_save.enabled") {
        bool enabled = value.toBool();
        applyToAllEditors([enabled](EditorWidget *editor) {
            editor->setAutoSaveEnabled(enabled);
        });
    }
}

void SettingsChangeHandler::handleAppearanceSetting(const QString &key, const QVariant &value)
{
    if (!m_settings) return;
    m_settings->setSettingOverride(key, value);

    // Bridge: apply color overrides to ThemeManager so editors render them.
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

    auto tokenIt = s_colorTokenMap.find(key);
    if (tokenIt != s_colorTokenMap.end()) {
        ThemeManager::instance().setOverride(tokenIt.value(), QColor(value.toString()));
    }

    // Reload colors in all editors
    if (key.startsWith(QStringLiteral("appearance.colors."))) {
        applyToAllEditors([](EditorWidget *editor) {
            editor->reloadEditorColors();
        });
    }
}

void SettingsChangeHandler::handleOutputPanelSetting(const QString &key, const QVariant &value)
{
    if (!m_settings) return;
    m_settings->setSettingOverride(key, value);

    // Note: actual OutputPanel reference must be handled by the caller
    // since this handler doesn't own BottomPanel/OutputPanel.
    // This method persists the setting; the caller applies it.
}

void SettingsChangeHandler::handlePreviewSetting(const QString &key, const QVariant &value)
{
    if (!m_settings) return;
    m_settings->setSettingOverride(key, value);

    if (!m_tabManager) return;
    auto *editor = m_tabManager->currentEditor();
    if (!editor) return;

    if (key == "preview.split_debounce_ms") {
        editor->setSplitPreviewDebounceMs(value.toInt());
    } else if (key == "preview.split_preview_ratio") {
        editor->applySplitPreviewRatio();
    }
}

void SettingsChangeHandler::handleSearchSetting(const QString &key, const QVariant &value)
{
    if (m_settings)
        m_settings->setSettingOverride(key, value);
    // Search settings take effect on next search operation
}

void SettingsChangeHandler::handleAiSetting(const QString &key, const QVariant &value)
{
    if (!m_settings) return;
    if (key == QStringLiteral("ai.api_key")) {
        m_settings->setAiApiKey(value.toString());
    } else {
        m_settings->setSettingOverride(key, value);
    }
}

void SettingsChangeHandler::handleToolSetting(const QString &key, const QVariant &value)
{
    if (!m_settings) return;
    m_settings->setSettingOverride(key, value);

    if (key == "open_judge.username" || key == "open_judge.password") {
        QString username = m_settings->settingOverride("open_judge.username", "").toString();
        QString password = m_settings->settingOverride("open_judge.password", "").toString();
        m_settings->setOpenJudgeCredentials(username, password);
    } else if (key == "open_judge.auto_login") {
        m_settings->setOpenJudgeAutoLogin(value.toBool());
    }
}

void SettingsChangeHandler::handleShortcutChanged(const QString &actionKey,
                                                   const QString &keySequenceText,
                                                   const QMap<QString, QAction*> &shortcutActions)
{
    if (!m_settings) return;
    m_settings->setSettingOverride("shortcuts." + actionKey, keySequenceText);

    if (auto *action = shortcutActions.value(actionKey))
        action->setShortcut(QKeySequence(keySequenceText));

    // Notify editor widgets to reload shortcuts
    if (!m_tabManager) return;
    if (auto *editor = m_tabManager->currentEditor()) {
        if (editor->isCodeEdit()) {
            if (auto *ce = editor->codeEditor())
                ce->reloadShortcuts();
        } else if (editor->isSmdEdit()) {
            if (auto *se = editor->smdEditor())
                se->reloadShortcuts();
        }
    }
}

void SettingsChangeHandler::handleResetToDefaults(const QMap<QString, QAction*> &shortcutActions)
{
    if (!m_settings || !m_tabManager)
        return;

    const auto &cfg = ConfigManager::instance();

    // Clear all setting overrides
    for (const QString &key : m_settings->allOverrideKeys())
        m_settings->removeSettingOverride(key);
    m_settings->setEditorDefaultZoom(cfg.zoomDefault());
    m_settings->flushOverrides();

    // Reset tab height to default and reload QSS
    ThemeManager::instance().setTabHeight(cfg.tabHeight());

    // Apply default zoom + editor font to all editors
    applyToAllEditors([&cfg](EditorWidget *editor) {
        editor->setZoomFactor(cfg.zoomDefault());
        editor->setEditorFont(cfg.editorFontFamily(), cfg.editorFontSize());
        if (editor->isCodeEdit())
            editor->setCodeIndentWidth(cfg.editorIndentWidth());
        editor->reloadEditorColors();
    });
    emit zoomLabelUpdateRequested();

    // Reset all shortcuts to defaults
    for (auto it = shortcutActions.constBegin(); it != shortcutActions.constEnd(); ++it) {
        QString defaultVal = cfg.shortcut(it.key(), "");
        it.value()->setShortcut(QKeySequence(defaultVal));
    }

    // Reload configurable shortcuts on all editor and explorer widgets
    for (int i = 0; i < m_tabManager->count(); ++i) {
        if (auto *editor = qobject_cast<EditorWidget*>(m_tabManager->widget(i))) {
            if (auto *ce = editor->codeEditor())
                ce->reloadShortcuts();
            if (auto *se = editor->smdEditor())
                se->reloadShortcuts();
        }
    }

    emit resetComplete();
}
