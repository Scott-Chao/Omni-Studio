# 代码重复与质量问题分析报告

> 生成于 2026-06-01，基于全项目扫描（ai/ config/ core/ editor/ index/ judge/ lsp/ panels/ runner/ smd/ widgets 模块）。

---

## 高优先级

### 1. 两个独立的 Markdown→HTML 渲染器

**位置**: `ai/chatbubble.cpp:108-231` (`markdownToHtml`) / `ai/errorlistpanel.cpp:223-282` (`renderMarkdown`)

**问题**: 两套独立的 Markdown 转 HTML 实现，功能重叠但输出质量不同（chatbubble 版本支持嵌套列表和代码块，更完整）。均用于渲染 AI 输出文本为富文本 HTML。

**建议**: 统一为单个工具函数，保留功能更完整的版本。

---

### 2. `AiMessage` vs `Message` 结构体字段重复

**位置**: `ai/aiproviders.h` (`Message`) / `ai/aiconversation.h` (`AiMessage`)

**问题**: 两个结构体包含完全相同的 `MessageRole role` 和 `QString content` 字段，`AiMessage` 多一个 `timestampMs`。`airequesthandler.cpp` 中需要逐字段手动转换。

**建议**: 让 `AiMessage` 继承或嵌入 `Message`，添加转换构造函数。

---

### 3. `CppCompletionProvider` 与 `SmdLspManager` LSP 解析逻辑重复

**位置**: `lsp/cppcompletionprovider.cpp` / `lsp/smdlspmanager.cpp`

**问题**: 以下逻辑在两份文件中完全重复：
- **语义标记解析** (`parseSemanticTokens`): delta 编码解码，5 元组解析
- **补全响应解析** (`parseCompletionResponse`): `textEdit.newText` → `insertText` → `label` 优先级、`CompletionItemKind` 映射、`documentation` 提取
- **Hover 响应解析**: 对象/字符串/数组三种结构的递归解析
- **签名帮助解析**: `parameters` 解析、`label` 作字符串或 `[start,end]` 处理
- **`sendInitialize()`**: 相同的 `processId`=null + `rootUri`=null + `capabilities` 构建
- **光标→行/列转换**: 相同的 `text[0..cursorPos]` 扫描逻辑出现 6+ 次
- **Python 进程启动**: 相同的 `completion_helper.py` 候选路径列表和解释器查找逻辑

**建议**: 提取 LSP 公共解析逻辑到共享基类或工具命名空间。

---

### 4. `CodeEditor` 与 `WikiLinkTextEdit` 制表/缩进/退格逻辑重复

**位置**: `editor/codeeditor.cpp` / `editor/wikilinktextedit.cpp`

**问题**: 以下方法在两个类中完全独立实现：
- `handleBackspaceIndent()`: 退格制表符停止逻辑，代码几乎逐行相同
- `handleTabKey()`: Tab 键插入/块缩进逻辑
- `indentString()`: 缩进字符串生成

**建议**: 提取为共享的 `IndentStrategy` 工具类，两者组合使用。

---

## 中优先级

### 5. 屏幕边界钳制逻辑三处重复

**位置**: `editor/completionpopup.cpp:154-162` / `editor/signaturehelpmanager.cpp:185-188` / `editor/hovermanager.cpp:394-414`

**问题**: 弹出窗口的可用屏幕区域边界检查（left/right/top/bottom 裁剪）在三处独立实现。

**建议**: 提取为 `clampToScreen(QWidget*, QRect&)` 工具函数。

---

### 6. `ConfigManager` 膨胀（847 行）

**位置**: `config/configmanager.cpp`

**问题**: 
- `buildDefaultConfig()` 约 365 行，硬编码整个默认 JSON 配置
- 约 100 个访问方法（`intValue`, `stringValue`, `boolValue` 等），本质是 `QJsonObject` 的薄包装
- `textFileExtensions()` 与 `buildDefaultConfig()` 中各有一套扩展名列表

**建议**: 将默认配置外置为 JSON 资源文件；考虑用代码生成替代手写 100+ 访问方法。

---

### 7. `CompileRunManager` 构造函数参数泛滥

**位置**: `runner/compilerunmanager.cpp:26-28`

**问题**: 构造函数接受 `TabManager *`, `BottomPanel *`, `SettingsManager *`, `FileExplorerWidget *`, `QSplitter *` 共 5 个参数，职责过于集中。

**建议**: 引入设置/配置聚合对象，或拆分子类减少耦合。

---

### 8. `CompileRunManager` compile/run/compileAndRun 前导代码重复

**位置**: `runner/compilerunmanager.cpp:165-330`

**问题**: 三个方法的前 ~20 行完全相同（IDE 模式检查、缓存保存、路径解析、面板显示等）。

**建议**: 提取 `resolveIdeFilePath()` 私有辅助方法。

---

## 低优先级

### 9. `ActivityBar.h` 5 个独立布尔状态字段

**位置**: `panels/activitybar.h:43-47`

**问题**: `m_explorerActive`, `m_searchActive`, `m_aiActive`, `m_settingsActive`, `m_judgeActive` 共 5 个布尔值，可合并为 `QVector<bool>` 或位域。

---

### 10. 爬虫 HTML 清理逻辑重复

**位置**: `judge/crawler.cpp:877-891` / `933-947`

**问题**: `parseProblemDetail()` 中两种 fallback 策略各有一套独立的 HTML 清理代码（`<script>`/`<style>` 移除、内联样式剥离、`fixBareLt`）。

**建议**: 提取为 `cleanSectionHtml()` lambda。

---

### 11. `EditorWidget` 预览内容更新逻辑重复

**位置**: `editor/editorwidget.cpp:946-1006` / `2037-2089`

**问题**: `updatePreviewContent()` 与 `updateSplitPreviewContentNow()` 约 80% 代码重复，唯一区别是操作的 `QWebEngineView*` 指针不同。

---

### 12. `SmdLspManager` 中 LSP 请求辅助函数重复

**位置**: `lsp/smdlspmanager.cpp`

**问题**: `sendCompletionRequest`, `sendHoverRequest`, `sendSignatureHelpRequest` 遵循相同模式，仅 method 字符串和 pendingType 不同。

---

### 13. 语义标记虚拟→本地行映射三处重复

**位置**: `lsp/smdlspmanager.cpp`

**问题**: 虚拟行到本地行的映射块在 C++ 响应处理、Python tokens 响应处理（基于结构）、Python tokens 响应处理（基于 pending）中出现三次。

---

### 14. `IndexManager` 异步构建方法重复

**位置**: `index/indexmanager.cpp:41-91` / `93-134`

**问题**: `startAsyncIndexBuild()` 与 `buildFileIndexAsync()` 约 60% 代码重复（取消令牌管理、`QDirIterator` 遍历、结果分派）。
