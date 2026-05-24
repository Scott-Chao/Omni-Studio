# 主线程阻塞操作分析与修复方案

> 2026-05-23 | 全面代码审查结果

---

## 一、审查范围与方法

对项目根目录（含 `ai/` 子目录）下所有 `.cpp` / `.h` 文件进行了系统审查，重点关注以下阻塞模式：

- `QDirIterator` 同步文件系统遍历
- `QFile` + `QTextStream` 同步文件读写
- `QProcess::waitForStarted / waitForBytesWritten / waitForFinished`
- `QWebEngineView::grab()` 同步截屏
- `QCoreApplication::processEvents()` 事件循环重入
- `QThread::wait()` 阻塞主线程等待工作线程
- JSON 解析 / 大型正则匹配 / 大规模循环
- `QNetworkAccessManager` 配合 `QEventLoop::exec()` 同步网络请求

---

## 二、用户报告的三个问题 — 验证结果

### 2.1 searchpanel.cpp — QDirIterator 同步遍历（确认）

**严重度：🔴 严重**

**位置**：`searchpanel.cpp:184-188` (`collectTextFiles`) + `searchpanel.cpp:138-176` (`performSearch`)

**问题描述**：
`performSearch()` 在主线程同步执行完整的搜索管道：
1. `collectTextFiles()` 使用 `QDirIterator` 同步遍历整个目录树
2. 然后逐文件打开、逐行读取、逐行做子串匹配

大项目（数千文件）会导致数秒的 UI 冻结。虽然有 300ms debounce 定时器延迟触发，但执行本身仍在主线程。

**关键代码**：
```cpp
// searchpanel.cpp:181-188 — 同步遍历目录树
void SearchPanel::collectTextFiles(const QString &rootPath, QStringList &outFiles) {
    QDirIterator it(rootPath, TextFileUtils::scanNameFilters(),
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        outFiles.append(it.next());
    }
}

// searchpanel.cpp:142-176 — 同步打开并逐行扫描所有文件
for (const QString &filePath : files) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
    QTextStream in(&file);
    int lineNum = 0;
    while (!in.atEnd() && fileMatches < maxPerFile && totalResults < maxTotalResults) {
        QString line = in.readLine();
        ++lineNum;
        if (line.toLower().contains(m_searchText)) { ... }
    }
}
```

---

### 2.2 backlinkindex.cpp — 增量操作同步执行（部分确认）

**严重度：🟡 中等**

**问题描述**：

**已做对的部分**：批量索引构建 `BacklinkIndex::buildFromPath()` 和 `TagIndex::buildFromPath()` 已经通过 `mainwindow.cpp:2241` 的 `QThread::create()` 在后台线程执行。`startAsyncIndexBuild()` 正确使用了取消令牌 `m_scanCancelled` 和 generation counter `m_scanId`。

**仍有问题的部分**：
- `mainwindow.cpp:2207` 的 `buildFileIndex()` 使用 `QDirIterator` 在主线程同步遍历，被 `onFileRenamedInIndex` (line 2301) 和 `onFileDeletedInIndex` (line 2313) 调用
- `backlinkindex.cpp:147` (`addFileLinks`) 同步读取文件内容，被 `mainwindow.cpp:1069, 1097, 2362` 的 `rebuildFile()` 在主线程调用
- `tagindex.cpp:180` (`addFileTags`) 同样在主线程同步读取文件

**影响范围**：这些增量操作仅处理单文件，通常 <10ms。但在密集文件操作（如批量重命名）时可能累积。

**关键代码**：
```cpp
// mainwindow.cpp:2207-2222 — buildFileIndex 在主线程同步执行
void MainWindow::buildFileIndex() {
    m_fileIndex.clear();
    QString root = m_explorer->rootPath();
    // ...
    QDirIterator it(root, TextFileUtils::scanNameFilters(), QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString fullPath = it.next();
        QFileInfo info(fullPath);
        m_fileIndex[baseName].append(fullPath);
    }
}
```

---

### 2.3 smdcell.cpp:736 — grab() 阻塞调用（确认，但缓解有限）

**严重度：🟡 中等**

**位置**：`smdcell.cpp:736` (`performGrab`)

**问题描述**：
`QPixmap pm = m_renderView->grab()` 对分离的顶层 `QWebEngineView` 窗口进行同步像素捕获。QWebEngine 在独立进程中运行 Chromium，grab() 需等待 GPU 合成完成，耗时 100ms-1s。

**现有缓解措施（已实施）**：
1. 自适应轮询机制 (`startGrabPolling` / `pollGrabReady`)：反复检查高度是否稳定 + Mermaid 图表是否渲染完毕，只有内容就绪才 grab
2. 重新渲染时 (re-render)：使用 1×1 像素初始尺寸避免闪烁，旧 pixmap 在 QStackedWidget 第 1 页作占位
3. grab 后立即 hide WebEngineView，减少资源占用

**为何难以消除**：SMD Markdown 单元格的渲染架构本质上是"在隐藏的 WebEngineView 中渲染 → 截图 → 显示静态图片"。要彻底消除 grab()，需要完全重写渲染方案（如直接使用 QTextDocument 渲染 Markdown），这在当前阶段不现实。

---

## 三、新发现的阻塞问题

### 3.1 smdeditor.cpp:1319 — waitForStarted(5000) 阻塞主线程

**严重度：🔴 严重**

**位置**：`smdeditor.cpp:1319` (`startPythonExecProcess`)

**问题描述**：
启动 Python 执行器进程时，`waitForStarted(5000)` 阻塞主线程最多 5 秒。Python 冷启动（导入模块等）通常耗时 0.5-2 秒，期间 UI 完全冻结。

```cpp
// smdeditor.cpp:1316-1323
m_pyExecProcess->start(python, {m_pyExecScriptPath});

if (!m_pyExecProcess->waitForStarted(5000)) {
    qWarning() << "SmdEditor: failed to start Python executor";
    m_pyExecProcess->deleteLater();
    m_pyExecProcess = nullptr;
}
```

**累积影响**：加上 `stopPythonExecProcess()` 中的 `waitForBytesWritten(500)` + `waitForFinished(200)` (lines 1336, 1342)，以及 `handleProcessStop()` 中的 `waitForFinished(200)` (line 1468)，累计最多阻塞 5.9 秒。

---

### 3.2 processrunner.cpp:121 — waitForFinished 阻塞主线程

**严重度：🔴 严重**

**位置**：`processrunner.cpp:121` (`stop()`)

**问题描述**：
`stop()` 中调用 `waitForFinished(ConfigManager::instance().processKillWaitMs())`。此方法在主线程被调用（如用户点击停止按钮），阻塞时间由配置决定（通常 1-5 秒）。

```cpp
// processrunner.cpp:117-123
void ProcessRunner::stop() {
    if (m_currentProcess) {
        m_currentProcess->kill();
        m_currentProcess->waitForFinished(ConfigManager::instance().processKillWaitMs());
        cleanupProcess();
    }
}
```

---

### 3.3 mainwindow.cpp:2328-2363 — updateWikiLinksAfterRename 同步批量文件读写

**严重度：🟡 中等（随 backlinks 数量扩展）**

**位置**：`mainwindow.cpp:2328-2363` (`updateWikiLinksAfterRename`)

**问题描述**：
文件重命名后，此函数对每个被影响的源文件执行：
1. 同步读取整个文件 (`QFile::open` + `QTextStream::readAll`)
2. 正则替换 `[[旧名]]` → `[[新名]]`
3. 同步写回磁盘

全部在主线程执行。如果某个文件名被大量页面引用（如 `README` 被 50+ 个文件链接），会产生累积阻塞。

---

### 3.4 lspclient.cpp / smdlspmanager.cpp / pythoncompletionprovider.cpp — waitForFinished 在 shutdown 中

**严重度：🟢 低**

**位置**：
- `lspclient.cpp:102-104` — `waitForFinished(500)` + `waitForFinished(100)`
- `smdlspmanager.cpp:154` — `waitForFinished(200)`
- `pythoncompletionprovider.cpp:38, 116` — `waitForFinished(200)`

**问题描述**：仅在进程关闭/析构时调用。超时短（100-500ms），且不在正常用户操作路径中。属于可接受的清理等待。

---

### 3.5 多处 processEvents() 调用

**严重度：🟢 低（有意的临时事件处理）**

**位置汇总**：

| 文件 | 行号 | 场景 |
|------|------|------|
| `smdcell.cpp` | 447, 455 | 渲染管道中让 Chromium 绘制 |
| `judgeengine.cpp` | 448 | 评测用例之间刷新 UI |
| `outputpanel.cpp` | 316 | stdin 粘贴处理中刷新输出 |
| `mainwindow.cpp` | 2577 | 最大化窗口拖拽复原时 |

**评估**：这些 `processEvents()` 调用均为瞬态、有意的使用。不构成持续性阻塞。维持现状。

---

## 四、验证为"已正确处理"的模式

以下模式经过代码审查，确认使用了正确的异步方式：

| 模式 | 文件 | 实现 |
|------|------|------|
| 批量索引构建 | `mainwindow.cpp:2241-2276` | `QThread::create()` + 取消令牌 + generation counter |
| 网络请求 | `crawler.cpp` | `QNetworkAccessManager` 信号/槽异步 |
| AI 流式请求 | `ai/anthropicprovider.cpp`, `ai/openaiprovider.cpp` | `QNetworkReply::readyRead` SSE 流 |
| 进程编译/运行 | `processrunner.cpp:140-161` | `QProcess::start()` 异步 + 信号通知 |

---

## 五、修复方案

### Fix 1: searchpanel.cpp — 后台搜索线程

**目标**：将搜索管道（目录遍历 + 文件读取 + 行匹配）全部移到后台线程

**改动文件**：`searchpanel.cpp`, `searchpanel.h`

**方案**：
1. 在 `performSearch()` 中创建 `QThread::create()` 工作线程
2. 在线程中执行 `collectTextFiles()` + 文件扫描
3. 使用 `std::atomic<bool>` 取消令牌：新搜索开始时取消旧搜索
4. 通过 `QMetaObject::invokeMethod` + `Qt::QueuedConnection` 分批将结果传回主线程更新 UI（每发现 N 个结果更新一次，避免一次性大量 UI 操作）
5. 搜索进行中禁用搜索输入，完成后恢复
6. 保留现有 debounce 定时器（300ms）

**风险**：多线程访问 `m_searchText` 需要 mutex 保护或在线程启动前拷贝

---

### Fix 2: smdeditor.cpp — 异步 Python 进程生命周期管理

**目标**：消除 `waitForStarted`、`waitForBytesWritten`、`waitForFinished` 阻塞

**改动文件**：`smdeditor.cpp`, `smdeditor.h`

**方案**：
- `startPythonExecProcess()`：移除 `waitForStarted(5000)`，改为连接 `QProcess::started` 信号表示就绪，连接 `QProcess::errorOccurred` 处理启动失败
- 需要等待进程就绪才能发送代码的操作：通过队列机制 — 将待发送代码入队，在 `started` 信号中出队发送
- `stopPythonExecProcess()`：移除 `waitForBytesWritten` 和 `waitForFinished`。改为 `write()` 退出命令后直接 `kill()` + `deleteLater()`。进程退出通过 `finished` 信号异步处理

**风险**：调用者可能依赖 `startPythonExecProcess()` 返回后进程已就绪的假设，需要审查所有调用点

---

### Fix 3: mainwindow.cpp — buildFileIndex() 异步化

**目标**：将 `buildFileIndex()` 的 QDirIterator 遍历移到后台线程

**改动文件**：`mainwindow.cpp`

**方案**：
1. 复用已有的 `startAsyncIndexBuild()` 中的线程 + 取消令牌模式
2. 或将 `buildFileIndex()` 改为启动后台线程，结果通过 QueuedConnection 回主线程更新 `m_fileIndex`
3. `onFileRenamedInIndex()` 和 `onFileDeletedInIndex()` 中的 `buildFileIndex()` 调用改为异步版本
4. 注意竞态：使用 `m_scanId` generation counter 拒绝过期结果

**风险**：`onFileRenamedInIndex()` 后续代码依赖更新后的 `m_fileIndex`，需要将依赖逻辑移到回调中

---

### Fix 4: processrunner.cpp — stop() 去同步等待

**目标**：移除 `stop()` 中 `waitForFinished()` 对主线程的阻塞

**改动文件**：`processrunner.cpp`

**方案**：
- 移除 `waitForFinished()`，改为 `kill()` + 延迟 `deleteLater()`
- `cleanupProcess()` 中执行 `disconnect()`（防止 kill 后信号到达），然后 `deleteLater()`
- 析构函数中保留短超时 `waitForFinished(200)` 确保子进程被清理（析构不在用户交互路径中）

**风险**：某些调用点可能假设 `stop()` 返回后进程已完全终止。需要审查 `stop()` 的所有调用方

---

### Fix 5: mainwindow.cpp — updateWikiLinksAfterRename() 异步化

**目标**：将文件批量读写移到后台线程

**改动文件**：`mainwindow.cpp`

**方案**：
1. 使用 `QThread::create()` 包裹文件读取 → 正则替换 → 文件写入的循环
2. 完成后通过 QueuedConnection 回主线程通知 `backlinkIndex->rebuildFile()` 更新索引
3. 加入取消令牌以支持快速连续操作

**风险**：在后台线程修改磁盘文件时，如果用户在编辑器中也修改了同一文件，可能产生冲突。可接受的风险 — 此类场景需要用户在编辑器中先保存。

---

### Fix 6: smdcell.cpp — grab() 优化（有限改进）

**目标**：减少 grab() 的感知卡顿

**改动文件**：`smdcell.cpp`

**方案**（择一）：
- **方案 A（最小改动）**：grab 前后设置 `QApplication::setOverrideCursor(Qt::WaitCursor)` 提供视觉反馈
- **方案 B（评估中）**：尝试使用 `QWebEnginePage::printToPdf()` + `QPdfDocument::render()` 替代 grab（需要 Qt 6.8+）

**风险**：方案 B 对其他 Qt 版本的兼容性不确定

---

## 六、优先级排序

| 优先级 | 修复项 | 预期改进 | 难度 |
|--------|--------|----------|------|
| P0 | Fix 2: smdeditor waitForStarted(5000) | 消除 5 秒 UI 冻结 | 中 |
| P0 | Fix 1: searchpanel 后台搜索 | 大项目搜索不再卡 UI | 中 |
| P1 | Fix 3: buildFileIndex 异步化 | 文件重命名/删除操作流畅 | 低 |
| P1 | Fix 4: processrunner stop() | 停止编译/运行即时响应 | 低 |
| P2 | Fix 5: wiki link 重命名异步化 | 重命名高频引用文件不再卡 | 中 |
| P3 | Fix 6: grab() 优化 | 渲染切换时的感知体验 | 低-中 |

---

## 七、验证清单

- [ ] `jom.exe -f Makefile.Release -j22` 编译通过，无新增警告
- [ ] 大项目搜索：在本项目根目录搜索 "include"，UI 保持响应，结果正确
- [ ] SMD Python 执行：打开 .smd 文件，执行 Python 单元格，无 5 秒卡顿
- [ ] SMD Python 停止：停止运行中的 Python 进程，UI 即时响应
- [ ] 编译运行：F5 编译 C++ 文件，停止按钮即时响应
- [ ] 文件操作：重命名一个有 [[wikilink]] 引用的文件，UI 不卡顿
- [ ] 回归：搜索面板、Judge、SMD 编辑、编译运行、WikiLink 等核心功能正常
- [ ] 主题切换、文件浏览等常规操作无回归
