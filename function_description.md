## 功能说明文档（v0.11.9）

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
- WikiLink 自动补全：输入 `[[` 时自动弹出文件名列表，方向键选择，Tab 补全并自动闭合 `]]`
- #tag 自动补全：输入 `#` 时自动弹出已有标签列表，Tab 补全标签名
- 代码编辑器模式：打开 C/C++、Python 等代码文件时，自动切换为代码编辑模式，提供语法高亮、行号显示、自动缩进、括号补全、智能退格、Ctrl+/ 行注释切换、Ctrl+[ / Ctrl+] 缩进调整等功能。语言支持可通过 `LanguageUtils` 注册表扩展。独立 `.cpp`/`.py` 文件支持 **LSP 代码补全**（Ctrl+I / 自动触发）、**悬停类型提示**、**函数签名帮助**和 **诊断波浪线**（错误/警告）。`.py` 文件通过 Jedi helper 进程的 `diagnostics` action 获取语法诊断，`.cpp` 文件通过 clangd 的 `textDocument/publishDiagnostics` 获取诊断。诊断信息通过 `Ctrl+D` 切换底部 `BottomPanel` 的诊断标签页查看。
- SMD LSP 代码智能*：`.smd` 文件中 C++/Python 单元格共享一个 LSP 后端（每种语言一个 clangd/Jedi 进程，而非每 cell 一个），通过 **虚拟文档拼接** 技术实现跨 cell 类型解析、代码补全、悬停提示和函数签名帮助。C++ 虚拟文档按 `main()` 函数边界**自动分组**，仅向 clangd 发送当前聚焦 cell 所在程序组的代码，避免多 `main()` 冲突。编辑器显示 **红色/黄色诊断波浪线**（错误/警告），cell 头部标签显示错误计数。切换 cell 时自动切换诊断上下文并缓存各组诊断结果。
- 文件树预览标签页：单击文件以临时标签页（斜体标题）预览，多次单击复用同一标签页；双击永久打开；编辑临时标签页内容后自动提升为永久标签页。
- 文件树与标签页联动：切换标签页时，文件树自动选中对应的文件，并展开折叠的父级目录，确保文件在树中可见。
- 编译运行：在代码编辑模式下，可通过工具栏或快捷键（F5 编译运行、F6 编译、F7 运行）编译运行 C/C++ 文件，或直接运行 Python 文件。**非代码文件（如 Markdown）时按钮完全隐藏**，快捷键同步失效。C/C++ 调用 g++ 或 MSVC 编译后运行；Python 调用解释器直接执行。按 F6（单独编译）对 Python 文件显示提示"Python 不需要编译"；按 F7（单独运行）若无可执行文件则自动转为编译运行流程。输出面板嵌入编辑器下方（右侧分割区），不延伸至文件树区域，与其他侧边面板互不遮挡。支持标准输入交互。隐藏输出面板时若进程运行中则自动终止并恢复按钮状态。
- 面包屑路径栏：文件树顶部展示当前根目录的完整路径，每个文件夹段可点击快速跳转。路径自动换行不撑宽左侧面板，根目录切换时同步更新。其下方为文件树工具栏，显示当前文件夹名称及操作按钮。
- 异步索引构建：切换到大目录时，文件索引、反向链接扫描与标签索引扫描在后台线程依次执行（Phase 1/2/3），UI 保持响应。支持快速切换取消旧扫描，仅最后选中的目录结果生效。
- 本地评测（Local Judge）：在代码编辑模式下，可通过评测面板（Ctrl+Shift+J）选择测试用例文件夹，一键批量运行所有测试用例，显示 OJ 风格结果（AC/WA/RE/TLE/MLE）和耗时/内存，点击失败行查看预期输出与实际输出对比。自动跳过空的 `.out` 文件；编译后先预热运行一次消除冷启动计时偏差；内存通过启动时同步捕获 + 退出时补充读取 + 定时轮询三重机制确保准确检测。支持 Python 评测。
- OpenJudge 题目爬虫集成：通过评测面板的"从OpenJudge获取"按钮打开独立浏览窗口，可登录 OpenJudge 或跳过登录直接浏览。支持作业列表（进行中 + 已结束）→ 题目列表 → 题目详情的三级导航，已结束的作业支持分页浏览。题目详情页左侧章节导航，右侧渲染题目内容（深色主题）。点击"选择此题目"自动提取样例输入/输出并写入临时缓存目录，回填至评测面板的测试用例文件夹。
- OpenJudge 登录管理：OpenJudge 浏览窗口工具栏登录/退出登录按钮，登录成功后按钮变为"退出登录"，显示绿色用户名标签；登录失败弹出错误提示。退出登录时清除 Cookie 并匿名重新加载主页。支持自动登录：登录对话框中提供"自动登录"复选框，勾选并登录成功后自动保存凭据到配置文件，下次未登录时自动登录无需手动输入。用户退出登录后自动清除自动登录凭据。
- OpenJudge 代码提交：评测面板新增"提交到OpenJudge"按钮，将当前代码文件直接提交到 OpenJudge 浏览窗口中选定的题目。自动映射文件扩展名到对应语言（.c→GCC, .cpp/.cc/.cxx→G++, .py/.pyw→Python3）。提交前检查登录状态、代码有效性、题目选择状态及作业是否进行中，不满足时弹出相应提示。
- 提交结果面板：提交后自动显示评测结果面板（暗色主题），大号彩色状态文字（AC 绿色、WA 红色、TLE 蓝色、MLE 紫色、RE 红色、PE 深橙、OLE 粉红、CE 橙色），显示用时(ms)和内存(KB)，CE 时展示编译错误日志。结果面板占据右侧分割区 1/3 高度，替换底部面板位置，可手动隐藏。
- Markdown 预览代码块语法高亮：预览模式下的代码块使用 C++ 端预处理方案，复用与代码编辑器完全一致的语法高亮规则，支持 C/C++ 和 Python，通过 `highlighted` 自定义围栏块绕过 marked.js 处理
- 分屏预览模式：在 Markdown 编辑模式下，可通过工具栏按钮或快捷键 `Ctrl+P` 进入分屏预览。编辑器区域被可拖拽的竖直分隔条分为左右两部分：左侧为 Markdown 源码，右侧为渲染预览。分屏预览与全屏预览模式互斥（开启一个自动关闭另一个），切换文件时自动记忆各标签页的预览状态。右侧预览采用防抖延迟更新策略（默认 500ms），仅在文本变化后才刷新，减少不必要的渲染开销。两侧字体大小与全局缩放同步。预览区域的 wikilink、tag、代码块运行等功能与全屏预览一致。
- 自定义标题栏与无边框窗口 + 工具栏重组：隐藏系统原生标题栏，QToolBar 上移至标题栏位置。左侧 [文件 ▼] 下拉菜单（打开目录/新建/保存/另存为），中间 Expanding spacer 拖拽区域（双击最大化/还原），右侧 [面板][预览][分屏][运行 ▼] 按钮组。运行按钮含下拉菜单（编译 F6/运行 F7/编译运行 F5），仅代码文件可见。最右侧最小化/最大化/关闭按钮（CaptionBtn 自绘悬停背景）。支持拖拽空白区域移动窗口、双击最大化/还原、边缘 10px 缩放、Aero Snap 贴靠。
- ActivityBar 左侧活动栏：48px 固定宽度竖条，#333337 背景。5 个 SVG 图标按钮（搜索/AI 助手/设置/导出PDF/评测），每个 48×48px。激活态左边框 #0078D4 高亮。搜索与 AI 在上方，stretch 后将设置/导出PDF/评测挤到底部。导出 PDF 仅 .md 文件可见。
- 右侧统一面板：RightPanelContainer 组件，历史/大纲/标签/反链合并为单个 QDockWidget。顶部 32px tab 栏（图标 + 文字），下方 QStackedWidget 切换面板内容。点击外部自动隐藏。工具栏 [面板] 按钮 (toggleRightPanelAction) 或 Ctrl+Shift+E toggle。
- 左 dock 区互斥：搜索面板与评测面板通过 tabifyDockWidget 共用左侧区域，显示一个时自动隐藏另一个。
- 设置面板：工具栏"设置"按钮（快捷键 `Ctrl+,`），打开悬浮式设置面板，背景自动变暗，支持拖拽标题栏移动和边缘拖拽调整大小，右上角关闭按钮或再次按快捷键关闭。面板内提供**默认缩放比例**设置：可拖动的滑块（范围 50%~300%，步长 10%），右侧数字框可直接输入数值（4 位限制，超出范围自动钳位，空输入恢复 100%）。设置自动保存至 `config.ini`，启动时自动读取；修改后所有已打开编辑器实时同步，新打开文件默认使用该缩放值。
- 快捷键自定义：设置面板快捷键页面支持交互式录制。点击快捷键区域进入录制状态，按下目标组合键完成设置，Delete/Backspace 清除，Escape 取消。冲突检测弹窗提示覆盖或取消，覆盖时自动清空冲突操作的快捷键。所有修改实时生效，自动持久化至 `config.ini`，支持"恢复默认"一键重置。
- 自动保存：编辑器自动保存机制，默认开启（30 秒间隔）。文件加载后及手动保存后自动启动定时器，有修改时自动写入文件。支持在设置面板中通过开关控件实时开启/关闭。
- 大纲/标题导航面板：集成在右侧统一面板中，打开右侧面板即可切换至大纲 tab。自动解析当前文档中所有标题（`#` ~ `######`，跳过围栏代码块），按层级缩进显示，h1 最亮 h6 逐级变暗，h1/h2 加粗。点击标题可精准跳转。切换标签页、保存文件时自动刷新。非 `.md` 文件时面板清空。
- .smd 文件格式：采用 `---smd:<type>` 分隔符的单元格分块格式，类似 Jupyter Notebook 的交互模式。编辑器自动识别 `.smd` 后缀切换为 SMD 编辑模式，保存/另存为对话框支持 `.smd` 格式。
  - 三种单元格类型：Markdown / C++ / Python，每单元格高度自适应内容，输出区独立置于单元格下方（1-15 行自适应，上限 1000 行，超出时保留前 1000 行并提示隐藏行数）
  - 双模式系统：命令模式（紫色边框 `#C586C0`，键盘导航）与编辑模式（蓝色边框 `#0078D4`，文本编辑），`Esc` 切换
  - 单元格增删：命令模式下 `A` 上方插入、`B` 下方插入、`Delete` 删除（至少保留一个），支持 `Ctrl+Shift+-` 在光标处分割单元格
  - *`Ctrl+K` 弹出语言选择菜单（Markdown/C++/Python），新建单元格时自动弹出
- 单元格执行：编辑模式下 `Ctrl+Enter`（不跳转）或 `Shift+Enter`（跳转下一个）触发执行。
  - Markdown 单元格：异步渲染为图片（QWebEngineView 加载 HTML → 轮询 Mermaid 完成 → 抓取 QPixmap 遮罩），空单元格跳过
  - C++ 单元格：按 `main()` 函数边界自动分组，同组 cell 合并写入临时文件编译运行，不同程序组互不干扰
  - Python 单元格：持久化进程执行（`python_executor.py` 守护进程 + JSON-line 协议），跨 cell 共享命名空间，每 cell 独立捕获 stdout/stderr，避免前面 cell 的输出污染
  - 输出持久化：分隔线 `---smd:<type>` 后支持 JSON 元数据，自动持久化代码输出（base64 编码）和 Markdown 渲染状态，重新打开文件时自动恢复
  - 执行安全：执行期间禁止增删单元格，`Ctrl+C` 可终止执行
- LSP 代码智能（`SmdLspManager` 共享后端，每种语言一个 clangd/Jedi 进程，非每 cell 一个）：
  - 虚拟文档拼接：同语言 cell 拼接为有效源文件，行号自动映射，跨 cell 类型解析
  - C++ 程序分组：按 `main()` 边界分组，仅向 clangd 发送当前活动组代码，避免多 `main()` 冲突
  - 代码补全（Ctrl+I / 自动触发）、**悬停类型提示**、**函数签名帮助**
  - 诊断波浪线：红色错误 / 黄色警告，cell 头部标签显示错误计数，切换 cell 时自动缓存/恢复诊断
  - 诊断面板：`Ctrl+D`（编辑模式）切换 `SmdDiagnosticsPanel`，分区展示错误和警告，点击跳转至对应 cell 和行号
- `.md` ↔ `.smd` 双向转换：`Ctrl+T` 一键转换，保留光标位置映射（通过行→单元格映射），源文件修改状态保持不变

### 修复 v0.11.9
- 优化全局悬浮提示（QToolTip）的边缘内边距，使其更紧凑。通过自定义 `CompactTooltipStyle`（QProxyStyle 子类）将 `PM_ToolTipLabelFrameWidth` 设为 0，消除 Qt Fusion 样式在 tooltip 内部额外添加的边框宽度；同时将全局 QSS 中 `QToolTip` 的 `padding` 从 `4px 8px` 缩减为 `0px 4px`、`margin` 显式设为 `0px`、新增 `font-size: 12px` 控制字号。`HoverManager` 中诊断提示与 LSP 悬停提示均同步应用紧凑样式（`padding: 0px 4px; margin: 0px;`），确保编辑器内代码提示浮窗与外层按钮/列表项的系统 tooltip 视觉一致。修复了 `HoverManager::hideHover()` 中仅诊断提示才恢复样式的问题，改为只要有保存的样式即恢复。

### 1. `MainWindow` - 主窗口控制器

**文件**：`mainwindow.h` / `mainwindow.cpp`

**职责**：
- 作为应用程序的主窗口，负责整体布局与用户交互。
- 聚合 `FileExplorerWidget`、`TabManager`、`QSplitter` 等子组件。
- 加载与保存应用程序的全局配置（通过 `SettingsManager`），包括窗口几何、分割条状态、上次访问的目录（打开目录和另存为分别记忆）。
- 协调文件树与标签管理器的双向联动：用户**单击**文件树中的文件时，以预览模式（临时标签页）打开，多次单击复用同一标签页；**双击**文件时永久打开（或提升已有预览标签页）；编辑预览标签页内容后自动提升为永久。切换标签页时文件树自动选中对应文件并展开父级目录。
- 接管保存与另存为的路径记忆逻辑：在保存新建文件或另存为时，读取并更新独立的另存为目录配置；保存已有文件不改变该记忆。
- 处理窗口关闭事件，调用 `TabManager::closeAllTabs()` 检查所有未保存的文件，并根据用户选择决定是否退出。
- 管理自定义标题栏（`setupCustomTitleBar()`）：隐藏系统原生标题栏（`FramelessWindowHint` + `WS_THICKFRAME`），将工具栏改造为标题栏。工具栏右侧添加最小化/最大化/关闭按钮（`CaptionBtn` 类，使用系统原生图标并通过 `QPainter::fillRect` 自绘 hover 背景确保即时响应）。工具栏空白区域拖拽移动窗口、双击切换最大化/还原、最大化状态拖拽自动还原。通过 `nativeEvent`（`WM_NCHITTEST`）和 `event()`（`startSystemResize`）双重机制支持窗口边缘缩放（10px），`WM_NCCREATE` 确保 `WS_THICKFRAME` 样式不被覆盖以支持 Aero Snap。构造函数中通过 `DwmSetWindowAttribute`（`DWMWA_WINDOW_CORNER_PREFERENCE = DWMWCP_ROUND`）向 DWM 请求系统级圆角，确保在 Fusion 样式下 Windows 11 窗口圆角依然生效（最大化时自动关闭）。
- 管理工具栏，包括文件操作（新建、保存、另存为、导出PDF）、帮助按钮（F1，位于 spacer 与面板按钮之间）、预览模式切换（全屏预览 / 分屏预览，均仅`.md`文件可见且互斥）、以及字体缩放控件（−、百分比标签、+、重置）。导出PDF按钮仅 `.md` 文件可见。
- 支持以下快捷键：
  - `Ctrl+N` 新建、`Ctrl+S` 保存、`Ctrl+Shift+S` 另存为、`Ctrl+E` 导出PDF（仅 `.md`）、`Ctrl+D` 切换底部诊断面板（仅代码文件）、`Ctrl+T` .md ↔ .smd 转换（仅 `.md` / `.smd` 文件）、`Ctrl+Shift+P` 全屏预览切换（仅 `.md`）、`Ctrl+P` 分屏预览切换（仅 `.md`，与全屏预览互斥）
  - `Ctrl+=` 放大字体、`Ctrl+-` 缩小字体、`Ctrl+0` 重置缩放至设置面板中配置的默认缩放比例
  - `Ctrl+H` 打开/关闭历史记录面板
  - `Ctrl+Shift+B` 打开/关闭反向链接面板
  - `Ctrl+Shift+T` 打开/关闭标签面板
  - `Ctrl+Shift+O` 打开/关闭大纲面板
  - `Ctrl+Shift+F` 打开/关闭搜索面板
  - `Ctrl+Shift+J` 打开/关闭评测面板
  - `Ctrl+,` 打开/关闭设置面板
  - `F1` 打开/关闭帮助面板
  - `F5` 编译运行（C/C++ 编译后运行，Python 直接运行）、`F6` 编译（仅 C/C++，Python 提示不需要编译）、`F7` 运行
  - `Ctrl+Break` 终止正在运行的编译或程序
  - `Delete`：在文件树中选中文件夹/文件时，直接触发删除操作（非重命名状态）
- 处理文件树的右键菜单请求：协调文件树的新建文件夹、重命名、删除操作。删除前检查是否有未保存的文件（或子文件），弹出确认对话框，强制关闭相关标签页后再执行删除，确保数据安全。
- 管理历史记录面板（`QDockWidget` + `HistoryPanel`），在工具栏最左侧提供显示/隐藏面板的按钮（状态与面板可见性联动）。
  在文件打开、另存为等操作成功后自动记录历史；响应历史文件点击，打开文件并视情况切换文件树根目录（仅当文件不在当前根目录内时才切换）。并通过全局事件过滤器实现点击面板外部自动隐藏。
- 管理反向链接面板（`QDockWidget` + `BacklinksPanel` + `BacklinkIndex`），在工具栏提供显示/隐藏面板的按钮（快捷键 `Ctrl+Shift+B`）。
  通过全局事件过滤器实现点击面板外部自动隐藏；标签页切换时自动查询反链索引并刷新面板显示；文件保存后增量更新反链索引并刷新面板。
- 管理搜索面板（`QDockWidget` + `SearchPanel`），在工具栏提供显示/隐藏面板的按钮（快捷键 `Ctrl+Shift+F`）。搜索面板不自动隐藏（持久侧边栏行为）。
  搜索结果显示文件名、行号和上下文片段；点击结果时打开文件并高亮匹配关键词。
- 管理底部统一面板（`BottomPanel`）：嵌入右侧垂直分割区（`m_rightSplitter`），置于编辑器下方，不延伸至文件树区域。默认隐藏，首次显示时自动设置高度为右侧分割器的 1/3。包含两个标签页：**输出**（`OutputPanel`，编译/运行输出和 stdin 交互）和**诊断**（`DiagnosticsTab`，代码诊断信息列表）。提供编译运行按钮的可见性控制：仅在代码编辑模式下显示，非代码模式完全隐藏（快捷键同步失效）。连接 `closeRequested` 信号，关闭面板时若进程运行中则先终止进程再隐藏。标签页切换时自动管理诊断 provider 连接：切换到代码文件时清空旧诊断、连接新 provider 并立即从 `CodeEditor::diagnostics()` 缓存恢复诊断；切换到非代码文件时自动隐藏面板。`diagnosticsLineClicked` 信号连接至编辑器行跳转。
- 管理评测面板（`QDockWidget` + `JudgePanel`），在工具栏提供显示/隐藏面板的按钮（快捷键 `Ctrl+Shift+J`）。评测面板默认隐藏，启动评测时自动显示并保持在可见状态。
- 管理帮助面板（`HelpPanel` + `m_helpOverlay`，`OverlayWidget` 顶层遮罩层）：工具栏帮助按钮（快捷键 `F1`）调用 `toggleHelp()` 切换显示/隐藏。遮罩层为独立顶层 `Qt::Tool` 窗口（`WA_TranslucentBackground` + `paintEvent` 绘制半透明黑色背景），覆盖整个主窗口，面板居中显示。通过事件过滤器监听顶层 overlay 的 `MouseButtonPress` 实现点击遮罩层背景关闭面板。`resizeEvent()` 和 `moveEvent()` 中通过 `positionOverlay()` 跟踪 overlay 位置同步，`changeEvent()` 中最小化时自动隐藏。
- 跳转与创建逻辑：处理 `wikiLinkClicked` 信号，搜索匹配文件并提供文件不存在时的自动创建交互。
- 项目索引管理：负责维护全局文件路径映射（通过 `TextFileUtils::scanNameFilters()` 扫描多种文本类型），确保双向链接在跨文件夹移动或重命名后依然有效。索引构建支持异步模式（`startAsyncIndexBuild()`），在后台线程依次执行文件扫描、反向链接索引构建和标签索引构建（Phase 1/2/3），使用代际计数器（`std::atomic<uint64_t> m_scanId`）和取消标志（`std::shared_ptr<std::atomic<bool>> m_scanCancelled`）防止过期结果覆盖和进行中扫描浪费资源。
- 响应文件树拖拽移动事件：连接 `FileExplorerWidget::fileRenamed` 信号到新槽 `onFileMovedOrRenamed`，统一执行路径更新与索引同步。
- `.md` ↔ `.smd` 双向转换：通过 `m_convertMdSmdAction`（快捷键 `Ctrl+T`）触发 `onConvertMdSmd()`。对当前 `.md` 或 `.smd` 文件调用 `convertMdToSmd()` / `convertSmdToMd()`，生成对应格式文件并写入磁盘（目标未打开时）或直接更新内存内容（目标已打开时）。转换使用 `SmdFormat::fromMarkdownWithMapping()` / `toMarkdownWithMapping()`，保留光标位置（通过行→单元格映射），源文件修改状态保持不变。

**主要接口（槽函数）**：
- `void onFileSelected(const QString &filePath)`：单击文件树 → 转发给 `TabManager::openPreview`（预览模式打开）。
- `void onFileDoubleClicked(const QString &filePath)`：双击文件树 → 若目标文件在预览标签页中则调用 `promotePreviewToPermanent()`，否则调用 `openFile()` 永久打开。
- `void newFile()`：转发给 `TabManager::newFile`。
- `void saveFile()`：若当前文件无路径则调用 `onSaveFileAs()`（使用记忆路径），否则直接调用编辑器的 `saveFile()`。
- `void onSaveFileAs()`：从配置读取另存为记忆路径，调用编辑器的 `saveAsFile()` 并在成功后更新配置。
- `void onExportPdf()`：弹出保存对话框，将当前 Markdown 文档通过新建隐藏 WebEngine 视图渲染后导出为 PDF，完成后在状态栏显示结果。
- `void onConvertMdSmd()`：处理 `Ctrl+T` 快捷键，根据当前文件扩展名（`.md` 或 `.smd`）分派到 `convertMdToSmd()` 或 `convertSmdToMd()`。
- `void onOpenFolder()`：从配置读取上次打开的目录，调用文件浏览器的 `selectFolder()`。
- `void onFolderChanged(const QString &newPath)`：响应文件浏览器目录变更，立即持久化打开目录记忆。
- `void loadSettings()` / `void saveSettings()`：配置读写。`loadSettings()` 中若无保存分割状态，默认设置左侧文件树与右侧编辑区拉伸比例为 1:4。
- `void onZoomIn()` / `void onZoomOut()` / `void onZoomReset()`：将缩放操作转发给 `TabManager` 当前激活的 `EditorWidget`。`onZoomReset()` 重置至 `SettingsManager::editorDefaultZoom()` 配置的默认缩放值（默认为 1.0），而非固定 100%。
- `void updateZoomLabel()`：更新状态栏中的缩放百分比标签。
- `void onDefaultZoomChanged(qreal zoom)`：响应设置面板的默认缩放变更，保存至 `SettingsManager`（`editor/defaultZoom`），遍历所有已打开编辑器调用 `setZoomFactor()` 实时应用，并刷新缩放标签。
- `void connectCurrentEditorZoomSignal()`：在标签切换或创建新编辑器时，连接/重连当前编辑器的 `zoomFactorChanged` 信号，确保百分比标签实时同步。
  同时，在 `connectCurrentEditorZoomSignal()` 中连接当前编辑器的 `filePathChanged` 信号到 `updatePreviewActionState`，以便文件路径（如通过外部重命名或另存为）变化时刷新按钮状态。
- `void syncFileTreeSelection()`：在标签页切换时将文件树的选中项同步到当前编辑器正在编辑的文件，内部获取当前文件路径并转发给 `FileExplorerWidget::selectFile()`。
- `void onRequestDelete(const QString &path, bool isDir)`：响应文件树发出的删除请求，检查未保存文件，弹出确认对话框，强制关闭相关标签页，最后执行实际删除。
- `void updatePreviewActionState()`：根据当前编辑器是否有效以及其文件是否为 `.md` 后缀，动态设置全屏预览按钮的可见性、启用状态和勾选状态。当前非 `.md` 文件且处于预览模式时，自动切回编辑模式。
- `void updateSplitPreviewActionState()`：与 `updatePreviewActionState()` 对称，管理分屏预览按钮的可见性、启用状态和勾选状态。确保两个预览按钮互斥（开启一个自动关闭另一个）。
- `void onHistoryFileClicked(const QString &filePath)`：处理历史面板中文件的点击，打开文件，并自动调整文件树根目录（若文件不在当前根目录下则切换至其所在文件夹）。若目标文件已不存在，弹出警告后自动从历史记录中移除该条目。
- `void onSearchResultClicked(const QString &filePath, int lineNumber, const QString &searchText)`：处理搜索结果的点击，打开文件并调用 `EditorWidget::scrollToLine` 跳转到匹配行并高亮所有匹配关键词。
- `void onWikiLinkClicked(const QString &fileName)`：处理来自编辑器的 WikiLink 点击信号，执行搜索或创建流程。 
- `void buildFileIndex()`：全量扫描当前根目录，更新文件名与绝对路径的映射关系（同步版本，保留用于重命名/删除后的即时更新）。
- `void startAsyncIndexBuild()`：异步版本的索引构建，使用 `QThread::create()` 在后台线程依次执行文件索引构建、反向链接扫描和标签索引构建（Phase 1/2/3）。支持取消令牌和扫描代际保护。完成后交付主线程并刷新补全列表、反链面板和标签面板。
- `void refreshBacklinks()`：查询当前文件的反链列表并更新面板显示与标题。
- `void refreshTags()` / `void onTagClicked(const QString &tag)`：刷新标签面板显示所有标签；响应标签点击时在面板显示关联文件列表并确保面板可见（`show` + `raise`）。
- `void refreshOutline()`：提取当前 Markdown 编辑器的所有标题（`extractHeadingsFromContent`，行级正则 + 跳过代码块），更新大纲面板显示。非 `.md` 文件时清空面板。
- `QString findWikiTarget(const QString &fileName)`：封装多级搜索策略，依次尝试已知文本扩展名进行路径匹配，并通过索引实现智能路径解析与就近匹配算法。
- `void onFileRenamedInIndex` / `void onFileDeletedInIndex`：响应动态文件操作，同步更新内存索引与标签索引。`onFileRenamedInIndex` 在索引迁移前通过 `backlinksFor(oldPath)` 捕获受影响的源文件，索引迁移后调用 `updateWikiLinksAfterRename` 将所有源文件中的 `[[旧名]]` 替换为 `[[新名]]`。`onFileDeletedInIndex` 同时调用 `HistoryPanel::removeFile` 清理历史记录中的失效条目。
- `void onFileMovedOrRenamed(const QString &oldPath, const QString &newPath)`：协调文件移动/重命名后的路径更新，依次调用 `onFileRenamedInIndex`、`TabManager::updatePathsAfterMove`、`HistoryPanel::replacePath`，确保编辑器、历史记录和索引一致。
- `void updateWikiLinksAfterRename(const QStringList &affectedSources, const QString &oldLinkText, const QString &newLinkText)`：文件重命名后更新所有引用文件中的 wiki 链接文本。从 BacklinkIndex 获取受影响源文件列表，使用 `replaceWikiLinkText` 精确匹配替换 `[[oldLinkText]]` → `[[newLinkText]]`。若源文件在打开的标签中，优先读取 `editor->toPlainText()` 以保留未保存更改，替换后写盘并重新加载编辑器。

**协作关系**：
- 持有 `FileExplorerWidget*`、`TabManager*`、`QSplitter*`、`SettingsManager*`。
- 连接文件浏览器的 `fileClicked` 信号到自己的 `onFileSelected` 槽。
- 连接文件浏览器的 `folderChanged` 信号到 `onFolderChanged`，以在用户通过对话框切换目录时记录路径。
- 工具栏的"保存"动作触发 `saveFile`，转为直接操作编辑器并处理记忆；"另存为"动作触发 `onSaveFileAs`。
- 标签页的创建、关闭或标题更新，这些职责全部委托给 `TabManager`。
- 工具栏中添加了"预览模式"按钮（可勾选），用于切换当前编辑器的预览状态。
- 持有缩放相关的 UI 元素：`QAction`（放大/缩小/重置）和 `QLabel`（百分比），并将它们布局在状态栏中。
- 监听 `TabManager::currentChanged` 信号，当标签页切换时调用 `updateZoomLabel()`、`connectCurrentEditorZoomSignal()` 和 `syncFileTreeSelection()`，保持缩放信息、信号连接以及文件树选中状态与当前编辑器同步。
- 在 `newFile()` 和 `onFileSelected()` 中确保新建立的编辑器连接了 `zoomFactorChanged` 信号，且新建/打开文件均从 `SettingsManager::editorDefaultZoom()` 读取默认缩放倍率（默认 100%），不再继承当前标签的缩放。
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
- 在关闭标签页时，检查编辑器是否已修改，弹出自定义保存提示对话框（显示当前文件名、自定义按钮文字"保存"、"不保存"、"取消"）。
- 提供 `closeAllTabs()` 方法，用于主窗口关闭时逐个尝试关闭所有标签页，若任一用户取消则返回 `false` 阻止退出。
- 使用自定义子类 `CustomTabBar` 替换默认标签栏，以实现拖拽边界限制。`CustomTabBar` 覆盖 `paintEvent` 和 `tabSizeHint`：对预览标签页使用斜体字体渲染并为斜体文字预留额外宽度（+8px），避免右侧裁剪。

**主要接口**：
- `EditorWidget* openFile(const QString &filePath)`：若文件已在某标签中打开则切换到该标签并自动提升预览标签页（若适用），否则新建标签并加载文件。
- `EditorWidget* openPreview(const QString &filePath)`：以预览模式打开文件。若文件已在永久标签页中则切换；若已在预览标签页中则仅切换；若预览标签页已有其他文件则替换内容（调用 `loadFile`）；若无预览标签页则新建。预览标签页标题以斜体显示。
- `void promotePreviewToPermanent()`：将当前预览标签页提升为永久标签页（清除预览标记，恢复正常字体）。
- `bool isPreviewEditor(EditorWidget* editor) const`：检查指定编辑器是否为当前预览编辑器。
- `EditorWidget* previewEditor() const`：返回当前预览编辑器指针，若无则返回 `nullptr`。
- `EditorWidget* newFile()`：创建一个未命名的空白编辑器，添加为新标签。
- `EditorWidget* currentEditor() const`：返回当前活动标签下的编辑器。
- `void saveCurrentFile()`：保存当前编辑器的内容（无路径时自动调用另存为）。
- `bool closeTab(int index)`：关闭指定索引的标签页，返回 `true` 表示已关闭（或用户选择不保存），`false` 表示用户取消了操作。关闭预览标签页时自动清理 `m_previewEditor` 指针。
- `bool closeAllTabs()`：依次关闭所有标签页，若任何一次 `closeTab` 返回 `false` 则立即停止并返回 `false`。
- `EditorWidget* findEditorByPath(const QString &filePath) const`：根据文件路径查找已打开的编辑器实例（大小写不敏感）。
- `bool closeTabByPath(const QString &filePath, bool askSave)`：关闭指定路径的标签页，`askSave` 为 `true` 时弹出保存提示，为 `false` 时强制丢弃修改。
- `QStringList allOpenedFilePaths() const`：返回所有已打开的文件路径列表（未保存的新建文件除外），路径统一为正斜杠格式。
- `void updateEditorFilePath(const QString &oldPath, const QString &newPath)`：当文件在外部被重命名时，更新对应编辑器的内部路径及标签标题。
- `void updatePathsAfterMove(const QString &oldBase, const QString &newBase)`：当文件或文件夹被移动时，批量更新所有已打开标签页的路径。支持精确匹配（文件本身）和前缀匹配（文件夹内文件），通过调用 `EditorWidget::setFilePath` 同步内部路径。

**信号**：
- `void tabCountChanged(int count)`：当标签数量变化时发出（供外部如窗口标题更新使用）。
- `void previewTabPromoted(EditorWidget *editor)`：预览标签页提升为永久时发出。

**自动提升机制**：在 `connectEditorSignals` 中，若编辑器是预览标签页，额外连接 `modificationChanged` 信号——当内容被修改（`modified == true`）且 `m_previewEditor` 仍指向该编辑器时，自动调用 `promotePreviewToPermanent()`。`openFile` 中若发现文件已在预览标签页中也会触发提升。

**协作关系**：
- 被 `MainWindow` 持有，主窗口将文件打开、新建、保存等操作直接转发给 `TabManager`。
- 内部创建和持有 `EditorWidget`，并连接其状态信号以更新标签标题和自动提升。
- `CustomTabBar` 通过 `qobject_cast<const TabManager*>(parent())` 获取 TabManager 引用，查询 `isPreviewEditor` 以决定斜体渲染。
- 与 `QMessageBox` 交互，提供自定义的文件保存提示对话框。

---

### 3. `EditorWidget` - 文本编辑器组件

**文件**：`editorwidget.h` / `editorwidget.cpp`

**职责**：
- 封装文本编辑功能，支持 **Markdown 源码编辑**、**全屏渲染预览**、**分屏预览**、**代码编辑**、**PDF 阅读** 与 **SMD 单元格编辑** 六种模式。
- Markdown 编辑模式使用 `WikiLinkTextEdit`（`QTextEdit` 子类，内嵌 `QCompleter` 支持 `[[` 自动补全）；全屏预览模式使用 `QWebEngineView` 加载内置 HTML 模板，通过 marked.js 将 Markdown 转为 HTML，并支持 KaTeX 数学公式渲染和 Mermaid 图表渲染；PDF 阅读模式使用 `QPdfView` + `QPdfDocument` 直接渲染 PDF 页面。
- **分屏预览模式**：新增的第 4 种布局模式，通过 `QSplitter(Qt::Horizontal)` 将编辑器区域分为左右两部分——左侧为 Markdown 源码编辑器（复用 `m_textEdit`），右侧为第二个独立的 `QWebEngineView` 渲染预览。分屏预览与全屏预览互斥，切换标签页时保留各自状态。右侧预览采用可配置的防抖机制延迟刷新（默认 500ms，可通过设置面板调节），仅在文本变化后更新。分屏比例从 `SettingsManager` 覆盖值读取（默认 50%，可通过设置面板调节），设置变更时实时调整分隔条位置。复用与全屏预览完全相同的 `preparePreviewContent()` 管线、`PreviewPage` 链接拦截和信号转发逻辑。
- 代码编辑模式使用 `CodeEditor`（`QPlainTextEdit` 子类），提供行号显示、语法高亮、自动缩进、括号补全、智能退格等功能。打开文件时根据扩展名自动判断编辑模式：已知代码扩展名（如 `.cpp`、`.h`）切换到代码模式，其余使用 Markdown 编辑模式。
- 预览引擎采用延迟初始化策略：`QWebEngineView` 在构造时仅创建外壳，首次切换预览模式时才通过 `setHtml()` 加载模板页面，避免构造时 GPU 进程冷启动造成窗口抖动。暗色容器 Widget（`m_previewContainer`，背景色 `#2d2d2d`）包裹 `QWebEngineView`，配合 `QWebEnginePage::setBackgroundColor()` 确保页面加载期间始终显示暗色背景。后续预览更新通过 `updatePreviewContent()` 将完整 Markdown 内容 Base64 编码后传入 JS 端 `renderFromBase64()`（使用 `TextDecoder` 正确处理 UTF-8 多字节字符），避免嵌入 JS 字符串时的转义问题。分屏预览的 `QWebEngineView` 同样采用延迟初始化（仅在首次进入分屏模式时创建），并共享同一套预处理和更新管线。
- 六种模式通过内部 `QStackedWidget` 切换：索引 0 = `WikiLinkTextEdit`（Markdown 编辑），索引 1 = `m_previewContainer`（暗色容器 > `QWebEngineView`），索引 2 = `CodeEditor`（代码编辑），索引 3 = `QSplitter`（左 `m_textEdit` + 右分屏预览 `QWebEngineView`），索引 4 = `QPdfView`（PDF 阅读，使用 `QPdfDocument` 加载），索引 5 = `SmdEditor`（SMD 单元格编辑器）。
- 管理当前编辑文件的路径和修改状态。
  内部维护一份保存/加载时的原始内容副本，当文本内容变化且停止输入 300ms 后自动与原始内容比对；若两者一致则自动清除修改标记，避免"输入再删除"导致的误标记。
- 支持从文件加载内容 (`loadFile`) 和将内容保存到文件 (`saveFile` / `saveAsFile`)。
- 发出 `fileLoaded`、`fileSaved` 和 `modificationChanged` 信号，便于标签管理器监听状态变化（例如更新标签标题中的星号）。
- 内置字体缩放功能：维护缩放因子，提供 `zoomIn`/`zoomOut`/`zoomReset` 方法。编辑器缩放通过 `QFont` 与 `QTextCursor::mergeCharFormat` 保证全文包括代码块字号同步；代码编辑模式下缩放后调用 `CodeEditor::refreshLineNumberArea()` 同步更新行号区域；预览缩放通过 `QWebEngineView::setZoomFactor()` 整体缩放页面（含 SVG 图表和数学公式）。
  缩放操作通过临时阻断文档信号并在完成后恢复修改状态，确保不会导致文件被错误标记为已修改。
- 预览内容预处理：`preparePreviewContent()` 统一编排预处理管线——先调用 `preHighlightCodeBlocks()` 对 fenced 代码块进行 C++ 端语法高亮（`highlightCodeBlock()` 使用直接正则匹配 + ConfigManager 颜色生成内联样式 HTML，经 Base64 编码后以 ````highlighted``` 自定义围栏块形式交给 marked.js 解码透传），再调用 `processWikiLinks()` 将 `[[Name]]` 转换为 `<a href="wikilink:编码目标">`（使用递归正则 `\[\[((?:[^\[\]]|\[(?1)\])*)\]\]`，链接目标通过 `QUrl::toPercentEncoding` 编码），接着通过 `TagIndex::processTagsForPreview()` 将 `#tag` 转换为 `<a href="tag:tag">#tag</a>`，最后转义 `</script>` 防止 HTML 注入。自定义 `PreviewPage`（继承 `QWebEnginePage`）重写 `acceptNavigationRequest()` 拦截 `wikilink:`、`tag:`、`runblock:` scheme 的导航请求并发出对应信号，外部链接交由系统浏览器打开。
- LaTeX 数学公式支持：通过 KaTeX 自动渲染 `$...$`（行内）和 `$$...$$`（块级）数学公式，支持 `\(...\)` 和 `\[...\]` 备用定界符。
- Mermaid 图表支持：通过 Mermaid.js 将 ` ```mermaid ` 代码块渲染为 SVG 图表，支持流程图、时序图、甘特图等。
- **PDF 导出**：`exportToPdf()` 创建临时隐藏 `QWebEngineView` + 普通 `QWebEnginePage`，通过 `preparePreviewContent()` 预处理 Markdown 内容后注入模板，将 CSS 变量替换为浅色打印主题（白底黑字），设置白色背景后加载页面。`loadFinished` 后通过 JS Promise 轮询等待 Mermaid 异步渲染完成，最后调用 `printToPdf()` 将当前渲染页面直接写入目标文件。完成后发射 `pdfExportCompleted` 信号。

**主要接口**：
- `bool loadFile(const QString &filePath)`：加载指定文件。`.pdf` 后缀自动切换到 `PdfView` 模式，使用 `QPdfDocument` 加载渲染；`.smd` 后缀自动切换到 `SmdEdit` 模式，通过 `SmdEditor::loadFile()` 解析单元格；已知代码扩展名切换到 `CodeEdit` 模式；其余文件使用 `MarkdownEdit` 模式。成功后更新内部路径并重置修改标记。
- `bool saveFile()`：保存到当前已打开的路径。如果路径为空则返回 `false`。
- `bool saveAsFile(const QString &defaultDir = QString())`：弹出文件对话框，另存为指定路径，并更新当前文件路径。 `defaultDir` 参数，可指定对话框的起始目录，为空则使用主文件夹。
- `QString currentFilePath() const`：返回当前正在编辑的文件路径（可能为空）。
- `QString toPlainText() const` / `void setPlainText(const QString &text)`：访问编辑器内容。
- `bool isModified() const` / `void setModified(bool)`：管理文档修改状态。
- `void setPreviewMode(bool preview)`：切换预览模式（`true` 显示渲染视图，`false` 显示源码视图）。代码编辑模式下为无操作。首次切换时加载模板页面（`setHtml`），`loadFinished` 后再切换视图栈；后续切换使用 `runJavaScript` 原地更新，通过回调在 JS 渲染完成后才切换以避免闪烁。
- `bool isPreviewMode() const`：返回当前是否为预览模式（代码编辑模式、SMD 编辑模式下始终返回 `false`）。
- `bool isCodeEdit() const` / `bool isPdfView() const` / `bool isSmdEdit() const`：查询当前编辑模式。
- `void setFileNames(const QStringList &names)`：设置 WikiLink 自动补全的文件名列表（代码编辑模式下为无操作）。
- `void setTagNames(const QStringList &names)`：设置 #tag 自动补全的标签列表。
- `void scrollToLine(int lineNumber, const QString &highlightText)`：跳转到指定行并高亮搜索关键词。预览模式下自动切回编辑模式。
- `void clearExtraSelections()`：清除搜索高亮。
- `void refreshPreview()`：强制刷新预览内容（委托 `updatePreviewContent(nullptr)` 异步更新）。
- `void refreshPreviewTheme()`：刷新预览页面主题颜色，更新 WebEngine 页面背景色并通过 `previewThemeJs()` 同步 CSS 变量到预览 DOM。设置面板关闭时由 `MainWindow::toggleSettings()` 调用，确保主题变更实时生效。
- `void updatePreviewContent(std::function<void()> onFinished)`：调用 `preparePreviewContent()` 获取预处理内容 → base64 编码 → `runJavaScript("window.renderFromBase64(...)")`，JS 执行完成后回调 `onFinished`。
- `QString preparePreviewContent(const QString &rawMarkdown)`：统一预处理管线——`preHighlightCodeBlocks()` → `processWikiLinks()` → `TagIndex::processTagsForPreview()` → `</script>` 转义。被 `setPreviewMode()` 和 `updatePreviewContent()` 共同使用。
- `QString preHighlightCodeBlocks(const QString &markdown)`：使用正则匹配所有 fenced 代码块，对可识别语言（C/C++、Python）调用 `highlightCodeBlock()` 生成内联样式 HTML，经 Base64 编码后替换为 `highlighted` 自定义围栏块。
- `QString highlightCodeBlock(const QString &code, const QString &langId)`：使用直接正则匹配（复用 CppSyntaxHighlighter / PythonSyntaxHighlighter 的规则和 ConfigManager 颜色），对代码逐行逐片着生成 `<span style="color:...">` 内联样式 HTML。
- `QString processWikiLinks(const QString &markdown)`：将 `[[链接]]` 转换为 `<a href="wikilink:...">` HTML 超链接格式（供首次加载和增量更新共用）。
- `void zoomIn()` / `void zoomOut()` / `void zoomReset()`：按 0.1 步长调整缩放因子（范围 0.5～3.0），并立即应用字体变化。
- `qreal zoomFactor() const`：返回当前缩放倍数。
- `void setZoomFactor(qreal factor)`：设置绝对缩放倍数，并触发 `applyZoom()` 与 `zoomFactorChanged` 信号。
- `void setFilePath(const QString &newPath)`：更新当前编辑器的文件路径（不改变文档内容），用于外部重命名后同步。
- `void setSplitPreviewDebounceMs(int ms)`：动态更新分屏预览的防抖延迟间隔，用于设置面板实时调整。
- `void applySplitPreviewRatio()`：从 `SettingsManager` 读取最新的分屏比例值并立即调整 `QSplitter` 分隔条位置。
- `void exportToPdf(const QString &filePath, const QPageLayout &layout)`：将当前 Markdown 内容通过临时隐藏 QWebEngineView 渲染后导出为 PDF。

**信号**：
- `void fileLoaded(const QString &filePath)`
- `void fileSaved(const QString &filePath)`
- `void modificationChanged(bool modified)`
- `void zoomFactorChanged(qreal factor)`：当缩放因子改变时发出，供主窗口更新百分比标签。
- `void filePathChanged(const QString &oldPath, const QString &newPath)`：当文件路径被 `setFilePath` 修改时发出，供标签管理器更新路径关联。
- `void wikiLinkClicked(const QString &fileName)`：当预览模式下的 WikiLink 被点击时发出。
- `void runCodeBlockRequested(const QString &language, const QString &code, int blockIndex)`：预览模式下点击代码块 ▶ Run 按钮时发出，由 PreviewPage::acceptNavigationRequest 拦截 `runblock:` scheme 后通过 `runJavaScript` 读取 JS 侧存储的代码和 blockIndex 并转发。
- `void tagClicked(const QString &tag)`：预览模式下点击 `#tag` 链接时发出，由 PreviewPage 拦截 `tag:` scheme 后转发。
- `void pdfExportCompleted(const QString &filePath, bool success)`：PDF 导出完成时发出，供主窗口显示状态信息。

**协作关系**：
- 被 `TabManager` 创建和管理，`TabManager` 连接其信号以更新标签标题。
- 主窗口通过 `setPreviewMode` 控制预览状态，并将预览按钮的勾选状态与当前编辑器同步。
- 在构造函数中对 `m_textEdit` 的 **viewport**、`m_codeEditor` 的 **viewport** 和 `m_previewView` 安装事件过滤器，拦截 `QWheelEvent`（Ctrl修饰）和 `QNativeGestureEvent`（缩放手势），统一转向 `zoomIn()`/`zoomOut()`。其中 `m_previewView` 额外通过 `QTimer::singleShot` 延迟安装到其 `focusProxy()`（Chromium 内部输入控件），并在首次页面 `loadFinished` 后重新安装，确保预览区的 Ctrl+滚轮缩放也能被正确拦截。
- 构造函数不再调用 `initPreviewEngine()`，WebEngine 页面加载延迟至首次 `setPreviewMode(true)` 时执行，避免打开文件时 GPU 进程冷启动造成窗口抖动。
- `applyZoom` 在模式切换或缩放变化时同步编辑区的字体，并通过 `QWebEngineView::setZoomFactor()` 缩放整个预览页面（含 SVG 图表和数学公式）。

---

### 4. `FileExplorerWidget` - 文件浏览器组件

**文件**：`fileexplorerwidget.h` / `fileexplorerwidget.cpp`

**职责**：
- 提供右键菜单交互，支持新建文件（默认 `.md`）、新建文件夹、重命名、删除。所有文件均可进行右键操作。
- 启用 `QTreeView` 的内联编辑功能（通过 `EditKeyPressed` 和 `SelectedClicked` 
- 自定义委托 (NoGhostDelegate)：为了修复原生内联编辑可能出现的"重影"问题（编辑框未完全覆盖原文本导致新旧文字重叠），以及避免编辑时文件图标消失，特实现了自定义委托。
  - updateEditorGeometry：通过 QStyle::SE_ItemViewItemText 获取精确的文本绘制区域，将编辑框（QLineEdit）仅放置于文本区域，保留图标区域不被遮挡。
  - paint：在编辑状态下，清空 option.text 后调用基类绘制，保留图标、背景等视觉元素，但不绘制原文本，彻底消除重影。
  - createEditor：设置编辑框背景不透明（跟随系统调色板或白色背景），去除边框和内边距，确保编辑框视效整洁。
  - setEditorData：重写以控制初始选中范围。重命名文件夹时全选整个名称；重命名文件时只选中主文件名（不含扩展名）。
  - setModelData：重写以校验用户输入。若用户清空文件夹名，或清空文件名使其只剩扩展名（如 `.md`），自动恢复为原始名称，防止产生仅含扩展名的无效文件。
- 监听 `QFileSystemModel::fileRenamed` 信号，并转发 `fileRenamed` 信号，供主窗口更新标签页路径。
- 提供 `deleteItem` 等公共方法，封装实际的文件系统操作。内联新建/重命名由内部方法处理。
- 使用自定义排序代理 `FileSortProxyModel` 确保文件夹优先于文件显示，且在新建、重命名后自动重排。代理模型内置 `QFileIconProvider` 成员缓存和惰性兜底图标，避免 Windows 上 `setRootPath` 切换后 HICON 失效导致的空心图标问题，同时消除每次 fallback 时重复调用 `SHGetFileInfo` 的性能开销。
- `setUniformRowHeights(true)` 开启统一行高模式，样式表已强制 24px 行高，开启后 Qt 跳过逐行高度查询，大幅提升大目录滚动流畅度。
- 重写 `eventFilter`，监听 `Delete` 键，在非编辑状态下直接对选中项发起删除请求。
- 右键菜单中使用 `DeleteKeyFilter` 事件过滤器，支持在菜单弹出时按 `Delete` 键触发删除。
- 发出 `operationFailed` 信号，用于向用户展示文件操作错误。
- 发出 `fileClicked` 信号（单击）和 `fileDoubleClicked` 信号（双击），分别携带被选中文件的绝对路径。单击以预览模式打开文件，双击永久打开。
- 提供 `selectFolder` 公共槽，弹出目录选择对话框并更新根目录。支持传入初始目录参数，以便对话框从上次记忆的路径开始浏览。
- 支持拖拽移动文件或文件夹（在该树视图内），通过事件过滤器拦截 DragEnter、DragMove、Drop 事件，在符合条件时执行文件系统移动并发送 `fileRenamed` 信号。
- 拖拽时对目标文件夹提供视觉反馈：通过自定义委托 `NoGhostDelegate` 在悬停文件夹底部绘制蓝色横条。
- 在构造函数中创建顶部面包屑路径栏（`m_breadcrumb`），使用 `FlowLayout` 布局实现路径过长时的自动换行。`setRootPath()` 调用时自动更新面包屑。`updateBreadcrumb()` 方法从叶到根收集路径段，反转后依次创建 `QPushButton`，当前目录白色不可点击，祖先目录灰色可点击跳转，段之间用灰色 `>` 标签分隔。
- 面包屑下方创建文件树工具栏（`m_toolbar`），通过 `QHBoxLayout` 左对齐显示文件夹名称（`m_folderLabel`，`QSizePolicy::Preferred`），右对齐放置操作按钮（收起所有文件夹、刷新）。`updateFolderLabel()` 在窗口大小变化时通过 `QFontMetrics::elidedText` 自动省略标题文字，保证按钮不被挤压。`resizeEvent()` 中调用 `updateFolderLabel()` 实时更新。`collapseAll()` 方法调用 `m_treeView->collapseAll()` 一键收起所有已展开的文件夹。`refreshTree()` 方法重新设置 `QFileSystemModel` 根路径并刷新排序代理，实现手动刷新文件树。

**主要接口**：
- `void setRootPath(const QString &path)`：设置文件树显示的根目录。
- `QString rootPath() const`：返回当前根目录。
- `void selectFolder(const QString &defaultDir = QString())`：弹出文件夹选择对话框，若 `defaultDir` 不为空则将其作为对话框的起始目录。用户选择后更新根目录并发出 `folderChanged` 信号。
- `void selectFile(const QString &filePath)`：在文件树中选中指定路径的文件，并自动展开所有折叠的父级目录，确保文件可见。空路径或无效路径时静默返回（例如新建未保存文件、文件不在当前根目录内）。
- `void deleteItem(const QString &path, bool isDir)`：删除文件或文件夹（递归删除）。
- `bool isDropTargetFolder(const QModelIndex &proxyIndex) const`：供自定义委托查询当前索引是否为拖拽目标文件夹。

**信号**：
- `void fileClicked(const QString &filePath)`：当用户单击文件树中的有效文件（非目录）时发出，以预览模式打开。
- `void fileDoubleClicked(const QString &filePath)`：当用户双击文件树中的有效文件时发出，永久打开（或提升预览标签页）。
- `void folderChanged(const QString &newPath)`：当用户通过 `selectFolder` 对话框选择了新目录后发出，用于主窗口记忆路径。
- `void fileRenamed(const QString &oldPath, const QString &newPath)`：重命名成功时发出，用于更新标签管理器中的路径。路径使用 `QDir::absoluteFilePath` 规范化（统一 `/` 分隔符），确保与 `BacklinkIndex` 存储的路径格式一致。
- `void operationFailed(const QString &errorMsg)`：文件操作失败时发出，由主窗口显示错误消息。
- `void itemDeleted(const QString &path)`：在成功删除文件/文件夹后发出。

**协作关系**：
- 被 `MainWindow` 使用，`fileClicked` 信号连接到 `onFileSelected`（调用 `openPreview`），`fileDoubleClicked` 信号连接到 `onFileDoubleClicked`（调用 `openFile` 或 `promotePreviewToPermanent`）。
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
- 提供设置覆盖（`settings_overrides`）机制：通过 `setSettingOverride` / `settingOverride` 读写用户通过设置面板修改的配置项，覆盖 `ConfigManager` 的静态默认值。v0.5.4 起覆盖值改为内存缓冲（`m_overrideMap`），避免设置变更时频繁磁盘 I/O，程序关闭时通过 `flushOverrides()` 统一写入。
- `editorDefaultZoom` / `setEditorDefaultZoom` 同样改为内存缓存（`m_cachedDefaultZoom`），随 `flushOverrides()` 一并持久化。

**主要接口**：
- `void setWindowGeometry(const QByteArray &geometry)` / `QByteArray windowGeometry() const`
- `void setSplitterState(const QByteArray &state)` / `QByteArray splitterState() const`
- `void setLastFolderPath(const QString &path)` / `QString lastFolderPath(const QString &defaultPath = QString()) const`
- `void setLastSaveAsFolderPath(const QString &path)` / `QString lastSaveAsFolderPath(const QString &defaultPath = QString()) const`
- `void flushOverrides()`：将所有待写的设置覆盖值和默认缩放值刷新到磁盘，在 `MainWindow::closeEvent` 中调用。
- `void clear()`：清除所有设置。
- `void setRecentFiles(const QStringList &files)` / `QStringList recentFiles() const`：读写最近打开的文件列表（最多50条），键名 `History/recentFiles`。
- **设置覆盖**：
  - `void setSettingOverride(const QString &key, const QVariant &value)`：写入内存 map，延迟持久化。
  - `QVariant settingOverride(const QString &key, const QVariant &defaultValue = QVariant()) const`：从内存 map 读取。
  - `void removeSettingOverride(const QString &key)` / `QStringList allOverrideKeys() const`：管理覆盖项。
- **编辑器默认缩放**：
  - `qreal editorDefaultZoom() const` / `void setEditorDefaultZoom(qreal zoom)`：读写编辑器默认缩放因子，键名 `editor/defaultZoom`，默认值 `1.0`（100%），内存缓存。
- **OpenJudge 自动登录**：
  - `void setOpenJudgeAutoLogin(bool enabled)` / `bool openJudgeAutoLogin() const`：读写自动登录开关，键名 `OpenJudge/autoLogin`。
  - `void setOpenJudgeCredentials(const QString &username, const QString &password)` / `QPair<QString, QString> openJudgeCredentials() const`：读写 OpenJudge 账号密码，密码经 Base64 混淆后存储。
  - `void clearOpenJudgeCredentials()`：清除已保存的凭据。

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
- 界面底部提供一个"清空历史记录"按钮，点击后弹出确认对话框，确认后立即删除所有历史记录并写入空列表到配置。

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
- 支持后台线程安全的异步全量扫描：通过 `BacklinkData` 结构体 + `static buildFromPath()` 在后台线程执行扫描并返回独立数据，`setData()` 在主线程通过 `std::move` 原子交换索引。

**主要接口**：
- `void buildIndex(const QString &rootPath, const QMap<QString, QStringList> &fileIndex)`：全量扫描重建索引（同步版本）。
- `static BacklinkData buildFromPath(const QString &rootPath, const QMap<QString, QStringList> &fileIndex)`：静态方法，在调用线程中执行全量扫描，返回包含 `backlinks` 和 `forwardLinks` 的 `BacklinkData` 结构体。用于 `MainWindow::startAsyncIndexBuild()` 的后台线程扫描。
- `void setData(BacklinkData data)`：通过 `std::move` 原子替换内部索引数据，仅在主线程调用，无需加锁。
- `void rebuildFile(const QString &filePath, const QString &rootPath, const QMap<QString, QStringList> &fileIndex)`：增量更新单个文件（保存时调用）。
- `void onFileRenamed(const QString &oldPath, const QString &newPath)`：迁移索引中的路径信息。同步更新 `m_backlinks` 的 target key 和值列表中的 source 路径引用，以及 `m_forwardLinks` 值列表中的 target 路径引用，确保后续 `rebuildFile → removeFile` 能找到正确的 target 进行清理。
- `void onFileDeleted(const QString &path)`：从索引中移除已删除文件，O(1)。
- `QStringList backlinksFor(const QString &filePath) const`：查询指定文件的来源列表，O(1)。
- `static QString resolveTarget(const QString &linkName, const QString &rootPath, const QMap<QString, QStringList> &fileIndex)`：静态方法，将 WikiLink 文本解析为目标文件绝对路径（不依赖实例状态，可在后台线程安全调用）。

**内部数据结构**：
- `QMap<QString, QStringList> m_backlinks`：目标绝对路径 → 来源绝对路径列表。
- `QMap<QString, QStringList> m_forwardLinks`：来源绝对路径 → 目标绝对路径列表（用于增量更新时的反向清理）。
- `BacklinkData` 结构体：包含 `backlinks` 和 `forwardLinks` 两个 `QMap`，作为异步扫描的返回值，将后台线程的扫描结果与实例状态解耦。

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

### 8.5. `TagIndex` — 标签索引

**文件**：`tagindex.h` / `tagindex.cpp`

**职责**：
- 维护标签索引：记录 `#tag` → 文件的双向映射关系，支持标签浏览和文件关联查询。
- 提取规则：正则 `(*UCP)#(?!\s)([\w-]+)` — Unicode 感知（支持中文/日文/韩文等），`(?!\s)` 排除 `# Heading` 的误匹配，行级跳过 ``` 围栏代码块（保护 `#include` 等误转换）。
- 全量扫描仅针对 `*.md` / `*.markdown` 文件（不扫描代码文件），使用 `QDirIterator` 递归遍历。
- 预览转换：`processTagsForPreview()` 将 Markdown 内容中的 `#tag` 替换为 `<a href="tag:tag">#tag</a>`，代码块内不转换。
- 支持后台线程安全的异步全量扫描：通过 `TagData` 结构体 + `static buildFromPath()` 返回独立数据，`setData()` 在主线程通过 `std::move` 原子交换索引，作为异步构建的 Phase 3 执行。
- 空状态处理：无标签时返回空列表，由面板负责展示占位文本。

**主要接口**：
- `static TagData buildFromPath(const QString &rootPath)`：全量扫描，返回 `tagToFiles` 和 `fileToTags` 映射。
- `void setData(TagData data)`：原子替换内部索引。
- `QStringList filesForTag(const QString &tag) const`：查询标签关联的文件列表。
- `QStringList allTags() const`：返回所有标签列表。
- `void rebuildFile(const QString &filePath)`：增量更新单个文件（保存时调用）。
- `void onFileRenamed(const QString &oldPath, const QString &newPath)`：迁移索引中的路径信息。
- `void onFileDeleted(const QString &path)`：从索引中移除已删除文件。
- `static QStringList extractTagsFromContent(const QString &content)`：从文本中提取标签。
- `static QString processTagsForPreview(const QString &markdown)`：Markdown 中的 #tag 转可点击 HTML 链接。

**内部数据结构**：
- `QMap<QString, QStringList> m_tagToFiles`：标签 → 文件路径列表。
- `QMap<QString, QStringList> m_fileToTags`：文件路径 → 标签列表（用于增量删除时的反向清理）。

**协作关系**：
- 由 `MainWindow` 创建并持有。
- `addFileTags()` 包含 `content.contains('#')` 快路径优化。
- 标签补全列表由 `updateCurrentEditorCompletions()` 推送到编辑器。

---

### 8.6. `TagPanel` — 标签面板

**文件**：`tagpanel.h` / `tagpanel.cpp`

**职责**：
- 以 `QListWidget` 双级导航展示标签系统：标签总览 → 点击标签 → 关联文件列表。
- `showAllTags(tags)`：列表显示所有标签（`#` 前缀），点击触发 `tagClicked` 信号。
- `showFilesForTag(tag, files)`：显示包含该标签的所有文件（文件名 + 完整路径 ToolTip），点击触发 `fileClicked` 信号。
- 返回按钮切回标签总览（`onBackClicked` → `showAllTags(m_allTags)`）。
- 空状态：无标签时灰色 "未找到标签"，无文件时灰色 "无文件包含此标签"。
- 面板宽度 `setMinimumWidth(200)` 确保稳定不塌缩。

**信号**：
- `void fileClicked(const QString &filePath)`：用户点击文件列表项时发出。
- `void tagClicked(const QString &tag)`：用户点击标签列表项时发出。

**协作关系**：
- 由 `MainWindow` 创建并持有，作为 `QDockWidget` 的内容部件，右侧停靠，默认隐藏。
- 文件点击复用 `MainWindow::onHistoryFileClicked` 槽。
- 标签点击触发 `MainWindow::onTagClicked`，在面板显示关联文件并确保面板可见。
- 通过 `MainWindow::refreshTags()` 在标签页切换、文件保存时刷新。
- 支持点击外部自动隐藏（与 History/Backlinks 面板共享同一 `eventFilter` 模式）。
- 工具栏快捷键 `Ctrl+Shift+T` 切换显示/隐藏。

---

### 9. `SearchPanel` — 全文搜索面板

**文件**：`searchpanel.h` / `searchpanel.cpp`

**职责**：
- 提供全文搜索功能，在当前根目录下的所有文本文件中检索关键词。
- 搜索输入支持 300ms 防抖，避免每次按键都触发磁盘扫描。
- 使用 `QDirIterator` + `TextFileUtils::scanNameFilters()` 递归收集文本文件列表。
- 使用 `QTextStream::readLine()` 逐行流式读取文件内容，`QString::toLower().contains()` 进行大小写不敏感匹配。
- 结果上限：每文件匹配数和总结果数从 `SettingsManager` 覆盖值读取（默认每文件 20、总计 500），片段最大长度同样可配置（默认 120）。设置面板中的修改实时生效。
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

**扩展名列表**：`md`、`markdown`、`txt`、`c`、`cpp`、`cxx`、`cc`、`h`、`hpp`、`hxx`、`hh`、`cs`、`java`、`py`、`pyw`、`pyx`、`js`、`jsx`、`ts`、`tsx`、`mjs`、`rs`、`go`、`rb`、`php`、`swift`、`kt`、`kts`、`html`、`htm`、`css`、`scss`、`sass`、`less`、`xml`、`svg`、`json`、`yaml`、`yml`、`toml`、`ini`、`cfg`、`conf`、`rst`、`tex`、`log`、`csv`、`tsv`、`sql`、`graphql`、`proto`、`in`、`out`、`sh`、`bash`、`zsh`、`fish`、`ps1`、`bat`、`cmd`、`cmake`、`mak`、`mk`、`pro`、`pri`、`qml`、`qrc`、`ui`、`diff`、`patch`。

**协作关系**：
- 被 `MainWindow`、`BacklinkIndex`、`EditorWidget` 引用，用于文件索引构建、Wiki 链接解析和另存为过滤器。

---

### 12. `CodeEditor` - 代码编辑器控件

**文件**：`codeeditor.h` / `codeeditor.cpp`

**职责**：
- 基于 `QPlainTextEdit` 的代码编辑器，提供 IDE 风格编辑体验。
- 行号区域（`LineNumberArea`）：自定义 `QWidget`，绘制在编辑器左侧视口边距内，显示深色背景（`#252525`）+ 灰色数字（`#858585`）。
- **补全弹出（CompletionPopup）**：`Qt::Tool | Qt::FramelessWindowHint` 无焦点浮动窗口，位于文本光标下方，列表项+提示栏。输入 `.`、`->`、`::` 或 `Ctrl+I` 触发，Tab/Enter 插入，Esc/点击外部关闭。
- **悬停提示（HoverManager）**：400ms 延迟定时器监听鼠标移动，停止后在鼠标位置通过 `QToolTip::showText()` 显示类型/文档信息。诊断提示（错误/警告）和 LSP 悬停提示均应用紧凑样式（`padding: 0px 4px; margin: 0px;`），与全局 `QToolTip` 样式一致。全局 tooltip 通过 `CompactTooltipStyle`（`main.cpp` 中的 QProxyStyle 子类，`PM_ToolTipLabelFrameWidth = 0`）进一步消除 Qt Fusion 样式的额外内框边距。鼠标移动/离开/点击/滚轮时关闭。
- **签名帮助（SignatureHelpManager + SignatureHelpPopup）**：光标进入 `(` 后 200ms 防抖触发，`SignatureHelpPopup` 为 `Qt::Tool` 浮动窗口，**始终定位在文本光标上方**（避免被 cell 边界遮挡），显示函数签名（活动参数 `#569CD6` 蓝色加粗高亮）、文档和重载导航 `◀ 1/3 ▶`。关闭条件：输入 `)`、光标移出括号区域、Esc、编辑器失焦、鼠标点击外部、cell 执行时主动隐藏。`SignatureHelpManager::hide()` 暴露为 `CodeEditor::hideSignatureHelp()` 供 SmdEditor 在执行 cell 前调用。
- 自动缩进（`handleAutoIndent`）：按 Enter 时提取当前行前导空白作为缩进。光标在 `{` 和 `}` 之间时，自动分割为三行（`{`、缩进空白行、`}`），光标定位在中间行。光标前的文本以 `{`（C 风格）或 `:`（Python）结尾时才增加一级缩进。
- 括号补全（`handleBracketCompletion`）：输入 `{`、`(`、`[`、`"`、`'` 时自动插入匹配对；有选中文本时包裹选中内容。在字符串或注释区域内不触发。
- 闭合括号跳过（`handleClosingBracketSkip`）：输入右括号时若光标后紧跟相同字符，则跳过而非重复插入。
- 退格成对删除（`handleBackspacePairRemoval`）：光标位于空括号对中间时，退格同时删除左右括号。
- 智能退格（`handleBackspaceIndent`）：光标在行首空白区域时，退格删除至前一缩进边界（4 空格为单位）。Tab 字符单独删除一个。
- Tab 缩进（`handleTabKey`）：插入 4 空格缩进；有选区时批量缩进选中行。
- 缩进调整（`handleIndentLeft` / `handleIndentRight` / `Ctrl+[` 向左缩进 / `Ctrl+]` 向右缩进）。无选区时调整当前行缩进；有选区时调整所有选中行的缩进，自动跳过空行。
- 当前行高亮（`highlightCurrentLine`）：以 `#2A2D2E` 背景色高亮当前行，与搜索高亮合并显示。
- 搜索高亮（`setSearchHighlights` / `clearSearchHighlights`）：存储搜索结果高亮列表（金色 `#FFD700`），与当前行高亮合并后通过 `setExtraSelections` 统一应用。
- 语法高亮集成（`setLanguage`）：通过 `LanguageUtils::createHighlighter()` 安装/替换 `QSyntaxHighlighter`。
- 编辑器主题：深色背景（`#1E1E1E`），浅灰前景（`#D4D4D4`），Consolas 12pt 等宽字体，禁用自动换行。

**主要接口**：
- `void setLanguage(const QString &langId)`：安装对应语言的语法高亮器，创建独立的 LSP CompletionProvider（仅独立代码文件使用）。
- `void setLanguageSyntaxOnly(const QString &langId)`：仅安装语法高亮器和文本同步信号，**不创建私有 LSP provider**。供 SMD cell 中的 CodeEditor 使用（LSP 由 SmdLspManager 共享管理）。
- `void setDocumentUri(const QString &uri)`：设置当前文档 URI（`file:///` + 文件路径），供 LSP 后端识别文件身份。独立代码文件在 `EditorWidget::loadFile()` 中调用。
- `QString languageId() const`：返回当前语言 ID。
- `CompletionProvider *completionProvider() const`：返回当前补全提供者。
- `void setCompletionProvider(CompletionProvider *provider)`：设置外部共享的 CompletionProvider（非拥有模式）。会先断开并 shutdown 旧私有 provider。SMD cell 通过此方法接入 SmdLspManager 的共享后端。
- `void hideSignatureHelp()`：隐藏签名帮助弹出窗口。由 `focusOutEvent` 失焦时调用，也由 `SmdEditor` 在执行 cell 前调用以确保弹出窗口不残留。
- `void setDiagnostics(const QList<SmdDiagnostic> &diagnostics)` / `void clearDiagnostics()`：设置或清除诊断信息，触发波浪线重绘。构造函数连接 `provider->diagnosticsUpdated` → `setDiagnostics()`。
- `QList<SmdDiagnostic> diagnostics() const`：返回缓存的诊断列表。供 `MainWindow` 在切换标签页时立即恢复诊断，无需等待 provider 重发。
- `void setSearchHighlights(const QString &searchText)` / `void clearSearchHighlights()`：设置或清除搜索高亮。
- `void refreshLineNumberArea()`：刷新行号区域，重算宽度与几何形状并触发重绘。用于字体缩放后同步更新行号区域。
- `int lineNumberAreaWidth() const`：计算行号区域所需宽度。
- `void lineNumberAreaPaintEvent(QPaintEvent *event)`：行号区域绘制逻辑（由 `LineNumberArea` 委托）。绘制时显式设置 painter 字体为编辑器当前字体，确保行号随缩放同步变化。

**诊断面板快捷键**（`toggle_diagnostics`，默认 `Ctrl+D`）：
- `reloadShortcuts()` 从 `SettingsManager` / `ConfigManager` 加载 `m_toggleDiagnostics` QKeySequence。
- `keyPressEvent()`：`matchShortcut(m_toggleDiagnostics)` → emit `diagnosticsToggleRequested()`。
- `eventFilter()`（覆盖 `QPlainTextEdit::eventFilter`）：在 `ShortcutOverride` 阶段 `event->accept()` 防止 Qt 快捷键系统拦截；在 `KeyPress` 阶段 emit 信号。同时监听 `this` 和 `viewport()`，确保 QAbstractScrollArea 转发机制下均能匹配。
- 信号链：`CodeEditor::diagnosticsToggleRequested` → `EditorWidget` 转发 → `MainWindow::toggleDiagnosticsInCodeEditor()` → 显示/隐藏 `BottomPanel` 的诊断标签页。

**信号**：
- `diagnosticsToggleRequested()`：用户按下诊断面板快捷键时发出。

**LSP 补全集成**：
- 独立代码文件：`setLanguage()` → `createCompletionProvider()` 创建私有 `CppCompletionProvider`（clangd）/ `PythonCompletionProvider`（Jedi），由 CodeEditor 拥有和管理（`m_ownsProvider = true`）。
  - `CppCompletionProvider::~CppCompletionProvider()` **不调用 `shutdown()`** — `stop()` 中的 `waitForFinished()` 阻塞主线程，导致关闭文件时卡死。LspClient 子对象通过 Qt 父子链自动清理。
  - `CppCompletionProvider::onResponseReceived()` 顶部仅检查 `!m_client`，`!m_initialized` 检查移至初始化响应处理 **之后**。原位置在 `m_initialized` 被设置前就拦截了 initialize 响应，导致 LSP 永不初始化。
  - `CppCompletionProvider::shutdown()` 保留用于 `setCompletionProvider()` 替换旧 provider 的场景（SMD cell 类型切换），通过 `m_ownsProvider` 标志判断。
- SMD cell：`setLanguageSyntaxOnly()` 只做语法高亮和信号连接，随后由 `SmdEditor::connectCellSignals()` 通过 `setCompletionProvider()` 注入 SmdLspManager 的共享 `CellCompletionAdapter`（`m_ownsProvider = false`）。
- 补全触发：输入 `.`、`->`、`::` 或 `Ctrl+I` → `triggerCompletion()` → provider → LSP 请求。

**诊断波浪线渲染**：
- `updateExtraSelectionsWithDiagnostics()`：将 `m_diagnostics` 列表转换为 `QTextEdit::ExtraSelection`（错误红色 `#F44747` / 警告黄色 `#CCA700` 波浪下划线，tooltip 显示诊断信息），与当前行高亮、搜索高亮合并后通过 `setExtraSelections()` 统一应用。
- 由 `highlightCurrentLine()` 和 `setDiagnostics()` 触发刷新。

**内部类 `LineNumberArea`**：
- 继承 `QWidget`，作为 `CodeEditor` 的子控件。
- `sizeHint()` 返回由 `CodeEditor::lineNumberAreaWidth()` 计算的宽度。
- `paintEvent()` 委托回 `CodeEditor::lineNumberAreaPaintEvent()` 完成绘制。

**协作关系**：
- 由 `EditorWidget` 创建并管理，作为 `QStackedWidget` 的第 3 页（索引 2）。
- 通过 `LanguageUtils::createHighlighter()` 获取语法高亮器。
- `EditorWidget` 在加载文件时根据扩展名判断是否切换到代码模式，并调用 `setLanguage()`。

---

### 13. `LanguageUtils` - 语言注册工具

**文件**：`languageutils.h` / `languageutils.cpp`

**职责**：
- 提供可扩展的编程语言注册表，通过 `LanguageInfo` 结构定义语言的显示名称、扩展名集合和高亮器工厂函数。
- 统一入口：其他模块无需了解具体高亮器类，仅通过语言 ID 即可创建高亮器。
- 扩展点：添加新语言支持仅需创建新的 `QSyntaxHighlighter` 子类并在 `languageMap()` 中添加一条记录即可。

**数据结构 `LanguageInfo`**：
- `QString displayName`：语言显示名称。
- `QSet<QString> extensions`：该语言的文件扩展名集合（不含点号，小写）。
- `std::function<QSyntaxHighlighter*(QTextDocument*)> factory`：高亮器工厂函数。

**主要接口**：
- `LanguageUtils::languageForExtension(const QString &ext)`：根据文件扩展名（不含点号）查找语言 ID，未找到返回空字符串。
- `LanguageUtils::isCodeFile(const QString &ext)`：判断扩展名是否为已知代码文件。
- `LanguageUtils::codeExtensions()`：返回所有注册语言的全部扩展名列表。
- `LanguageUtils::createHighlighter(const QString &langId, QTextDocument *doc)`：根据语言 ID 创建对应的高亮器实例。

**当前注册语言**：
| 语言 ID | 显示名称 | 扩展名 |
|---------|----------|--------|
| `"cpp"` | C/C++ | cpp, hpp, cxx, cc, c, h, hxx, hh |
| `"python"` | Python | py, pyw, pyx |

**协作关系**：
- 被 `EditorWidget` 在 `loadFile()` 和 `setFilePath()` 中调用，用于判断文件是否应进入代码编辑模式。
- 被 `CodeEditor::setLanguage()` 调用以创建语法高亮器。

---

### 14. `CppSyntaxHighlighter` - C/C++ 语法高亮器

**文件**：`cppsyntaxhighlighter.h` / `cppsyntaxhighlighter.cpp`

**职责**：
- 继承 `QSyntaxHighlighter`，对 C/C++ 源代码进行深色主题语法高亮。
- 高亮规则具体颜色方案：
  - **关键字**（`#569CD6`，粗体）：`if`、`else`、`for`、`while`、`class`、`struct`、`return`、`const`、`static`、`virtual`、`override`、`namespace`、`using`、`template`、`public`、`private`、`protected`、`new`、`delete`、`auto`、`nullptr` 等，含 C++20 新增关键字（`co_await`、`co_return`、`consteval`、`constinit` 等）。
  - **预处理器**（`#C586C0`）：`#include`、`#define`、`#ifdef`、`#ifndef`、`#endif`、`#pragma` 等。
  - **类型**（`#4EC9B0`）：`int`、`void`、`bool`、`char`、`double`、`float`、`size_t`、`QString` 等常见类型。
  - **字符串**（`#CE9178`）：双引号字符串、单引号字符字面量、原始字符串字面量，均支持转义字符。
  - **数字**（`#B5CEA8`）：十进制、十六进制数字字面量。
  - **单行注释**（`#6A9955`）：`// ...`，通过 `highlightBlock` 中的字符扫描实现——逐字符跟踪 `"..."` 字符串和 `'...'` 字符字面量状态，仅在字符串外检测到 `//` 时应用注释格式，避免误将字符串内的 `//` 着色为注释。
  - **多行注释**（`#6A9955`）：`/* ... */`，通过 `setCurrentBlockState()` / `previousBlockState()` 实现跨行状态跟踪。

**协作关系**：
- 由 `LanguageUtils::createHighlighter()` 工厂函数创建，作为 `"cpp"` 语言的高亮器实现。
- 被 `CodeEditor::setLanguage()` 调用并安装到 `QTextDocument` 上。

---

### 14.5. `PythonSyntaxHighlighter` — Python 语法高亮器

**文件**：`pythonsyntaxhighlighter.h` / `pythonsyntaxhighlighter.cpp`

**职责**：
- 深色主题 Python 语法高亮，继承 `QSyntaxHighlighter`。
- 关键字（`def`/`class`/`if`/`for`/`while`/`import` 等）：蓝色 `#569CD6` 加粗。
- 内建类型与函数（`int`/`str`/`list`/`print`/`len`/`Exception` 等）：青色 `#4EC9B0`。
- 常量（`True`/`False`/`None`）：蓝色加粗。
- 装饰器（`@` 开头）：紫色 `#C586C0`。
- `self`/`cls`：黄色 `#DCDCAA`。
- 字符串（普通、f-string、raw string，含前缀 `f`/`r`/`b`/`u`）：橙色 `#CE9178`。
- 数字：绿色 `#B5CEA8`。
- **注释**（`# 行`）：绿色 `#6A9955`。注释通过 `highlightBlock` 中的字符扫描实现——逐字符跟踪 `'...'` 和 `"..."` 字符串状态（含转义字符和前缀处理），仅在字符串外检测到 `#` 时应用注释格式。这确保了注释内部的数字、字符串、关键字等内容不会被其他规则错误高亮（例如 `# {1:[2]}` 中 `1` 和 `2` 不会被错误着色为数字），同时字符串内部的 `#` 不会被误当作注释（例如 `x = "# not a comment"`）。
- 三引号字符串（`"""` / `'''`）：绿色，支持跨行块状态跟踪。

**协作关系**：
- 由 `LanguageUtils::createHighlighter()` 工厂函数创建，作为 `"python"` 语言的高亮器实现。
- 被 `CodeEditor::setLanguage()` 调用并安装到 `QTextDocument` 上。

---

### 15. `CompilerUtils` — 编译器检测工具

**文件**：`compilerutils.h`

**职责**：
- 头文件-only 工具命名空间，检测系统中可用的 C/C++ 编译器和 Python 解释器。
- `findCompilers()` 返回可用编译器列表：优先检测 g++（通过 `QStandardPaths::findExecutable`），其次检测 MSVC cl.exe（仅在 VS 开发命令提示符环境中）。
- `findPython()` 检测 `python` 或 `python3` 解释器。
- `getCompileArgs(compilerId, sourceFile, outputFile)` 根据编译器类型生成编译参数：
  - g++：`-std=c++17 -Wall -Wextra source.cpp -o output.exe`
  - MSVC：`/std:c++17 /W4 /EHsc source.cpp /Feoutput.exe`
- `getOutputPath(sourceFile)` 根据源文件路径推导输出的 `.exe` 路径。

---

### 16. `ProcessRunner` — 编译运行管理器

**文件**：`processrunner.h` / `processrunner.cpp`

**职责**：
- 基于 `QProcess` 的编译→运行两阶段管线管理器（支持 C/C++ 和 Python）。
- `startCompile(sourceFile)`：启动编译器进程，编译完成后发出 `compileFinished(success)`。
- `startRun(executable)`：运行可执行文件，完成后发出 `runFinished(exitCode)`。
- `startCompileAndRun(sourceFile)`：先编译，成功后再自动运行。
- `startRunPython(sourceFile)`：检测 python 解释器并直接运行 `.py` 源文件。
- `stop()`：终止当前正在执行的进程。
- `writeInput(const QString &text)`：向正在运行的进程写入 stdin 数据（自动追加换行符）。
- `writeRaw(const QString &text)`：向正在运行的进程写入 stdin 数据（不追加换行符，用于原始字节写入）。
- 实时输出流：通过 `readyReadStandardOutput/Error` 读取原始输出并通过 `outputReceived(text, isStderr)` 信号发出，不进行 `.trimmed()` 处理，保留原始格式。
- 编译阶段自动禁用 stdin 写入（`isAcceptingInput()` 仅当模式为 `RunOnly` 时返回 `true`）。

**信号**：
- `outputReceived(const QString &text, bool isStderr)`
- `compileFinished(bool success)`
- `runFinished(int exitCode)`
- `processStarted()` / `processStopped()`

---

### 17. `OutputPanel` — 输出面板

**文件**：`outputpanel.h` / `outputpanel.cpp`

`OutputPanel` 现在是 `BottomPanel` 的子组件，嵌入 BottomPanel 的「输出」标签页中。

**职责**：
- 深色终端风格（`QPlainTextEdit`，Consolas 10pt，背景 `#1E1E1E`），显示编译信息和程序运行输出，同时支持运行时标准输入交互。stdout 白色（`#D4D4D4`），stderr 红色（`#F48771`）。
- 进程运行时，输出区域自动变为可编辑，支持终端式直接输入：
  - 键盘输入：字符实时回显到输出区域，同时缓冲到 `m_inputBuffer`
  - Enter：将缓冲行通过 `sendInput` 信号发送到进程 stdin，光标换行
  - Backspace：从缓冲区删除最后一个字符，同时从输出区域移除
  - Ctrl+V / 右键：直接粘贴（无菜单），支持多行文本。粘贴内容逐行发送（20ms 间隔），每行发送后调用 `processEvents()` 使程序输出交错显示。最后一行无尾部换行符时，回显并放入输入缓冲区，用户可编辑后按 Enter 发送
  - Ctrl+C：终止正在运行的进程（emit `stopRequested()`）
  - 屏蔽方向键、Ctrl+Z 等编辑快捷键，防止破坏输出内容；粘贴发送期间也屏蔽按键
- 编译阶段完全禁用交互（`NoTextInteraction`，吞噬按键事件）；运行结束后恢复文本选中但保持只读；进程结束后焦点移至编辑器，下次运行需手动点击终端
- 底部工具栏：状态标签（编译成功绿色/失败红色）+ 终止按钮 + 清除按钮。隐藏按钮已移至 `BottomPanel` 标题栏。
- `appendOutput(text, isStderr)`：追加输出到面板，stdout 不添加人工换行。
- `setStatus(status, isError)`：更新状态标签文字和颜色。
- `setRunning(running)`：控制输入模式（设置 `m_acceptingInput`、切换编辑器只读状态、清空输入缓冲/粘贴队列/定时器）。运行阶段启用 `TextEditorInteraction`，结束后设为 `TextSelectableByMouse`。
- `setMaxBlocks(int max)`：动态更新输出面板的最大行数限制，从 `SettingsManager` 覆盖值读取。

**快捷键（可配置）**：
- `stop_in_output`（默认 `Ctrl+C`）：终止运行
- `paste_in_output`（默认 `Ctrl+V`）：粘贴到终端

**信号**：
- `stopRequested()`：用户点击终止按钮或按 Ctrl+C 时发出。
- `sendInput(const QString &text)`：用户输入一行数据时发出，由 MainWindow 转发到 ProcessRunner 写入进程 stdin。

---

### 17b. `BottomPanel` — 底部统一面板（输出 + 诊断）

**文件**：`bottompanel.h` / `bottompanel.cpp`

**职责**：
- 统一底部面板，替代原来的独立 `OutputPanel`。通过标题栏标签页切换：「输出」（编译/运行结果和 stdin 交互）和「诊断」（代码诊断列表）。
- 标题栏：28px 高度，#2d2d2d 背景。左侧标签页按钮（输出/诊断，当前激活加粗高亮），右侧 ✕ 关闭按钮（emit `closeRequested`）。
- 内容区使用 `QStackedWidget`：索引 0 为 `OutputPanel`，索引 1 为诊断页面。
- 诊断页面（`m_diagnosticsPage`）：
  - `QScrollArea` 包含两个 `DiagnosticSection`（错误 / 警告），分别以红色（`#F44747`）和黄色（`#CCA700`）标注。
  - `setDiagnostics(const QList<SmdDiagnostic> &diagnostics)`：按 severity 分组统计并重建诊断条目。每条诊断显示行号和消息，点击发射 `diagnosticsLineClicked(line)`。
  - `clearDiagnostics()`：清空所有诊断。
  - `setCurrentEditor(CodeEditor *editor)`：记录当前编辑器引用（供后续使用）。
  - `rebuildDiagnostics()`：自动隐藏诊断条目数为 0 的分区，全部为空时显示"无诊断信息"占位文本。
- 切换标签页时自动管理 provider 连接：`MainWindow` 在 `currentChanged` 中切换到代码文件时 `disconnect` 旧 provider → `connect` 新 provider → 通过 `CodeEditor::diagnostics()` 立即恢复缓存诊断。切换到 `.md` 文件时加载该文件缓存代码块诊断（通过 `loadMdDiagnosticsForCurrentTab()`）；切换到其他非代码文件时自动 `hide()`。

**信号**：
- `closeRequested()`：标题栏关闭按钮点击。
- `diagnosticsLineClicked(int line)`：诊断条目点击，MainWindow 连接至 `EditorWidget::navigateEditorToLine()`。

**内部类 `DiagnosticSection`**（定义于 `smddiagnosticspanel.h`）：
- 继承 `QWidget`，带标题和边框色的分区容器。`setDiagnostics()` 过滤指定 severity 的诊断，按行号排序后填充条目。`count()` 返回当前条目数。支持展开/折叠。

---

### 18. `FlowLayout` - 流式布局组件

**文件**：`flowlayout.h` / `flowlayout.cpp`

**职责**：
- 继承 `QLayout`，实现类似 Java `FlowLayout` 的自动换行布局。当一行空间不足以容纳下一个子控件时，自动将其放置到下一行。
- 支持 `heightForWidth()`：根据给定的宽度计算所需总高度，与 Qt 布局系统集成以正确分配垂直空间。
- 支持通过 `horizontalSpacing()` 和 `verticalSpacing()` 返回控件间距（可用户指定或从样式派生）。
- `doLayout(const QRect &rect, bool testOnly)` 为核心布局函数：遍历所有子项，按水平方向排列，检测到超出右边界时换行。`testOnly=true` 时仅计算高度不移动控件（供 `heightForWidth()` 使用）。
- 子项可见性自动管理：布局时将所有子控件的可见性设为 `true`。

**主要接口**：
- `FlowLayout(QWidget *parent, int margin, int hSpacing, int vSpacing)`：构造函数，可指定边距和水平/垂直间距。
- `void addItem(QLayoutItem *item)` / `QLayoutItem *itemAt(int index)` / `QLayoutItem *takeAt(int index)`：布局项管理。
- `int heightForWidth(int width) const`：给定宽度计算所需高度。
- `Qt::Orientations expandingDirections() const`：返回 0（不向任何方向扩展）。
- `bool hasHeightForWidth() const`：返回 `true`。

**协作关系**：
- 被 `FileExplorerWidget` 用于面包屑路径栏布局，使长路径自动换行而不会强制撑宽左侧面板。

---

### 19. `JudgeEngine` — 评测引擎

**文件**：`judgeengine.h` / `judgeengine.cpp`

**职责**：
- 独立的 `QObject`，管理编译和测试的 `QProcess` 管线，不依赖 `ProcessRunner`。支持 C/C++ 和 Python（`.py`）语言。
- `discoverTests()` 扫描测试文件夹，匹配同名的 `.in`/`.out` 文件对，自动跳过内容为空的 `.out` 文件。
- 编译阶段：对 C/C++ 使用 `CompilerUtils` 检测编译器并编译。Python 文件自动跳过编译，检测解释器后直接进入预热阶段。
- 预热阶段：编译（或跳过编译）成功后、首个测试前，空输入运行一次可执行文件或 Python 脚本，将程序载入 OS 磁盘缓存，消除冷启动对第一组测试计时的影响。
- 测试阶段：对每个用例 spawn `QProcess` 运行可执行文件或 `python source.py`，写入 `.in` 内容到 stdin，捕获 stdout，逐行 `.trimmed()` 对比（OJ 风格，去尾部空行）。
- 超时控制：每个用例 1000ms（复用 `QTimer`，`setSingleShot(true)`），超时标记 TLE。
- 内存监控：三重捕获策略确保准确读取——① 进程启动后立即同步调用 `captureMemory()`（此时进程等待 stdin 输入，保证存活）；② 100ms `QTimer` 轮询续传（`captureMemory()` 抽取为共享方法，使用 `PROCESS_QUERY_LIMITED_INFORMATION`）；③ 进程退出时补充读取。峰值内存超过 65536KB 标记 MLE 并杀进程。
- `m_testHandled` 标志位防止超时和进程结束双重触发。

**信号**：
- `judgeOutput(const QString &text, bool isStderr)`：引擎运行过程中的日志输出。
- `compileFinished(bool success, QString errorOutput)`
- `testStarted(int index, QString name)`
- `testFinished(int index, TestResult result)`
- `allTestsFinished(int passed, int total)`
- `judgeStopped()`

---

### 20. `JudgePanel` — 评测面板 UI

**文件**：`judgepanel.h` / `judgepanel.cpp`

**职责**：
- 评测面板 UI，嵌入 `QDockWidget`，在右侧停靠区域，默认隐藏。
- 顶部第一行：文件夹选择行（`QLineEdit` + "浏览..." 按钮）。
- 顶部第二行："从OpenJudge获取" 按钮，点击 emit `openJudgeRequested()` 信号，由 `MainWindow` 创建/激活 `OpenJudgeWindow`。
- 顶部第三行："提交到OpenJudge" 按钮，点击 emit `submitToOpenJudgeRequested()` 信号，由 `MainWindow::onSubmitToOpenJudge()` 处理。
- 中部：5 列 `QTableWidget`（#、测试用例、结果、耗时(ms)、内存(KB)），结果列按状态码着色：AC 绿色（`#52C41A`）、WA 红色（`#E74C3C`）、TLE 蓝色（`#3498DB`）、MLE 紫色（`#9B59B6`）、RE 橙色（`#F39C12`）。
- 中下部：`QPlainTextEdit` 详情区，点击失败行显示状态码、峰值内存、预期输出与实际输出。
- 底部：摘要 `QLabel` + "运行全部" / "停止" 按钮。
- 内部持有 `JudgeEngine`，连接所有信号。`runJudge(sourceFile)` 设置源文件和测试目录后启动评测。
- `setTestFolder(path)` 设置文件夹路径并自动清除已有结果，供 OpenJudge 集成使用。
- 信号 `runAllRequested()` 由 `MainWindow::onJudgeRunAll()` 触发。
- 信号 `openJudgeRequested()` 由 `MainWindow::onOpenJudgeRequested()` 处理，创建单例 OpenJudge 窗口。
- 信号 `submitToOpenJudgeRequested()` 由 `MainWindow::onSubmitToOpenJudge()` 处理，执行代码提交流程。检查顺序：代码有效性 → 题目选择状态 → 登录状态 → 作业是否进行中，不满足时弹出相应提示。

---

---

### 21. `Crawler` — OpenJudge 网络爬虫引擎

**文件**：`crawler.h` / `crawler.cpp`

**职责**：
- 基于 `QNetworkAccessManager` + `QNetworkCookieJar` 的 HTTP 爬虫，目标站点 `http://cxsjsx.openjudge.cn`。
- **登录流程**（PHP JSON API）：先 GET `/auth/login/` 建立 PHPSESSID 会话，再 POST 凭据到 `/api/auth/login/`（`email` + `password` 参数），解析 JSON 响应 `{"result":"SUCCESS"}`，成功后 GET 主页验证登录状态（检测用户名、退出链接等指标）。失败时自动回退到 CSRF 旧版登录（GET `/login/` → 提取 token → POST）。
- `fetchMainPage()` 获取主页 HTML，`parseMainPage()` 解析"进行中的作业"（`<ul class="current-contest">`）和"已结束的作业"（`<div class="past-contest">`）两部分，提取作业条目和"更多"分页链接。
- `fetchPastPage(url)` 获取 `/contests/past` 分页，解析比赛链接（匹配关键词 `hw`、`practise`、`midexam`、`pool`、`contest`），支持分页导航。
- `fetchHomeworkProblems(url)` 获取指定作业的题目列表，过滤导航链接（排名、状态、提交等），自动跳过 user/profile/auth 等非题目路径段，保留题目编号链接，去重排序。
- `fetchProblemDetail(url)` 获取题目详情 HTML，`parseProblemDetail()` 使用双策略（`<dt>/<dd>` 主策略 + `<h3>` 回退策略）提取章节（描述、输入、输出、样例输入、样例输出、提示），保留原始 HTML 结构供渲染。
- **代码提交**：`submitCode(problemUrl, sourceCode, languageId)` 先 GET 提交页面（`problemUrl/submit/`），解析隐藏字段 contestId、problemNumber 和 language radio 值，手动拼接 POST body（百分号编码，不使用 QUrlQuery 以避免 `+` → 空格问题），POST 到 `/api/solution/submitv2/`。发送原始源码而非 base64 编码，以避免某些竞赛实例的 base64 解码 bug 导致源码为空。
- **结果轮询**：解析 JSON 响应中的 `redirect` URL 作为 `m_pollStatusUrl`，通过 QTimer（2s 间隔，最多 15 次 = 30s 超时）轮询解决方案页面。`doPollSubmissionStatus()` 提取 body 纯文本，用正则解析 `状态: Accepted`、`时间: 23ms`、`内存: 7272kB`。检测到 CE 时自动获取编译错误详情。
- 静态工具方法 `decodeHtmlEntities()` 和 `stripHtmlTags()`（public static），供 `OpenJudgeWindow` 的样例提取使用。
- 调试日志写入 `crawler_debug.log`（启动时自动清空），记录 HTML 长度、关键标签匹配数、章节提取结果、POST 数据等。
- `clearCookies()` 替换 CookieJar 实现清除会话；`stopPolling()` 停止结果轮询定时器。

**信号**：
- `loginSuccess()` / `loginFailed(const QString &error)`
- `mainPageReady(const QList<HomeworkItem> &ongoing, const QList<HomeworkItem> &past, const PageInfo &pastPage)`
- `pastPageReady(const QList<HomeworkItem> &past, const PageInfo &pageInfo)`
- `homeworkProblemsReady(const QString &homeworkTitle, const QList<HomeworkItem> &problems)`
- `problemDetailReady(const ProblemDetail &detail)`
- `networkError(const QString &error)`
- `submissionResultReady(const SubmissionResult &result)` / `submissionFailed(const QString &error)` / `submitPollTimeout()`

**数据结构**：
- `HomeworkItem`：`title`（标题）、`url`（完整 URL）、`deadline`（截止日期字符串，仅进行中作业）。
- `ProblemSection`：`heading`（章节标题，如"描述"、"样例输入"）、`contentHtml`（原始 HTML 内容，保留 `<pre>` 等结构标签）。
- `ProblemDetail`：`title`（题目标题）、`sections`（`QList<ProblemSection>` 章节列表）。
- `PageInfo`：`url`（当前页 URL）、`currentPage`（页码）、`hasPrev` / `hasNext`（翻页边界）。
- `SubmissionResult`：`runId`（运行编号）、`status`（状态字符串如 "Accepted"/"Wrong Answer"）、`timeMs`（耗时）、`memoryKb`（内存）、`compileError`（CE 详情）。

**协作关系**：
- 由 `OpenJudgeWindow` 创建并持有，所有信号连接到 `OpenJudgeWindow` 的对应槽方法。
- 继承自 `QObject`，使用 Qt 父子内存管理。

---

### 22. `LoginDialog` — OpenJudge 登录对话框

**文件**：`logindialog.h` / `logindialog.cpp`

**职责**：
- 简单的 `QDialog`，包含用户名和密码输入框，附带"自动登录"复选框（默认关闭）。
- 提供"登录"和"跳过"两个按钮，跳过时 `reject()` 对话框。
- `username()` / `password()` 返回用户输入的凭据，`isAutoLoginEnabled()` 返回复选框状态。
- `setAutoLoginEnabled(bool)` 设置复选框的初始状态（从配置读取）。

**协作关系**：
- 由 `OpenJudgeWindow::onReLogin()` 以模态方式弹出，用户选择登录则将凭据传入 `Crawler::login()`。

---

### 23. `OpenJudgeWindow` — OpenJudge 题目浏览窗口

**文件**：`openjudgewindow.h` / `openjudgewindow.cpp`

**职责**：
- 独立的 `QMainWindow`，提供 OpenJudge 题目浏览与样例选择功能，深色主题（背景 `#2D2D30`，前景 `#D4D4D4`）。
- 顶部工具栏：[选择此题目] [← 返回] [stretch] [用户名标签] [登录/退出登录]。选择按钮仅在题目详情页可见，蓝色（`#0078D4`）突出显示。点击后切换为"已选择"状态（颜色变为 `#4A9BE0`），再次点击或查看其他题目时取消选中状态。
- **登录状态管理**：`m_loginBtn` 同时作为"登录"和"退出登录"按钮，根据 `m_isLoggedIn` 状态切换文本。登录成功后显示绿色 `m_userLabel`（`用户: xxx`），`m_isLoggedIn = true`，emit `loginStateChanged(true, username)`。登录失败弹出警告。退出登录时调用 `Crawler::clearCookies()` 清除会话，同时清除自动登录凭据，匿名重新加载主页。
- **自动登录**：构造函数接收 `SettingsManager*` 用于读写自动登录配置。`onReLogin()` 优先调用 `tryAutoLogin()` 尝试自动登录：若配置中 `autoLogin=true` 且凭据存在，直接调用 `Crawler::login()` 异步登录，不弹出对话框。登录成功后在 `onLoginSuccess()` 中将对话框勾选的凭据持久化（Base64 混淆）。自动登录失败时清除凭据并回退到手动登录对话框。退出登录时自动禁用 autoLogin 并清除凭据。
- 作业列表页（`OJ_HOMEWORK_LIST`）：展示"进行中的作业"和"已结束的作业"两个分区，使用 `HomeworkDelegate` 在右侧灰色显示截止日期。已结束作业支持分页（上一页/下一页），直接加载 `/contests/past` 分页子页面。
- 题目列表页（`OJ_PROBLEM_LIST`）：展示指定作业下的所有题目，显示题目数量。点击题目时自动判断作业是否进行中（对比 URL 与 `m_ongoingItems`），设置 `m_currentHomeworkOngoing` 标志。
- 题目详情页（`OJ_PROBLEM_DETAIL`）：左侧章节导航（`m_sectionList`，固定 100px）+ 右侧渲染内容（`QTextBrowser`），无间隔紧凑布局。选择按钮在此页可见。
- 样例提取（`extractSamples()`）：从 `ProblemDetail.sections` 中匹配章节标题含"样例"+"输入"或"样例"+"输出"的章节，正则 `<pre[^>]*>(.*?)</pre>` 提取文本，`decodeHtmlEntities` 解码 HTML 实体，按 1:1 配对输入输出。
- 临时缓存（`writeSamplesToCache()`）：将提取的样例写入 `QStandardPaths::TempLocation + "/SM-OJ-Cache"`（固定目录），每次写入前清空已有内容，文件命名 `testN.in` / `testN.out`，返回缓存目录路径。
- **代码提交接口**：`submitCurrentProblem(sourceCode, languageId)` 公开方法，按顺序校验：题目是否已选择 → 登录状态 → 作业是否进行中，不满足时通过 `submissionFailed` 信号返回错误。`hasCurrentProblem()` 公开方法供 `MainWindow` 在提交前预检题目选择状态。
- "选择此题目"按钮支持选中/取消选中切换：选中时 `m_currentProblemSelected = true`，emit `sampleSelected(folderPath)` 信号，按钮变"已选择"（`#4A9BE0`）；取消选中时仅恢复按钮状态，不触发信号。切换题目时 `onProblemDetailReady()` 自动重置 `m_currentProblemSelected = false`。`submitCurrentProblem()` 新增 `m_currentProblemSelected` 校验，未选中时拒绝提交。
- 窗口为单例：`MainWindow` 通过 `QPointer<OpenJudgeWindow>` 管理，关闭后自动置 null，再次点击重新创建。

**信号**：
- `sampleSelected(const QString &folderPath)`：用户选择题目后发出，携带样例缓存目录路径。
- `loginStateChanged(bool loggedIn, const QString &username)`：登录/登出状态变化时发出。
- `submissionResultReady(const SubmissionResult &result)`：转发自 `Crawler`。
- `submissionFailed(const QString &error)`：转发自 `Crawler`。

**内部枚举 `OjViewState`**：`OJ_HOMEWORK_LIST`、`OJ_PROBLEM_LIST`、`OJ_PROBLEM_DETAIL`，独立于 web-crawler 的 `ViewState`，避免符号冲突。

**协作关系**：
- 由 `MainWindow::onOpenJudgeRequested()` 创建（传入 `SettingsManager*`），`sampleSelected` 信号连接到 `MainWindow::onOpenJudgeSampleSelected()`。
- 持有 `Crawler` 实例，连接其全部信号到自身槽方法。
- 调用 `LoginDialog` 进行登录交互，通过 `SettingsManager` 持久化自动登录凭据。
- 依赖 `Crawler::decodeHtmlEntities()` 静态方法解码 HTML 实体。

---

### 24. `SettingsPanel` — 设置面板

**文件**：`settingspanel.h` / `settingspanel.cpp`

**职责**：
- 悬浮式设置面板 `QWidget`，以半透明遮罩层 + 居中面板的方式覆盖在主窗口上方。
- 遮罩层（`m_settingsOverlay`，`OverlayWidget` 类）由 `MainWindow` 创建并管理，是独立的顶层 `Qt::Tool` 窗口（`WA_TranslucentBackground` + `paintEvent` 绘制 `QColor(0,0,0,128)` 半透明黑色背景），由 Windows DWM 逐像素 alpha 合成，覆盖整个主窗口。顶层窗口方案避免了非原生子 widget 覆盖原生 QWebEngineView 时 Qt 裁剪 HWND 导致的黑屏问题。
- 面板为无边框 `QWidget`，深色主题：背景 `#2b2b2b`、圆角 8px、边框 `#555555`。
- 标题栏（36px 高）：左侧 "设置" 标签（`#cccccc`，13px 粗体），右侧关闭按钮（`✕`，悬停变红色 `#c42b1c`）。
- **分类侧边栏布局**：`QHBoxLayout`（0 边距、0 间距）左侧 `QVBoxLayout` 内含 `QListWidget`（170px 宽、深色 `#252525`）+ 底部"恢复默认设置"按钮（确认后清除所有覆盖值），右侧 `QStackedWidget` 显示对应分类页面。每个分类页面为 `QScrollArea` 内含内容 Widget。
- **7 个分类页面**：
  - **编辑器**：默认缩放比例滑块+输入框（50%-300%）、代码缩进宽度微调框（1-8）、Markdown 缩进宽度微调框（1-8，默认 2）、编辑器字体下拉框（系统字体列表）、字号微调框（8-24）。
  - **外观**：主题选择下拉框（深色/浅色/跟随系统）+ 恢复主题默认值按钮；文件树条目行高微调框（24-32px，默认 28px，实时生效）；6 个颜色按钮+十六进制预览标签——编辑器背景/前景、行号背景/前景、当前行高亮、搜索高亮。点击弹出 `QColorDialog`，实时应用并持久化。
  - **输出面板**：输出面板字号微调框（8-24）、最大行数微调框。
  - **预览**：分屏防抖延迟微调框（100-2000ms，百分号移出框外使用独立 `%` 标签）、分屏比例微调框（30-70）。
  - **搜索**：每文件最大匹配数（1-50）、总结果上限（50-2000）、片段最大长度（50-500）。
  - **快捷键**：只读 `QTableWidget`，两列（动作名 + 按键序列），从 ConfigManager 读取。
- **信号**：`editorSettingChanged`、`appearanceSettingChanged`、`outputPanelSettingChanged`、`previewSettingChanged`、`searchSettingChanged`，均为 `(const QString &key, const QVariant &value)` 泛型模式。
- `syncFromSettings(SettingsManager &sm)`：面板打开时由 `MainWindow` 调用，从 `SettingsManager::value()` 读取已持久化的覆盖值回填所有控件（使用 ConfigManager 默认值作为 fallback）。恢复默认设置后同样调用此方法重置控件。
- 支持标题栏拖拽移动：在标题栏区域按住鼠标左键拖动可移动面板位置，移动范围限制在遮罩层内。
- 支持右下角 QSizeGrip 拖拽调整大小。最小尺寸 400×300 像素。八方向边缘缩放已禁用（`kEdgeMargin = 0`），避免缩放时误操作主窗口。
- `QSizeGrip` 放置在右下角，提供可视化的调整大小手柄。
- 尺寸从 `ConfigManager` 读取（`settings_panel.width` / `settings_panel.height`，默认 680×480）。
- 点击遮罩层背景区域自动关闭面板（通过 `MainWindow::eventFilter` 处理）。

**信号**：
- `void closeRequested()`：用户点击关闭按钮时发出，由 `MainWindow::toggleSettings()` 响应。
- `void defaultZoomChanged(qreal zoom)`：默认缩放值变更时发出，由 `MainWindow::onDefaultZoomChanged()` 响应。
- `void resetToDefaultsRequested()`：点击"恢复默认设置"并确认后发出，由 `MainWindow::onResetToDefaults()` 响应。

**协作关系**：
- 由 `MainWindow` 创建并持有（`m_settingsPanel`），父控件为顶层遮罩层 `m_settingsOverlay`（`OverlayWidget`）。
- 工具栏"设置"按钮和 `Ctrl+,` 快捷键统一调用 `MainWindow::toggleSettings()` 切换显示/隐藏。显示时通过 `positionOverlay()` 定位 overlay 覆盖 MainWindow 并居中面板，隐藏时调用 `refreshPreviewTheme()` 刷新预览主题。
- `MainWindow::resizeEvent()` 和 `moveEvent()` 中跟踪 overlay 位置同步（`mapToGlobal` 定位），`changeEvent` 中最小化时自动隐藏 overlay。
- 点击 overlay 背景区域（设置面板外部）通过 `eventFilter` 监听 `MouseButtonPress` 自动关闭面板。
- `MainWindow` 连接所有 5 个分类信号到对应 slot，每个 slot 调用 `m_settings->setSettingOverride(key, value)` 持久化并遍历所有编辑器实时应用设置。

---

### 25. `SmdFormat` — SMD 文件格式解析/序列化

**文件**：`smdformat.h`

**职责**：
- 头文件（header-only）命名空间 `SmdFormat`，提供 `.smd` 文件格式的解析与序列化。
- 文件格式以 `---smd:<type>` 为单元格分隔符，`type` 可取 `markdown`、`cpp`、`python`。分隔线后可选 JSON 元数据（如 `{"rendered":true,"output":"..."}`），用于持久化输出内容和渲染状态。
- 首个分隔符之前的任何内容视为默认 markdown 单元格（或忽略空内容）。
- 每个单元格的前导和尾部空白行在解析时自动裁剪。

**数据结构 `SmdFormat::Cell`**：
- `QString type`：单元格类型（"markdown"/"cpp"/"python"）。
- `QString content`：单元格文本内容（已裁剪前后空白行）。
- `bool rendered`：Markdown 单元格的渲染状态（默认 false）。
- `QString output`：代码单元格的执行输出文本（默认空，序列化时 base64 编码）。

**主要接口**：
- `QList<Cell> parse(const QString &text)`：将原始 `.smd` 文本解析为单元格列表。按行扫描，以正则 `^---smd:(\w+)\s*(\{.*\})?$` 匹配分隔符，类型名自动转小写。若存在 JSON 元数据则解析 `rendered` 和 `output`（base64 解码）字段。
- `QString serialize(const QList<Cell> &cells)`：将单元格列表序列化为 `.smd` 格式文本。每单元格写入 `---smd:<type>`，若有非默认元数据则追加 JSON（`rendered` 为 true 或 `output` 非空时 base64 编码）。单元格间空行分隔。
- `QString toMarkdown(const QList<Cell> &cells)`：将 SMD 转换为 `.md` 格式。Markdown 单元格直接拼接内容，C++/Python 单元格包装为 fenced code block。
- `QList<Cell> fromMarkdown(const QString &markdown)`：从 `.md` 文本反向转换为单元格列表。检测 fenced code block 分隔符并拆分单元格，Mermaid 图表保留为独立 Markdown 单元格。
- `FromMarkdownResult fromMarkdownWithMapping(const QString &markdown)`：增强版，额外返回 `mdLineToCell` / `mdLineToCellLine` 映射，用于 `.md` → `.smd` 光标定位。
- `ToMarkdownResult toMarkdownWithMapping(const QList<Cell> &cells)`：增强版，额外返回 `cellContentStartLine` 向量，用于 `.smd` → `.md` 光标定位。行数按 `\n` 计数 + 1 统一计算（空内容计为 1 行），避免空 cell 导致后续 cell 光标映射偏移。

**协作关系**：
- 被 `SmdEditor` 在 `loadFile()` 和 `saveFile()` 中调用以解析和序列化文件内容。
- `output` 字段的 base64 编解码仅在序列化/反序列化边界，内存中为原始文本。

---

### 26. `SmdCell` — SMD 单元格控件

**文件**：`smdcell.h` / `smdcell.cpp`

**职责**：
- 继承 `QFrame`，表示 `.smd` 文件中的一个单元格（Cell），包含类型标签和编辑器/渲染视图栈。输出区域已移出至独立的 `SmdOutputWidget`，由 `SmdEditor` 管理。
- 支持三种 `CellType`：`Markdown`、`Cpp`、`Python`。
- **自适应高度**：编辑器高度通过遍历所有 `QTextBlock` 的 `QTextLayout::boundingRect()` 精确求和得出，覆盖通过 `QFontMetricsF` 获取子像素精度，加上 `contentsMargins` 边距和 `+2px` 浮点舍入缓冲，确保编辑器内容完整可见无内部滚动条。整个页面通过父级 `QScrollArea` 统一滚动。
- **选中视觉效果**：active 状态下，编辑模式显示 2px 蓝色边框（`#0078d4`），命令模式显示 2px 紫色边框（`#C586C0`），背景微亮（`#252526`）；非 active 命令模式显示灰色边框（`#3c3c3c`）；编辑模式透明边框。

**布局结构（垂直）**：
1. **头部栏**（`m_headerBar`，24px 固定高度）：左侧类型标签（`QLabel`，彩色圆角背景——MD 蓝色 `#3a6ea5`、C++ 绿色 `#2d8a56`、Python 黄色 `#b8952e`），右侧操作提示。
2. **编辑器/视图栈**（`m_editorStack`，`QStackedWidget`）：
   - Page 0：编辑器——Markdown 单元格使用 `QPlainTextEdit`（等宽字体、深色主题），C++/Python 单元格使用 `CodeEditor`（带语法高亮和行号）。
   - Page 1：渲染视图（仅 Markdown）——`RenderPixmapWidget`（自定义 QWidget，以 `QPainter` 绘制 `QPixmap` 实现 `scaledContents` 等效行为），通过 QWebEngineView 独立顶层窗口渲染 Markdown（含 LaTeX/Mermaid），`performGrab()` 抓取为 `QPixmap` 后由 RenderPixmapWidget 显示，销毁 QWebEngineView 释放 GPU 资源。RenderPixmapWidget 的 `sizeHint()` 返回 `(-1,-1)`，不传播 pixmap 尺寸，避免父布局被锁定在渲染宽度而无法缩小。

**主要接口**：
- `CellType cellType() const` / `void setCellType(CellType type)`：获取/设置单元格类型。`setCellType()` 会销毁旧编辑器并重建新类型对应的编辑器，保留内容，最后调用 `setCommandMode(m_commandMode)` 将当前命令/编辑模式状态重新应用到新编辑器。
- `QString content() const` / `void setContent(const QString &text)`：获取/设置单元格文本内容。
- `void setActive(bool active)` / `void setCommandMode(bool cmd)`：控制选中和命令模式的视觉样式（`updateBorderStyle()`）及光标可见性。`setActive(false)` 时清除编辑器文本选中（`QTextCursor::clearSelection()`）并设置 `setCursorWidth(0)` 隐藏光标。`setActive(true)` 且非命令模式时恢复 `setCursorWidth(1)`。`setCommandMode(true)` 时先显式 `QTextCursor::clearSelection()` 清除选中，再通过 `Qt::NoTextInteraction` + `setCursorWidth(0)` 禁用交互和光标；对已渲染单元格还会直接操作隐藏的 `m_markdownEditor`。退出命令模式时恢复 `setCursorWidth(1)`。
- `bool isRendered() const` / `void setRendered(bool rendered)`：Markdown 单元格渲染/取消渲染。true 时创建独立顶层 QWebEngineView 加载 HTML 模板，轮询测量高度和 Mermaid 完成状态后抓取为 QPixmap；false 时切回编辑器并清理渲染资源，根据 `m_commandMode` 决定是否聚焦编辑器（编辑模式）或保持只读无光标状态（命令模式）。
- `void setRenderedState(bool rendered)`：仅设置渲染标志位，不触发实际渲染管线。用于文件加载时预置渲染状态，避免 `toPlainText()` 序列化结果与文件内容不一致导致误判为已修改。
- `QWidget *editorWidget() const`：返回当前活跃的编辑器控件（Markdown 编辑器、CodeEditor 或渲染静态 RenderPixmapWidget）。
- `void setEditorFocus()`：将焦点设置到编辑器控件并恢复 `setCursorWidth(1)`。若为渲染视图则先返回编辑模式。
- `void applyZoom(qreal factor, int baseFontSize)`：保存缩放因子至 `m_zoomFactor` 和 `m_baseFontSize`。对于非渲染单元格，调整编辑器字体大小及行号区域；对于已渲染单元格，将 `m_lastRenderWidth` 置零并触发防抖重渲染，重渲染时在 HTML 模板中注入 `body{font-size:Npx!important}` 使渲染内容的字体随缩放同步变化。
- `void checkReRender()`：供 `SmdEditor` 在 `resizeEvent` 中调用的公共接口，检查当前 cell 宽度与 `m_lastRenderWidth` 的差异，大于 20px 时触发防抖重渲染。
- `void updateEditorHeight()`：遍历编辑器中所有 `QTextBlock`，累加 `QTextLayout::boundingRect().height()` 得到总文档高度，加上 `contentsMargins` 和缓冲后调用 `setFixedHeight`。连接 `blockCountChanged` 与 `contentsChanged` 触发。
  - **递归护盾**：`m_updatingHeight` 标志防止 `setFixedHeight` → layout → document 信号 → `updateEditorHeight` 的跨 cell 递归风暴。函数入口检查并设置标志，所有 return 路径复位。
  - **内容变更计数器**：`m_pendingContentChanges` 由 document 信号 lambda 递增、`updateEditorHeight` 入口原子性获取并清零。仅当计数器 > 0（真正的用户编辑）时才 `emit contentChanged()`，避免 layout 触发的重算虚假发射 LSP `cellContentChanged` → `syncVirtualDoc` 通知风暴。
- `void setDiagnostics(const QList<SmdDiagnostic> &diagnostics)`：存储诊断列表并调用 `updateTypeLabel()` 刷新头部标签。错误计数（severity=1）和警告计数（severity=2）显示在类型标签旁，有错误时标签背景变红 `#d43838`。

**信号**：同上。

**生命周期安全**：
- `setCellType()` 切换类型时，先 `removeWidget()` 将旧编辑器从 `QStackedWidget` 移除，调用 `setCompletionProvider(nullptr)` 断开共享 LSP adapter，再 `hide()` 隐藏。**旧编辑器不显式删除** — 它仍是 `m_editorStack` 的子控件，Qt 父子系统在 `SmdCell` 销毁时自动清理。在事件处理期间（如 `MouseButtonRelease`）调用 `delete`/`deleteLater()` 会破坏 Qt 内部事件状态，导致闪退。
- 类型变更信号 `cellTypeChanged(CellType oldType)` 携带旧类型参数。`connectCellSignals()` 中 lambda 用 `oldType` 计算 `oldLangId` 并调用 `m_lspManager->cellTypeChanged(index, oldLangId, newLangId, content)`，确保旧语言的 `cellOrder` 和缓存被正确清理。lambda 还对新 editor 安装 `eventFilter`。信号在 re-index 循环中先 `disconnect` 再 `connect`，防止重复连接累积。
- `m_lspManager` 在 `setPlainText()` 中 **晚于 `addCell()` 创建**。`addCell()` 执行时 `m_lspManager` 为 null，无法注入共享 provider。`setPlainText()` 中 `cellAdded()` 循环后额外遍历 cell 调用 `providerForCell() → setCompletionProvider()` 完成初始注入。
- 渲染管线 `runJavaScript` 回调使用 `QPointer<SmdCell>` 守卫替代裸 `this` 捕获，cell 删除后自动为 null。
- `cleanupRenderView()` 在 delete 前先 `disconnect()` QWebEngineView 和 QWebEnginePage 的所有信号。

**事件处理**：
- 重写 `eventFilter(QObject*, QEvent*)`：拦截 `FocusIn` 事件发射 `focusEntered()` 信号（`MouseButtonPress` 不再触发，改由 `SmdEditor::eventFilter` 全局过滤器统一处理点击激活）。设置 `m_grabbing` 标志位时抑制发射（防止 `performGrab()` 期间顶层窗口隐藏导致的焦点回跳）。
- 重写 `resizeEvent(QResizeEvent*)`：检测 cell 宽度变化（`event->size().width()` 与 `m_lastRenderWidth` 差异 > 20px）时调用 `scheduleReRender()` 启动 300ms 防抖定时器。`performReRender()` 在定时器超时时执行完整重渲染：保留本地遮罩层避免闪烁 → `setRendered(false)` → 恢复内容 → `setRendered(true)` → 恢复命令模式。

**协作关系**：
- 由 `SmdEditor` 创建和管理，作为 `m_cellContainer` 的子控件。
- 输出内容由独立的 `SmdOutputWidget` 管理，与 SmdCell 分离。

---

### 27. `SmdEditor` — SMD 单元格编辑器主控件

**文件**：`smdeditor.h` / `smdeditor.cpp`

**职责**：
- 继承 `QWidget`，作为 `EditorWidget` 的 `SmdEdit` 模式的内部主控件，管理 `.smd` 文件的加载、保存、单元格编辑和代码执行。
- 拥有一个 `QScrollArea`，内含 `m_cellContainer`（`QVBoxLayout`），所有 `SmdCell` 和 `SmdOutputWidget` 交错排列（每个单元格下方紧跟一个输出控件），超出视口时滚动。
- 拥有独立的 `ProcessRunner` 实例，用于代码单元格的编译和执行。
- 管理 `m_outputWidgets` 列表，与 `m_cells` 一一对应，负责输出的持久化与恢复。

**模式管理**：
- **编辑模式**（默认）：当前活动单元格的编辑器获得焦点，用户可直接输入内容。边框透明。
- **命令模式**（`Esc` 进入）：所有编辑器失焦，活动单元格显示紫色边框（`#C586C0`）。通过键盘快捷键操作单元格。
- 按 `Enter`（命令模式）回到编辑模式；按 `Esc`（编辑模式）进入命令模式。

**命令模式快捷键**：
| 快捷键 | 功能 |
|--------|------|
| `A` | 当前单元格上方插入新单元格（Markdown），弹出语言选择菜单 |
| `B` | 当前单元格下方插入新单元格（Markdown），弹出语言选择菜单 |
| `↑` / `↓` | 上下导航单元格 |
| `Enter`（无修饰键） | 进入编辑模式 |
| `Ctrl+Shift+Z` | MD 单元格：取消渲染 / 代码单元格：删除输出 |
| `Delete` | 删除当前单元格（至少保留一个） |

**编辑模式快捷键**：
| 快捷键 | 功能 |
|--------|------|
| `Ctrl+Enter` | 执行当前单元格（不跳转，保持编辑模式） |
| `Shift+Enter` | 执行当前单元格并跳转到下一个 |
| `Ctrl+Shift+-` | 在光标处将当前单元格一分为二（类型不变），选中下方单元格 |
| `Ctrl+Shift+Z` | MD 单元格：取消渲染 / 代码单元格：删除输出 |
| `Ctrl+D` | 切换诊断面板显示/隐藏 |

**通用快捷键（命令模式与编辑模式均有效）**：
| 快捷键 | 功能 |
|--------|------|
| `Ctrl+K` | 弹出语言选择菜单修改当前单元格类型 |
| `Ctrl+C` | 终止正在执行的代码单元格 |
| `Esc` | 进入命令模式（语言选择菜单弹出时：关闭菜单） |

**语言选择器（`LangSelectorPopup`）**：
- 继承 `QFrame`（`Qt::Popup`），深色主题无边框下拉菜单，出现于编辑区域顶部居中位置。
- 三个选项（`QLabel`）：`1. Markdown`、`2. C++`、`3. Python`，默认选中第一个（蓝色高亮背景 `#094771`）。
- 键盘操作：`↑/↓` 导航、`Enter` 确认、`1/2/3` 直接选择、`Esc` 取消。
- 鼠标点击选项直接确认并关闭；点击菜单外部取消。
- 确认后通过回调设置单元格类型，自动进入编辑模式。
- **取消行为**：如果在新建单元格流程中弹出（按 `A`/`B`），取消时自动删除该单元格、恢复原始活动单元格、清除修改状态标识。

**单元格执行**：
- `executeCurrentCell()`：根据当前活动单元格类型分发执行。**仅编辑模式**下 `Ctrl+Enter`（不跳转）或 `Shift+Enter`（跳转）触发，通过 `eventFilter` 处理 `ShortcutOverride` 事件确保不被 Qt 快捷键系统拦截。`m_jumpAfterExecute` 标志控制执行后是否跳转到下一个单元格。执行前后不改变编辑/命令模式状态。执行前通过 `CodeEditor::hideSignatureHelp()` 主动关闭签名帮助弹出窗口，防止执行后弹出窗口残留。
- **Markdown 单元格**：空单元格（`content().trimmed().isEmpty()`）跳过渲染，直接根据 `m_jumpAfterExecute` 决定是否跳转。非空且未渲染时调用 `SmdCell::setRendered(true)` 启动异步渲染流程（QWebEngineView 顶层窗口加载 HTML → 轮询高度与 Mermaid 完成 → 抓取 QPixmap → 销毁 WebEngineView）。已渲染的单元格跳过渲染。`m_jumpAfterExecute` 为 true 时跳转下一个单元格。
- **C++ 单元格**：执行时按 `main()` 函数边界**自动分组**（`cppGroupForCell()`），仅合并与当前 cell **同组** 的 C++ cell 内容写入临时文件（不同程序组互不干扰）→ `ProcessRunner::startCompileAndRun()`（或 `startCompileOnly`，当不含 `main()` 时仅编译不链接）→ stdout/stderr 流式输出到独立的 `SmdOutputWidget`（输出控件仅在有实际输出时通过 `appendText()` 自动显示，无输出时保持隐藏） → 清理临时文件 → `m_jumpAfterExecute` 为 true 时跳转下一个单元格。
- **Python 单元格**（`executePythonCell()`）：采用持久化进程执行模型。首个 Python cell 执行时通过 `startPythonExecProcess()` 启动后台 `python_executor.py` 守护进程（JSON-line stdin/stdout 协议）。代码在发送前进行预处理：规范化行尾（`\r\n`/`\r` → `\n`）、替换孤立 UTF-16 surrogate。通过 **base64 编码**传输代码以避免 JSON 换行转义问题。守护进程解码后在共享命名空间中 `exec()` 代码并独立捕获 stdout/stderr，返回 JSON 响应 `{"ok":true,"stdout":"...","stderr":"..."}` 或 `{"ok":false,"error":"..."}`。输出仅路由到当前 cell 的 `SmdOutputWidget`（仅在 stdout/stderr 非空时调用 `appendText()` 显示控件），前面 cell 的 print 输出不会出现在后续 cell 中。进程崩溃时自动重启（1 秒延迟），Ctrl+C 终止时 kill 并自动重启进程。文件关闭或新文件打开时通过 `stopPythonExecProcess()` 发送 exit 命令并清理进程。C++ 单元格保持原有合并+临时文件方式不变。
- **空单元格**：Markdown 和代码单元格均跳过执行/渲染流程，`m_jumpAfterExecute` 为 true 时直接跳转。
- 执行期间不支持标准输入交互（Python 持久进程中 `input()` 会干扰 JSON 协议）。
- 跳转保护：仅在执行单元格仍为当前活动单元格时执行跳转，用户已导航至其他单元格时不跳转。
- 修改跟踪：`isModified()` 使用 `toPlainText()`（含 output 字段）与 `m_originalContent` 比较，输出内容变化（执行产生新输出、`Ctrl+Shift+Z` 清除输出）均会触发修改标记。执行完成后通过 `emit contentChanged()` 通知 `EditorWidget` 执行内容检查，确保输出变化反映在文件修改状态上。

**文件 I/O**：
- `loadFile()`：读取文件 → `SmdFormat::parse()` 解析（含元数据） → `addCell()` 创建单元格和输出控件。恢复输出到 `m_outputWidgets[i]`，若有已渲染的 MD 单元格则加入 `m_autoRenderQueue`。
- `saveFile()`：遍历单元格获取 `content()` 和 `isRendered()`，遍历输出控件获取 `outputText()` → `SmdFormat::serialize()` 序列化为带 JSON 元数据的格式 → 写入文件。
- `saveAsFile()`：通过 `EditorWidget::saveAsFile()` 间接支持（对话框包含 `*.smd` 过滤器）。
- 自动渲染：`startAutoRender()` 以 200ms 间隔的 QTimer 依次对队列中的 MD 单元格调用 `setRendered(true)` + `setCommandMode(true)`，不改变活动单元格和焦点，隐式完成渲染。
- 修改状态同步：文件加载时先通过 `setRenderedState(true)` 预置已渲染单元格的标志位，然后采集 `m_originalContent`，使序列化结果与文件内容一致，避免自动渲染完成后误判为已修改。

**修改状态**：通过比较当前序列化内容（含 output 字段）与加载时的原始内容判断，`modificationChanged` 信号连接至 `EditorWidget::modificationChanged`。修改单元格内容或输出区内容均可触发修改标记（输出变化、`Ctrl+Shift+Z` 清除输出）。

**LSP 集成（SmdLspManager）**：
- `setPlainText()` 中创建 `SmdLspManager` 实例，解析完成后遍历所有代码 cell 调用 `cellAdded()` 注册，**并额外遍历 cell 调用 `providerForCell() → setCompletionProvider()` 注入共享 provider**（因 `addCell()` 时 `m_lspManager` 为 null 无法注入）。加载完成后调用 `focusCell()` 初始化活动程序组。
- `connectCellSignals()` 中为 C++/Python cell 注入共享 CompletionProvider（`codeEditor->setCompletionProvider(m_lspManager->providerForCell(...))`）。re-index 循环中先 `disconnect` 所有相关信号（`focusEntered`、`contentChanged`、**`cellTypeChanged`**）再 `connect`，防止信号重复连接累积。`focusEntered` 信号直接调用 `setActiveCell(index)` 激活 cell，滚动由 `m_clickSuppressScroll` 标志控制（鼠标点击期间置 true 跳过滚动）。
- `cell->contentChanged` 信号连接至 `m_lspManager->cellContentChanged()`，触发虚拟文档重建和 `textDocument/didChange` 通知。C++ cell 内容变化后额外调用 `focusCell(m_activeCellIndex)` 重新检测程序组边界（支持动态键入/删除 `main()` 时实时切换组）。
- `cell->cellTypeChanged(CellType oldType)` 信号携带旧类型参数。lambda 用 `oldType` 计算 `oldLangId` 并调用 `m_lspManager->cellTypeChanged(index, oldLangId, newLangId, content)`，确保旧语言的 `cellOrder` 和内容缓存被正确清理后再为新语言创建 adapter。
- `addCell()` / `removeCell()` 中通知 `m_lspManager->cellAdded()` / `cellRemoved()` 更新虚拟文档。`removeCell()` 在 `cellRemoved()` 删除共享 adapter **之前**先调用 `codeEditor->setCompletionProvider(nullptr)` 断开，防止旧编辑器持有悬空指针。`addCell()` 创建 cell 后调用 `cell->applyZoom(m_zoomFactor, m_baseFontSize)` 使新 cell 继承当前编辑器的缩放状态。增删 cell 后若活动 cell 为 C++ 则调用 `focusCell()` 重新检测组边界。
- `setActiveCell()`：调用旧单元格 `setActive(false)` 清除编辑器选中 + `m_outputWidgets[oldIndex]->clearSelection()` 清除输出区选中 → 激活新单元格 → 同步命令模式状态。若 `m_clickSuppressScroll` 为 false，则通过 `QTimer::singleShot(0)` 延迟调用 `ensureWidgetVisible`（确保布局已处理完 `insertWidget` 等延迟事件后再读取 cell 位置）。鼠标点击激活时该标志为 true，跳过滚动。若目标 cell 为 C++ 则调用 `m_lspManager->focusCell(index)`，切换 clangd 虚拟文档至目标 cell 所在程序组并恢复缓存诊断。
- `m_lspManager->diagnosticsUpdated` 信号连接至 cell 的 `SmdCell::setDiagnostics()` 和 `CodeEditor::setDiagnostics()`，更新头部标签计数和编辑器波浪线。C++ 和 Python 诊断统一通过此信号分发，下游可视化组件（CodeEditor 波浪线、SmdCell 标签、SmdDiagnosticsPanel 面板、HoverManager 工具提示）均为语言无关。
- 打开新文件或重新解析内容时，先 `shutdown()` 旧 SmdLspManager 再创建新实例。

**执行安全**：
- 执行期间（`m_executingCell` 非空），`removeCell()` / `insertCellAbove()` / `insertCellBelow()` / `splitCellAtCursor()` 直接返回，阻止 cell 增删操作。
- `m_executingCell` 使用 `QPointer<SmdCell>`，cell 被删除后自动置 null，`onProcessOutput/Finished/Stop` 中先检查再通过 `m_cells.indexOf()` 定位索引。

**单元格增删**：
- `insertCellAbove()`：在当前活动单元格**上方**插入 Markdown 空单元格 → `setActiveCell(idx)` 激活 → 延迟 `QTimer::singleShot(0)` 调用 `m_cellLayout->activate()` 后手动将新 cell 滚至视口顶部（`target = cellY - 8`），钳制到 `maxScroll`。弹出语言选择菜单。
- `insertCellBelow()`：在当前活动单元格**下方**插入 Markdown 空单元格 → `setActiveCell(idx)` 激活 → 向布局底部**临时添加视口高度的 QSpacerItem**（`m_insertScrollPad`）→ 立即 `m_cellLayout->activate()` 使 `maxScroll` 反映扩展后的内容高度 → 延迟回调中再次 `activate() + ensureWidgetVisible` 后手动将新 cell 滚至视口底部（`target = cellTop + cellH - vpH + 8`），钳制到 `maxScroll`。弹出语言选择菜单。
- 临时衬垫通过 `removeInsertScrollPad()` 清除，调用点包括：语言弹窗的 `onSelected` 回调（确认选择后）、`onCancelled` 回调（取消后，在 `removeCell` 之前）。另有 15 秒超时兜底防止泄漏。
- `removeCell()`：删除指定索引的单元格，断开 LSP 连接，更新 `m_activeCellIndex`（若删除的索引小于等于当前活动索引则递减），调用 `setActiveCell()` 刷新活动单元格。至少保留一个单元格。

**单元格分割**（`splitCellAtCursor()`）：
- 编辑模式下 `Ctrl+Shift+-` 触发，在光标位置将当前单元格内容一分为二，两个单元格保持相同类型。
- 使用 `QPlainTextEdit::textCursor().position()` 定位分割点，前段保留在原单元格，后段创建新单元格插入下方。
- 分割后自动选中下方单元格，光标置于其开头（`QTextCursor::Start`）。
- 高度更新采用两阶段延迟策略：两个 `QTimer::singleShot(0, ...)` 确保外层布局（cell 宽度分配）和内层布局（QStackedWidget → QPlainTextEdit → QTextDocument 重排）均完成后再调用 `updateEditorHeight()`，避免因 QPlainTextEdit 宽度未更新导致的文档高度计算偏差。之后第三个 `QTimer::singleShot(0)` 将视图滚动到新 cell 光标位置，上方留 50px 边距确保 cell 顶部栏也能完整显示。

**命令模式编辑禁用与光标管理**：
- 进入命令模式时 `SmdCell::setCommandMode(true)` 将编辑器设为 `readOnly`、`Qt::NoTextInteraction`、`Qt::NoFocus`，并设置 `setCursorWidth(0)` 显式隐藏光标；同时清除所有输出控件（`SmdOutputWidget`）的文本选中，确保进入命令模式后无残留选中高亮。对已渲染的 Markdown 单元格，直接操作隐藏的 `m_markdownEditor` 设置 `setCursorWidth(0)`（因为 `editorWidget()` 返回的是 `RenderPixmapWidget`，不是 `QPlainTextEdit`）。
- 退出命令模式时 `setCommandMode(false)` 恢复 `readOnly=false`、`Qt::TextEditorInteraction`、`Qt::StrongFocus`、`setCursorWidth(1)`。
- `setActive(false)` 时额外设置 `setCursorWidth(0)` 隐藏非活动单元格的光标；`setActive(true)` 时仅在非命令模式下恢复 `setCursorWidth(1)`（命令模式光标由 `setCommandMode` 统一管理）。
- `setEditorFocus()` 设置焦点前先调用 `setCursorWidth(1)` 确保活动单元格光标可见。
- `setRendered(false)` 取消渲染时，若当前为命令模式则保持编辑器只读且光标隐藏，避免非法聚焦。
- `setCellType()` 切换类型重建编辑器后调用 `setCommandMode(m_commandMode)` 确保新编辑器继承当前模式状态（ReadOnly、NoFocus、cursorWidth 等），然后调用 `applyZoom(m_zoomFactor, m_baseFontSize)` 将当前缩放状态应用到新编辑器，保证字体大小与已有 cell 一致。

**主要接口**：
- `bool loadFile(const QString &filePath)` / `bool saveFile()`：文件加载与保存。
- `QString toPlainText() const` / `void setPlainText(const QString &text)`：序列化/反序列化所有单元格内容。
- `bool isModified() const` / `void setModified(bool modified)`：修改状态管理。
- `int cppGroupForCell(int cellIndex) const`：遍历 `m_cells` 中位于 `cellIndex` 之前的 C++ cell，按 `main()` 边界计算所属程序组 ID（与 `SmdLspManager::computeCppGroup()` 算法相同）。供 `executeCodeCell()` 确定同组合并范围。
- `void applyZoom(qreal factor, int baseFontSize)`：存储当前缩放因子和基准字号至 `m_zoomFactor` / `m_baseFontSize`，然后遍历所有单元格调用 `SmdCell::applyZoom()`，确保后续新建的 cell 可继承当前缩放状态。
- `void checkCellRenderWidths()`：遍历所有已渲染单元格调用 `SmdCell::checkReRender()`，在 `resizeEvent` 中延迟执行，确保布局稳定后检测宽度变化。
- `void setEditorFont(const QString &family, int size)`：存储基准字号至 `m_baseFontSize`，遍历所有单元格调用 `SmdCell::applyZoom(1.0, size)`。注意此处 zoom 因子固定为 1.0，实际的缩放因子由 `EditorWidget` 后续调用 `applyZoom()` 重新应用。
- `void reloadColors()`：颜色更新。

**事件处理**：
- 重写 `resizeEvent(QResizeEvent*)`：父类处理完成后，通过 `QTimer::singleShot(0)` 延迟调用 `checkCellRenderWidths()`，在主窗口缩放后对所有已渲染 cell 检查宽度变化并触发防抖重渲染。
- 重写 `keyPressEvent(QKeyEvent*)`：在命令模式下处理快捷键（A/B/Enter/Esc/↑/↓/Ctrl+Shift+Z/Delete）。命令模式下 `Enter`（无修饰键）进入编辑模式，`Ctrl+Enter`/`Shift+Enter` 不再在此处理（仅编辑模式有效，由 `eventFilter` 处理）。
- 重写 `eventFilter(QObject*, QEvent*)`：
  - **滚动抑制**：任何 `MouseButtonPress` 事件（无论是否命中 SmdCell）都会设置 `m_clickSuppressScroll = true` 并启动 50ms 单次定时器清除。这确保点击 toolbar 或空白区域导致焦点恢复至 cell 时不会意外滚动。
  - **Cell 激活**：`FocusIn` / `MouseButtonPress` 事件向上查找父 SmdCell，找到则调用 `setActiveCell(idx)` 激活。`m_clickSuppressScroll` 标志在 setActiveCell 中阻止滚动。
  - 同时处理 `ShortcutOverride` 和 `KeyPress` 事件。
  - `Ctrl+Enter` / `Shift+Enter`：仅编辑模式生效。`ShortcutOverride` 阶段 `event->accept()` 防止被 Qt 快捷键拦截；`KeyPress` 阶段设置 `m_jumpAfterExecute`（Shift 为 true）并调用 `executeCurrentCell()`。
  - `Esc`：`ShortcutOverride` 阶段 `event->accept()`；`KeyPress` 阶段进入命令模式。当 `Qt::Popup` 窗口（如语言选择菜单）激活时跳过拦截，使菜单能正常响应 `Esc`。
  - `Ctrl+K`：`ShortcutOverride` 阶段 `event->accept()`；`KeyPress` 阶段弹出语言选择菜单（命令模式与编辑模式均生效）。
  - `Ctrl+C`：执行期间终止进程（`ShortcutOverride` + `KeyPress`）。
  - `Ctrl+Shift+Z`：`ShortcutOverride` 阶段拦截防止 Qt 转换为 Redo；`KeyPress` 阶段对 MD 单元格取消渲染，对代码单元格清除输出。命令模式放行给 `keyPressEvent`。
  - `Ctrl+Shift+-`：仅编辑模式生效。`ShortcutOverride` 阶段 `event->accept()` 防止被 Qt 缩放快捷键拦截；`KeyPress` 阶段调用 `splitCellAtCursor()` 将当前单元格在光标处一分为二（类型保持不变），选中下方单元格并将光标置于其开头。
  - `Ctrl+D`（`toggle_diagnostics`，可配置）：仅编辑模式。切换 `SmdDiagnosticsPanel` 显示/隐藏。
  - **快捷键作用域守卫**：由于 `eventFilter` 同时安装在 `QApplication`（全局，用于 FocusIn/MousePress 激活 cell）、自身和顶层窗口上，键盘快捷键处理入口添加了 widget 层级检查——仅处理事件目标为本 SmdEditor 后代控件的快捷键，防止从 `CodeEditor` 等其他编辑器控件「窃取」`Ctrl+D` 等快捷键。

**信号**：
- `void modificationChanged(bool modified)`、`void fileLoaded(const QString &filePath)`、`void fileSaved(const QString &filePath)`：转发给 `EditorWidget`。

**内部类 `LangSelectorPopup`**：
- 在 `smdeditor.cpp` 中定义，`Q_OBJECT` 宏，MOC 通过 `#include "smdeditor.moc"` 处理。
- 键盘事件处理：`↑/↓` 导航选项、`Enter` 确认选择、`1/2/3` 快捷键、`Esc` 关闭。
- 点击 `QLabel` 选项通过 `eventFilter` 检测 `MouseButtonRelease` 并确认选择。
- 确认时设置 `m_confirmed = true` 再关闭，避免 `hideEvent` 触发取消回调。
- 重写 `hideEvent`：若未确认且有取消回调则执行取消逻辑（如新建单元格时删除单元格）。**不在此处调用 `deleteLater()`** — `Qt::Popup` 隐式设置 `WA_DeleteOnClose` 会重复触发，且 `hideEvent` 可能被 Qt 多次投递导致 double-delete。
- 构造函数中 `setAttribute(Qt::WA_DeleteOnClose, false)` 禁用自动删除，`confirm()` 通过 `QTimer::singleShot(0, this, [this]() { close(); })` 延迟关闭。延迟避免 `QWidget::close()` 在 `m_onSelected`（`setCellType`）修改控件树后立即遍历焦点链导致崩溃。
- 提供 `setOnSelected()` / `setOnCancelled()` setter，`showLanguageSelector()` 创建 popup 后通过 setter 注入回调（含 popup 指针捕获以支持取消时清理）。

**协作关系**：
- 由 `EditorWidget` 创建并作为 `QStackedWidget` 的第 5 页（索引 5）。
- 被 `EditorWidget` 的 `loadFile()`、`saveFile()`、`toPlainText()`、`applyZoom()` 等方法调用调度。
- 拥有独立的 `ProcessRunner`，通过 `CompilerUtils` 检测编译器。
- 通过 `SmdFormat` 解析和序列化文件格式。
- `.smd` 文件扩展名已在 `config.json` 的 `text_files` 数组中注册，使其可被 `TextFileUtils` 和文件浏览器识别。

---

### 28. `SmdOutputWidget` — SMD 输出控件

**文件**：`smdoutputwidget.h` / `smdoutputwidget.cpp`

**职责**：
- 继承 `QWidget`，位于每个 `SmdCell` 下方，显示代码单元格的执行输出（stdout/stderr）。
- 内部包含一个只读 `QPlainTextEdit`，`NoFocus` 策略确保键盘焦点永不落在此控件上，无蓝色焦点边框，文本仍可鼠标选中复制。
- 深色主题（`#1E1E1E` 背景、`#D4D4D4` 前景、`#264F78` 选区、顶部 `1px solid #3c3c3c` 分隔线），等宽字体 Consolas 10pt。

**自适应高度**：
- `kMaxVisibleLines = 15`：高度随内容行数增长，最多显示 15 行。未达上限时滚动条强制关闭（`ScrollBarAlwaysOff`），超出时启用滚动条（`ScrollBarAsNeeded`）。
- `kMaxOutputLines = 1000`：内容行数上限。超出的行从开头删除，替换为灰色 `[... more lines]` 提示。
- `updateHeight()` 槽连接 `QTextDocument::blockCountChanged`，在 `appendText()` 追加文本时自动触发调整固定高度；`setOutput()` 中通过 `QTimer::singleShot(0, ...)` 延迟调用，避免加载时控件尚未完成布局导致高度计算异常。

**主要接口**：
- `void setOutput(const QString &text)`：加载文件时恢复已保存的输出文本。自动检测文本格式：若以 `<!DOCTYPE HTML` 开头则通过 `QTextDocument::setHtml()` 恢复带颜色的格式化输出；否则视为纯文本（兼容旧文件），按行分割、超行截断后通过 `setPlainText()` 设置。先阻断文档信号防止设置过程中提前触发 `updateHeight()`，设置文本后再解除信号阻塞，显示控件，最后通过 `QTimer::singleShot(0, ...)` 延迟计算高度确保布局就绪。
- `void appendText(const QString &text, bool isStderr = false)`：追加文本到末尾，首次追加时自动显示控件。stderr 以红色 `#F48771` 显示。每次追加后检查行数上限并截断。
- `void clearOutput()`：清空内容并隐藏控件。执行前调用以重置输出区域，控件保持隐藏直至 `appendText()` 收到实际输出。
- `void clearSelection()`：仅清除 `m_outputEdit` 的文本选中状态，不清空内容。用于切换单元格或进入命令模式时移除输出区的选中高亮。
- `QString outputText() const`：返回输出文本的 HTML 格式（保留颜色等字符格式），供序列化保存。输出为空时返回空字符串，避免空输出被持久化到文件。
- `bool hasOutput() const`：是否有输出内容。

**协作关系**：
- 由 `SmdEditor` 创建和管理，与 `SmdCell` 通过 `m_outputWidgets` 列表一一对应。
- 内容通过 `SmdEditor::toPlainText()` 序列化（HTML 格式的 base64 编码存入文件元数据），通过 `SmdEditor::setPlainText()` 恢复。HTML 格式保留了 stderr 红色、截断提示灰色等字符格式信息，使保存后重新打开仍能显示彩色输出。


### 29. `SmdLspManager` — SMD 共享 LSP 管理器

**文件**：`smdlspmanager.h` / `smdlspmanager.cpp`

**职责**：
- 继承 `QObject`，为 SMD 文件每种编程语言管理一个共享 LSP 后端。每个 SMD 文件仅启动 1 个 clangd（C++）+ 1 个 Jedi（Python），而非每 cell 一个。
- 通过 **虚拟文档** 技术将同语言所有 cell 拼接为一个有效源文件，实现跨 cell 类型解析。
- 管理 cell 本地位置 ↔ 虚拟文档位置的 **行号映射**。
- 处理 `textDocument/publishDiagnostics` 通知，将诊断信息分发到各 cell。
- 为每个 cell 创建 `CellCompletionAdapter`，实现 `CompletionProvider` 接口，供 CodeEditor 作为共享 Provider 使用。

**虚拟文档策略**：
- 同语言所有 cell 按 cellOrder 顺序拼接，每个 cell 内容前插入一行注释分隔符（C++ 用 `// --- smd:cell:N ---`，Python 用 `# --- smd:cell:N ---`）。
- **C++ 程序分组**：`computeCppGroup()` 按 `main()` 函数边界将 C++ cell 划分为程序组（`m_activeCppGroup`）。`rebuildVirtualDoc()` 和 `buildVirtualDocContent()` 仅向 clangd 发送当前活动组的 cell，确保 clangd 每份虚拟文档最多含一个 `main()`。默认活动组为 0（首个程序组）。
- clangd/Jedi 将虚拟文档视为一个翻译单元，同组内 cell 0 定义的类型在 cell 1 中可解析。
- clangd 使用全量文本同步（`textDocument/didChange` 始终发送完整虚拟文档）。

**行号映射**：
- `LanguageServer::cellRanges[cellIndex] = {firstVirtualLine, localLineCount}`：记录每个 cell 在虚拟文档中的起始行和本地行数。
- 补全/悬停请求时：cell 本地 (line, col) → 虚拟 (firstVirtualLine + line, col)。
- C++ 诊断反向映射：虚拟 line → 查找所在 cell → 本地 line = virtualLine - firstVirtualLine。`processDiagnostics()` 仅清除同语言 cell 的过期诊断（`srv->cellRanges.contains(ci)`），防止 C++ 诊断清除 Python 诊断（反之亦然）。
- Python 诊断**不使用虚拟文档**：每 cell 代码独立编译，返回 cell 本地行号，直接发射 `diagnosticsUpdated`。

**核心数据结构**：
- `SmdDiagnostic`：诊断信息结构体（cellIndex、startLine/Col、endLine/Col、message、severity 1=错误,2=警告,3=信息,4=提示）。已从 `smdlspmanager.h` 移至独立的 `smddiagnostic.h`，供所有诊断生产者/消费者（SmdLspManager、CppCompletionProvider、PythonCompletionProvider、CodeEditor、BottomPanel、SmdDiagnosticsPanel）共享。
- `LanguageServer`：单语言 LSP 后端状态（`LspClient*`、初始化标志、虚拟文档 URI、cellRanges 映射表、cellOrder 顺序列表、请求 ID 跟踪）。
- `m_activeCppGroup`：当前活动 C++ 程序组 ID（0 起始），虚拟文档仅向 clangd 发送该组的 cell。
- `m_groupDiagnostics`：两级 Map（groupId → cellIndex → diagnostics），缓存各组诊断结果，支持切换 cell 时即时恢复，避免闪烁。

**内部类 `CellCompletionAdapter`**：
- 继承 `CompletionProvider`，持有 `SmdLspManager*` + `cellIndex`。
- `requestCompletion/Hover/SignatureHelp(text, cursorPos)` 将绝对 cursorPos 转换为 cell 本地 (line, col)，委托给 `SmdLspManager::requestCompletion/Hover/SignatureHelp(cellIndex, line, col)`。
- SmdLspManager 构造函数中连接 `completionReadyForCell`/`hoverReadyForCell`/`signatureHelpReadyForCell` 信号到对应 adapter 的标准 `CompletionProvider` 信号。

**C++ LSP 后端**（clangd）：
- `startCppServer()`：查找 clangd → 创建 `LspClient` → 连接信号 → `--fallback-style=Google` 参数启动。
- `sendInitialize()`：发送 `initialize` 请求（JSON-RPC）。
- 初始化完成后发送 `textDocument/didOpen` 通知（完整虚拟文档），之后每次 cell 内容变化通过 `syncVirtualDoc()` 发送 `textDocument/didChange`。
- `onCppResponseReceived()`：分发 initialize/completion/hover/signatureHelp 响应。
- `onCppNotificationReceived()`：处理 `textDocument/publishDiagnostics` → `processDiagnostics()` 翻译行号 → `diagnosticsUpdated` 信号。
- 崩溃自动重启（1s 延迟的 `QTimer::singleShot`）。

**Python 后端**（Jedi + 诊断）：
- `startPythonProcess()`：查找 Python → 启动 `completion_helper.py` 子进程（MergedChannels）。进程启动后若已有 Python cell 则自动触发初次诊断请求。
- 补全/悬停/签名请求：将完整 Python 虚拟文档作为 `code` 字段发送，cursor 位置转换为虚拟文档 (line, col)。响应分发至 `completionReadyForCell`/`hoverReadyForCell`/`signatureHelpReadyForCell`。
- **Python 诊断**（新增）：
  - `m_pyDiagnosticsTimer`（500ms 单次定时器）：在 `cellAdded`、`cellContentChanged`、Python 进程 `started` 后启动，防抖触发 `requestPythonDiagnostics()`。
  - `requestPythonDiagnostics()`：遍历 `m_pyServer.cellOrder`，对每个 Python cell 的代码调用 `sanitizeForPython()`（规范化 `\r\n`/`\r` → `\n`、替换孤立 surrogate），通过 **base64 编码**发送 `{"action":"diagnostics","cells":[{cellIndex,code}]}` 以避免 JSON 换行转义问题。
  - `completion_helper.py` 的 `handle_diagnostics(cells)` 对每个 cell 代码独立调用 `compile()`，返回 cell 本地行号（0-based）的诊断列表，无需虚拟文档坐标映射。
  - 响应在 `processPythonResponse()` 的 `PyPending::Diagnostics` 分支直接构建 `SmdDiagnostic` 并发射 `diagnosticsUpdated`，绕过 `processDiagnostics()`（后者为虚拟文档坐标映射设计）。过期诊断仅清除同语言（`m_pyCellContents.contains(ci)`）cell 的条目。
  - `PyPending` 枚举新增 `Diagnostics` 值；超时/错误时不主动清除诊断（静默忽略）。

**主要接口**：
- `void initialize(const QString &smdFilePath)`：基于 SMD 文件名生成虚拟文档 URI。
- `void cellAdded/cellRemoved/cellContentChanged/cellTypeChanged(cellIndex, langId, content)`：维护 cell 缓存和虚拟文档，延迟启动 LSP 后端。`cellAdded()` 在插入 `cellOrder` 前检查 `contains()` 护盾，防止重复信号连接导致同一 cell 重复插入；`cellRemoved()` 删除共享 adapter（`CellCompletionAdapter`）并从 `cellOrder` 中移除。
- `void focusCell(int cellIndex)`：程序组切换入口。计算 cell 所属组，若与当前活动组不同则保存当前诊断至 `m_groupDiagnostics` 缓存、清除旧诊断、恢复新组缓存诊断、重建虚拟文档并同步至 clangd。
- `int computeCppGroup(int cellIndex) const`：遍历 `cellOrder` 中位于 `cellIndex` 之前的 C++ cell，通过正则 `\bmain\s*\(` 计数 `main()` 函数出现次数，返回 cell 所属程序组 ID。
- `void requestCompletion/Hover/SignatureHelp(int cellIndex, int cursorLine, int cursorCol)`：公共请求 API，cell 本地位置 → 虚拟位置 → LSP 请求。
- `CompletionProvider *providerForCell(int cellIndex, const QString &langId)`：返回 cell 对应的 CellCompletionAdapter。
- `QList<SmdDiagnostic> diagnosticsForCell(int cellIndex) const`：获取 cell 的诊断列表。
- `void shutdown()`：安全关闭（设置 `m_shuttingDown` 标志、断开信号、停止 LSP 进程、删除 adapter、重置活动组和诊断缓存）。

**信号**：
- `diagnosticsUpdated(int cellIndex, QList<SmdDiagnostic>)`：SMD 诊断更新，由 SmdEditor 连接至 cell 的 CodeEditor 和 SmdCell。

**独立文件诊断**（`.cpp`/`.py`，非 SMD）：
- `CompletionProvider` 基类新增 `diagnosticsUpdated(QList<SmdDiagnostic>)` 信号（`completionprovider.h`），统一所有 provider 的诊断接口。
- **C++ 独立文件**（`CppCompletionProvider`）：`onNotificationReceived()` 处理 clangd 的 `textDocument/publishDiagnostics` 通知，`parseDiagnostics()` 将 LSP 诊断 JSON 转换为 `SmdDiagnostic` 列表（`cellIndex = 0` 表示平面文件），emit `diagnosticsUpdated`。
- **Python 独立文件**（`PythonCompletionProvider`）：新增 `sendDiagnosticsRequest()`，将文件全文 base64 编码后发送至 Jedi helper 的 `diagnostics` action。`openDocument()` 立即请求诊断，`updateText()` 通过 500ms 防抖定时器延迟请求。`processResponse()` 解析诊断列表并 emit `diagnosticsUpdated`。
- `CodeEditor` 构造函数连接 `provider->diagnosticsUpdated` → `CodeEditor::setDiagnostics()`，存储诊断于 `m_diagnostics` 并绘制波浪线。
- `MainWindow` 在 `currentChanged` 中连接 `provider->diagnosticsUpdated` → `BottomPanel::setDiagnostics()`，使 BottomPanel 诊断标签页自动同步。
- `completionReadyForCell/hoverReadyForCell/signatureHelpReadyForCell`：LSP 响应就绪（内部使用，转发至 adapter）。
- `serverReady/serverFailed(langId, reason)`：LSP 后端状态通知。

**协作关系**：
- 由 `SmdEditor` 创建、持有和管理（`m_lspManager`）。
- `SmdEditor::setActiveCell()` 调用 `focusCell()` 切换程序组；`contentChanged`/`addCell`/`removeCell` 后调用 `focusCell()` 重新检测组边界。
- `SmdEditor::connectCellSignals()` 调用 `providerForCell()` 获取 adapter 并注入 CodeEditor。
- `SmdEditor::executeCodeCell()` 调用自身的 `cppGroupForCell()`（与 `computeCppGroup` 算法相同但遍历 `m_cells`）确定合并范围。
- 引用 `LspClient`（C++ 端）和 `QProcess`（Python 端）管理 LSP 进程。


### 30. `SmdDiagnosticsPanel` — SMD 诊断面板

**文件**：`smddiagnosticspanel.h` / `smddiagnosticspanel.cpp`

**职责**：
- 继承 `QFrame`，作为 SMD 编辑器的诊断信息汇总面板，以 `Ctrl+D`（编辑模式）切换显示。嵌入 `SmdEditor` 的 `m_splitter` 中，默认隐藏。
- `DiagnosticSection` 内部类（复用于 `BottomPanel`）：继承 `QWidget`，带标题和边框色（错误 `#F44747` / 警告 `#CCA700`）。`setDiagnostics()` 过滤指定 severity 的诊断，按行号排序后填充可点击条目。每条诊断显示行号和消息，`lineClicked(cellIndex, line)` 信号触发 `SmdEditor` 跳转。`count()` 返回当前条目数。支持展开/折叠（`setExpanded()`），头部显示 ▾/▸ + 标题 + 计数。
- `refresh()`：从 `SmdLspManager` 获取当前所有 cell 的诊断列表，按 severity 分组填充两个 `DiagnosticSection`。通过 `scheduleRefresh()` + 50ms 定时器防抖更新。
- 关闭按钮（✕）点击隐藏面板。

**与 BottomPanel 的关系**：
- `SmdDiagnosticsPanel` 仅用于 SMD 编辑器（cell 粒度诊断）。
- `BottomPanel` 的「诊断」标签页用于独立代码文件（`.cpp`/`.py` 平面文件诊断）。
- 两者共享 `SmdDiagnostic` 数据结构和 `DiagnosticSection` UI 组件。

---

### 31. `ScrollbarHider` — 滚动条自动隐藏控制器

**文件**：`scrollbarhider.h` / `scrollbarhider.cpp`

**职责**：
- 所有可滚动区域（`QAbstractScrollArea` 子类，含 `QPlainTextEdit`/`QTextEdit`/`QTreeView`/`QListWidget`/`QScrollArea`/`QPdfView` 等）的滚动条统一样式与自动隐藏控制。
- 在 `MainWindow` 构造完成后自动扫描所有现有可滚动区域并注册管理；通过 `QTabWidget::currentChanged` 信号动态识别新打开的标签页（如 PDF 视图），自动将其内部的滚动区域纳入管理。

**自动隐藏行为**：
- **鼠标进入区域**：立即显示滚动条（`#555555` 纯色手柄、`#1E1E1E` 轨道）。
- **鼠标离开区域**：启动 150ms 延迟计时器，期间鼠标回到区域内则取消隐藏；超时后滚动条渐隐（手柄和轨道均变为 `transparent`）。
- 同时监控视口（viewport）、垂直滚动条和水平滚动条的 `Enter`/`Leave` 事件，确保在视口与滚动条之间移动时不会闪动。

**统一样式**（所有面板一致）：
- 垂直滚动条：10px 宽，5px 圆角，`#555555` 手柄，`#1E1E1E` 轨道。
- 水平滚动条：10px 高，5px 圆角，`#555555` 手柄，`#1E1E1E` 轨道。
- 无 `:hover` 宽窄变化，始终统一粗细。
- 隐藏时滚动条占位保留，不触发布局重排。

**协作关系**：
- 由 `MainWindow` 创建并持有（`QObject` 父子关系）。
- `MainWindow` 在构造末尾调用 `hider->manage(area)` 注册所有现有可滚动区域。
- `MainWindow` 通过 `QTabWidget::currentChanged` 信号注册新标签页内部的滚动区域。
- 被管理的区域销毁时自动清理（`QObject::destroyed` 连接），避免悬空指针。


### 32. `HelpPanel` — 帮助面板

**文件**：`helppanel.h` / `helppanel.cpp`

**职责**：
- 悬浮式帮助面板 `QWidget`，以半透明遮罩层 + 居中面板的方式覆盖在主窗口上方，与 `SettingsPanel` 类似但更简洁。
- 面板为无边框深色 `QWidget`（背景 `#2b2b2b`、圆角 8px、边框 `#555555`），初始尺寸 880×620。
- 标题栏（36px 高）：左侧 "帮助" 标签（`#cccccc`，13px 粗体），右侧关闭按钮（`✕`，悬停变红色 `#c42b1c`）。
- 支持标题栏拖拽移动，移动范围限制在遮罩层内，不可缩放。

**布局结构**：
- 左侧 `QListWidget`（170px 宽，深色 `#252525`），列出 14 个模块分类作为导航索引。
- 右侧 `QTextBrowser` 加载内置 HTML 资源（`:/help/content`），深色主题（背景 `#1e1e1e`、前景 `#d4d4d4`），支持外部链接跳转。
- 每个 `QListWidgetItem` 通过 `Qt::UserRole` 存储 section ID，选中时调用 `scrollToAnchor()` 跳转到 HTML 中对应 `<div id="...">` 位置。

**滚动同步机制**：
- 首次显示（`showEvent`）时调用 `computeSectionPositions()`：遍历所有 section 的 `matchText`，通过 `QTextDocument::find()` 定位文本块，使用 `documentLayout()->blockBoundingRect().y()` 获取每个章节在文档中的精确像素 Y 坐标。
- 滚动时（`onScrollChanged`）：将滚动条的 pixel value 与各章节的 Y 坐标比较，选中最接近当前滚动位置的章节。通过 `m_updatingCategory` 布尔标志防止类别选择和滚动同步之间的循环触发。
- 点击左侧分类列表时：通过 `scrollToAnchor()` 滚动到对应章节，滚动过程中不会触发类别切换。

**主要接口**：
- `void loadContent()`：从 `:/help/content` 加载 HTML 资源到 QTextBrowser。
- `void computeSectionPositions()`：使用 `QTextDocument::find()` 匹配每个 section 的 `matchText`，记录每个章节在文档中的像素 Y 坐标。
- `void onScrollChanged(int value)`：根据滚动条的 pixel value 自动选中对应分类。

**信号**：
- `void closeRequested()`：用户点击关闭按钮时发出。

**协作关系**：
- 由 `MainWindow` 创建并持有（`m_helpPanel`），父控件为遮罩层 `m_helpOverlay`。
- 工具栏帮助按钮和 `F1` 快捷键统一调用 `MainWindow::toggleHelp()` 切换显示/隐藏。
- `MainWindow::resizeEvent()` 中同步遮罩层尺寸和面板位置。
- 全局事件过滤器：点击遮罩层外部区域自动关闭帮助面板。


### 33. `ThemeManager` — 主题管理器

**文件**：`thememanager.h` / `thememanager.cpp`

**职责**：
- 单例 `QObject`，管理 VS Code 2026 Dark/Light 双主题。内置 `QMap<QString, QColor>` 调色板覆盖所有 UI 组件（editor, panel, activitybar, scrollbar, input, button, tab, overlay, treeview, separator, labels, accents, close button, slider, toggle, cell, chat, titlebar, menu, statusbar 等 20+ 分类）。
- 主题枚举：`Dark=0, Light=1, System=2`。`setTheme(System)` 自动检测系统主题：优先读取 Windows 注册表 `AppsUseLightTheme`，兜底根据当前时间（6:00-18:00 → Light，其余 → Dark）判断。
- System 模式下 5 分钟 `QTimer` 自动刷新系统主题检测。
- `themeChanged(Theme)` 信号在主题实际变化时发出；`applyCurrentTheme()` 重新发射信号强制所有组件刷新。
- 语义化颜色访问：`color("editor.background")` 返回 `QColor`，`hex("panel.border")` 返回 `"#RRGGBB"` 字符串。缺失 key 返回品红色 `#FF00FF` 方便开发时发现遗漏。

**主要接口**：
- `static ThemeManager &instance()`：单例访问。
- `void setTheme(Theme theme)`：切换主题，System 模式自动检测。
- `Theme currentTheme() const` / `Theme requestedTheme() const`：当前生效主题和用户请求模式。
- `void refreshSystemTheme()`：System 模式下重新检测。
- `QColor color(const QString &key) const` / `QString hex(const QString &key) const`：语义化颜色查询。
- `bool applyCurrentTheme()`：重新发射 `themeChanged` 信号。

**信号**：
- `themeChanged(ThemeManager::Theme newTheme)`：主题变更时发出，各组件在槽中重新应用 QSS/Palette。

**协作关系**：
- 由 `MainWindow` 在构造时调用 `ThemeManager::instance().setTheme(...)` 初始化。
- `MainWindow::onAppearanceSettingChanged("theme", val)` 中调用 `setTheme(static_cast<Theme>(val))`。
- `ConfigManager` 提供 `theme` 默认值（0=Dark）。
- 各 widget 的 `setupStyles()`/`reloadColors()` 连接 `themeChanged` 信号实现响应。


### 34. `CompilerErrorParser` — 编译器/Python 错误解析器

**文件**：`compilererrorparser.h`

**职责**：
- 头文件（header-only）工具命名空间 `CompilerErrorParser`，将编译器 stderr 输出和 Python traceback 解析为 `QList<SmdDiagnostic>` 结构化诊断数据。
- 解析完成后 `cellIndex` 设为 `blockIndex`（对应 MD 文件中代码块的序号），`startLine` 为 0-based。

**接口**：
- `QList<SmdDiagnostic> parseCompileErrors(const QString &stderrText, int blockIndex)`：解析 g++ 格式（`file:line:col: error: message`）和 MSVC 格式（`file(line,col): error Cxxxx: message`）。跳过 `note:` 补充行。severity: 1=Error, 2=Warning。
- `QList<SmdDiagnostic> parsePythonTraceback(const QString &stderrText, int blockIndex)`：提取最后一个 `File "...", line N` 位置和最终的异常类型+消息。无结构化行信息时使用第 0 行 + 全文消息。

**协作关系**：
- 被 `MainWindow::parseAndShowBlockDiagnostics()` 调用，根据 `m_currentBlockLanguage`（`"cpp"` / `"python"`）选择对应的解析函数。


### 35. MD 代码块诊断（Preview Code Block Diagnostics）

**概述**：
- MD 文件预览模式下，点击代码块的 ▶ Run 按钮运行代码后，底部面板诊断标签页显示当前代码块的编译器/运行时错误诊断。预览中的代码块对应行通过 JS 注入红色/黄色波浪线（与 `CodeEditor` 的 `WaveUnderline` 样式一致）。
- 诊断信息来源于 `CompilerErrorParser` 对 stderr 的解析。每次运行新代码块时立即清空旧诊断，运行结束后（编译失败或运行结束）重新解析并显示。
- 用户手动终止（Ctrl+C / Stop 按钮）时跳过诊断解析，避免"进程异常退出"等进程级消息被错误识别为代码错误。

**数据流**：
```
用户点击 ▶ Run
  → JS: _runCodeData = {lang, code, blockIndex}
  → PreviewPage::acceptNavigationRequest("runblock:")
  → EditorWidget::runCodeBlockRequested(lang, code, blockIndex)
  → MainWindow::onCodeBlockRequested(lang, code, blockIndex)
    ├─ 记录 m_currentMdFilePath / m_currentBlockIndexMd / m_currentBlockLanguage
    ├─ 立即: clearBlockDiagnostics() + BottomPanel::clearDiagnostics()
    ├─ 保存临时文件 → ProcessRunner 编译/运行
    └─ 缓冲 stderr (m_stderrBufferConnection: outputReceived → m_mdStderrBuffer)
  → onCompileFinished(false) 或 onRunFinished(exitCode)
    → parseAndShowBlockDiagnostics()
      ├─ 跳过: 若 m_processManuallyStopped == true（手动终止）
      ├─ CompilerErrorParser 解析 m_mdStderrBuffer
      ├─ 存储: m_mdDiagnostics[filePath][blockIndex] = diags
      ├─ m_bottomPanel->setDiagnostics(diags)
      └─ editor->applyBlockDiagnostics({blockIndex: diags})
        → JS: window.applyBlockDiagnostics(blockDiagnostics)
        → 查找 .code-block-wrapper[data-block-index="N"]
        → 按换行拆分 <code> 为 <span class="code-line" data-line="N">
        → 匹配行添加 .diagnostic-error-line / .diagnostic-warning-line + title
```

**预览刷新时的诊断保持**：
- `EditorWidget` 在 `applyBlockDiagnostics()` 时通过 `extractCodeBlockContents()` 快照各代码块的当前文本内容（`m_blockDiagnosticCode`）。
- 预览因编辑而刷新时（`updatePreviewContent()` / `updateSplitPreviewContentNow()` 回调），提取新的代码块内容，仅对内容未变的代码块重新注入诊断波浪线。内容已变的代码块的诊断自动清除，避免波浪线错位到错误行号。
- 用户可重新点击 Run 获取基于最新代码内容的诊断。

**blockIndex 机制**：
- `preHighlightCodeBlocks()` 在处理围栏代码块时为每个已知语言代码块的 `.code-block-wrapper` 添加 `data-block-index="N"` 属性（N 从 0 递增，只计已知语言/可运行代码块）。
- Run 按钮同时携带 `data-block-index="N"`，通过 JS click 事件提取并传入 `_runCodeData`。

**底部面板行为**：
- **MD 文件内**：运行代码块时自动显示底部面板（Output 标签），用户可手动切换到诊断标签页查看诊断。无快捷键支持（`toggleDiagnosticsInCodeEditor()` 仅对 `isCodeEdit()` 生效）。
- **切换到其他文件**：`currentChanged` 中若新文件非代码文件且非 `.md`，自动隐藏底部面板；若为 `.md` 文件，调用 `loadMdDiagnosticsForCurrentTab()` 加载该文件上次运行的代码块诊断（或清除）。
- **诊断存储**：`MainWindow::m_mdDiagnostics[filePath][blockIndex]` 按文件路径和代码块索引存储，`m_lastRunBlockIndexMd[filePath]` 追踪每文件最后运行的代码块。

**相关文件**：
- `compilererrorparser.h`（新增）：编译器/Python 错误解析器
- `preview-template.html`（修改）：CSS 波浪线样式 + JS `applyBlockDiagnostics`/`clearBlockDiagnostics` + blockIndex 传递
- `mainwindow.h/.cpp`（修改）：`m_mdDiagnostics`/`m_isRunningCodeBlock`/`m_processManuallyStopped` 等状态，`parseAndShowBlockDiagnostics()`/`loadMdDiagnosticsForCurrentTab()` 方法，`currentChanged` 标签切换 MD 逻辑
- `editorwidget.h/.cpp`（修改）：`runCodeBlockRequested(lang, code, blockIndex)` 信号，`applyBlockDiagnostics()`/`clearBlockDiagnostics()`/`extractCodeBlockContents()` 方法


### 配置存储说明

采用双配置系统，职责分离：

**静态配置 `config.json`**（`ConfigManager` 单例）：
- 存放颜色主题、字体、编辑器/面板尺寸、编译器参数、评测限制、OpenJudge URL/超时、快捷键、文件扩展名列表等所有可配置的静态值。
- `config.json` 缺失时所有接口返回内置默认值，行为完全不变。
- 搜索路径：当前工作目录 → 可执行文件所在目录。
- 通过 `ConfigManager::instance().xxx()` 类型安全访问。

**会话配置 `config.ini`**（`SettingsManager`）：
- 存放窗口几何、分割条状态、最近文件列表（`History/recentFiles`，最多 50 条，运行期间内存维护，关闭时统一写入）、最后打开/另存为目录、OpenJudge 自动登录凭据（Base64 混淆）。
- 使用 `QSettings(IniFormat)` 持久化在可执行文件目录。

两者共存互补，互不冲突。`config.json` 可在项目根目录直接编辑，无需复制到 release 目录。

### 界面定制细节

- **标签页样式**：通过 `QTabWidget` 的样式表设置了标签最小高度、左右内边距（`padding: 4px 12px`）、圆角以及选中/悬停背景色，解决了标签左右空位过小的问题。
- **保存提示对话框**：使用 `QMessageBox` 并设置自定义按钮文字（"保存(&S)"、"不保存(&D)"、"取消(&C)"），提示文本中包含当前文件名，且通过样式表设置最小尺寸（400×200 像素）。
- **Markdown 预览模式**：在工具栏添加了"预览模式"按钮（快捷键 `Ctrl+Shift+P`）和"分屏预览"按钮（快捷键 `Ctrl+P`），**两个按钮仅在当前编辑的文件为 `.md` 后缀时可见且可用**。切换到其他类型文件（包括代码文件）或没有标签页时，按钮自动隐藏，对应的快捷键同时失效。全屏预览与分屏预览互斥——开启一个自动关闭另一个，切换标签页时记忆各自的预览状态。
  如果当前正处在预览模式但切换到了一个非 `.md` 文件，编辑器会自动退回到源码编辑模式。预览引擎采用延迟初始化避免文件打开时抖动，暗色容器 + 页面背景色双重保障消除切换白屏闪烁，base64 + TextDecoder 确保中文内容正确显示。分屏预览复用同一套预处理管线和信号转发逻辑。
- **缩放控件**：在状态栏底部右侧放置缩小按钮（`−`）、百分比标签（如 `100%`）、放大按钮（`+`）和重置按钮，同时支持快捷键 `Ctrl+=`、`Ctrl+-` 和 `Ctrl+0`。百分比标签随当前编辑器的缩放因子实时更新，且当前编辑器的缩放变化会触发该标签刷新。重置按钮和 `Ctrl+0` 快捷键将缩放恢复至设置面板中配置的默认缩放比例（通过 `SettingsManager::editorDefaultZoom()` 获取），而非固定 100%。
- **文件树右键菜单**：通过 `QMenu` 动态构建。在文件夹或空白处可内联新建文件/文件夹，新建后立即进入命名编辑状态；对已有项目支持重命名（内联编辑）和删除。删除前弹出确认对话框。
- **面包屑路径栏**：位于文件树顶部，深色背景（`#252525`），底部有 1px 分割线（`#3c3c3c`）。按钮扁平无边框，当前目录白色、祖先目录灰色（`#b0b0b0`），悬停时背景变为 `#3c3c3c`。段之间用灰色 `>` 标签（`#858585`）分隔。使用 `FlowLayout` 实现自动换行。
- **文件树工具栏**：面包屑下方，左侧显示当前文件夹名称（过长时自动省略），右侧放置收起所有文件夹按钮（方框内减号图标）和刷新按钮。收起按钮调用 `collapseAll()` 一键收起所有已展开的目录；刷新按钮调用 `refreshTree()` 重新加载文件树。
- **预览标签页**：单击文件树中的文件以预览模式打开（临时标签页，斜体标题）。多次单击不同文件复用同一标签页（通过 `loadFile` 替换内容）。双击文件永久打开（调用 `openFile`，若文件已在预览标签页中则自动提升）。编辑预览标签页内容后 `modificationChanged(true)` 信号触发 `promotePreviewToPermanent()` 自动提升为永久标签页。`CustomTabBar::paintEvent` 为预览标签页应用斜体字体，`tabSizeHint` 为斜体文字预留额外 8px 宽度避免裁剪。
- **排序规则**：文件树始终按”文件夹优先、名称升序”排列，且新建或重命名后实时重排。`FileSortProxyModel` 内聚图标有效性检测与兜底修复（成员 `QFileIconProvider` 缓存 + 惰性图标），避免 Windows 空心图标和重复 SHGetFileInfo 调用。
- **滚动性能**：`setUniformRowHeights(true)` 配合固定行高样式表，Qt 以 O(1) 乘法计算行位置替代 O(n) 逐行高度查询。
- **文件树选中同步**：切换标签页时，文件树自动选中当前编辑的文件，并逐级展开折叠的父目录；新建未保存文件或文件不在当前根目录时，文件树选中状态保持不变。
- **删除确认对话框**：删除前弹出 `QMessageBox::question`，根据是否存在未保存修改提供差异化提示文本。
- **标签拖拽限制**：拖动标签重排时，被拖动的标签整体始终保持在标签栏区域内，不会出现标签部分或全部移出栏外的情况。
- **历史记录面板**：通过 `QDockWidget` 嵌入窗口右侧，默认隐藏（快捷键 `Ctrl+H`）。列表项设置为不可选中（`NoSelection`），点击可触发打开文件操作（若文件不存在则自动弹出警告并清理该条目）。鼠标悬停会有完整路径提示。同时提供清空功能。文件删除或移动后自动同步更新历史记录。运行期间仅维护内存数据，程序关闭时统一持久化以减少磁盘 I/O。点击编辑器、文件树等其他区域时，面板自动收起，减少手动操作。
- **反向链接面板**：通过 `QDockWidget` 嵌入窗口右侧，默认隐藏（快捷键 `Ctrl+Shift+B`）。列表项不可选中（`NoSelection`），点击可跳转至来源文件。反链为空时显示灰色占位文本"无反向链接"，面板宽度通过 `setMinimumWidth(200)` 保持稳定。与历史记录面板共享同一外部点击自动隐藏逻辑。面板标题固定为"反向链接"，不显示数字计数以保持简洁。
- **搜索面板**：通过 `QDockWidget` 嵌入窗口左侧，默认隐藏（快捷键 `Ctrl+Shift+F`）。搜索输入框带清除按钮，输入后 300ms 自动触发搜索。结果列表每项包含文件名（粗体，显示行号）和灰色上下文片段。点击结果跳转至文件并金色高亮所有匹配关键词。面板显示时自动聚焦输入框；不实现点击外部自动隐藏，方便多次点击结果。
- **大纲面板**：通过 `QDockWidget` 嵌入窗口右侧，默认隐藏（快捷键 `Ctrl+Shift+O`）。打开 `.md` 文件时自动解析并展示所有标题（`#` ~ `######`），按层级缩进显示，h1 最亮 h6 逐级变暗。点击标题跳转到编辑器对应行。跳过围栏代码块。切换标签页和保存文件时自动刷新。非 Markdown 文件时面板清空。点击面板外部自动隐藏。
- **编译运行输出面板（BottomPanel）**：`BottomPanel` 嵌入右侧垂直分割区（`m_rightSplitter`），置于编辑器下方，默认隐藏。包含两个标签页——「输出」（`OutputPanel`，编译/运行输出和 stdin 交互）和「诊断」（代码诊断列表，按错误/警告分区展示）。切换标签页时自动管理 provider 连接：代码文件连接 LSP provider，`.md` 文件加载缓存代码块诊断，其他文件自动隐藏。输出面板在首次编译/运行时自动显示（运行标签页）。深色终端风格只读文本区域，stdout 白色、stderr 红色。标题栏包含标签页按钮（输出/诊断）+ ✕ 关闭按钮。底部工具栏包含状态标签、终止、清除按钮。运行期间通过关闭按钮关闭面板时自动终止进程。MD 文件代码块运行支持：每次点击 Run 按钮立即清空旧诊断，运行结束后解析编译/运行时错误（通过 `CompilerErrorParser`）更新诊断标签页，同时在预览代码块中通过 JS 注入红色/黄色波浪线；手动终止时不解析诊断。
- **评测面板**：通过 `QDockWidget` 嵌入窗口右侧，默认隐藏（快捷键 `Ctrl+Shift+J`）。评测开始前需选择测试用例文件夹，评测过程中实时更新每个用例的状态。评测面板在启动评测时自动显示，评测完成后保持可见（不自动隐藏），方便用户查看结果。点击失败行可在详情区查看预期输出与实际输出对比。
- **滚动条统一样式与自动隐藏**：所有可滚动面板（文件树、编辑器编辑区、PDF 视图、BottomPanel 输出/诊断面板、搜索/评测/历史/大纲/标签/反链面板、AI 助手对话区、设置面板、SMD 诊断面板等）使用完全一致的滚动条样式——垂直 10px 宽、水平 10px 高、5px 圆角、始终 `#555555` 手柄，无 `:hover` 变细行为。通过 `ScrollbarHider`（`QAbstractScrollArea` 通用事件过滤器 + 150ms 延迟计时器）实现鼠标进入区域时立即显示、离开区域后自动隐藏的效果，滚动条占位始终保留，不触发布局重排。
- **代码编辑器主题**：代码编辑模式采用深色开发风格主题——编辑区背景 `#1E1E1E`、前景 `#D4D4D4`，行号区背景 `#252525`、数字 `#858585`，当前行高亮 `#2A2D2E`，Consolas 12pt 等宽字体。括号补全、自动缩进、智能退格等行为由 `CodeEditor` 统一管理，受 `m_indentWidth`（默认 4 空格）控制。
- **拖拽移动视觉反馈**：当用户在文件树中拖拽文件经过文件夹时，目标文件夹底部会显示一条 3 像素高的蓝色指示条（颜色 `#2196F3`），拖拽离开或释放鼠标后消失。
- **设置面板**：通过工具栏"设置"按钮或快捷键 `Ctrl+,` 打开/关闭。遮罩层为独立顶层 `Qt::Tool` 窗口（`OverlayWidget`），通过 `WA_TranslucentBackground` + `paintEvent` 绘制 `QColor(0,0,0,128)` 实现半透明背景变暗效果，由 Windows DWM 逐像素 alpha 合成。面板居中显示，深色主题（背景 `#2b2b2b`、边框 `#555555`、圆角 8px），标题栏背景 `#333333`。Obsidian 风格分类侧边栏：左侧 6 个分类（编辑器/外观/输出面板/预览/搜索/快捷键）+ 底部"恢复默认设置"按钮（确认后清除所有覆盖值），右侧对应设置页面。编辑器字体、缩进宽度、外观颜色、输出面板字号、预览参数、搜索限制等配置项均实时应用，自动持久化至 `config.ini`（v0.5.4 起采用内存缓冲 + 关闭时统一写入，避免频繁 I/O）。新打开文件继承已有设置值。所有 QSpinBox 上下按钮和 QComboBox 下拉按钮使用内嵌 SVG 三角形图标渲染。八方向边缘缩放已禁用（`kEdgeMargin = 0`）。关闭后通过 `refreshPreviewTheme()` 刷新预览主题。
- **帮助面板**：通过工具栏帮助按钮或快捷键 `F1` 打开/关闭。与设置面板相同，使用 `OverlayWidget` 顶层遮罩层 + 居中面板模式。`resizeEvent()` 和 `moveEvent()` 中通过 `positionOverlay()` 跟踪 overlay 位置，点击 overlay 背景自动关闭，仅支持标题栏拖拽移动，不可缩放。
