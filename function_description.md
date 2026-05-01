## 功能说明文档

### 1. `MainWindow` - 主窗口控制器

**文件**：`mainwindow.h` / `mainwindow.cpp`

**职责**：
- 作为应用程序的主窗口，负责整体布局与用户交互。
- 聚合 `FileExplorerWidget`、`EditorWidget`、`QSplitter` 等子组件。
- 管理工具栏、快捷键（如 `Ctrl+S` 保存）。
- 加载与保存应用程序的全局配置（通过 `SettingsManager`）。
- 协调文件选择与编辑器的联动：当用户在文件树中点击 Markdown/TXT 文件时，通知编辑器加载并显示内容。
- 处理窗口关闭事件，自动保存窗口大小、拆分条位置、最后浏览的文件夹等状态。

**主要接口**：
- `void onFileSelected(const QString &filePath)`：槽函数，接收文件浏览器发出的文件选中信号，调用编辑器加载文件。
- `void saveFile()`：槽函数，保存当前编辑器中的内容到当前文件。
- `void loadSettings()` / `void saveSettings()`：配置读写。

**协作关系**：
- 持有 `FileExplorerWidget*`、`EditorWidget*`、`QSplitter*`、`SettingsManager*`。
- 连接文件浏览器的 `fileClicked` 信号到自己的 `onFileSelected` 槽。
- 工具栏的“保存”动作触发 `saveFile`，最终调用编辑器的 `saveFile` 方法。

---

### 2. `FileExplorerWidget` - 文件浏览器组件

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
- 被 `MainWindow` 使用，其 `fileClicked` 信号直接连接到主窗口的文件加载逻辑。
- 主窗口通过 `selectFolder` 槽响应用户的“打开目录”工具栏按钮。

---

### 3. `EditorWidget` - 文本编辑器组件

**文件**：`editorwidget.h` / `editorwidget.cpp`

**职责**：
- 封装 `QTextEdit`，提供基本的文本编辑功能。
- 管理当前编辑文件的路径和修改状态。
- 支持从文件加载内容 (`loadFile`) 和将内容保存到文件 (`saveFile` / `saveAsFile`)。
- 发出 `fileLoaded`、`fileSaved` 和 `modificationChanged` 信号，便于主窗口监听状态变化（例如显示“已修改”标题栏星号）。

**主要接口**：
- `bool loadFile(const QString &filePath)`：加载指定文件，成功后更新内部路径并重置修改标记。
- `bool saveFile()`：保存到当前已打开的路径。如果路径为空则返回 `false`。
- `bool saveAsFile(const QString &filePath)`：另存为指定路径，并更新当前文件路径。
- `QString currentFilePath() const`：返回当前正在编辑的文件路径（可能为空）。
- `QString toPlainText() const` / `void setPlainText(const QString &text)`：访问编辑器内容。
- `bool isModified() const` / `void setModified(bool)`：管理文档修改状态。

**信号**：
- `void fileLoaded(const QString &filePath)`
- `void fileSaved(const QString &filePath)`
- `void modificationChanged(bool modified)`

**协作关系**：
- 被 `MainWindow` 聚合，主窗口在用户点击文件树时调用 `loadFile`，在用户保存时调用 `saveFile`。
- 未来可独立扩展语法高亮、行号、Markdown 预览等功能，无需修改主窗口。

---

### 4. `SettingsManager` - 配置管理类

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
- `MainWindow` 不再直接接触 `QSettings` 的键名和原始 `QVariant`，降低了耦合。

---

### 5. `Ui::MainWindow` (自动生成)

**文件**：`ui_mainwindow.h`（由 `mainwindow.ui` 文件通过 `uic` 生成）

**职责**：
- 包含由 Qt Designer 设计的界面布局，但本项目完全采用代码手动构建界面，因此该 UI 类未被实际使用。
- 保留 `ui->setupUi(this)` 调用以保持框架完整性，但不对其添加任何控件。

**协作关系**：
- 仅被 `MainWindow` 引用，但实际界面布局完全由代码中的 `QSplitter` 和工具栏构建。

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
