#ifndef PROMPTTEMPLATES_H
#define PROMPTTEMPLATES_H

#include <QString>
#include <QVector>
#include "aicontextmanager.h"

// ── Prompt template types ───────────────────────────────────────

struct PromptBundle {
    QString systemPrompt;
    QString userPrompt;
};

// Available actions that map to prompt templates
enum class AiAction {
    // Markdown
    ImproveWriting,
    SummarizeNote,
    ExtractTags,
    SelfTest,
    Translate,
    // Code
    ExplainCode,
    FindBugs,
    AddComments,
    OptimizeCode,
    // Judge Error Analysis
    ErrorAnalysis,
    // General
    FreeChat,
};

// ── Actions available for each editor mode ──────────────────────

inline QVector<AiAction> actionsForMode(AiEditorMode mode)
{
    switch (mode) {
    case AiEditorMode::Markdown:
        return {
            AiAction::ImproveWriting,
            AiAction::SummarizeNote,
            AiAction::ExtractTags,
            AiAction::SelfTest,
            AiAction::Translate,
        };
    case AiEditorMode::Code:
        return {
            AiAction::ExplainCode,
            AiAction::FindBugs,
            AiAction::AddComments,
            AiAction::OptimizeCode,
        };
    default:
        return {};
    }
}

#endif // PROMPTTEMPLATES_H
