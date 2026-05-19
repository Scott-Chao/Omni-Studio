# AI 助手设计文档

## 概述

为 Smart-Markdown 添加 AI 功能，首先实现 AI 助教面板（Phase 1），
随后实现与 Judge 系统集成的智能错题本（Phase 2）。
AI 通过远程 LLM API 通信（优先 Anthropic，同时支持 OpenAI 兼容提供商如 DeepSeek）。

## 路线图

```
Phase 1 (已完成) → AI 助教面板
Phase 2 (当前)    → 智能错题本
```

### Phase 1 分步实现

共 8 个步骤，每步均可独立编译和测试：

1. **AI 设置页** — ConfigManager AI 配置项 + SettingsPanel 新增"AI 服务"页面（API 端点、Key、模型、Max Tokens、系统提示词）。测试：打开设置面板，看到新页面，填字段，关闭再打开确认持久化。

2. **Provider 层** — AiProvider 抽象基类 + AiProviderFactory（多 Provider 注册与创建）+ AnthropicProvider（Anthropic Messages API）+ OpenAiCompatibleProvider（OpenAI 兼容 HTTP + SSE 流式解析 + 超时/错误处理）。测试：写一个临时 main() 或用 debug 输出，传入真实 API Key 调用，验证流式 chunk 到达。

3. **AiContextManager** — 编辑器上下文收集器：当前模式、文件路径、内容、选中文本、语言。测试：打开不同类型的文件，打印 context 数据验证正确性。

4. **PromptTemplates** — Header-only prompt 模板系统，覆盖 Markdown/代码/OJ 三类动作。测试：用 mock ContextBundle 调用各模板，验证输出格式。

5. **ChatBubble + ChatArea** — 消息气泡组件 + 可滚动消息列表（区分 user/assistant，支持 Markdown 渲染）。测试：用程序添加 mock 消息，验证正确渲染。

6. **ActivityBar + Dock 集成** — AI 图标按钮 + AiPanel dock 注册 + 与右面板 tabify + 切换逻辑 + Ctrl+Shift+A 快捷键。测试：点击按钮切换 AI 面板显示/隐藏。

7. **ActionBar** — 根据上下文动态显示动作按钮，点击发出 trigger 信号。测试：切换不同文件类型，验证按钮列表更新。

8. **端到端集成** — 串联 ActionBar → AiContextManager → PromptTemplates → AiProviderFactory → ChatArea + 底部 InputBar 自由提问 + 错误处理 UI 反馈。测试：真实 API 调用完整走通全流程。

### Phase 2 分步实现

新增文件：`ai/errorjournal.h/cpp`, `ai/errorlistpanel.h/cpp`
修改文件：`ai/prompttemplates.h`, `ai/aipanel.h/cpp`, `judgepanel.h/cpp`, `mainwindow.cpp`, `smart-markdown.pro`

共 7 个步骤，每步均可独立编译和测试：

1. **ErrorJournal 核心** — ErrorRecord 数据结构 + ErrorJournal 单例 + recordFailure + JSON save/load。不涉及任何 UI 和 AI。测试：添加几条 mock record，save()，重新 load()，验证数据完整。

2. **JudgePanel 错误钩子** — 在 JudgePanel::onTestFinished 中对非 AC 结果调用 ErrorJournal::recordFailure()。不改 JudgeEngine。测试：运行带有错误测试用例的 Judge，检查 `error_journal/records.json` 生成正确。

3. **AiAction::ErrorAnalysis** — 在 prompttemplates.h 新增 ErrorAnalysis action，定义分析代码错误的 prompt（含源码、实际/期望输出、错误类型）。测试：用构造的 ContextBundle 调用 buildPrompt，验证输出格式。

4. **ErrorListPanel UI** — 错题列表 Widget：状态筛选下拉 + 关键词搜索 + 按时间倒序的错题列表（状态/问题名/源文件/时间/标签）。测试：用 ErrorJournal::allRecords() 填充，验证列表渲染正确。

5. **AiPanel 标签切换** — AiPanel 标题栏新增 "AI 助手" / "错题本" 标签按钮，内部 QStackedWidget 切换聊天区和错题列表。切换时不丢失状态。测试：点击标签在两个视图间切换无 crash。

6. **AI 分析执行** — ErrorJournal::requestAnalysis 调用 AiProvider::chatStream，分析结果回填到 ErrorRecord::aiAnalysis。ErrorListPanel 点击错题展开详情展示 AI 分析结果。测试：手动触发分析，验证流式文本显示在详情面板。

7. **端到端 + 操作按钮** — 错题详情完整展示（状态/耗时/内存/输入输出对比/AI 分析）+ "重新分析" / "删除" / "已阅" 按钮功能 + 错题数 badge 在标签上。测试：从 Judge 运行到错题记录到 AI 分析的完整流程。



## 架构

```
EditorWidget (current file / selection / mode)
        │  context info
AiContextManager
        │  ContextBundle
AiPanel ───────────────────────────────────────
  ├── ActionBar    (dynamic buttons per context)
  ├── ChatArea     (scrollable message thread)
  │     └── ChatBubble  (single user/assistant msg)
  └── InputBar     (free-text query)
        │
  AiProviderFactory
        │  createProvider(type) → AiProvider*
  AiProvider (abstract)
        │  chatStream(messages) → partialResponse / finished / error
  ├── AnthropicProvider          (Anthropic Messages API)
  └── OpenAiCompatibleProvider   (OpenAI / DeepSeek 等)
```

## 文件清单

```
ai/
  aiprovider.h              抽象基类：setApiKey, chatStream, cancel
  aiproviderfactory.h/cpp   工厂模式：注册、创建 Provider
  anthropicprovider.h/cpp   Anthropic Messages API 实现
  openaicompatibleprovider.h/cpp  OpenAI 兼容 HTTP + SSE 解析
  aicontextmanager.h/cpp    收集编辑器上下文信息
  aipanel.h/cpp             右侧面板（ActionBar + ChatArea + InputBar）
  chatarea.h/cpp            可滚动的消息列表
  chatbubble.h/cpp          单条消息气泡（Markdown 渲染）
  actionbar.h/cpp           根据上下文动态显示动作按钮
  prompttemplates.h         Header-only prompt 模板定义
```

需要修改的已有文件：
- `activitybar.h/cpp` — 新增 AI 图标按钮
- `mainwindow.h/cpp` — 注册 dock、与右面板 tabify、切换逻辑
- `settingspanel.h/cpp` — 新增"AI 服务"设置页
- `configmanager.h/cpp` — AI 配置项（endpoint、model、max_tokens）
- `settingsmanager.h` — API Key 持久化存储
- `smart-markdown.pro` — 添加新源文件

## UI：AI 面板布局

```
┌──────────────────────────────┐
│ AI 助手             [✕]     │  标题栏
├──────────────────────────────┤
│ ✏️ 改进  📝 总结  💡 解释   │  ActionBar（动态按钮）
│ ──── ──── ────              │  分隔线
│ Q: 请分析这段代码...         │
│ A: 这段代码的时间复杂度...    │  ChatArea（可滚动）
│ 正在生成... ▌                │  流式输出光标
├──────────────────────────────┤
│ 输入消息...          [发送]  │  InputBar
└──────────────────────────────┘
```

## UI：位置

- 右侧 QDockWidget，与 RightPanelContainer 做 tabify
- 通过 ActivityBar AI 按钮或 Ctrl+Shift+A 切换
- 点击外部自动隐藏（与右面板相同模式）
- 切换显示时：如果右面板当前可见，自动切换到 AI 面板
- 切换隐藏时：隐藏 AI 面板，恢复右面板之前的内容

## ActionBar：上下文相关动作

| 上下文 | 动作 |
|--------|------|
| Markdown 笔记 | 改进写作, 总结笔记, 提取标签, 出题自测, 翻译 |
| 代码文件/单元格 | 解释代码, 寻找 Bug, 添加注释, 优化建议 |
| OpenJudge 题目 | 分析题目, 解题思路, 边界情况 |
| 通用 | 自由提问（InputBar 始终可用）|

动作由 AiContextManager::currentEditorMode() 决定。

## AiProvider 接口

```cpp
class AiProvider : public QObject {
    Q_OBJECT
public:
    virtual void setApiKey(const QString &key) = 0;
    virtual void setModel(const QString &model) = 0;
    virtual void setSystemPrompt(const QString &prompt) = 0;
    virtual void chatStream(const QList<Message> &messages) = 0;
    virtual void cancel() = 0;

signals:
    void partialResponse(const QString &text);
    void finished();
    void error(const QString &message);
};
```

## Provider 工厂

```cpp
class AiProviderFactory {
public:
    enum ProviderType { Anthropic, OpenAiCompatible };

    static AiProvider* createProvider(ProviderType type, QObject *parent = nullptr);
    static ProviderType typeFromString(const QString &name);
    static QStringList availableProviders();
};
```

- `createProvider` 根据类型创建对应的 Provider 实例
- 设置页 "API 类型" 下拉框通过 `availableProviders()` 填充
- 切换 Provider 时，面板自动重建连接

## AnthropicProvider

- 使用 QNetworkAccessManager POST 到 `{endpoint}/messages`
- 请求体：`{ model, messages, stream: true, max_tokens }`
- Anthropic 使用 `content` 块数组格式，stream 返回 `content_block_delta` 事件
- 从 SSE 帧提取 `type: content_block_delta` → `delta.text`
- 遇到 `message_stop` 事件时停止
- 连接超时：30s

## OpenAiCompatibleProvider

- 使用 QNetworkAccessManager POST 到 `{endpoint}/chat/completions`
- 请求体：`{ model, messages, stream: true, max_tokens }`
- 从 reply::readyRead 解析 SSE 帧
- 从 `data: {"choices":[{"delta":{...}}]}` 提取 `delta.content`
- 遇到 `data: [DONE]` 或错误帧时停止
- 连接超时：30s
- 兼容 DeepSeek、OpenAI 等所有 OpenAI 格式的 API

## 设置页

在 SettingsPanel 中新增"Ai 服务"分类（第 7 页）：

| 字段 | 控件 | 默认值 |
|------|------|--------|
| API 类型 | ComboBox | Anthropic |
| API 端点 | LineEdit | `https://api.deepseek.com/v1` |
| API Key | Password LineEdit | (空) |
| 模型 | LineEdit | `deepseek-v4-flash` |
| Max Tokens | SpinBox (256–16384) | 4096 |
| 系统提示词 | TextEdit (多行) | (空) |

API Key 通过 SettingsManager 存储（base64 混淆，与 OJ 密码相同方式）。

## Prompt 系统

- `prompttemplates.h`：header-only，返回 `{ systemPrompt, userPrompt }`
- 每个 Action 映射到一个 PromptTemplate
- ContextBundle 提供所有动态值：`{file}`, `{selection}`, `{language}`,
  `{problem_description}`, `{sample_input}` 等
- System prompt 设定角色（"你是一位 C++/Python 编程助教..." 等）
- 动作执行流程：清空历史 → 发送 system prompt + user prompt → 显示流式响应
- 自由提问：保留消息历史（接近 token 限制时裁剪最早的消息）

## 错误处理

| 场景 | 行为 |
|------|------|
| 网络故障 | 重试 2 次，间隔 1s；然后显示"网络连接失败" |
| API Key 无效 | 显示"API Key 无效，请在设置中检查" |
| 频率限制 (429) | 显示"请求过于频繁，请稍后再试" |
| 超时 (30s) | 中断，显示"响应超时" |
| 空响应 | 显示"AI 未返回有效结果，请重试" |
| 用户取消 | `cancel()` → 中止网络请求 → 不显示任何内容 |

## 历史管理

- `QList<Message>` 配合 maxTokens 上限（约 4096 tokens）
- `prune()`：超过限制时丢弃最早的 user/assistant 对（保留 system prompt）
- 动作执行：清空所有历史，设置新的 system prompt
- 自由提问：追加到历史，超出时裁剪
- 标题栏中放置"清空对话"按钮

## SSE 解析（轻量级）

各 Provider 内部各自实现 SSE 解析。OpenAiCompatibleProvider 的解析逻辑：

```
buffer += raw bytes
loop:
  find "\n\n" → extract one SSE frame
  skip "data: " prefix
  if "[DONE]" → finished
  parse JSON with QJsonDocument
  extract choices[0].delta.content → partialResponse
  remove frame from buffer
```

AnthropicProvider 解析类似，但事件类型不同（`content_block_delta` / `message_stop`），
提取路径为 `delta.text`。

## Phase 2 — 智能错题本 (Smart Error Notebook)

利用 Phase 1 的 AI 基础设施，每次 Judge 评测失败时自动记录并分析错题。

### 数据流

```
用户点击 "运行全部"
  → JudgeEngine::start()
    → 逐个编译/运行测试用例
    → testFinished(index, TestResult)
      ↑ 对非 AC 结果：
      → ErrorJournal::recordFailure(result, sourceFile, testFolder)
        → 保存原始数据 (JSON)
      → ErrorJournal::requestAnalysis(result)
        → AiProvider::chatStream (分析 prompt)
        → 分析结果回填到 ErrorJournal
    → allTestsFinished(passed, total)
      → ErrorJournal::onJudgeSessionComplete()
        → UI 刷新错题列表
```

### 新增文件

```
ai/
  errorjournal.h/cpp     — 错题记录管理器: 存储、查询、AI 分析调度
  errorlistpanel.h/cpp   — 错题列表 UI 面板
```

### ErrorJournal 设计

```cpp
struct ErrorRecord {
    QString id;                // UUID
    QString problemName;       // 题目名（从 test folder 名推断）
    QString sourceFile;        // 被评测的源文件路径
    QString testFolder;        // 测试用例文件夹路径
    QString statusCode;        // WA / RE / TLE / MLE
    qint64 elapsedMs;
    quint64 memoryKb;
    QString actualOutput;
    QString expectedOutput;
    QString detail;            // 错误详情（如 "超出时间限制"）
    QString aiAnalysis;        // AI 分析结果（异步填充）
    QStringList tags;          // AI 自动提取的知识点标签
    QDateTime timestamp;
    bool reviewed = false;     // 用户是否已查阅
};

class ErrorJournal : public QObject {
    Q_OBJECT
public:
    static ErrorJournal &instance();

    // 记录一次评测失败（由 JudgePanel 或 JudgeEngine 触发）
    void recordFailure(const JudgeEngine::TestResult &result,
                       const QString &sourceFile,
                       const QString &testFolder);

    // 请求 AI 分析错误原因
    void requestAnalysis(const QString &recordId);

    // 查询
    QVector<ErrorRecord> allRecords() const;
    QVector<ErrorRecord> recordsByProblem(const QString &problemName) const;
    QVector<ErrorRecord> recordsByStatus(const QString &statusCode) const;
    ErrorRecord recordById(const QString &id) const;

    // 持久化
    void load();
    void save();

    // 管理
    void deleteRecord(const QString &id);
    void clearAll();

signals:
    void recordAdded(const ErrorRecord &record);
    void analysisReady(const QString &recordId);
};
```

### 存储格式

JSON 文件存储在 `{executable_dir}/error_journal/records.json`。

```json
{
  "version": 1,
  "records": [
    {
      "id": "a1b2c3d4-...",
      "problemName": "A+B Problem",
      "sourceFile": "C:/Users/.../main.cpp",
      "testFolder": "C:/Users/.../tests/ab_problem",
      "statusCode": "WA",
      "elapsedMs": 15,
      "memoryKb": 4096,
      "actualOutput": "3\n",
      "expectedOutput": "4\n",
      "detail": "答案错误",
      "aiAnalysis": "代码在读取输入时使用了 int 类型...",
      "tags": ["整数溢出", "输入处理"],
      "timestamp": "2026-05-19T14:30:00",
      "reviewed": false
    }
  ]
}
```

### AI 分析 Prompt

当评测失败时，将错误信息打包发送给 AI 分析：

```
System: 你是一位 C/C++ 编程助教。分析以下代码在评测中的错误原因，
指出问题所在和修复方法。输出格式：
## 错误原因
[简要分析]

## 具体问题
[指出代码中的错误位置和原因]

## 修复建议
[给出修复后的代码片段]

## 相关知识点
[逗号分隔的 1-4 个知识点标签]

User: 代码（文件: main.cpp）：
```cpp
{source_code}
```

评测状态：{statusCode}
执行时间：{elapsedMs}ms
输入：
{input_data}
期望输出：
{expected_output}
实际输出：
{actual_output}
错误详情：{detail}
```

### UI：错题列表面板

在 AI 面板的标题栏添加一个切换按钮，在 "AI 助手" 和 "错题本" 视图之间切换。

```
AI 面板（错题本视图）：
┌──────────────────────────────┐
│ AI 助手  | [错题本]   [✕]    │  ← 标题栏切换标签
├──────────────────────────────┤
│ ┌────────────────────────┐   │
│ │ 筛选: [全部|WA|RE|TLE] │   │  ← 状态筛选下拉
│ │ 搜索: [___________]    │   │  ← 关键词搜索
│ └────────────────────────┘   │
│                               │
│ ┌─ WA  A+B Problem  ───────┐ │
│ │ main.cpp   2026-05-19   >│ │  ← 错题列表项
│ │ 整数溢出、输入处理       │ │  │  (按时间倒序)
│ ├─ TLE Sorting Analysis ──┤ │
│ │ sort.cpp   2026-05-18   >│ │
│ │ 时间复杂度、递归         │ │
│ ├─ RE  Pointer Test ──────┤ │
│ │ ptr.cpp    2026-05-17   >│ │
│ │ 空指针、数组越界         │ │
│ └─────────────────────────┘ │
│                               │
│ [全部删除]      共 12 条记录 │
└──────────────────────────────┘
```

点击一条错题 → 展开详情：

```
┌─ WA  A+B Problem ────────────┐
│ 文件: main.cpp               │
│ 时间: 2026-05-19 14:30       │
│ 状态: WA (15 ms, 4096 KB)    │
│                               │
│ ┌─ AI 分析 ────────────────┐ │
│ │ ## 错误原因               │ │
│ │ 程序未正确处理多组输入... │ │
│ │                           │ │
│ │ ## 具体问题               │ │
│ │ 第 12 行 while 循环...    │ │
│ │                           │ │
│ │ ## 修复建议               │ │
│ │ ```cpp                    │ │
│ │ while (cin >> a >> b) {...│ │
│ │ ```                       │ │
│ │                           │ │
│ │ ## 相关知识点             │ │
│ │ 输入处理、EOF判断         │ │
│ └───────────────────────────┘ │
│                               │
│ [重新分析] [删除] [已阅]     │
└──────────────────────────────┘
```

### 集成方式

在 MainWindow 中：

```cpp
// JudgePanel 连接 ErrorJournal
connect(judgePanel, &JudgePanel::judgeSessionFinished,
        &ErrorJournal::instance(), &ErrorJournal::onJudgeSessionFinished);

// 或者更细粒度：在 JudgePanel 的 testFinished 回调中调用 ErrorJournal
```

实际上更好的方式是在 JudgePanel::onTestFinished 中，对非 AC 结果调用 ErrorJournal::recordFailure()。这样改动最小，只需修改 JudgePanel 一处即可。

### 实现步骤

1. **ErrorJournal 核心** — 数据结构、recordFailure、持久化（JSON 读写）
2. **AI 分析 prompt** — 在 prompttemplates.h 增加 ErrorAnalysis action，调用 AiProvider 分析
3. **JudgePanel 集成** — 在 onTestFinished 中拦截非 AC 结果，传给 ErrorJournal
4. **ErrorListPanel UI** — 错题列表视图、筛选、搜索
5. **AI 面板集成** — 标题栏切换标签（AI 助手 / 错题本），QStackedWidget 切换两个视图
6. **详情 view** — 点击错题展示详细信息 + AI 分析结果 + 操作按钮
7. **端到端测试** — 运行 Judge 产生非 AC 结果，验证自动记录和分析

### 与 Phase 1 的关系

- 完全复用 AiProvider / AiProviderFactory / SettingsManager (API Key)
- 新增 prompt 模板：AiAction::ErrorAnalysis
- 新增 AI 面板的子视图（错题本 = QWidget，与现有聊天区通过 QStackedWidget 切换）
- 不修改 JudgeEngine 核心逻辑（只在 JudgePanel 层添加钩子）

## 约束与非目标

- 无独立 AI 聊天窗口（面板始终嵌入编辑器中）
- 无本地模型推理（始终使用云端 API）
- 无流式输出到文件或离线响应
- Phase 1 不包含错误持久化（那是 Phase 2）
- JWT/OAuth/登录墙 API 不在范围内（仅支持 plain API key）
