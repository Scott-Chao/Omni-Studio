# 性能分析报告

> 源码规模：~35000 行，193 个 .cpp/.h 文件，28 个组件目录

## 1. 主题切换：全局级联风暴

**影响：高**

`ThemeManager::themeChanged` 信号被 **28 个 widget 文件**监听，共 30+ 独立连接。每次主题切换的完整链路：

```
loadTheme()
  ├── applyPalette()      // QApplication::setPalette()
  ├── loadQss()           // QRegularExpression::globalMatch 替换 60+ 占位符
  │                        // → qApp->setStyleSheet() → 全面样式重算
  └── emit themeChanged()
       └── 28 receivers
            ├── each calls setStyleSheet() / refreshStyle()
            ├── each triggers Qt style engine recursion
            └── ChatBubble: updateContent() → setHtml() → 全量重渲染
```

`loadQss()` 使用 `QRegularExpression` 的 `globalMatch` 对 QSS 模板做全文替换（约 60+ 个 `%%token.name%%` 占位符），然后 `qApp->setStyleSheet()` 触发 Qt 样式引擎重新计算所有 widget 的样式层叠。28 个接收器中大部分又各自调用 `setStyleSheet()`，形成 O(28 × widget_tree_depth) 的级联开销。

**建议**：
- 建一个全局 QSS 模板，`loadQss()` 一次性生成完整 QSS、只调一次 `qApp->setStyleSheet()`，接收器不再各自 setStyleSheet
- 对高频刷新的控件（ChatBubble、滚动条）使用直接属性设置而非 QSS
- 合并同帧内的多次 `themeChanged` 触发（QTimer::singleShot 防抖）

---

## 2. QWebEngineView × 2 每 EditorWidget 实例

**影响：高**

每个 `EditorWidget` 构造时立即创建 `m_previewView = new QWebEngineView(this)`。`m_splitPreviewView` 在 `createSplitPreviewWidgets()` 中懒创建。但 `m_previewView` 是 eagerly 创建的 —— 每个 WebEngine 实例对应一个独立的 Chromium 进程（约 80-150MB 私有内存 + GPU 资源）。

如果用户通过 TabManager 打开了 3 个 MD 标签，潜在的内存开销：3 × 1（preview）+ 3 × 1（split preview，若启用）。

**WebEngine 的隐藏页面不渲染帧**，但进程和内存仍被占用。另外每个 `PreviewPage`（QWebEnginePage 子类）都持有 `onWikiLinkClicked`、`onRunCodeBlock`、`onTagClicked` 等 lambda，捕获了 `this`（EditorWidget），延长了对象生命周期。

**建议**：
- 只在 `setPreviewMode(true)` 时懒创建 `m_previewView`，关闭预览时 `deleteLater()` 释放
- 或使用全局 `QWebEngineView` 池，各 tab 共享，切换时重新绑定 page

---

## 3. 全局 qApp eventFilter 全事件拦截

**影响：中**

```cpp
qApp->installEventFilter(this);   // MainWindow.cpp:969
```

`MainWindow::eventFilter()` 拦截 **应用中每个 widget 的每个事件**，包括：

- `MouseButtonPress` 分支调 `QApplication::widgetAt(QCursor::pos())` —— 跨进程 Windows API（`WindowFromPoint`），每次点击都走一遍
- 每次事件做 `qobject_cast<QToolBar*>()` dynamic_cast
- 两个 `QWidget::geometry().contains()` 几何测试（overlay 面板）

对于文档编辑场景（频繁鼠标移动 + 键盘输入），这是每事件的固定 overhead。

**建议**：
- 不在 qApp 级别装 filter，只装到具体需要拦截的对象：`m_toolbarSpacer`、`m_settingsOverlay`、`m_helpOverlay`
- overlay 的点击关闭改为在 `nativeEvent()` 中处理 `WM_NCHITTEST` 时顺便检测

---

## 4. findChildren 构造期和 Tab 切换时重复遍历

**影响：中**

```cpp
// mainwindow.cpp:982 — 构造时遍历整个 widget 树
const auto areas = findChildren<QAbstractScrollArea*>();

// mainwindow.cpp:995 — 每次 Tab 切换再遍历编辑器的子控件
const auto areas = editor->findChildren<QAbstractScrollArea*>();
```

`findChildren` 是深度优先递归遍历。`EditorWidget` 的 `QStackedWidget` 内含嵌套结构：`QTextEdit` → `QWebEngineView` → `QWidget` 容器 → `CodeEditor` → `SmdEditor` → `QPdfView`... 每次遍历构建 `QObjectList` 并进行 RTTI 匹配。

**建议**：在 `EditorWidget` 上暴露已知的 `QAbstractScrollArea*` 列表（如 `scrollAreas()` 方法），或只管理 `m_tabManager` 添加/移除 tab 时增量管理，而非每次切换时全量扫描。

---

## 5. ChatBubble 流式渲染全量重绘

**影响：中**

80ms 防抖 timer 是正确的设计，但 `appendText()` → timer → `updateContent()` 路径中，`markdownToHtml()` 每次都处理**全部累积内容**：

```
m_text += text              // O(1) append
→ 80ms timer
→ markdownToHtml(m_text)    // O(len(m_text)) — 长度不断增长
→ m_browser->setHtml(html)  // QTextDocument 重新解析完整 HTML
→ updateBrowserHeight()     // QTextDocument::setTextWidth + size()
```

一次长回复（500+ 行 Markdown，100+ SSE chunks），每次 debounce 触发都是全量 O(n) 的重新解析和渲染。

**建议**：
- 改用增量追加：将新 chunk 转换为 HTML 片段，通过 `QTextCursor` 插入到 QTextBrowser 的 document 尾部，而非每次都 `setHtml()` 全量替换
- 或维护 `m_accumulatedHtml`，只转换增量 chunk 后追加

---

## 6. ProcessRunner::stop() 阻塞主线程

**影响：中**

```cpp
void ProcessRunner::stop() {
    m_currentProcess->kill();
    m_currentProcess->waitForFinished(timeout);  // 阻塞主线程！
}
```

`waitForFinished()` 在主线程（MainWindow 的 `onStopProcess`）调用，阻塞事件循环直到进程退出或超时。期间应用完全无响应。`kill()` 对应 Windows 的 `TerminateProcess`，只杀顶层进程——如果编译进程有子进程（如 `g++` → `cc1plus`），子进程可能变成孤儿。

**建议**：
- 异步化：`kill()` 后启动单次 `QTimer` 检查进程状态，超时再发 `terminate()`
- 或设置较短的 timeout（如 200ms），超时后放弃等待，让 OS 异步清理

---

## 7. 异步索引三阶段串行

**影响：低**

```cpp
QThread::create([...]() {
    // Phase 1: QDirIterator 遍历整个目录树    ← I/O bound
    // Phase 2: BacklinkIndex::buildFromPath() ← CPU（文件解析 + regex）
    // Phase 3: TagIndex::buildFromPath()     ← CPU（文件解析 + regex）
});
```

Phase 2 和 Phase 3 各自独立扫描文件内容——重复的磁盘读取和文本解析。只有在 `startAsyncIndexBuild()` 被频繁触发（如快速切换根目录）时才显著。

**建议**：
- 合并 Phase 2/3 的扫描：一次读取文件内容，同时提取 backlinks 和 tags
- 或分两个线程并行执行 Phase 2 和 Phase 3

---

## 8. MainWindow.cpp 架构规模

**影响：低（可维护性）**

- 3621 行，单文件
- 构造函数约 1006 行
- 大量 lambda inline，多个设置处理器重复 `for (int i = 0; i < m_tabManager->count(); ++i)` 模式 6+ 次
- `m_shortcutActions` 映射（30+ action）在各处手动维护

**建议**：渐进式提取——将 AI 请求逻辑、设置变更处理、索引逻辑分别迁出到独立的 `MainWindowHelper` 或子模块。

---

## 优先级总结

| 优先级 | 问题 | 预期收益 |
|--------|------|----------|
| P0 | 主题切换级联风暴 | 切换延迟从 ~200ms 降至 ~20ms |
| P0 | QWebEngineView 内存 | 每额外 MD tab 省 80-150MB |
| P1 | qApp eventFilter | 减少每事件固定开销 |
| P1 | findChildren 重复遍历 | 降低 Tab 切换延迟 |
| P1 | ChatBubble 全量重渲染 | 长回复流式渲染平滑度 |
| P2 | ProcessRunner blocking kill | 避免 UI 线程阻塞 |
| P2 | 索引三阶段串行 | 大目录索引加速 1.5-2x |
| P3 | MainWindow 拆分 | 可维护性，为进一步优化铺路 |
