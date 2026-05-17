# AI 助手设计文档

## 概述

为 Smart-Markdown 添加 AI 功能，首先实现 AI 助教面板（Phase 1），
随后实现与 Judge 系统集成的智能错题本（Phase 2）。
AI 通过远程 LLM API 通信（优先 Anthropic，同时支持 OpenAI 兼容提供商如 DeepSeek）。

## 路线图

```
Phase 1 (当前) → AI 助教面板
Phase 2 (后续) → 智能错题本
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

## 未来：Phase 2 — 智能错题本

下一阶段的概要设计：

- JudgeEngine 钩子：WA/RE/TLE 时捕获输入、期望输出、实际输出、错误日志
- AiProvider 分析："代码为什么失败？"、"如何修复？"
- 结果保存到本地存储（JSON/SQLite），按题目组织
- UI：AI 面板新增标签页或独立面板，按错误类型和知识点分类展示历史
- 复用 Phase 1 搭建的 AiProvider 基础设施

## 约束与非目标

- 无独立 AI 聊天窗口（面板始终嵌入编辑器中）
- 无本地模型推理（始终使用云端 API）
- 无流式输出到文件或离线响应
- Phase 1 不包含错误持久化（那是 Phase 2）
- JWT/OAuth/登录墙 API 不在范围内（仅支持 plain API key）
