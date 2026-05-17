#ifndef PROMPTTEMPLATES_H
#define PROMPTTEMPLATES_H

#include <QString>
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
    // General
    FreeChat,
};

// ── Template builder ────────────────────────────────────────────

inline PromptBundle buildPrompt(AiAction action, const ContextBundle &ctx,
                                 const QString &freeQuery = QString())
{
    PromptBundle result;
    const bool hasSelection = ctx.hasSelection && !ctx.selectedText.isEmpty();
    const QString sel = hasSelection ? ctx.selectedText : QString();
    const QString lang = ctx.language;
    const QString ext = ctx.filePath.section('.', -1);

    switch (action) {

    // ═══════════════ Markdown actions ═══════════════

    case AiAction::ImproveWriting: {
        result.systemPrompt = QStringLiteral(
            "你是一位专业的中文写作编辑。请改进以下文本的语法、表达和逻辑，"
            "保持原意不变，使行文更流畅、更清晰。直接输出改进后的结果，无需解释。"
        );
        if (hasSelection)
            result.userPrompt = sel;
        else
            result.userPrompt = QStringLiteral("请改进以下文本：\n\n%1").arg(ctx.fileContent);
        break;
    }

    case AiAction::SummarizeNote: {
        result.systemPrompt = QStringLiteral(
            "你是一位阅读助手。请用中文总结以下笔记的核心要点，"
            "使用 Markdown 列表格式，条理清晰，突出重点。"
        );
        result.userPrompt = hasSelection
            ? QStringLiteral("总结以下内容：\n\n%1").arg(sel)
            : QStringLiteral("总结以下笔记：\n\n%1").arg(ctx.fileContent);
        break;
    }

    case AiAction::ExtractTags: {
        result.systemPrompt = QStringLiteral(
            "你是一位知识管理助手。请从以下文本中提取 3-8 个关键词作为标签，"
            "以 `#标签名` 格式输出，每行一个。仅输出标签列表，不要额外内容。"
        );
        result.userPrompt = hasSelection ? sel : ctx.fileContent;
        break;
    }

    case AiAction::SelfTest: {
        result.systemPrompt = QStringLiteral(
            "你是一位教师。根据以下笔记内容生成 3-5 道自测题，"
            "包含选择题和简答题，每题附参考答案。使用 Markdown 格式。"
        );
        result.userPrompt = hasSelection
            ? QStringLiteral("根据以下内容出题：\n\n%1").arg(sel)
            : QStringLiteral("根据以下笔记出题：\n\n%1").arg(ctx.fileContent);
        break;
    }

    case AiAction::Translate: {
        result.systemPrompt = QStringLiteral(
            "你是一位专业翻译。将以下文本翻译成中文。"
            "直接输出翻译结果，不要附加解释。保留 Markdown 格式。"
        );
        result.userPrompt = hasSelection ? sel : ctx.fileContent;
        break;
    }

    // ═══════════════ Code actions ═══════════════

    case AiAction::ExplainCode: {
        result.systemPrompt = QStringLiteral(
            "你是一位编程助教。请用中文解释以下代码：它的功能、"
            "输入输出、关键算法、时间/空间复杂度。"
            "使用 Markdown 格式，必要时给出代码示例。"
        );
        if (hasSelection)
            result.userPrompt = QStringLiteral("解释这段 %1 代码：\n\n```%2\n%3\n```")
                .arg(lang, ext, sel);
        else
            result.userPrompt = QStringLiteral("解释这个 %1 文件中的代码：\n\n```%2\n%3\n```")
                .arg(lang, ext, ctx.fileContent);
        break;
    }

    case AiAction::FindBugs: {
        result.systemPrompt = QStringLiteral(
            "你是一位代码审查专家。请检查以下代码中的潜在 Bug、"
            "未定义行为、内存泄漏、逻辑错误和安全问题。"
            "对每个问题说明位置、原因和修复建议。如果没有问题，请说明。"
        );
        if (hasSelection)
            result.userPrompt = QStringLiteral("审查这段 %1 代码：\n\n```%2\n%3\n```")
                .arg(lang, ext, sel);
        else
            result.userPrompt = QStringLiteral("审查这个 %1 文件：\n\n```%2\n%3\n```")
                .arg(lang, ext, ctx.fileContent);
        break;
    }

    case AiAction::AddComments: {
        result.systemPrompt = QStringLiteral(
            "你是一位代码文档专家。请为以下代码添加中文注释："
            "在关键部分添加行注释，在函数/类前添加文档注释（/* */）。"
            "直接输出添加注释后的完整代码，不要额外解释。"
        );
        const QString code = hasSelection ? sel : ctx.fileContent;
        result.userPrompt = QStringLiteral("为以下 %1 代码添加注释：\n\n```%2\n%3\n```")
            .arg(lang, ext, code);
        break;
    }

    case AiAction::OptimizeCode: {
        result.systemPrompt = QStringLiteral(
            "你是一位性能优化专家。请分析以下代码的性能瓶颈，"
            "给出优化建议并展示优化后的代码。"
            "解释每个优化的理由和预期提升。使用 Markdown 格式。"
        );
        const QString code = hasSelection ? sel : ctx.fileContent;
        result.userPrompt = QStringLiteral("优化以下 %1 代码：\n\n```%2\n%3\n```")
            .arg(lang, ext, code);
        break;
    }

    // ═══════════════ General ═══════════════

    case AiAction::FreeChat: {
        result.systemPrompt = QStringLiteral(
            "你是一位智能编程助教，精通 C/C++、Python、算法与数据结构。"
            "请用中文回答问题，语言简洁准确。必要时给出可运行的代码示例。"
        );
        result.userPrompt = freeQuery.isEmpty() ? QStringLiteral("你好，请介绍一下自己。") : freeQuery;
        break;
    }
    }

    return result;
}

// ── Action metadata ─────────────────────────────────────────────

struct ActionInfo {
    AiAction action;
    const char *label;       // Display label in button
    const char *tooltip;     // Hover tooltip
};

inline const ActionInfo *actionInfos()
{
    static const ActionInfo infos[] = {
        // Markdown
        { AiAction::ImproveWriting,  "改进写作",  "改进文本表达和语法" },
        { AiAction::SummarizeNote,   "总结笔记",  "总结核心要点" },
        { AiAction::ExtractTags,     "提取标签",  "提取关键词标签" },
        { AiAction::SelfTest,        "出题自测",  "生成自测题目" },
        { AiAction::Translate,       "翻译",      "翻译为中文" },
        // Code
        { AiAction::ExplainCode,     "解释代码",  "解释代码功能和算法" },
        { AiAction::FindBugs,        "寻找 Bug",  "检查潜在问题" },
        { AiAction::AddComments,     "添加注释",  "为代码添加中文注释" },
        { AiAction::OptimizeCode,    "优化建议",  "性能优化建议" },
        // terminator
        { AiAction::FreeChat,        nullptr,     nullptr },
    };
    return infos;
}

inline const ActionInfo *findActionInfo(AiAction action)
{
    for (const ActionInfo *p = actionInfos(); p->label; ++p) {
        if (p->action == action)
            return p;
    }
    return nullptr;
}

// ── Actions available for each editor mode ──────────────────────
// Returns a null-terminated list (terminated by AiAction::FreeChat)

inline const AiAction *actionsForMode(AiEditorMode mode)
{
    static const AiAction markdownActions[] = {
        AiAction::ImproveWriting,
        AiAction::SummarizeNote,
        AiAction::ExtractTags,
        AiAction::SelfTest,
        AiAction::Translate,
        AiAction::FreeChat, // terminator
    };
    static const AiAction codeActions[] = {
        AiAction::ExplainCode,
        AiAction::FindBugs,
        AiAction::AddComments,
        AiAction::OptimizeCode,
        AiAction::FreeChat, // terminator
    };
    static const AiAction generalActions[] = {
        AiAction::FreeChat, // terminator (only free chat)
    };

    switch (mode) {
    case AiEditorMode::Markdown: return markdownActions;
    case AiEditorMode::Code:     return codeActions;
    default:                     return generalActions;
    }
}

#endif // PROMPTTEMPLATES_H
