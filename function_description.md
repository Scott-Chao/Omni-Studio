## 功能说明文档（v0.0.1）

### 已实现的主要功能
- 打开指定根目录，并以树视图呈现文件
- 显示 `.txt` 和 `.md` 文件
- 文件的新建，保存，另存为操作，支持快捷键
- 关闭文件时提示未保存的修改
- 同时打开多个文件，显示在标签页栏中
- **支持 Markdown 预览模式**：可在源码编辑与渲染预览之间切换

### 1. `MainWindow` - 主窗口控制器

**文件**：`mainwindow.h` / `mainwindow.cpp`

**职责**：
- 作为应用程序的主窗口，负责整体布局与用户交互。
- 聚合 `FileExplorerWidget`、`TabManager`、`QSplitter` 等子组件。
- 管理工具栏、快捷键（如 `Ctrl+N` 新建、`Ctrl+S` 保存、`Ctrl+Shift+S` 另存为、`Ctrl+Shift+P` 预览切换）。
- 加载与保存应用程序的全局配置（通过 `SettingsManager`）。
- 协调文件树与标签管理器的联动：当用户在文件树中点击 Markdown/TXT 文件时，通知 `TabManager` 打开或切换到对应文件。
- 处理窗口关闭事件，调用 `TabManager::closeAllTabs()` 检查所有未保存的文件，并根据用户选择决定是否退出。

**主要接口（槽函数）**：
- `void onFileSelected(const QString &filePath)`：转发文件路径给 `TabManager::openFile`。
- `void newFile()`：转发给 `TabManager::newFile`。
- `void saveFile()`：转发给 `TabManager::saveCurrentFile`。
- `void loadSettings()` / `void saveSettings()`：配置读写。

**协作关系**：
- 持有 `FileExplorerWidget*`、`TabManager*`、`QSplitter*`、`SettingsManager*`。
- 连接文件浏览器的 `fileClicked` 信号到自己的 `onFileSelected` 槽。
- 工具栏的“保存”动作触发 `saveFile`，最终调用 `TabManager` 的保存逻辑。
- 标签页的创建、关闭或标题更新，这些职责全部委托给 `TabManager`。
- 工具栏中添加了“预览模式”按钮（可勾选），用于切换当前编辑器的预览状态。

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
- 支持从文件加载内容 (`loadFile`) 和将内容保存到文件 (`saveFile` / `saveAsFile`)。
- 发出 `fileLoaded`、`fileSaved` 和 `modificationChanged` 信号，便于标签管理器监听状态变化（例如更新标签标题中的星号）。

**主要接口**：
- `bool loadFile(const QString &filePath)`：加载指定文件，成功后更新内部路径并重置修改标记。
- `bool saveFile()`：保存到当前已打开的路径。如果路径为空则返回 `false`。
- `bool saveAsFile()`：弹出文件对话框，另存为指定路径，并更新当前文件路径。
- `QString currentFilePath() const`：返回当前正在编辑的文件路径（可能为空）。
- `QString toPlainText() const` / `void setPlainText(const QString &text)`：访问编辑器内容。
- `bool isModified() const` / `void setModified(bool)`：管理文档修改状态。
- `void setPreviewMode(bool preview)`：切换预览模式（`true` 显示渲染视图，`false` 显示源码视图）。
- `bool isPreviewMode() const`：返回当前是否为预览模式。
- `void refreshPreview()`：强制刷新预览内容（将当前 Markdown 源码转换为 HTML 并显示）。

**信号**：
- `void fileLoaded(const QString &filePath)`
- `void fileSaved(const QString &filePath)`
- `void modificationChanged(bool modified)`

**协作关系**：
- 被 `TabManager` 创建和管理，`TabManager` 连接其信号以更新标签标题。
- 主窗口通过 `setPreviewMode` 控制预览状态，并将预览按钮的勾选状态与当前编辑器同步。

---

### 4. `FileExplorerWidget` - 文件浏览器组件

**文件**：`fileexplorerwidget.h` / `fileexplorerwidget.cpp`

**职责**：
- 封装 `QFileSystemModel` 和 `QTreeView`，提供一个可嵌入的文件树。
- 支持切换根目录（通过 `setRootPath`）。
- 过滤显示：只显示文件夹和 `.md` / `.txt` 文件（在信号发出前进行过滤）。
- 发出 `fileClicked` 信号，携带被选中文件的绝对路径。
- 提供 `selectFolder` 公共槽，弹出目录选择对话框并更新根目录。

**主要接口**：
- `void setRootPath(const QString &path)`：设置文件树显示的根目录。
- `QString rootPath() const`：返回当前根目录。
- `void selectFolder()`：弹出文件夹选择对话框，改变根目录。

**信号**：
- `void fileClicked(const QString &filePath)`：当用户点击一个有效文件（非目录且后缀为 .md/.txt）时发出。

**协作关系**：
- 被 `MainWindow` 使用，其 `fileClicked` 信号连接到主窗口的 `onFileSelected` 槽，最终转发给 `TabManager`。

---

### 5. `SettingsManager` - 配置管理类

**文件**：`settingsmanager.h` / `settingsmanager.cpp`

**职责**：
- 封装 `QSettings`，提供统一、类型安全的配置读写接口。
- 负责管理 `config.ini` 文件的存储位置（默认与可执行文件同目录）。
- 定义所有配置项的键名常量，避免魔法字符串分散在代码中。
- 支持窗口几何信息、拆分条状态、最后访问文件夹路径等持久化。
- 提供扩展方法，便于未来添加字体大小、快捷键配置等。

**主要接口**：
- `void setWindowGeometry(const QByteArray &geometry)` / `QByteArray windowGeometry() const`
- `void setSplitterState(const QByteArray &state)` / `QByteArray splitterState() const`
- `void setLastFolderPath(const QString &path)` / `QString lastFolderPath(const QString &defaultPath = QString()) const`
- `void setEditorFontSize(int size)` / `int editorFontSize(int defaultValue = 12) const`（预留）
- `void clear()`：清除所有设置。

**协作关系**：
- 仅被 `MainWindow` 使用，在 `loadSettings` 和 `saveSettings` 中调用对应方法。

---

### 6. `main.cpp` - 应用程序入口

**文件**：`main.cpp`

**职责**：
- 创建 `QApplication` 实例。
- 调用 `QApplication::setAttribute(Qt::AA_EnableHighDpiScaling)` 启用高 DPI 适配。
- 创建并显示 `MainWindow` 主窗口。
- 进入事件循环。

---

### 配置存储说明

- 配置文件名为 `config.ini`，默认保存在 **应用程序可执行文件所在的目录**（通过 `QCoreApplication::applicationDirPath()` 获得）。

### 界面定制细节

- **标签页样式**：通过 `QTabWidget` 的样式表设置了标签最小高度、左右内边距（`padding: 4px 12px`）、圆角以及选中/悬停背景色，解决了标签左右空位过小的问题。
- **保存提示对话框**：使用 `QMessageBox` 并设置自定义按钮文字（“保存(&S)”、“不保存(&D)”、“取消(&C)”），提示文本中包含当前文件名，且调用 `resize(450, 180)` 增大对话框尺寸，改善了用户体验。
- **Markdown 预览模式**：在工具栏添加了“预览模式”按钮（快捷键 `Ctrl+Shift+P`），可切换当前文档的源码编辑与渲染预览视图。预览基于 Qt 原生的 `QTextDocument::setMarkdown`（要求 Qt ≥ 5.14），支持 GitHub 风格的 Markdown 方言。
