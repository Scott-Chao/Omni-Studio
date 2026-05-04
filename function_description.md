## 功能说明文档（v0.0.4）

### 已实现的主要功能
- 打开指定根目录，并以树视图呈现文件
- 显示 `.txt` 和 `.md` 文件
- 文件的新建，保存，另存为操作，支持快捷键
- 关闭文件时提示未保存的修改
- 同时打开多个文件，显示在标签页栏中
- 支持 Markdown 预览模式：可在源码编辑与渲染预览之间切换
- 对话框路径记忆：打开目录和另存为对话框会自动定位到上次使用的文件夹，两个路径独立记忆
- 字体缩放：支持对编辑器和 Markdown 预览进行字体缩放，可通过工具栏按钮、快捷键、Ctrl+鼠标滚轮或触控板手势操作

### 问题修复（v0.0.4)
- 修复了上一个版本中，缩放操作也导致文件被标记为已修改的bug
- **无实质变更后自动清除修改标记**：当用户修改内容后手动恢复至原始状态时，编辑器会在停止输入 300ms 后自动比较内容并同步更新修改标记。
- 更新文档，修改其中存在问题的部分

### 1. `MainWindow` - 主窗口控制器

**文件**：`mainwindow.h` / `mainwindow.cpp`

**职责**：
- 作为应用程序的主窗口，负责整体布局与用户交互。
- 聚合 `FileExplorerWidget`、`TabManager`、`QSplitter` 等子组件。
- 加载与保存应用程序的全局配置（通过 `SettingsManager`），包括窗口几何、分割条状态、上次访问的目录（打开目录和另存为分别记忆）。
- 协调文件树与标签管理器的联动：当用户在文件树中点击 Markdown/TXT 文件时，通知 `TabManager` 打开或切换到对应文件。
- 接管保存与另存为的路径记忆逻辑：在保存新建文件或另存为时，读取并更新独立的另存为目录配置；保存已有文件不改变该记忆。
- 处理窗口关闭事件，调用 `TabManager::closeAllTabs()` 检查所有未保存的文件，并根据用户选择决定是否退出。
- 管理工具栏，包括文件操作（新建、保存、另存为）、预览模式切换、以及字体缩放控件（−、百分比标签、+、重置）。
- 支持以下快捷键：
  - `Ctrl+N` 新建、`Ctrl+S` 保存、`Ctrl+Shift+S` 另存为、`Ctrl+Shift+P` 预览切换
  - `Ctrl+=` 放大字体、`Ctrl+-` 缩小字体、`Ctrl+0` 重置缩放

**主要接口（槽函数）**：
- `void onFileSelected(const QString &filePath)`：转发文件路径给 `TabManager::openFile`。
- `void newFile()`：转发给 `TabManager::newFile`。
- `void saveFile()`：若当前文件无路径则调用 `onSaveFileAs()`（使用记忆路径），否则直接调用编辑器的 `saveFile()`。
- `void onSaveFileAs()`：从配置读取另存为记忆路径，调用编辑器的 `saveAsFile()` 并在成功后更新配置。
- `void onOpenFolder()`：从配置读取上次打开的目录，调用文件浏览器的 `selectFolder()`。
- `void onFolderChanged(const QString &newPath)`：响应文件浏览器目录变更，立即持久化打开目录记忆。
- `void loadSettings()` / `void saveSettings()`：配置读写。
- `void onZoomIn()` / `void onZoomOut()` / `void onZoomReset()`：将缩放操作转发给 `TabManager` 当前激活的 `EditorWidget`。
- `void updateZoomLabel()`：更新状态栏中的缩放百分比标签。
- `void connectCurrentEditorZoomSignal()`：在标签切换或创建新编辑器时，连接/重连当前编辑器的 `zoomFactorChanged` 信号，确保百分比标签实时同步。

**协作关系**：
- 持有 `FileExplorerWidget*`、`TabManager*`、`QSplitter*`、`SettingsManager*`。
- 连接文件浏览器的 `fileClicked` 信号到自己的 `onFileSelected` 槽。
- 连接文件浏览器的 `folderChanged` 信号到 `onFolderChanged`，以在用户通过对话框切换目录时记录路径。
- 工具栏的“保存”动作触发 `saveFile`，转为直接操作编辑器并处理记忆；“另存为”动作触发 `onSaveFileAs`。
- 标签页的创建、关闭或标题更新，这些职责全部委托给 `TabManager`。
- 工具栏中添加了“预览模式”按钮（可勾选），用于切换当前编辑器的预览状态。
- 持有缩放相关的 UI 元素：`QAction`（放大/缩小/重置）和 `QLabel`（百分比），并将它们布局在状态栏中。
- 监听 `TabManager::currentChanged` 信号，当标签页切换时调用 `updateZoomLabel()` 和 `connectCurrentEditorZoomSignal()`，保持缩放信息与当前编辑器同步。
- 在 `newFile()` 和 `onFileSelected()` 中确保新建立的编辑器连接了 `zoomFactorChanged` 信号，且新建文件会继承当前活动标签的缩放倍率。

---

### 2. `TabManager` - 多标签页管理器

**文件**：`tabmanager.h` / `tabmanager.cpp`

**职责**：
- 继承自 `QTabWidget`，封装所有与编辑器标签页相关的操作。
- 管理多个 `EditorWidget` 实例，每个标签页对应一个打开的文件或新建的未命名文档。
- 提供统一的接口：打开文件（若已存在则切换）、新建空白文件、保存当前文件、关闭标签页等。
- 监听每个编辑器的 `modificationChanged` 和 `fileSaved` 信号，自动更新对应标签标题（修改时添加 `*` 号）。
- 在关闭标签页时，检查编辑器是否已修改，弹出自定义保存提示对话框（显示当前文件名、自定义按钮文字“保存”、“不保存”、“取消”）。
- 提供 `closeAllTabs()` 方法，用于主窗口关闭时逐个尝试关闭所有标签页，若任一用户取消则返回 `false` 阻止退出。

**主要接口**：
- `EditorWidget* openFile(const QString &filePath)`：若文件已在某标签中打开则切换到该标签，否则新建标签并加载文件。
- `EditorWidget* newFile()`：创建一个未命名的空白编辑器，添加为新标签。
- `EditorWidget* currentEditor() const`：返回当前活动标签下的编辑器。
- `void saveCurrentFile()`：保存当前编辑器的内容（无路径时自动调用另存为）。
- `bool closeTab(int index)`：关闭指定索引的标签页，返回 `true` 表示已关闭（或用户选择不保存），`false` 表示用户取消了操作。
- `bool closeAllTabs()`：依次关闭所有标签页，若任何一次 `closeTab` 返回 `false` 则立即停止并返回 `false`。

**信号**：
- `void tabCountChanged(int count)`：当标签数量变化时发出（供外部如窗口标题更新使用）。

**协作关系**：
- 被 `MainWindow` 持有，主窗口将文件打开、新建、保存等操作直接转发给 `TabManager`。
- 内部创建和持有 `EditorWidget`，并连接其状态信号以更新标签标题。
- 与 `QMessageBox` 交互，提供自定义的文件保存提示对话框。

---

### 3. `EditorWidget` - 文本编辑器组件

**文件**：`editorwidget.h` / `editorwidget.cpp`

**职责**：
- 封装文本编辑功能，支持 **Markdown 源码编辑** 与 **渲染预览** 两种模式。
- 编辑模式使用 `QTextEdit` 编写 Markdown 源码；预览模式使用 `QTextBrowser` 通过 `QTextDocument::setMarkdown` 将源码转为富文本显示。
- 两种模式通过内部 `QStackedWidget` 切换，共用文件路径和修改状态。
- 管理当前编辑文件的路径和修改状态。
  内部维护一份保存/加载时的原始内容副本，当文本内容变化且停止输入 300ms 后自动与原始内容比对；若两者一致则自动清除修改标记，避免“输入再删除”导致的误标记。
- 支持从文件加载内容 (`loadFile`) 和将内容保存到文件 (`saveFile` / `saveAsFile`)。
- 发出 `fileLoaded`、`fileSaved` 和 `modificationChanged` 信号，便于标签管理器监听状态变化（例如更新标签标题中的星号）。
- 内置字体缩放功能：维护缩放因子，提供 `zoomIn`/`zoomOut`/`zoomReset` 方法，可统一调整编辑器与预览器的字体大小（通过 `applyZoom` 实现）。
  编辑器缩放通过 `QFont` 与 `QTextCursor::mergeCharFormat` 保证全文包括代码块字号同步；预览缩放通过刷新 HTML 并设置默认字体及样式表（强制所有元素继承基准字号）来完成。
  缩放操作通过临时阻断文档信号并在完成后恢复修改状态，确保不会导致文件被错误标记为已修改。

**主要接口**：
- `bool loadFile(const QString &filePath)`：加载指定文件，成功后更新内部路径并重置修改标记。
- `bool saveFile()`：保存到当前已打开的路径。如果路径为空则返回 `false`。
- `bool saveAsFile(const QString &defaultDir = QString())`：弹出文件对话框，另存为指定路径，并更新当前文件路径。 `defaultDir` 参数，可指定对话框的起始目录，为空则使用主文件夹。
- `QString currentFilePath() const`：返回当前正在编辑的文件路径（可能为空）。
- `QString toPlainText() const` / `void setPlainText(const QString &text)`：访问编辑器内容。
- `bool isModified() const` / `void setModified(bool)`：管理文档修改状态。
- `void setPreviewMode(bool preview)`：切换预览模式（`true` 显示渲染视图，`false` 显示源码视图）。
- `bool isPreviewMode() const`：返回当前是否为预览模式。
- `void refreshPreview()`：强制刷新预览内容（将当前 Markdown 源码转换为 HTML 并显示）。
- `void zoomIn()` / `void zoomOut()` / `void zoomReset()`：按 0.1 步长调整缩放因子（范围 0.5～3.0），并立即应用字体变化。
- `qreal zoomFactor() const`：返回当前缩放倍数。
- `void setZoomFactor(qreal factor)`：设置绝对缩放倍数，并触发 `applyZoom()` 与 `zoomFactorChanged` 信号。

**信号**：
- `void fileLoaded(const QString &filePath)`
- `void fileSaved(const QString &filePath)`
- `void modificationChanged(bool modified)`
- `void zoomFactorChanged(qreal factor)`：当缩放因子改变时发出，供主窗口更新百分比标签。

**协作关系**：
- 被 `TabManager` 创建和管理，`TabManager` 连接其信号以更新标签标题。
- 主窗口通过 `setPreviewMode` 控制预览状态，并将预览按钮的勾选状态与当前编辑器同步。
- 在构造函数中对 `m_textEdit` 和 `m_previewBrowser` 的 **viewport** 安装事件过滤器，拦截 `QWheelEvent`（Ctrl修饰）和 `QNativeGestureEvent`（缩放手势），统一转向 `zoomIn()`/`zoomOut()`，避免 Qt 内置缩放绕开自定义状态管理。
- `applyZoom` 在模式切换或缩放变化时同步编辑区和预览区的字体；`refreshPreview` 在生成 HTML 时嵌入当前缩放字号，并强制 `pre`、`code` 等元素继承字体大小。

---

### 4. `FileExplorerWidget` - 文件浏览器组件

**文件**：`fileexplorerwidget.h` / `fileexplorerwidget.cpp`

**职责**：
- 封装 `QFileSystemModel` 和 `QTreeView`，提供一个可嵌入的文件树。
- 支持切换根目录（通过 `setRootPath`）。
- 过滤显示：只显示文件夹和 `.md` / `.txt` 文件（在信号发出前进行过滤）。
- 发出 `fileClicked` 信号，携带被选中文件的绝对路径。
- 提供 `selectFolder` 公共槽，弹出目录选择对话框并更新根目录。支持传入初始目录参数，以便对话框从上次记忆的路径开始浏览。

**主要接口**：
- `void setRootPath(const QString &path)`：设置文件树显示的根目录。
- `QString rootPath() const`：返回当前根目录。
- `void selectFolder(const QString &defaultDir = QString())`：弹出文件夹选择对话框，若 `defaultDir` 不为空则将其作为对话框的起始目录。用户选择后更新根目录并发出 `folderChanged` 信号。

**信号**：
- `void fileClicked(const QString &filePath)`：当用户点击一个有效文件（非目录且后缀为 .md/.txt）时发出。
- `void folderChanged(const QString &newPath)`：当用户通过 `selectFolder` 对话框选择了新目录后发出，用于主窗口记忆路径。

**协作关系**：
- 被 `MainWindow` 使用，其 `fileClicked` 信号连接到主窗口的 `onFileSelected` 槽，最终转发给 `TabManager`。
- `folderChanged` 信号连接到 `MainWindow::onFolderChanged`，实现路径记忆。

---

### 5. `SettingsManager` - 配置管理类

**文件**：`settingsmanager.h` / `settingsmanager.cpp`

**职责**：
- 封装 `QSettings`，提供统一、类型安全的配置读写接口。
- 负责管理 `config.ini` 文件的存储位置（默认与可执行文件同目录）。
- 支持窗口几何信息、拆分条状态、最后打开的文件夹路径、最后另存为的文件夹路径等持久化，两者独立记忆。
- 提供扩展方法，便于未来添加字体大小、快捷键配置等。

**主要接口**：
- `void setWindowGeometry(const QByteArray &geometry)` / `QByteArray windowGeometry() const`
- `void setSplitterState(const QByteArray &state)` / `QByteArray splitterState() const`
- `void setLastFolderPath(const QString &path)` / `QString lastFolderPath(const QString &defaultPath = QString()) const`
- `void setLastSaveAsFolderPath(const QString &path)` / `QString lastSaveAsFolderPath(const QString &defaultPath = QString()) const`
- `void clear()`：清除所有设置。

**协作关系**：
- 仅被 `MainWindow` 使用，在 `loadSettings` 和 `saveSettings` 中调用对应方法。

---

### 6. `main.cpp` - 应用程序入口

**文件**：`main.cpp`

**职责**：
- 创建 `QApplication` 实例。
- 根据系统 UI 语言尝试加载并安装对应的翻译文件（`smart-markdown_<locale>.qm`）
- 创建并显示 `MainWindow` 主窗口。
- 进入事件循环。

---

### 配置存储说明

- 配置文件名为 `config.ini`，默认保存在 **应用程序可执行文件所在的目录**（通过 `QCoreApplication::applicationDirPath()` 获得）。

### 界面定制细节

- **标签页样式**：通过 `QTabWidget` 的样式表设置了标签最小高度、左右内边距（`padding: 4px 12px`）、圆角以及选中/悬停背景色，解决了标签左右空位过小的问题。
- **保存提示对话框**：使用 `QMessageBox` 并设置自定义按钮文字（"保存(&S)"、"不保存(&D)"、"取消(&C)"），提示文本中包含当前文件名，且通过样式表设置最小尺寸（400×200 像素）。
- **Markdown 预览模式**：在工具栏添加了“预览模式”按钮（快捷键 `Ctrl+Shift+P`），可切换当前文档的源码编辑与渲染预览视图。预览基于 Qt 原生的 `QTextDocument::setMarkdown`，支持 GitHub 风格的 Markdown 方言。
- **缩放控件**：在状态栏底部右侧放置缩小按钮（`−`）、百分比标签（如 `100%`）、放大按钮（`+`）和重置按钮，同时支持快捷键 `Ctrl+=`、`Ctrl+-` 和 `Ctrl+0`。百分比标签随当前编辑器的缩放因子实时更新，且当前编辑器的缩放变化会触发该标签刷新。
