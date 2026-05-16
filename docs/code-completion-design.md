# 代码补全与智能提示设计文档

## 概述

为 Smart-Markdown 编辑器的 CodeEditor 组件添加代码补全、悬停提示和签名帮助功能，使编辑器更接近 VS Code 的开发体验。

支持两种语言：C++ 和 Python。C++ 使用最小化 LSP 协议对接 clangd，Python 使用 Jedi 库。

---

## 功能范围

三个功能，共享同一后端接口：

1. **代码补全（含成员补全）** — 输入时弹出候选列表，带分类图标和类型标签
2. **悬停提示** — 鼠标悬停 400ms 后显示符号类型和文档（Ctrl 悬停可强制触发）
3. **函数签名提示** — 输入 `(` 后显示函数签名、参数列表、重载导航

**不在范围内**：实时诊断（红色波浪线）、跳转定义/引用、格式化和重构。

---

## 系统构架

```
CodeEditor
  ├── CompletionManager          ← 调度器，按语言创建 Provider
  │    ├── CppCompletionProvider ← QProcess ↔ clangd (最小 LSP, 6 条消息)
  │    └── PythonCompletionProvider ← QProcess ↔ Jedi helper (自定义 JSON)
  ├── CompletionPopup            ← 漂浮补全列表 QWidget
  ├── HoverManager               ← 鼠标事件监听 + 延迟触发 + tooltip
  └── SignatureHelpManager       ← cursorPosition 监听 + 参数浮窗
```

### 核心接口

```cpp
struct CompletionItem {
    QString name;
    QString type;           // "function", "class", "variable", "method", "keyword"
    QString signature;      // 完整签名文字（可选）
    QString detail;         // 额外信息（如所属模块）
    QString doc;            // 文档（可选）
    QIcon icon;             // 类型图标
};

struct HoverInfo {
    QString signature;      // 完整类型签名
    QString doc;            // 文档说明
    QString definition;     // 定义位置
};

struct SignatureInfo {
    QString label;          // 完整签名文字
    QString doc;            // 文档
    QList<QString> parameters;  // 参数列表
    int activeParameter;    // 当前参数索引
};

class CompletionProvider : public QObject {
    Q_OBJECT
public:
    virtual void requestCompletion(const QString &text, int cursorPos) = 0;
    virtual void requestHover(const QString &text, int cursorPos) = 0;
    virtual void requestSignatureHelp(const QString &text, int cursorPos) = 0;
signals:
    void completionReady(QList<CompletionItem> items);
    void hoverReady(HoverInfo info);
    void signatureHelpReady(QList<SignatureInfo> signatures, int activeIndex);
};
```

---

## 数据流

```
用户按键 → CodeEditor::keyPressEvent
  → CompletionManager 判断触发条件
    → 调用 Provider 的 requestXxx(text, cursorPos)
      → QProcess 向后端发送 JSON 请求
      → 后端处理并返回 JSON
    → Provider 解析 JSON → emit signal
  → UI 组件显示结果（弹窗/浮窗/签名）
```

### 触发条件

| 功能 | 触发方式 |
|------|---------|
| 代码补全 | 自动：输入 `::` `.` `->` 时；手动：Ctrl+Space |
| 成员补全 | 同上 |
| 函数签名 | 输入 `(` 后自动触发 |
| 悬停提示 | 鼠标悬停 400ms QTimer；Ctrl 悬停绕过延迟 |

---

## C++ 端设计（CppCompletionProvider）

### LSP 通信

仅使用 6 个 LSP 消息的超集，用自定义 `LspClient` 类封装 JSON-RPC（Content-Length 帧协议，约 120 行）。

```cpp
class LspClient : public QObject {
    Q_OBJECT
public:
    bool start(const QString &serverPath);
    void sendRequest(const QString &method, QJsonObject params);
    void sendNotification(const QString &method, QJsonObject params);
    bool isRunning() const;
signals:
    void responseReceived(QJsonObject msg);
    void serverError(QProcess::ProcessError err);
private:
    QProcess *m_process;
    QByteArray m_buffer;
    void parseFrames();
};
```

### LSP 消息列表

| 方向 | 消息 | 用途 |
|------|------|------|
| C→S | `initialize` | 启动握手 |
| C→S | `initialized` | 确认启动 |
| C→S | `textDocument/didOpen` | 文件打开 |
| C→S | `textDocument/didChange` | 内容变更同步 |
| C→S→C | `textDocument/completion` | 补全请求 |
| C→S→C | `textDocument/hover` | 悬停请求 |
| C→S→C | `textDocument/signatureHelp` | 签名请求 |

### clangd 启动参数

```
clangd --fallback-style=Google
```

`--fallback-style` 使 clangd 无需 `compile_commands.json` 即可运行，使用 Google 风格的默认编译参数。

### 后备方案

当 clangd 不可用/超时/崩溃时，退化到基于文档内标识符和预置 C++ 关键词的简单补全。

---

## Python 端设计（PythonCompletionProvider）

### Jedi 辅助脚本（completion_helper.py）

~80 行 Python，通过 stdin/stdout 通信：

```json
// 请求
{"action": "complete", "code": "...", "cursor": [line, col]}
{"action": "hover",    "code": "...", "cursor": [line, col]}
{"action": "signature", "code": "...", "cursor": [line, col]}

// 回复
{"ok": true, "data": [...]}
```

### 依赖

- `pip install jedi`（用户环境必须安装）
- 启动时检测 Jedi 是否可用，不可用时退化到关键词补全

---

## UI 组件设计

### CompletionPopup

- 继承 `QWidget`（`Qt::ToolTip` 或 `Qt::Popup` 窗口标志）
- 无边框，浮动于编辑器上方
- 深色主题（#252526 背景，#1E1E1E 配合编辑器）
- 每项：类型图标 + 名称 + 类型标签
- 选中项蓝色高亮（#094771）
- 底部操作提示条（Tab 接受 / ↑↓ 选择 / Esc 关闭）
- 跟随光标位置实时更新

### HoverManager

- 在 CodeEditor 上安装 `eventFilter`
- `QTimer` 实现 400ms 延迟
- Ctrl 修饰键检测绕过延迟
- 浮窗使用 `QToolTip` 或自绘 `QLabel`
- 显示：类型签名（顶部） + 文档说明（底部） + 定义位置

### SignatureHelpManager

- 连接 `cursorPositionChanged` 信号
- 检测光标前是否有未匹配的 `(`
- 浮窗样式：重载导航（◀ 1/2 ▶）+ 签名 + 当前参数高亮 + 文档
- 关闭条件：输入 `)` / Esc / 点击其他位置

---

## 错误处理与退化

| 场景 | 处理方式 |
|------|---------|
| clangd 未安装 | 检测 `QStandardPaths`，找不到时 fallback 到关键词补全 |
| Jedi 未安装 | try import，捕获 ImportError 后 fallback |
| 请求超时（500ms） | Provider 端 QTimer 超时，放弃请求，UI 不显示 |
| clangd 崩溃 | `QProcess::finished` 信号触发自动重启 |
| 文件内容过大（>1MB） | 不启动补全，减少性能开销 |
| 中文路径（Windows） | 使用 clangd 参数 `--path-mappings` 处理 |

---

## 文件清单

| 文件 | 内容 |
|------|------|
| `completionmanager.h/cpp` | CompletionManager 调度器 |
| `completionprovider.h` | Provider 接口 + CompletionItem/HoverInfo/SignatureInfo |
| `cppcompletionprovider.h/cpp` | C++ clangd provider + LspClient |
| `pythoncompletionprovider.h/cpp` | Python Jedi provider |
| `completionpopup.h/cpp` | 补全弹窗 UI |
| `hovermanager.h/cpp` | 悬停管理器 + tooltip |
| `signaturehelpmanager.h/cpp` | 签名帮助管理器 |
| `completione_helper.py` | Python Jedi 辅助脚本 |
| `smart-markdown.pro` | 添加新文件 |

---

## 分步实现 Roadmap

以下 Roadmap 将上述设计拆分为 12 个可独立编译验证的步骤，每步尽可能小，便于测试与回退。

### 阶段 0：基础架子

**Step 1 — 数据结构 + Provider 抽象接口**
- 新建 `completionprovider.h`
- 定义 `CompletionItem` / `HoverInfo` / `SignatureInfo` 结构体
- 定义 `CompletionProvider` 抽象基类（纯虚函数 + signals）
- 验证：编译通过

**Step 2 — CompletionManager + CodeEditor 集成**
- 新建 `completionmanager.h/cpp`
- `CompletionManager` 负责按语言创建 Provider、中转 signal
- CodeEditor 获得 `CompletionManager` 成员，`setLanguage()` 时自动构建对应 Provider
- 验证：打开 `.cpp` / `.py` 文件不崩溃，日志可见 Provider 创建

### 阶段 1：C++ (clangd LSP)

**Step 3 — LspClient（JSON-RPC 通信层）**
- 内嵌于 `cppcompletionprovider.h/cpp`
- QProcess 启动 clangd，Content-Length 帧协议解析
- `initialize` / `initialized` 握手
- 验证：clangd 进程启动，收到 `InitializeResult`

**Step 4 — CppCompletionProvider：文本同步**
- `didOpen`（文件打开时发送全文）
- `didChange`（内容变更时发送增量/全文）
- 验证：输入代码时 clangd 收到更新（通过 clangd `--log=verbose` 确认）

**Step 5 — CppCompletionProvider：补全请求**
- `textDocument/completion` 请求 → 解析 LSP 响应 → 填充 `CompletionItem` 列表 → emit `completionReady`
- 验证：Ctrl+Space 触发后 qDebug 打印补全列表

### 阶段 2：UI 组件

**Step 6 — CompletionPopup（补全浮窗）**
- 新建 `completionpopup.h/cpp`
- QWidget + Qt::ToolTip 标志，无边框，深色主题
- 列表显示：类型图标 + 名称 + 类型标签
- 键盘导航（↑↓ 选择，Enter 接受，Esc 关闭）
- 验证：用 mock 数据手动 show() 弹出

**Step 7 — 集成 C++ 补全全链路**
- CompletionPopup 连接到 CompletionManager → CppCompletionProvider
- `.` / `::` / `->` 自动触发 + Ctrl+Space 手动触发
- 选中项插入编辑器
- 验证：打开 `.cpp` 文件，输入 `std::` → 弹出补全列表，选中后插入

**Step 8 — 悬停提示（HoverManager + C++）**
- 新建 `hovermanager.h/cpp`：在 CodeEditor 上安装 eventFilter
- 400ms QTimer 延迟 + Ctrl 绕过延迟
- CppCompletionProvider 的 `textDocument/hover` 请求
- 浮窗用 QToolTip 显示
- 验证：鼠标悬停在 C++ 符号 400ms → 弹出类型签名和文档

**Step 9 — 函数签名提示（SignatureHelpManager + C++）**
- 新建 `signaturehelpmanager.h/cpp`
- 监听 `cursorPositionChanged`，检测光标前未匹配的 `(`
- CppCompletionProvider 的 `textDocument/signatureHelp` 请求
- 浮窗显示：签名 + 参数高亮 + 重载导航
- 关闭条件：输入 `)` / Esc / 点击外部
- 验证：在函数名后输入 `(` → 签名弹出，↑↓ 切换重载

### 阶段 3：Python 支持

**Step 10 — completion_helper.py（Jedi 辅助脚本）**
- 新建 `completion_helper.py`，~80 行
- stdin/stdout JSON 协议：`complete` / `hover` / `signature`
- 验证：手动 echo JSON 输入 | python completion_helper.py，看输出

**Step 11 — PythonCompletionProvider**
- 新建 `pythoncompletionprovider.h/cpp`
- QProcess 启动 helper 脚本，发送请求、解析响应
- 完成三项功能的支持
- 验证：Python 文件中补全 / 悬停 / 签名全部可用

### 阶段 4：收尾

**Step 12 — 错误处理 + 退化策略 + .pro 文件**
- clangd/Jedi 不可用时退化为关键词补全
- 500ms 超时处理（Provider 端 QTimer）
- 文件 >1MB 跳过补全
- clangd 崩溃自动重启
- 整理 `.pro` 文件，确保全平台编译
- 验证：无 clangd 环境下打开 `.cpp` 仍有基础关键词补全
