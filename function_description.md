## 功能说明文档（v0.2.1）

### 已实现的主要功能
- 打开指定根目录，并以树视图呈现文件
- 支持多种文本文件格式（`.md`、`.txt`、`.c`、`.cpp`、`.py`、`.js`、`.html`、`.css`、`.json`、`.xml` 等 40+ 种常见文本文件扩展名），所有文件均以纯文本形式呈现在编辑器中
- 文件的新建，保存，另存为操作，支持快捷键
- 关闭文件时提示未保存的修改
- 同时打开多个文件，显示在标签页栏中，拖拽标签页时，整个标签矩形始终不超出标签页栏的左右边界
- 支持 Markdown 预览模式：可在源码编辑与渲染预览之间切换，仅在打开`.md`文件时可用。预览支持 GitHub Flavored Markdown、LaTeX 数学公式和 Mermaid 图表渲染
- 对话框路径记忆：打开目录和另存为对话框会自动定位到上次使用的文件夹，两个路径独立记忆
- 字体缩放：支持对编辑器和 Markdown 预览进行字体缩放，可通过工具栏按钮、快捷键、Ctrl+鼠标滚轮或触控板手势操作
- 文件树右键菜单：支持内联新建文件（`.md`）、新建文件夹；支持重命名（内联编辑，空名称自动恢复原名）和删除操作，删除前会提示确认，并自动处理已打开文件的关闭。
- 历史记录功能：记录之前打开过的文件，通过历史记录面板快速访问。文件删除后自动清理对应历史条目；点击已不存在文件的条目时自动移除。
- 文件树支持拖拽移动，并自动进行路径同步。
- 双向链接：支持 `[[文件名]]` 语法。在预览模式下自动识别为超链接，点击可跳转至对应文件；若文件不存在，支持一键自动创建。文件重命名时，自动更新所有引用文件中的双向链接文本。
- 反向链接面板：自动扫描并展示当前文件的引用来源，点击可跳转至来源文件
- 全文搜索面板：支持在当前目录所有文本文件中检索关键词，搜索结果展示文件名、行号与上下文片段，点击可跳转至文件并高亮匹配关键词

### 新增 v0.2.1
- **全文搜索面板**：支持在当前目录所有文本文件中检索关键词（快捷键 `Ctrl+Shift+F`）。搜索结果展示文件名、行号与上下文片段，点击可跳转至文件并金色高亮所有匹配关键词。面板位于左侧停靠区域，300ms 输入防抖，不自动隐藏以支持连续点击。新增 `SearchPanel` 组件和 `EditorWidget::scrollToLine` 方法。

### 1. `MainWindow` - 主窗口控制器

**文件**：`mainwindow.h` / `mainwindow.cpp`

**职责**：
- 作为应用程序的主窗口，负责整体布局与用户交互。
- 聚合 `FileExplorerWidget`、`TabManager`、`QSplitter` 等子组件。
- 加载与保存应用程序的全局配置（通过 `SettingsManager`），包括窗口几何、分割条状态、上次访问的目录（打开目录和另存为分别记忆）。
- 协调文件树与标签管理器的联动：当用户在文件树中点击任意文件时，通知 `TabManager` 打开或切换到对应文件。
- 接管保存与另存为的路径记忆逻辑：在保存新建文件或另存为时，读取并更新独立的另存为目录配置；保存已有文件不改变该记忆。
- 处理窗口关闭事件，调用 `TabManager::closeAllTabs()` 检查所有未保存的文件，并根据用户选择决定是否退出。
- 管理工具栏，包括文件操作（新建、保存、另存为）、预览模式切换（仅`.md`文件可见）、以及字体缩放控件（−、百分比标签、+、重置）。
- 支持以下快捷键：
  - `Ctrl+N` 新建、`Ctrl+S` 保存、`Ctrl+Shift+S` 另存为、`Ctrl+Shift+P` 预览切换（仅当编辑`.md`文件时可用）
  - `Ctrl+=` 放大字体、`Ctrl+-` 缩小字体、`Ctrl+0` 重置缩放
  - `Ctrl+H` 打开/关闭历史记录面板
  - `Ctrl+Shift+B` 打开/关闭反向链接面板
  - `Ctrl+Shift+F` 打开/关闭搜索面板
  - `Delete`：在文件树中选中文件夹/文件时，直接触发删除操作（非重命名状态）
- 处理文件树的右键菜单请求：协调文件树的新建文件夹、重命名、删除操作。删除前检查是否有未保存的文件（或子文件），弹出确认对话框，强制关闭相关标签页后再执行删除，确保数据安全。
- 管理历史记录面板（`QDockWidget` + `HistoryPanel`），在工具栏最左侧提供显示/隐藏面板的按钮（状态与面板可见性联动）。
  在文件打开、另存为等操作成功后自动记录历史；响应历史文件点击，打开文件并视情况切换文件树根目录（仅当文件不在当前根目录内时才切换）。并通过全局事件过滤器实现点击面板外部自动隐藏。
- 管理反向链接面板（`QDockWidget` + `BacklinksPanel` + `BacklinkIndex`），在工具栏提供显示/隐藏面板的按钮（快捷键 `Ctrl+Shift+B`）。
  通过全局事件过滤器实现点击面板外部自动隐藏；标签页切换时自动查询反链索引并刷新面板显示；文件保存后增量更新反链索引并刷新面板。
- 管理搜索面板（`QDockWidget` + `SearchPanel`），在工具栏提供显示/隐藏面板的按钮（快捷键 `Ctrl+Shift+F`）。搜索面板不自动隐藏（持久侧边栏行为）。
  搜索结果显示文件名、行号和上下文片段；点击结果时打开文件并高亮匹配关键词。
- 跳转与创建逻辑：处理 `wikiLinkClicked` 信号，搜索匹配文件并提供文件不存在时的自动创建交互。
- 项目索引管理：负责维护全局文件路径映射（通过 `TextFileUtils::scanNameFilters()` 扫描多种文本类型），确保双向链接在跨文件夹移动或重命名后依然有效。
- 响应文件树拖拽移动事件：连接 `FileExplorerWidget::fileRenamed` 信号到新槽 `onFileMovedOrRenamed`，统一执行路径更新与索引同步。

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
  同时，在 `connectCurrentEditorZoomSignal()` 中连接当前编辑器的 `filePathChanged` 信号到 `updatePreviewActionState`，以便文件路径（如通过外部重命名或另存为）变化时刷新按钮状态。
- `void onRequestDelete(const QString &path, bool isDir)`：响应文件树发出的删除请求，检查未保存文件，弹出确认对话框，强制关闭相关标签页，最后执行实际删除。
- `void updatePreviewActionState()`：根据当前编辑器是否有效以及其文件是否为 `.md` 后缀，动态设置预览按钮的可见性、启用状态和勾选状态。当前非 `.md` 文件且处于预览模式时，自动切回编辑模式。
- `void onHistoryFileClicked(const QString &filePath)`：处理历史面板中文件的点击，打开文件，并自动调整文件树根目录（若文件不在当前根目录下则切换至其所在文件夹）。若目标文件已不存在，弹出警告后自动从历史记录中移除该条目。
- `void onSearchResultClicked(const QString &filePath, int lineNumber, const QString &searchText)`：处理搜索结果的点击，打开文件并调用 `EditorWidget::scrollToLine` 跳转到匹配行并高亮所有匹配关键词。
- `void onWikiLinkClicked(const QString &fileName)`：处理来自编辑器的 WikiLink 点击信号，执行搜索或创建流程。 
- `void buildFileIndex()`：全量扫描当前根目录，更新文件名与绝对路径的映射关系。
- `void refreshBacklinks()`：查询当前文件的反链列表并更新面板显示与标题。
- `QString findWikiTarget(const QString &fileName)`：封装多级搜索策略，依次尝试已知文本扩展名进行路径匹配，并通过索引实现智能路径解析与就近匹配算法。
- `void onFileRenamedInIndex` / `void onFileDeletedInIndex`：响应动态文件操作，同步更新内存索引。`onFileRenamedInIndex` 在索引迁移前通过 `backlinksFor(oldPath)` 捕获受影响的源文件，索引迁移后调用 `updateWikiLinksAfterRename` 将所有源文件中的 `[[旧名]]` 替换为 `[[新名]]`。`onFileDeletedInIndex` 同时调用 `HistoryPanel::removeFile` 清理历史记录中的失效条目。
- `void onFileMovedOrRenamed(const QString &oldPath, const QString &newPath)`：协调文件移动/重命名后的路径更新，依次调用 `onFileRenamedInIndex`、`TabManager::updatePathsAfterMove`、`HistoryPanel::replacePath`，确保编辑器、历史记录和索引一致。
- `void updateWikiLinksAfterRename(const QStringList &affectedSources, const QString &oldLinkText, const QString &newLinkText)`：文件重命名后更新所有引用文件中的 wiki 链接文本。从 BacklinkIndex 获取受影响源文件列表，使用 `replaceWikiLinkText` 精确匹配替换 `[[oldLinkText]]` → `[[newLinkText]]`。若源文件在打开的标签中，优先读取 `editor->toPlainText()` 以保留未保存更改，替换后写盘并重新加载编辑器。

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
- 通过 `updatePreviewActionState()` 方法统一控制预览按钮的可见性、启用状态和勾选状态，标签切换、文件路径变化、新建/打开/保存文件时都会调用该方法，确保按钮只在当前文件为 `.md` 时出现。
- 持有 `HistoryPanel*` 和 `QDockWidget*`，将面板放置于右侧停靠区域，默认隐藏。
- 持有 `BacklinkIndex*`、`BacklinksPanel*` 和对应的 `QDockWidget*`，反链面板同样放置在右侧停靠区域，默认隐藏。
  - 在标签页切换时自动调用 `refreshBacklinks()` 更新面板。
  - 在文件保存时调用 `BacklinkIndex::rebuildFile` 增量更新反链索引。
  - 在文件重命名/移动时调用 `BacklinkIndex::onFileRenamed` 迁移索引路径，并调用 `updateWikiLinksAfterRename` 将所有引用文件中的 `[[旧名]]` 替换为 `[[新名]]`。
  - 在文件删除时调用 `BacklinkIndex::onFileDeleted` 清理索引。
  - 工具栏显示/隐藏按钮通过 `m_dockBacklinks->toggleViewAction()` 实现，行为与历史面板一致。
- 连接 `HistoryPanel::fileClicked` 到 `onHistoryFileClicked`，并在所有会获得有效文件路径的地方（`onFileSelected`, `onSaveFileAs`, `saveFile` 等）调用 `addToRecentFiles` 更新历史。
- 安装全局事件过滤器，当历史面板或反链面板可见时，若鼠标点击发生在面板外部，则自动隐藏对应面板。工具栏按钮点击不触发隐藏，由 toggle 动作自行处理。

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
- 使用自定义子类 `CustomTabBar` 替换默认标签栏，以实现拖拽边界限制而不影响原有布局。

**主要接口**：
- `EditorWidget* openFile(const QString &filePath)`：若文件已在某标签中打开则切换到该标签，否则新建标签并加载文件。
- `EditorWidget* newFile()`：创建一个未命名的空白编辑器，添加为新标签。
- `EditorWidget* currentEditor() const`：返回当前活动标签下的编辑器。
- `void saveCurrentFile()`：保存当前编辑器的内容（无路径时自动调用另存为）。
- `bool closeTab(int index)`：关闭指定索引的标签页，返回 `true` 表示已关闭（或用户选择不保存），`false` 表示用户取消了操作。
- `bool closeAllTabs()`：依次关闭所有标签页，若任何一次 `closeTab` 返回 `false` 则立即停止并返回 `false`。
- `EditorWidget* findEditorByPath(const QString &filePath) const`：根据文件路径查找已打开的编辑器实例（大小写不敏感）。
- `bool closeTabByPath(const QString &filePath, bool askSave)`：关闭指定路径的标签页，`askSave` 为 `true` 时弹出保存提示，为 `false` 时强制丢弃修改。
- `QStringList allOpenedFilePaths() const`：返回所有已打开的文件路径列表（未保存的新建文件除外），路径统一为正斜杠格式。
- `void updateEditorFilePath(const QString &oldPath, const QString &newPath)`：当文件在外部被重命名时，更新对应编辑器的内部路径及标签标题。
- `void updatePathsAfterMove(const QString &oldBase, const QString &newBase)`：当文件或文件夹被移动时，批量更新所有已打开标签页的路径。支持精确匹配（文件本身）和前缀匹配（文件夹内文件），通过调用 `EditorWidget::setFilePath` 同步内部路径。

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
- 编辑模式使用 `QTextEdit` 编写 Markdown 源码；预览模式使用 `QWebEngineView` 加载内置 HTML 模板，通过 marked.js 将 Markdown 转为 HTML，并支持 KaTeX 数学公式渲染和 Mermaid 图表渲染。
- 两种模式通过内部 `QStackedWidget` 切换，共用文件路径和修改状态。
- 管理当前编辑文件的路径和修改状态。
  内部维护一份保存/加载时的原始内容副本，当文本内容变化且停止输入 300ms 后自动与原始内容比对；若两者一致则自动清除修改标记，避免”输入再删除”导致的误标记。
- 支持从文件加载内容 (`loadFile`) 和将内容保存到文件 (`saveFile` / `saveAsFile`)。
- 发出 `fileLoaded`、`fileSaved` 和 `modificationChanged` 信号，便于标签管理器监听状态变化（例如更新标签标题中的星号）。
- 内置字体缩放功能：维护缩放因子，提供 `zoomIn`/`zoomOut`/`zoomReset` 方法。编辑器缩放通过 `QFont` 与 `QTextCursor::mergeCharFormat` 保证全文包括代码块字号同步；预览缩放通过 `QWebEngineView::setZoomFactor()` 整体缩放页面（含 SVG 图表和数学公式）。
  缩放操作通过临时阻断文档信号并在完成后恢复修改状态，确保不会导致文件被错误标记为已修改。
- WikiLink 转换：在渲染预览前通过正则将 `[[Name]]` 转换为 `<a href=”wikilink:目标”>` 超链接；自定义 `PreviewPage`（继承 `QWebEnginePage`）重写 `acceptNavigationRequest()` 拦截 `wikilink:` scheme 的导航请求并发出跳转信号，同时将外部链接交由系统浏览器打开。
- LaTeX 数学公式支持：通过 KaTeX 自动渲染 `$...$`（行内）和 `$$...$$`（块级）数学公式，支持 `\(...\)` 和 `\[...\]` 备用定界符。
- Mermaid 图表支持：通过 Mermaid.js 将 ` ```mermaid ` 代码块渲染为 SVG 图表，支持流程图、时序图、甘特图等。

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
- `void setFilePath(const QString &newPath)`：更新当前编辑器的文件路径（不改变文档内容），用于外部重命名后同步。

**信号**：
- `void fileLoaded(const QString &filePath)`
- `void fileSaved(const QString &filePath)`
- `void modificationChanged(bool modified)`
- `void zoomFactorChanged(qreal factor)`：当缩放因子改变时发出，供主窗口更新百分比标签。
- `void filePathChanged(const QString &oldPath, const QString &newPath)`：当文件路径被 `setFilePath` 修改时发出，供标签管理器更新路径关联。
- `void wikiLinkClicked(const QString &fileName)`：当预览模式下的 WikiLink 被点击时发出。

**协作关系**：
- 被 `TabManager` 创建和管理，`TabManager` 连接其信号以更新标签标题。
- 主窗口通过 `setPreviewMode` 控制预览状态，并将预览按钮的勾选状态与当前编辑器同步。
- 在构造函数中对 `m_textEdit` 的 **viewport** 和 `m_previewView` 安装事件过滤器，拦截 `QWheelEvent`（Ctrl修饰）和 `QNativeGestureEvent`（缩放手势），统一转向 `zoomIn()`/`zoomOut()`，避免 Qt 内置缩放绕开自定义状态管理。
- `applyZoom` 在模式切换或缩放变化时同步编辑区的字体，并通过 `QWebEngineView::setZoomFactor()` 缩放整个预览页面（含 SVG 图表和数学公式）。

---

### 4. `FileExplorerWidget` - 文件浏览器组件

**文件**：`fileexplorerwidget.h` / `fileexplorerwidget.cpp`

**职责**：
- 提供右键菜单交互，支持新建文件（默认 `.md`）、新建文件夹、重命名、删除。所有文件均可进行右键操作。
- 启用 `QTreeView` 的内联编辑功能（通过 `EditKeyPressed` 和 `SelectedClicked` 
- 自定义委托 (NoGhostDelegate)：为了修复原生内联编辑可能出现的“重影”问题（编辑框未完全覆盖原文本导致新旧文字重叠），以及避免编辑时文件图标消失，特实现了自定义委托。
  - updateEditorGeometry：通过 QStyle::SE_ItemViewItemText 获取精确的文本绘制区域，将编辑框（QLineEdit）仅放置于文本区域，保留图标区域不被遮挡。
  - paint：在编辑状态下，清空 option.text 后调用基类绘制，保留图标、背景等视觉元素，但不绘制原文本，彻底消除重影。
  - createEditor：设置编辑框背景不透明（跟随系统调色板或白色背景），去除边框和内边距，确保编辑框视效整洁。
  - setEditorData：重写以控制初始选中范围。重命名文件夹时全选整个名称；重命名文件时只选中主文件名（不含扩展名）。
  - setModelData：重写以校验用户输入。若用户清空文件夹名，或清空文件名使其只剩扩展名（如 `.md`），自动恢复为原始名称，防止产生仅含扩展名的无效文件。
- 监听 `QFileSystemModel::fileRenamed` 信号，并转发 `fileRenamed` 信号，供主窗口更新标签页路径。
- 提供 `createNewFolderInline`、`createNewFileInline`、`deleteItem` 等公共方法，封装实际的文件系统操作。
- 使用自定义排序代理 `FileSortProxyModel` 确保文件夹优先于文件显示，且在新建、重命名后自动重排。
- 重写 `eventFilter`，监听 `Delete` 键，在非编辑状态下直接对选中项发起删除请求。
- 右键菜单中使用 `DeleteKeyFilter` 事件过滤器，支持在菜单弹出时按 `Delete` 键触发删除。
- 发出 `operationFailed` 信号，用于向用户展示文件操作错误。
- 发出 `fileClicked` 信号，携带被选中文件的绝对路径。
- 提供 `selectFolder` 公共槽，弹出目录选择对话框并更新根目录。支持传入初始目录参数，以便对话框从上次记忆的路径开始浏览。
- 支持拖拽移动文件或文件夹（在该树视图内），通过事件过滤器拦截 DragEnter、DragMove、Drop 事件，在符合条件时执行文件系统移动并发送 `fileRenamed` 信号。
- 拖拽时对目标文件夹提供视觉反馈：通过自定义委托 `NoGhostDelegate` 在悬停文件夹底部绘制蓝色横条。

**主要接口**：
- `void setRootPath(const QString &path)`：设置文件树显示的根目录。
- `QString rootPath() const`：返回当前根目录。
- `void selectFolder(const QString &defaultDir = QString())`：弹出文件夹选择对话框，若 `defaultDir` 不为空则将其作为对话框的起始目录。用户选择后更新根目录并发出 `folderChanged` 信号。
- `void createNewFolderInline(const QString &parentDir)`：在指定父目录下内联新建文件夹，并立即进入重命名状态。
- `void createNewFileInline(const QString &parentDir)`：在指定父目录下内联新建 `.md` 文件，并立即进入重命名状态。
- `void deleteItem(const QString &path, bool isDir)`：删除文件或文件夹（递归删除）。
- `void handleDropEvent(QDropEvent *event)`：处理拖放事件的内部方法，解析源与目标路径，执行文件移动，刷新模型并发送信号。
- `bool isDropTargetFolder(const QModelIndex &proxyIndex) const`：供自定义委托查询当前索引是否为拖拽目标文件夹。

**信号**：
- `void fileClicked(const QString &filePath)`：当用户点击一个有效文件（非目录）时发出。
- `void folderChanged(const QString &newPath)`：当用户通过 `selectFolder` 对话框选择了新目录后发出，用于主窗口记忆路径。
- `void fileRenamed(const QString &oldPath, const QString &newPath)`：重命名成功时发出，用于更新标签管理器中的路径。路径使用 `QDir::absoluteFilePath` 规范化（统一 `/` 分隔符），确保与 `BacklinkIndex` 存储的路径格式一致。
- `void operationFailed(const QString &errorMsg)`：文件操作失败时发出，由主窗口显示错误消息。
- `void itemDeleted(const QString &path)`：在成功删除文件/文件夹后发出。

**协作关系**：
- 被 `MainWindow` 使用，其 `fileClicked` 信号连接到主窗口的 `onFileSelected` 槽，最终转发给 `TabManager`。
- `folderChanged` 信号连接到 `MainWindow::onFolderChanged`，实现路径记忆。
- 内部使用 NoGhostDelegate 附加到 QTreeView，无需外部干预。
- 事件过滤器同时安装于 `m_treeView->viewport()`，确保拖放事件能正确捕获。

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
- `void setRecentFiles(const QStringList &files)` / `QStringList recentFiles() const`：读写最近打开的文件列表（最多50条），键名 `History/recentFiles`。

**协作关系**：
- 仅被 `MainWindow` 使用，在 `loadSettings` 和 `saveSettings` 中调用对应方法。
- 历史记录面板通过该类存取最近文件列表。

---

### 6. `HistoryPanel` - 历史记录面板

**文件**：`historypanel.h` / `historypanel.cpp`

**职责**：
- 以 `QListWidget` 纵向展示用户最近打开的文件列表，顶部为最新打开的文件，底部为最早的文件。
- 列表上限为 50 条，自动去重：如果新加入的文件已在历史中存在，会先删除所有同名项（不区分大小写），再将新条目插入顶部，确保无重复。
- 提供 `addFile(const QString &filePath)` 公共方法用于添加历史记录，内部自动规范化路径、去重，但不负责持久化。
- 从 `SettingsManager` 加载/保存最近文件列表，通过 `loadHistory()` 和 `saveHistory()` 与配置文件同步。
- 当用户点击列表中的文件时，发出 `fileClicked(const QString &filePath)` 信号，供主窗口调用打开与目录切换逻辑。
- 面板自身是一个 `QWidget`，被嵌入 `QDockWidget` 中由主窗口管理显示/隐藏。
- 界面底部提供一个“清空历史记录”按钮，点击后弹出确认对话框，确认后立即删除所有历史记录并写入空列表到配置。

**主要接口**：
- `void addFile(const QString &filePath)`：将指定文件加入历史（已存在则置顶），并自动限制数量。操作仅更新内存数据，不立即持久化。
- `void removeFile(const QString &filePath)`：从历史记录中移除指定文件路径。支持精确文件匹配和文件夹前缀匹配（删除文件夹时一并清理其下所有文件的历史条目）。操作仅更新内存数据，不立即持久化。
- `void loadHistory()` / `void saveHistory()`：从配置读取/保存最近文件列表。`saveHistory()` 仅在程序关闭时由 `MainWindow::closeEvent` 调用，运行期间不主动写磁盘。
- `void clearHistory()`：清空所有历史记录并立即写入配置文件（破坏性操作，即时持久化）。
- `void replacePath(const QString &oldBase, const QString &newBase)`：在文件移动后，将历史记录中相关路径更新为新路径，支持精确/前缀匹配。更新后重建 UI 列表，但不立即持久化（延迟至程序关闭时统一保存）。

**信号**：
- `void fileClicked(const QString &filePath)`：用户点击某个历史文件时发出。

**协作关系**：
- 由 `MainWindow` 创建并持有，作为 `QDockWidget` 的内容部件。
- 通过 `SettingsManager` 读写最近文件列表，配置键名称为 `History/recentFiles`。
- `MainWindow` 在打开文件、另存为等操作成功后调用 `addFile` 以更新历史；文件删除时调用 `removeFile` 清理对应条目；文件移动/重命名时调用 `replacePath` 更新路径。所有增删改操作仅更新内存，窗口正常关闭时调用 `saveHistory()` 统一持久化到配置。
  同时连接其 `fileClicked` 信号到 `onHistoryFileClicked` 槽，处理文件打开与目录切换，若文件不存在则自动清理历史条目。

---

### 7. `BacklinkIndex` - 反向链接索引

**文件**：`backlinkindex.h` / `backlinkindex.cpp`

**职责**：
- 维护反链索引：记录每个目标文件被哪些来源文件通过 `[[链接]]` 语法引用。
- 全量扫描已知文本类型文件，使用与 `EditorWidget::refreshPreview` 一致的递归正则 `\[\[((?:[^\[\]]|\[(?1)\])*)\]\]` 提取所有 WikiLink。
- 将 WikiLink 解析为实际文件路径，解析策略为：根目录下精确路径匹配 → 通过文件名索引（`completeBaseName`）全局查找 → 多匹配时取最短路径（确定性，不依赖当前编辑器上下文）。
- 同一文件内多次出现相同的 `[[链接]]` 自动去重，确保每条"来源→目标"关系只记录一次。

**主要接口**：
- `void buildIndex(const QString &rootPath, const QMap<QString, QStringList> &fileIndex)`：全量扫描重建索引。
- `void rebuildFile(const QString &filePath, const QString &rootPath, const QMap<QString, QStringList> &fileIndex)`：增量更新单个文件（保存时调用）。
- `void onFileRenamed(const QString &oldPath, const QString &newPath)`：迁移索引中的路径信息。同步更新 `m_backlinks` 的 target key 和值列表中的 source 路径引用，以及 `m_forwardLinks` 值列表中的 target 路径引用，确保后续 `rebuildFile → removeFile` 能找到正确的 target 进行清理。
- `void onFileDeleted(const QString &path)`：从索引中移除已删除文件，O(1)。
- `QStringList backlinksFor(const QString &filePath) const`：查询指定文件的来源列表，O(1)。

**内部数据结构**：
- `QMap<QString, QStringList> m_backlinks`：目标绝对路径 → 来源绝对路径列表。
- `QMap<QString, QStringList> m_forwardLinks`：来源绝对路径 → 目标绝对路径列表（用于增量更新时的反向清理）。

**协作关系**：
- 由 `MainWindow` 创建并持有，所有接口均由 `MainWindow` 在合适的时机调用。
- 依赖 `MainWindow::m_fileIndex`（文件名→路径映射）进行 WikiLink 目标解析。

---

### 8. `BacklinksPanel` - 反向链接面板

**文件**：`backlinkspanel.h` / `backlinkspanel.cpp`

**职责**：
- 以 `QListWidget` 纵向展示引用当前文件的来源文件列表。
- 列表项显示来源文件的文件名，ToolTip 显示完整路径，点击可发出 `fileClicked` 信号供 `MainWindow` 打开该文件。
- 如果当前文件没有被任何文件引用，显示灰色（`#888`）占位文本"无反向链接"。
- 面板宽度通过 `setMinimumWidth(200)` 确保稳定，无论是否有反链内容都不会塌缩。

**主要接口**：
- `void showBacklinks(const QStringList &sourceFiles)`：刷新面板显示内容。

**信号**：
- `void fileClicked(const QString &filePath)`：用户点击某个来源文件时发出。

**协作关系**：
- 由 `MainWindow` 创建并持有，作为 `QDockWidget` 的内容部件。
- `MainWindow` 在标签页切换、文件保存等操作后调用 `showBacklinks` 刷新面板。
- 点击事件复用 `MainWindow::onHistoryFileClicked` 槽，打开文件的同时自动处理文件树目录切换。

---

### 9. `SearchPanel` — 全文搜索面板

**文件**：`searchpanel.h` / `searchpanel.cpp`

**职责**：
- 提供全文搜索功能，在当前根目录下的所有文本文件中检索关键词。
- 搜索输入支持 300ms 防抖，避免每次按键都触发磁盘扫描。
- 使用 `QDirIterator` + `TextFileUtils::scanNameFilters()` 递归收集文本文件列表。
- 使用 `QTextStream::readLine()` 逐行流式读取文件内容，`QString::toLower().contains()` 进行大小写不敏感匹配。
- 结果上限：每文件最多 20 个匹配，总计最多 500 条结果。
- 每个结果项展示文件名、行号和上下文片段（自动截断并对齐匹配关键词位置）。
- 面板置于左侧 `QDockWidget`，默认隐藏（快捷键 `Ctrl+Shift+F`），显示时自动聚焦搜索输入框。
- 与历史/反链面板不同，搜索面板**不**在点击外部时自动隐藏（持久侧边栏行为）。

**主要接口**：
- `void setRootPath(const QString &path)`：设置搜索根目录（由 `MainWindow` 在打开文件夹或切换目录时调用）。
- `void clearSearch()`：清空搜索输入和结果列表。
- `void focusSearchInput()`：聚焦搜索框并全选内容。

**信号**：
- `void resultClicked(const QString &filePath, int lineNumber, const QString &searchText)`：用户点击某个搜索结果时发出。

**协作关系**：
- 由 `MainWindow` 创建并持有，作为 `QDockWidget` 的内容部件放置在左侧停靠区域。
- `MainWindow` 在 `loadSettings` 和 `onFolderChanged` 时调用 `setRootPath` 同步搜索根目录。
- 搜索结果点击连接到 `MainWindow::onSearchResultClicked` 槽，打开文件并调用 `EditorWidget::scrollToLine` 完成跳转和高亮。

---

### 10. `main.cpp` - 应用程序入口

**文件**：`main.cpp`

**职责**：
- 创建 `QApplication` 实例。
- 根据系统 UI 语言尝试加载并安装对应的翻译文件（`smart-markdown_<locale>.qm`）
- 创建并显示 `MainWindow` 主窗口。
- 进入事件循环。

---

### 11. `TextFileUtils` — 文本文件工具命名空间

**文件**：`fileutils.h`

**职责**：
- 统一定义项目中支持的文本文件扩展名列表（40+ 种），涵盖编程语言、Web、配置、脚本等常见文本格式。
- 提供三个内联工具函数，供其他模块引用：
  - `textExtensions()`：返回支持的文本文件扩展名列表（`QStringList`），`.md` 排在首位以保证 Wiki 链接优先匹配 Markdown 文件。
  - `scanNameFilters()`：返回 `QDirIterator` 所需的名称过滤器列表（如 `*.md`、`*.txt`、`*.cpp` 等）。
  - `isTextExtension(const QString &suffix)`：判断给定的后缀是否在已知文本扩展名列表中。

**扩展名列表**：`md`、`markdown`、`txt`、`c`、`cpp`、`cxx`、`cc`、`h`、`hpp`、`hxx`、`hh`、`cs`、`java`、`py`、`pyw`、`js`、`jsx`、`ts`、`tsx`、`mjs`、`rs`、`go`、`rb`、`php`、`swift`、`kt`、`kts`、`html`、`htm`、`css`、`scss`、`sass`、`less`、`xml`、`svg`、`json`、`yaml`、`yml`、`toml`、`ini`、`cfg`、`conf`、`rst`、`tex`、`log`、`csv`、`tsv`、`sql`、`graphql`、`proto`、`sh`、`bash`、`zsh`、`fish`、`ps1`、`bat`、`cmd`、`cmake`、`mak`、`mk`、`pro`、`pri`、`qml`、`qrc`、`ui`、`diff`、`patch`。

**协作关系**：
- 被 `MainWindow`、`BacklinkIndex`、`EditorWidget` 引用，用于文件索引构建、Wiki 链接解析和另存为过滤器。

---

### 配置存储说明

- 配置文件名为 `config.ini`，默认保存在 **应用程序可执行文件所在的目录**（通过 `QCoreApplication::applicationDirPath()` 获得）。
- 最近文件列表：键 `History/recentFiles`，存储为 `QStringList`，按最新在前的顺序保存。运行期间仅维护内存数据，程序关闭时统一写入磁盘。

### 界面定制细节

- **标签页样式**：通过 `QTabWidget` 的样式表设置了标签最小高度、左右内边距（`padding: 4px 12px`）、圆角以及选中/悬停背景色，解决了标签左右空位过小的问题。
- **保存提示对话框**：使用 `QMessageBox` 并设置自定义按钮文字（"保存(&S)"、"不保存(&D)"、"取消(&C)"），提示文本中包含当前文件名，且通过样式表设置最小尺寸（400×200 像素）。
- **Markdown 预览模式**：在工具栏添加了“预览模式”按钮（快捷键 `Ctrl+Shift+P`），**该按钮仅在当前编辑的文件为 `.md` 后缀时可见且可用**。切换到其他类型文件或没有标签页时，按钮自动隐藏，对应的快捷键同时失效。
  如果当前正处在预览模式但切换到了一个非 `.md` 文件，编辑器会自动退回到源码编辑模式。
- **缩放控件**：在状态栏底部右侧放置缩小按钮（`−`）、百分比标签（如 `100%`）、放大按钮（`+`）和重置按钮，同时支持快捷键 `Ctrl+=`、`Ctrl+-` 和 `Ctrl+0`。百分比标签随当前编辑器的缩放因子实时更新，且当前编辑器的缩放变化会触发该标签刷新。
- **文件树右键菜单**：通过 `QMenu` 动态构建。在文件夹或空白处可内联新建文件/文件夹，新建后立即进入命名编辑状态；对已有项目支持重命名（内联编辑）和删除。删除前弹出确认对话框。
- **排序规则**：文件树始终按“文件夹优先、名称升序”排列，且新建或重命名后实时重排。
- **删除确认对话框**：删除前弹出 `QMessageBox::question`，根据是否存在未保存修改提供差异化提示文本。
- **标签拖拽限制**：拖动标签重排时，被拖动的标签整体始终保持在标签栏区域内，不会出现标签部分或全部移出栏外的情况。
- **历史记录面板**：通过 `QDockWidget` 嵌入窗口右侧，默认隐藏（快捷键 `Ctrl+H`）。列表项设置为不可选中（`NoSelection`），点击可触发打开文件操作（若文件不存在则自动弹出警告并清理该条目）。鼠标悬停会有完整路径提示。同时提供清空功能。文件删除或移动后自动同步更新历史记录。运行期间仅维护内存数据，程序关闭时统一持久化以减少磁盘 I/O。点击编辑器、文件树等其他区域时，面板自动收起，减少手动操作。
- **反向链接面板**：通过 `QDockWidget` 嵌入窗口右侧，默认隐藏（快捷键 `Ctrl+Shift+B`）。列表项不可选中（`NoSelection`），点击可跳转至来源文件。反链为空时显示灰色占位文本"无反向链接"，面板宽度通过 `setMinimumWidth(200)` 保持稳定。与历史记录面板共享同一外部点击自动隐藏逻辑。面板标题固定为"反向链接"，不显示数字计数以保持简洁。
- **搜索面板**：通过 `QDockWidget` 嵌入窗口左侧，默认隐藏（快捷键 `Ctrl+Shift+F`）。搜索输入框带清除按钮，输入后 300ms 自动触发搜索。结果列表每项包含文件名（粗体，显示行号）和灰色上下文片段。点击结果跳转至文件并金色高亮所有匹配关键词。面板显示时自动聚焦输入框；不实现点击外部自动隐藏，方便多次点击结果。
- **拖拽移动视觉反馈**：当用户在文件树中拖拽文件经过文件夹时，目标文件夹底部会显示一条 3 像素高的蓝色指示条（颜色 `#2196F3`），拖拽离开或释放鼠标后消失。
