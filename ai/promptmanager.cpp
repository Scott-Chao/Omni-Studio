#include "promptmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// Map AiAction → JSON key (must match prompts.json keys)
static const char *actionKey(AiAction action)
{
    switch (action) {
    case AiAction::ImproveWriting:  return "ImproveWriting";
    case AiAction::SummarizeNote:   return "SummarizeNote";
    case AiAction::ExtractTags:     return "ExtractTags";
    case AiAction::SelfTest:        return "SelfTest";
    case AiAction::Translate:       return "Translate";
    case AiAction::ExplainCode:     return "ExplainCode";
    case AiAction::FindBugs:        return "FindBugs";
    case AiAction::AddComments:     return "AddComments";
    case AiAction::OptimizeCode:    return "OptimizeCode";
    case AiAction::ErrorAnalysis:   return "ErrorAnalysis";
    case AiAction::FreeChat:        return "FreeChat";
    }
    return "FreeChat";
}

// ── Singleton ─────────────────────────────────────────────────────────

PromptManager &PromptManager::instance()
{
    static PromptManager mgr;
    return mgr;
}

PromptManager::PromptManager()
    : QObject(nullptr)
{
    // Determine external file path: next to the executable
    QString exeDir = QCoreApplication::applicationDirPath();
    m_externalPath = exeDir + QStringLiteral("/prompts.json");

    if (QFileInfo::exists(m_externalPath)) {
        if (!loadFromFile(m_externalPath))
            loadFromResource();
    } else {
        loadFromResource();
    }
}

// ── Public API ────────────────────────────────────────────────────────

PromptBundle PromptManager::buildPrompt(AiAction action, const ContextBundle &ctx,
                                         const QString &freeQuery)
{
    PromptBundle result;
    auto it = m_prompts.find(action);
    if (it == m_prompts.end()) {
        result.systemPrompt = QStringLiteral("You are a helpful assistant.");
        result.userPrompt = freeQuery.isEmpty() ? QStringLiteral("Hello.") : freeQuery;
        return result;
    }

    result.systemPrompt = it->systemPrompt;

    const bool hasSelection = !ctx.selectedText.isEmpty();
    const QString tmpl = hasSelection && !it->userPromptWithSelection.isEmpty()
                             ? it->userPromptWithSelection
                             : it->userPrompt;

    QString resolved = resolveTemplate(tmpl, ctx, freeQuery);

    // Use defaultQuery as fallback when the resolved prompt is empty
    // (primarily for FreeChat when freeQuery is empty)
    if (resolved.isEmpty() && !it->defaultQuery.isEmpty())
        resolved = it->defaultQuery;

    result.userPrompt = resolved;
    return result;
}

QString PromptManager::actionLabel(AiAction action) const
{
    auto it = m_prompts.find(action);
    if (it == m_prompts.end())
        return {};
    return it->label;
}

QString PromptManager::actionTooltip(AiAction action) const
{
    auto it = m_prompts.find(action);
    if (it == m_prompts.end())
        return {};
    return it->tooltip;
}

void PromptManager::reload()
{
    bool ok = false;
    if (QFileInfo::exists(m_externalPath))
        ok = loadFromFile(m_externalPath);
    if (!ok)
        loadFromResource();
    emit promptsReloaded();
}

// ── Loading ───────────────────────────────────────────────────────────

bool PromptManager::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    QJsonObject root = doc.object();
    int version = root.value(QStringLiteral("version")).toInt(0);
    if (version < 1)
        return false;

    // Clean old entries, keeping FreeChat default alive
    m_prompts.clear();

    QJsonObject promptsObj = root.value(QStringLiteral("prompts")).toObject();

    // Iterate over all known actions so unknown JSON keys are ignored
    auto tryLoad = [&](AiAction action) {
        const QString key = QString::fromUtf8(actionKey(action));
        QJsonValue val = promptsObj.value(key);
        if (!val.isObject())
            return;

        QJsonObject obj = val.toObject();
        PromptEntry entry;
        entry.systemPrompt          = obj.value(QStringLiteral("systemPrompt")).toString();
        entry.userPrompt            = obj.value(QStringLiteral("userPrompt")).toString();
        entry.userPromptWithSelection = obj.value(QStringLiteral("userPromptWithSelection")).toString();
        entry.defaultQuery           = obj.value(QStringLiteral("defaultQuery")).toString();
        entry.label                 = obj.value(QStringLiteral("label")).toString();
        entry.tooltip               = obj.value(QStringLiteral("tooltip")).toString();
        m_prompts.insert(action, entry);
    };

    tryLoad(AiAction::ImproveWriting);
    tryLoad(AiAction::SummarizeNote);
    tryLoad(AiAction::ExtractTags);
    tryLoad(AiAction::SelfTest);
    tryLoad(AiAction::Translate);
    tryLoad(AiAction::ExplainCode);
    tryLoad(AiAction::FindBugs);
    tryLoad(AiAction::AddComments);
    tryLoad(AiAction::OptimizeCode);
    tryLoad(AiAction::ErrorAnalysis);
    tryLoad(AiAction::FreeChat);

    return !m_prompts.isEmpty();
}

void PromptManager::loadFromResource()
{
    QString resPath = QStringLiteral(":/prompts/prompts.json");
    if (loadFromFile(resPath))
        return;

    // Last resort: hardcoded fallback
    loadDefaults();
}

void PromptManager::loadDefaults()
{
    m_prompts.clear();

    auto add = [&](AiAction action, const QString &label, const QString &tooltip,
                   const QString &system, const QString &user,
                   const QString &userWithSel, const QString &defaultQuery = QString()) {
        PromptEntry e;
        e.label = label;
        e.tooltip = tooltip;
        e.systemPrompt = system;
        e.userPrompt = user;
        e.userPromptWithSelection = userWithSel;
        e.defaultQuery = defaultQuery;
        m_prompts.insert(action, e);
    };

    add(AiAction::ImproveWriting,
        QStringLiteral("改进写作"), QStringLiteral("改进文本表达和语法"),
        QStringLiteral("你是一位专业的中文写作编辑。请改进以下文本的语法、表达和逻辑，"
                       "保持原意不变，使行文更流畅、更清晰。直接输出改进后的结果，无需解释。"),
        QStringLiteral("请改进以下文本：\n\n{fileContent}"),
        QStringLiteral("{selectedText}"));

    add(AiAction::SummarizeNote,
        QStringLiteral("总结笔记"), QStringLiteral("总结核心要点"),
        QStringLiteral("你是一位阅读助手。请用中文总结以下笔记的核心要点，"
                       "使用 Markdown 列表格式，条理清晰，突出重点。"),
        QStringLiteral("总结以下笔记：\n\n{fileContent}"),
        QStringLiteral("总结以下内容：\n\n{selectedText}"));

    add(AiAction::ExtractTags,
        QStringLiteral("提取标签"), QStringLiteral("提取关键词标签"),
        QStringLiteral("你是一位知识管理助手。请从以下文本中提取 3-8 个关键词作为标签，"
                       "以 `#标签名` 格式输出，每行一个。仅输出标签列表，不要额外内容。"),
        QStringLiteral("{fileContent}"),
        QStringLiteral("{selectedText}"));

    add(AiAction::SelfTest,
        QStringLiteral("出题自测"), QStringLiteral("生成自测题目"),
        QStringLiteral("你是一位教师。根据以下笔记内容生成 3-5 道自测题，"
                       "包含选择题和简答题，每题附参考答案。使用 Markdown 格式。"),
        QStringLiteral("根据以下笔记出题：\n\n{fileContent}"),
        QStringLiteral("根据以下内容出题：\n\n{selectedText}"));

    add(AiAction::Translate,
        QStringLiteral("翻译"), QStringLiteral("翻译为中文"),
        QStringLiteral("你是一位专业翻译。将以下文本翻译成中文。"
                       "直接输出翻译结果，不要附加解释。保留 Markdown 格式。"),
        QStringLiteral("{fileContent}"),
        QStringLiteral("{selectedText}"));

    add(AiAction::ExplainCode,
        QStringLiteral("解释代码"), QStringLiteral("解释代码功能和算法"),
        QStringLiteral("你是一位编程助教。请用中文解释以下代码：它的功能、"
                       "输入输出、关键算法、时间/空间复杂度。"
                       "使用 Markdown 格式，必要时给出代码示例。"),
        QStringLiteral("解释这个 {language} 文件中的代码：\n\n```{extension}\n{fileContent}\n```"),
        QStringLiteral("解释这段 {language} 代码：\n\n```{extension}\n{selectedText}\n```"));

    add(AiAction::FindBugs,
        QStringLiteral("寻找 Bug"), QStringLiteral("检查潜在问题"),
        QStringLiteral("你是一位代码审查专家。请检查以下代码中的潜在 Bug、"
                       "未定义行为、内存泄漏、逻辑错误和安全问题。"
                       "对每个问题说明位置、原因和修复建议。如果没有问题，请说明。"),
        QStringLiteral("审查这个 {language} 文件：\n\n```{extension}\n{fileContent}\n```"),
        QStringLiteral("审查这段 {language} 代码：\n\n```{extension}\n{selectedText}\n```"));

    add(AiAction::AddComments,
        QStringLiteral("添加注释"), QStringLiteral("为代码添加中文注释"),
        QStringLiteral("你是一位代码文档专家。请为以下代码添加中文注释："
                       "在关键部分添加行注释，在函数/类前添加文档注释（/* */）。"
                       "直接输出添加注释后的完整代码，不要额外解释。"),
        QStringLiteral("为以下 {language} 代码添加注释：\n\n```{extension}\n{fileContent}\n```"),
        QStringLiteral("为以下 {language} 代码添加注释：\n\n```{extension}\n{selectedText}\n```"));

    add(AiAction::OptimizeCode,
        QStringLiteral("优化建议"), QStringLiteral("性能优化建议"),
        QStringLiteral("你是一位性能优化专家。请分析以下代码的性能瓶颈，"
                       "给出优化建议并展示优化后的代码。"
                       "解释每个优化的理由和预期提升。使用 Markdown 格式。"),
        QStringLiteral("优化以下 {language} 代码：\n\n```{extension}\n{fileContent}\n```"),
        QStringLiteral("优化以下 {language} 代码：\n\n```{extension}\n{selectedText}\n```"));

    add(AiAction::ErrorAnalysis,
        QStringLiteral("错题分析"), QStringLiteral("分析评测错误原因"),
        QStringLiteral("你是一位 C/C++ 编程助教。分析以下代码在评测中的错误原因，"
                       "指出问题所在和修复方法。使用以下 Markdown 格式输出：\n\n"
                       "## 错误原因\n"
                       "[简要分析出错的原因]\n\n"
                       "## 具体问题\n"
                       "[指出代码中的错误位置和原因]\n\n"
                       "## 修复建议\n"
                       "[给出修复后的代码片段]\n\n"
                       "## 相关知识点\n"
                       "[逗号分隔的 1-4 个知识点标签]"),
        QStringLiteral("代码（文件：{filePath}）：\n```{language}\n{fileContent}\n```\n\n"
                       "评测状态：{errorStatusCode}\n"
                       "执行时间：{elapsedMs}ms\n"
                       "输入：\n{inputData}\n"
                       "期望输出：\n{expectedOutput}\n"
                       "实际输出：\n{actualOutput}\n"
                       "错误详情：{errorDetail}"),
        QString());

    add(AiAction::FreeChat,
        QString(), QString(),
        QStringLiteral("你是一位智能编程助教，精通 C/C++、Python、算法与数据结构。"
                       "请用中文回答问题，语言简洁准确。必要时给出可运行的代码示例。"),
        QStringLiteral("{freeQuery}"),
        QString(),
        QStringLiteral("你好，请介绍一下自己。"));
}

// ── Template resolution ───────────────────────────────────────────────

QString PromptManager::resolveTemplate(const QString &tmpl, const ContextBundle &ctx,
                                        const QString &freeQuery) const
{
    QString result = tmpl;

    result.replace(QStringLiteral("{fileContent}"),     ctx.fileContent);
    result.replace(QStringLiteral("{selectedText}"),    ctx.selectedText);
    result.replace(QStringLiteral("{language}"),        ctx.language);
    result.replace(QStringLiteral("{extension}"),       ctx.filePath.section('.', -1));
    result.replace(QStringLiteral("{filePath}"),        ctx.filePath);

    result.replace(QStringLiteral("{errorStatusCode}"), ctx.errorStatusCode);
    result.replace(QStringLiteral("{elapsedMs}"),       QString::number(ctx.elapsedMs));
    result.replace(QStringLiteral("{memoryKb}"),        QString::number(ctx.memoryKb));
    result.replace(QStringLiteral("{inputData}"),       ctx.inputData);
    result.replace(QStringLiteral("{expectedOutput}"),  ctx.expectedOutput);
    result.replace(QStringLiteral("{actualOutput}"),    ctx.actualOutput);
    result.replace(QStringLiteral("{errorDetail}"),     ctx.errorDetail);

    result.replace(QStringLiteral("{freeQuery}"),       freeQuery);

    // Cursor position (not widely used but available)
    result.replace(QStringLiteral("{cursorLine}"),      QString::number(ctx.cursorLine));
    result.replace(QStringLiteral("{cursorColumn}"),    QString::number(ctx.cursorColumn));

    return result;
}
