#ifndef PROMPTMANAGER_H
#define PROMPTMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QJsonObject>
#include "prompttemplates.h"
#include "aicontextmanager.h"

class PromptManager : public QObject
{
    Q_OBJECT

public:
    static PromptManager &instance();

    PromptBundle buildPrompt(AiAction action, const ContextBundle &ctx,
                             const QString &freeQuery = QString());

    QString actionLabel(AiAction action) const;
    QString actionTooltip(AiAction action) const;

    /// Hot-reload prompts from external file (fallback to resource)
    void reload();

    /// Path to the external prompts file (next to executable)
    QString externalPath() const { return m_externalPath; }

signals:
    void promptsReloaded();

private:
    PromptManager();
    ~PromptManager() override = default;
    PromptManager(const PromptManager &) = delete;
    PromptManager &operator=(const PromptManager &) = delete;

    bool loadFromFile(const QString &path);
    void loadFromResource();
    void loadDefaults(); // hardcoded fallback if both file and resource fail

    QString resolveTemplate(const QString &tmpl, const ContextBundle &ctx,
                            const QString &freeQuery) const;

    struct PromptEntry {
        QString systemPrompt;
        QString userPrompt;
        QString userPromptWithSelection; // empty → use userPrompt
        QString defaultQuery;            // fallback when resolved query is empty
        QString label;
        QString tooltip;
    };

    QMap<AiAction, PromptEntry> m_prompts;
    QString m_externalPath;
};

#endif // PROMPTMANAGER_H
