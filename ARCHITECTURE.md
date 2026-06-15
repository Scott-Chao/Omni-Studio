# Architecture

## Component Map

```
main.cpp                  → QApplication + MainWindow bootstrap
core/mainwindow.*         → orchestrator: owns all widgets, routes signals/slots — frameless window with toolbar-as-title-bar (Win/Lin) or native title bar with traffic light buttons (macOS); window drag/resize via nativeEvent & event(); macOS QMenuBar replaces toolbar file button and window controls; right panel auto-hide via QApplication::focusChanged (no global event filter). rightSplitter has 2 items (TabManager + BottomPanel — terminal panel merged into BottomPanel as a tab).
  ├── core/macoswindow.*      → macOS native window bridge (Obj-C++ via AppKit). `enableFullSizeContentView()`: `setTitlebarAppearsTransparent:YES`, `NSWindowStyleMaskFullSizeContentView`, `NSWindowTitleHidden`. Keeps native traffic light buttons while custom toolbar renders behind them. Invoked via `QTimer::singleShot(0)` in `showEvent` to ensure native NSWindow exists.
  ├── ai/airequesthandler.*    → AI request lifecycle (provider management, streaming, history, token estimation / context pruning)
  ├── index/indexmanager.*     → File index, backlinks, tags, async index build, wiki-link resolution, completion data
  ├── core/crashrecoverymanager.* → Stale recovery file cleanup, recovery directory management
  ├── core/thememanager.*      → Singleton, VS Code 2026 Dark/Light palettes, Windows registry auto-detect, system theme 5-min refresh. Provides **watchTheme<Receiver>(slot)** template and **colorStyle(property, colorKey)** helper.
  ├── runner/compilerunmanager.* → Compile/run/stop lifecycle, now routes through TerminalPanel::openCommandTerminal() with PowerShell commands (no longer uses ProcessRunner for main compile/run), handles IDE mode routing, manages bottom panel visibility and action enable/disable
  ├── runner/codeblockrunner.* → MD preview ▶ button execution, saves temp files, buffers stderr, parses diagnostics via CompilerErrorParser, caches per-file/block diagnostics, loads cached diagnostics on tab switch
  ├── judge/openjudgemanager.* → OpenJudge tab creation (delegates to TabManager), code submission (IDE mode + normal), submission result display via BottomPanel (delegated, no longer creates SubmitResultPanel itself), error analysis via background JudgeEngine + ErrorJournal. Constructor takes BottomPanel* instead of QSplitter*.
  ├── config/settingschangehandler.* → Setting change application across all editors (font/indent/zoom), ThemeManager color bridge for appearance overrides, shortcut rebinding, reset-to-defaults logic
  ├── panels/activitybar.*     → 48px fixed left bar, 5 SVG icon buttons (Search/AI/Settings/Export PDF/Judge), active state with left border highlight (#0078D4)
  ├── widgets/titlebarbutton.* → QPushButton subclass, system-native title bar icons (SP_TitleBarMin/Max/Normal/CloseButton), QPainter hover bg
  ├── panels/fileexplorerwidget.* → QTreeView + QFileSystemModel, file tree (in splitter, left of editor)
  │   ├── Breadcrumb bar  → FlowLayout path segments, click-to-navigate
  │   └── Toolbar         → Folder label (auto-elided) + collapse-all + refresh buttons
  ├── editor/tabmanager.*      → QTabWidget, owns EditorWidget tabs (center, right splitter top)
  │   └── editor/editorwidget.* → QStackedWidget[WikiLinkTextEdit | QWebEngineView(lazy) | CodeEditor | QSplitter(edit+preview,lazy) | QPdfView | SmdEditor], six-mode editor, preview WebEngine released on close
  │       ├── editor/wikilinktextedit.* → QTextEdit subclass with QCompleter for [[wikilink]] autocomplete
  │       ├── editor/codeeditor.* → QPlainTextEdit subclass with line numbers, syntax highlighting, auto-indent, LSP diagnostics (squiggly underlines), CompletionProvider integration
  │       ├── smd/smdeditor.*   → QScrollArea-based cell editor for `.smd` files, Jupyter-like command/edit dual mode
  │       │   ├── smd/smdcell.* → QFrame subclass, one cell (Markdown/C++/Python) with editor/view stack
  │       │   ├── panels/smdoutputwidget.* → per-cell output display (stdout/stderr), 1000-line cap, auto-height
  │       │   ├── panels/smddiagnosticspanel.* → error/warning summary panel, toggled via Ctrl+D in edit mode
  │       │   └── lsp/smdlspmanager.* → shared LSP backend for SMD code cells, virtual-document stitching per group. Diagnostics flow: SmdLspManager/CppCompletionProvider/PythonCompletionProvider → diagnosticsUpdated(QList<SmdDiagnostic>) → CodeEditor (squiggly) + BottomPanel/SmdDiagnosticsPanel (list)
  │       └── smd/smdformat.h   → header-only namespace, parse/serialize `---smd:<type>` delimiter format
  ├── runner/compilererrorparser.h → header-only namespace, parse g++/MSVC/Python stderr → QList<SmdDiagnostic> (MD block diagnostics)
  ├── panels/rightpanelcontainer.* → Unified right QDockWidget with tab bar (History/Outline/Tags/Backlinks) + QStackedWidget. Toggled via toolbar [面板] button or Ctrl+Shift+E.
  ├── ai/aipanel.*              → AI assistant panel (right QDockWidget, tabbed with RightPanelContainer). Toggled via ActivityBar AI button or Ctrl+Shift+A.
  │   ├── ai/actionbar.*        → dynamic QHBoxLayout of action buttons, context-sensitive: Markdown actions (改进写作/总结笔记/提取标签/出题自测/翻译) or Code actions (解释代码/寻找Bug/添加注释/优化建议) based on AiContextManager::currentEditorMode()
  │   ├── ai/chatarea.*         → QScrollArea with vertical layout of ChatBubble items, auto-scrolls to bottom, supports streaming append
  │   │   └── ai/chatbubble.*   → QWidget with role label + QTextBrowser. User bubbles right-aligned blue, Assistant left-aligned gray. Lightweight Markdown→HTML converter for bold/italic/code/link/headings/lists/code blocks. 80ms debounce timer coalesces SSE chunks.
  │   ├── InputBar              → QHBoxLayout: QLineEdit (text input) + QPushButton (发送). ReturnPressed / button click emit sendMessage.
  │   └── ai/aihistorylistwidget.* → searchable history QListWidget with date grouping (今天/昨天/更早), green dot for active conversation, right-click context menu (rename/delete/export Markdown)
  ├── ai/aiconversation.h       → data structs (AiConversation, AiMessage) with JSON serialization/deserialization, standalone header
  ├── ai/aihistorymanager.*     → Singleton (QObject). Persistent conversation history storage at `{exeDir}/ai_history/`. index.json + per-conversation `conv_{uuid}.json` files. CRUD operations, per-file filtering, Markdown export.
  ├── ai/errorjournal.*         → Singleton (QObject). ErrorRecord struct + JSON persistence to error_journal/records.json. recordFailure() called from JudgePanel::onTestFinished for non-AC results (WA/RE/TLE/MLE). requestAnalysis() builds ErrorAnalysis prompt and streams AI response back into ErrorRecord::aiAnalysis. Signals: analysisReady(recordId), recordsChanged().
  ├── ai/errorlistpanel.*       → QWidget with status filter (ComboBox: 全部/WA/RE/TLE/MLE), keyword search (LineEdit), scrollable error list (QListWidget), expand-to-detail view (QStackedWidget switching list↔detail). Detail shows problem info, I/O comparison, AI analysis Markdown rendering, action buttons (re-analyze/delete/review). Delete/delete-all/clear-all operations through ErrorJournal.
  ├── ai/aicontextmanager.*     → Static utility class. collectContext(EditorWidget*) returns ContextBundle{mode, filePath, fileContent, selectedText, language, cursorLine/Column, plus error analysis fields}. currentEditorMode() returns AiEditorMode::Markdown|Code|Unknown. Handles SMD cells, PDF view, code files.
  ├── ai/prompttemplates.h      → AiAction enum (11 values), PromptBundle struct, actionsForMode() — editor-mode-to-action mapping. Prompt text moved to external prompts.json.
  ├── ai/promptmanager.*        → QObject singleton, loads prompts from {exeDir}/prompts.json / Qt resource / C++ fallback. buildPrompt(), actionLabel(), actionTooltip(). reload() hot-reload. 14 template placeholders, defaultQuery fallback.
  ├── ai/aiproviders.*           → `ai/aiproviders.h/cpp`: AiProvider (abstract), AnthropicProvider, OpenAiProvider, AiProviderFactory, Message struct, MessageRole enum. All AI provider classes in one unit.
  ├── panels/searchpanel.*      → QDockWidget + QLineEdit + QListWidget, full-text search (left dock, tabbed with Judge)
  ├── panels/judgepanel.*       → QDockWidget + QTableWidget + JudgeEngine, local judge (left dock, tabbed with Search)
  │   └── judge/judgeengine.*   → QObject managing compile→test QProcess pipeline, OJ-style results (AC/WA/RE/TLE/MLE)
  ├── index/backlinkindex.*     → QMap-based reverse index: target file → source files for `[[wikilinks]]`
  ├── index/tagindex.*          → QMap-based bidirectional index: tag ↔ files for `#tag` syntax
  ├── config/configmanager.*    → Singleton, reads config.json for static configuration
  ├── config/settingsmanager.*  → QSettings wrapper, config.ini for runtime session state
  ├── core/languageutils.*      → Extensible language registry: extension→highlighter factory map
  ├── runner/processrunner.*    → QObject managing compile→run QProcess pipeline
  ├── panels/bottompanel.*      → unified bottom panel (right splitter bottom) with three tabs: DiagnosticsTab (error/warning list via DiagnosticSection), TerminalTab (multi-tab PowerShell terminal + run output), JudgeTab (OpenJudge submission results). 28px header: tab buttons + [+] new terminal + [×] close (SVG icon). Manages provider connection lifecycle on tab switch. showSubmissionResult() for OpenJudge results.
  │   ├── panels/terminalpanel.*  → multi-tab PowerShell terminal (QTabWidget with StableWidthTabBar), owns RunTerminalPanel as child, openCommandTerminal() for compile/run/pipeline routing with tab reuse by source file
  │   │   └── panels/runterminalpanel.* → (new) TerminalView-based run output widget replacing old OutputPanel, Stop/Clear buttons, ANSI stderr coloring, setStatus()/setRunning()
  │   └── editor/diagnosticsection.* → reusable QWidget (header with ▾/▸ toggle + count badge), shared by BottomPanel and SmdDiagnosticsPanel
  ├── smd/smddiagnostic.h       → standalone struct header (`smddiagnostic.h`): cellIndex, startLine/Col, endLine/Col, message, severity (1=Error, 2=Warning). Shared by all diagnostic producers/consumers.
  ├── panels/openjudgewidget.*  → QWidget (non-EditorWidget tab in TabManager), browse OpenJudge homework→problems→detail, submit code
  │   └── judge/crawler.*       → QNetworkAccessManager + QNetworkCookieJar, HTTP crawler for cxsjsx.openjudge.cn
  ├── panels/submissionpanel.*  → QWidget, dark-themed submission result display
  ├── judge/logindialog.*       → QDialog, username/password + auto-login checkbox
  ├── config/keyrecorder.*      → Click-to-record widget with Normal/Recording/Cleared states, conflict detection via ShortcutOverride event
  ├── panels/settingspanel.*    → Floating overlay with dimming background, drag/resize, 7 category pages (including Shortcuts with KeyRecorder)
  ├── panels/helppanel.*        → Floating overlay help panel, 14-category sidebar + QTextBrowser, scroll-sync, drag-move only, F1 toggle
  └── widgets/flowlayout.*      → Custom QLayout subclass, auto-wrapping flow layout
```

## Key Data Flow

- **File opening**: FileExplorerWidget click → TabManager::openPreview (single-click) or TabManager::openFile (double-click). Single-click opens a temporary preview tab (italic title), reused across clicks; double-click opens permanently or promotes preview to permanent. Editing preview content auto-promotes to permanent.
- **SMD editing**: Cell widgets with header badge + editor/render stack + output area. Default edit mode (blue active border); Esc→command mode (purple active border, A/B insert cell, ↑/↓ navigate, Ctrl+K change cell language, Ctrl+Enter execute, Ctrl+Shift+Z un-render MD, Delete remove). Ctrl+D toggles diagnostics panel. C++ cells auto-group by main() boundaries for per-group compilation; Python cells use a persistent process with shared namespace across cells. Markdown cells toggle rendered view via Ctrl+Enter; code cells (C++/Python) execute via temp file → ProcessRunner → output below cell. Auto-height based on block count (max ~40 lines).
- **Code editing**: Enter auto-indents (copies leading ws, adds level on `{`, splits `{|}`). Tab inserts 4 spaces. Bracket auto-pair for `{}()[]""''` (skips in string/comment). Backspace removes empty pair or deletes to tab-stop.
- **WikiLinks**: `[[filename]]` → regex-converted to `<a href="wikilink:...">` in preview. Click → `acceptNavigationRequest` intercepts `wikilink:` scheme → multi-level filename search → opens existing or prompts create.
- **Preview code block run**: marked renderer adds ▶ button on code blocks. Click stores code + `blockIndex` in JS → navigates `runblock:execute` → C++ intercepts → `CodeBlockRunner::runCodeBlock()` saves temp file → ProcessRunner compiles/runs. Stderr buffered by CodeBlockRunner; on completion, `CompilerErrorParser` parses errors/warnings → `SmdDiagnostic`, shown in BottomPanel + as JS wave underlines in preview (`applyBlockDiagnostics`). Manual stop skips diagnostics. Diagnostics cached per file/block, restored on tab switch via `CodeBlockRunner::loadDiagnosticsForCurrentTab()`.
- **File indexing**: Async rebuild on folder change via `IndexManager` worker thread with cancellation token + generation counter. Synchronous incremental updates on rename/delete/move.
- **Backlinks**: Built asynchronously from scratch. Forward + reverse index. Incremental ops (rebuildFile, rename, delete) remain synchronous.
- **Tags**: Extracted from `.md` files via Unicode-aware regex (skips headings and code blocks). Bidirectional index built async as Phase 3 of startup scan.
- **Rename**: FileExplorerWidget rename → update backlink index → rewrite `[[oldName]]→[[newName]]` in all linking files (reads from open editors if unsaved).
- **Split preview**: Ctrl+P toggles QSplitter with edit left + WebEngine right. 500ms debounce, content-diff guard. Mutually exclusive with full preview.
- **AI assistant**: ActivityBar AI click or Ctrl+Shift+A → m_dockAi toggle. ActionBar button click → MainWindow → `AiRequestHandler::startAiRequest(action)` → AiContextManager::collectContext(editor) → buildPrompt(action, ctx) → AiProviderFactory::createProvider(type) → provider.chatStream(messages) → SSE stream → partialResponse→appendToLastAssistant → finished→enable input. FreeChat via InputBar sends AiAction::FreeChat with text.
- **All actions now preserve history** for multi-turn continuation (no longer cleared per action). Each user message is persisted to `AiHistoryManager` alongside the in-memory history owned by `AiRequestHandler`. On API request, `AiRequestHandler::pruneContextWindow()` creates a token-aware COPY of the history (never mutates the canonical history), using model-specific context limits (`modelContextLimit()`) and a rough token estimator (`estimateTokens()` distinguishing CJK vs ASCII). Assistant responses are persisted on `finished()`. The `AiHistoryListWidget` (third tab in AiPanel) shows conversations filtered by the current editor's `sourceFile`, with search, date grouping, and right-click rename/delete/export. Streaming performance optimized with 80ms debounce timer in ChatBubble and `setUpdatesEnabled(false)` guard to prevent blank flash. Both providers guard against duplicate `finished()` emissions via `m_finished` flag. Settings: API type/endpoint/key/model/max_tokens/system_prompt configurable via SettingsPanel AI Service page (index 6), persisted to config.ini via SettingsManager.
- **Compile & run**: F5/F6/F7 → `CompileRunManager::compile/run/compileAndRun()` → routes through `TerminalPanel::openCommandTerminal()` with PowerShell commands (psInvoke for compiler/executable). C/C++ compiles via g++/MSVC in a terminal tab (tab reuse per source file via `cppReuseKey()`), then runs the executable; Python runs directly via interpreter. IDE mode routes through OpenJudgeWidget's embedded editor, compile/run in terminal tabs. No longer uses ProcessRunner for initiating compile/run — ProcessRunner retained only for MD code block stderr buffering in CodeBlockRunner. Bottom panel auto-shows with terminal tab; first-show resizes rightSplitter to 1/3 height. Action enable/disable managed by `CompileRunManager::updateActions()`.
- **Diagnostics**: Three sources — (1) `SmdDiagnosticsPanel` for SMD cells, (2) `BottomPanel` DiagnosticsTab for standalone `.cpp`/`.py` via LSP providers, (3) `BottomPanel` + JS wave underlines for MD code blocks via `CompilerErrorParser`. All `CompletionProvider` subclasses emit `diagnosticsUpdated`. C++: clangd → `CppCompletionProvider::parseDiagnostics()`. Python: Jedi `diagnostics` action. MD blocks: `CompilerErrorParser::parseCompileErrors()` / `parsePythonTraceback()` on stderr after ▶ Run. `MainWindow::currentChanged` restores diagnostics from cache on tab switch: code files → LSP provider, `.md` → `loadMdDiagnosticsForCurrentTab()`, others → hide. Tab switch now calls `clearDiagnostics()` instead of hiding BottomPanel.
- **Local Judge**: Compile → warmup → per-test execution (1s timeout, 64MB memory limit). Triple-capture memory monitoring. Line-by-line trimmed output comparison. AC/WA/TLE/MLE/RE color-coded table.
- **OpenJudge integration**: Embedded tab via OpenJudgeWidget (non-EditorWidget, managed by TabManager). Tab creation and signal wiring managed by `OpenJudgeManager::open()`. Submission flow (IDE mode + normal editor) via `OpenJudgeManager::submit()`. Crawler-based HTTP (cxsjsx.openjudge.cn). Homework browsing → problem detail → sample extraction → cache → submit → poll 30s → show results via BottomPanel::showSubmissionResult() (SubmitResultPanel now owned by BottomPanel as Judge tab, not inserted into rightSplitter). Non-AC/CE results auto-run local tests via background JudgeEngine for ErrorJournal recording. Cross-cutting signals (ideModeChanged, diagnosticsToggleRequested) routed through OpenJudgeManager → MainWindow.
- **QSS scope**: Theme swaps apply QSS only to MainWindow (via `ThemeManager::setStyleSheetTarget`) instead of `qApp->setStyleSheet()`, avoiding O(widget_count) repaint on every theme change. Other top-level widgets use QPalette + individual `refreshStyle()`.
- **Window drag performance**: No `qApp->installEventFilter` — right panel auto-hide uses `QApplication::focusChanged` + `QTimer::singleShot(0)` guard. Maximized window drag defers `showNormal()` to `MouseMove` (via `m_toolbarDragPending`); `MouseButtonRelease` without move clears the flag, preventing accidental un-maximize on click. Left/right panels controlled independently.
- **ScrollbarHider registration**: Tab switch `findChildren<QAbstractScrollArea*>` runs once per editor. `QSet<EditorWidget*> m_editorScrollAreasRegistered` caches already-registered editors, skipping O(widget_tree_depth) traversal on subsequent switches.
- **ChatBubble streaming**: Incremental HTML conversion — `m_accumulatedHtml` cache accumulates inline-only (`processInline()`) delta chunks. `isStructuralDelta()` detects code block fences/headings/lists/HR for fallback to full `markdownToHtml()`. `m_inCodeBlock` tracking forces full rebuild inside code blocks. `flushUpdate()` triggers final full rebuild to correct incremental approximation errors.

## Component Details

### EditorWidget
Six-mode `QStackedWidget` (page 0: WikiLinkTextEdit, page 1: full preview WebEngine (lazy), page 2: CodeEditor, page 3: QSplitter(edit+preview, lazy), page 4: QPdfView, page 5: SmdEditor). Modes are mutually exclusive: enabling split preview transfers `m_textEdit` between page 0 and page 3's splitter. PdfView and SmdEdit skip preview/wikilink operations. Preview pipeline shares marked.js + KaTeX + Mermaid across full and split modes. Code blocks are pre-highlighted in C++ (regex-based, matching highlighter colors), base64-encoded into custom fenced blocks. Zoom 0.5–3.0, step 0.1. 300ms debounce clears modified flag on text revert.

**Lazy WebEngine**: `m_previewView`/`m_previewContainer`/`m_splitSplitter` are not created in the constructor — `ensurePreviewView()` creates them on first preview demand; `destroyPreviewView()` / `destroySplitPreviewWidgets()` release the Chromium process and widgets when exiting preview mode (via `deleteLater()` after `removeWidget`). `m_previewReady` and `m_splitPreviewReady` flags track initialization state. All pointer accesses are null-guarded, including `loadFinished` callbacks and `refreshPreviewTheme()`. `setCurrentWidget()` replaces hardcoded `setCurrentIndex()` for robustness.

### PDF export
Temporary hidden QWebEngineView loads light-themed preview, waits for Mermaid async rendering via JS Promise polling, then calls `printToPdf()`.

### SmdEditor (`smd/smdeditor.h/cpp`)
QScrollArea of SmdCell widgets in QVBoxLayout. Dual mode: edit (default, blue active border) and command (Esc, purple active border). Owns separate ProcessRunner for cell execution. C++ cells auto-group by `main()` boundaries — only cells in the same group are compiled together for each execution. Python cells use a persistent process (`python_executor.py`) that maintains a shared namespace across the same `.smd` file, enabling cross-cell variable reuse. `Ctrl+D` toggles `SmdDiagnosticsPanel` (public `toggleDiagnosticsPanel()`) for aggregated error/warning display. Temp files: `smd_cell_<PID>_<counter>.ext`. Modification tracking compares serialized content with original. Forwards `modificationChanged`/`fileLoaded`/`fileSaved` to EditorWidget. **eventFilter**: installed on QApplication (global, for FocusIn/MousePress cell activation), self, and top-level window. Keyboard shortcut entry has a widget-hierarchy guard — only processes shortcuts for widgets within this SmdEditor, preventing global filter from stealing shortcuts from CodeEditor.

### SmdCell (`smd/smdcell.h/cpp`)
QFrame: header badge (MD blue/C++ green/Python yellow) + QStackedWidget(editor↔render) + hidden output area. Markdown uses QPlainTextEdit, code uses CodeEditor with highlighter. Auto-height via blockCount (min 1 line, max ~40, no internal scrollbar). Active cell borders: 2px blue (`#0078d4`) in edit mode, 2px purple (`#C586C0`) in command mode; inactive command-mode gray (`#3c3c3c`); edit mode transparent.

### SmdOutputWidget (`panels/smdoutputwidget.h/cpp`)
Per-cell output display for SMD code cell execution. Dark terminal style (#1E1E1E bg), monospace font. 1000-line hard cap; auto-height limited to ~15 visible lines with internal scrollbar beyond that. stdout white, stderr red. Each SmdOutputWidget corresponds one-to-one with a SmdCell, managed by SmdEditor.

### SmdDiagnosticsPanel (`panels/smddiagnosticspanel.h/cpp`)
QFrame-based diagnostics summary panel, toggled via Ctrl+D in edit mode. Embedded in SmdEditor's splitter, default hidden. Contains two `DiagnosticSection` widgets (Error red #F44747 / Warning yellow #CCA700) listing all LSP diagnostics from SmdLspManager. Each entry shows cell index and line number; click jumps to the corresponding cell and line. Supports debounced refresh (scheduleRefresh + 50ms timer). Shares `DiagnosticSection` component with `BottomPanel`'s diagnostics tab.

### SmdLspManager (`lsp/smdlspmanager.h/cpp`)
Shared LSP backend for SMD code cells. Manages per-language LSP client adapters (clangd for C++, Jedi for Python) with virtual-document stitching — groups related cells into a single virtual translation unit for clangd, or presents individual cells to Python LSP. Tracks cell order and content changes, translates `contentChanged` signals into `textDocument/didChange` notifications. Aggregates diagnostics (red/yellow squiggles) from all cells and forwards to SmdCell::setDiagnostics() and CodeEditor::setDiagnostics(). Created and destroyed on each file load; coordinates with SmdEditor for C++ program group detection.

### SmdFormat (`smd/smdformat.h`)
Header-only. `---smd:<type>` delimiters. Parse splits on regex, trims leading/trailing blank lines. Serialize writes delimiter + content.

### FileExplorerWidget (`panels/fileexplorerwidget.h/cpp`)
QFileSystemModel + custom QSortFilterProxyModel (folders-first). NoGhostDelegate prevents text ghosting during inline rename (empty name auto-restores). Drag-drop moves within root only. Breadcrumb bar using FlowLayout.

### TabManager (`editor/tabmanager.h/cpp`)
CustomTabBar constrains drag bounds. Custom save prompt on close. batch updatePathsAfterMove for file tree drag operations.

### HistoryPanel (`panels/historypanel.h/cpp`)
Max 50 entries, auto-dedup by path. Persisted at shutdown only (SettingsManager). Auto-removes deleted entries on next access. Hosted inside RightPanelContainer (right dock, tab 0).

### SearchPanel (`panels/searchpanel.h/cpp`)
Full-text via QDirIterator + scanning. 20 matches/file, 500 total. Gold (#FFD700) highlights on result click. Left dock (tabbed with JudgePanel). Persistent sidebar (no auto-hide).

### RightPanelContainer (`panels/rightpanelcontainer.h/cpp`)
Unified right QDockWidget. Top 32px tab bar with 4 icon+text tabs (History/Outline/Tags/Backlinks), bottom QStackedWidget. Toggled via toolbar [面板] button or Ctrl+Shift+E. Click-outside auto-hides via MainWindow::eventFilter. Sub-panels: HistoryPanel (tab 0), OutlinePanel (tab 1), TagPanel (tab 2), BacklinksPanel (tab 3).

### OutlinePanel (`panels/outlinepanel.h/cpp`)
Title outline panel hosted in RightPanelContainer (tab 1). Parses current document headings (`#` to `######`, skipping fenced code blocks) and displays them in an indented tree with hierarchical brightness (h1 brightest, h6 dimmest, h1/h2 bold). Clicking a heading jumps to the corresponding line in the editor. Auto-refreshes on tab switch and file save; clears for non-Markdown files.

### BacklinkIndex (`index/backlinkindex.h/cpp`)
Reverse index (target→sources) + forward index (source→targets) for `[[wikilinks]]`. Async full build via static `buildFromPath()`. Target resolution: exact path → global index by baseName → shortest-path tiebreaker. Unlike `IndexManager::findWikiTarget` (which disambiguates via current editor context), `resolveTarget` is purely deterministic with no editor bias. Incremental ops synchronous. Owned by `IndexManager`.

### BacklinksPanel (`panels/sidebarpanels.h/cpp`)
Display-only QListWidget (NoSelection). Hosted inside RightPanelContainer (right dock, tab 3). Shows placeholder when empty. Min width 200px.

### TagIndex (`index/tagindex.h/cpp`)
Non-QObject bidirectional index. Unicode-aware regex `(*UCP)#(?!\s)([\w-]+)`. Scans only `.md`/`.markdown`. Async Phase 3 of startup scan. Owned by `IndexManager`.

### TagPanel (`panels/sidebarpanels.h/cpp`)
Dual-mode: tag list ↔ files per tag. Back button to return. Hosted inside RightPanelContainer (right dock, tab 2).

### ConfigManager (`config/configmanager.h/cpp`)
Singleton, reads config.json. Dot-path resolution (e.g. `"editor.zoom.min"`). Built-in defaults for missing file. All static configuration.

### SettingsManager (`config/settingsmanager.h/cpp`)
Writes config.ini next to executable. Runtime state: geometry, recent files, OJ credentials (base64-obfuscated), user overrides. Singleton with override→ConfigManager→default chain.

### TextFileUtils (`core/utilities.h`)
Header-only. 40+ text extension list + scan name filters.
**I/O helpers** (added v0.13.10): `readTextFile(path)`, `writeTextFile(path, content)`, `readJsonFile(path)`, `writeJsonFile(path, doc)`, `isSafeRootPath(path)` — eliminate 20+ QFile/QTextStream/QJsonDocument boilerplate blocks across the project.

### WikiLinkTextEdit (`editor/wikilinktextedit.h/cpp`)
QTextEdit with QCompleter. `[[` triggers filename popup (case-insensitive prefix). `#` triggers tag autocomplete. Tab accepts, first item auto-selected.

### CodeEditor (`editor/codeeditor.h/cpp`)
QPlainTextEdit with line numbers, auto-indent, bracket completion, search highlights, code completion. Dark theme Consolas/Menlo 12pt (Menlo on macOS, Consolas on other platforms). setLanguage() installs highlighter via LanguageUtils and creates a CompletionProvider (CppCompletionProvider / PythonCompletionProvider / KeywordCompletionProvider fallback). setDocumentUri() sets file identity for LSP diagnostics. Owns CompletionPopup, HoverManager, SignatureHelpManager. Platform-specific Esc key handling for tool window dismissal:
- **Windows**: `EscNativeFilter` installed via `qApp->installNativeEventFilter()` — catches `VK_ESCAPE` Windows message directly (required because Qt routes Esc to the Tool HWND when a Qt::Tool window is visible).
- **Linux and macOS**: `EscEventFilter` installed via `installEventFilter()` on the editor — catches `QEvent::KeyPress` with `Qt::Key_Escape`.
Both implementations close CompletionPopup and SignatureHelpManager identically.
**Diagnostics**: connects `provider->diagnosticsUpdated` → `setDiagnostics()` to cache `m_diagnostics` and draw squiggly underlines. `diagnostics()` getter exposes cache for BottomPanel restoration on tab switch. **Ctrl+D shortcut**: `m_toggleDiagnostics` QKeySequence; eventFilter handles ShortcutOverride (accept) + KeyPress (emit `diagnosticsToggleRequested()`); keyPressEvent has backup matchShortcut check.

### CompletionProvider (`lsp/completionprovider.h`)
Abstract QObject interface. Defines CompletionItem/HoverInfo/SignatureInfo structs. Pure virtual: requestCompletion, requestHover, requestSignatureHelp. Virtual no-op openDocument/updateText (overridden by LSP providers for text sync). Signals: completionReady, hoverReady, signatureHelpReady, **diagnosticsUpdated(QList<SmdDiagnostic>)**. Includes `smd/smddiagnostic.h` for the shared `SmdDiagnostic` struct.

### LspClient (`lsp/lspclient.h/cpp`)
QProcess wrapper for LSP JSON-RPC 2.0 over stdin/stdout with Content-Length framing. start() launches server process. sendRequest/sendNotification with auto-incrementing message IDs. Signals: responseReceived(id, result), notificationReceived(method, params), serverError, serverStopped. Internal parseFrames() buffers and splits incoming frames from raw byte stream.

### CppCompletionProvider (`lsp/cppcompletionprovider.h/cpp`)
CompletionProvider for C/C++ via clangd LSP. Starts clangd with --fallback-style=Google (no compile_commands.json required). Sends initialize → initialized → didOpen → didChange → [completion|hover|signatureHelp]. Tracks pending requests with 500ms timeout QTimer. Parses LSP CompletionList/Hover/SignatureHelp responses into provider structs. Auto-restarts on crash. **Diagnostics**: `onNotificationReceived()` intercepts `textDocument/publishDiagnostics`, `parseDiagnostics()` converts LSP JSON ranges → `QList<SmdDiagnostic>` (cellIndex=0 for flat files), emits `diagnosticsUpdated`.

### PythonCompletionProvider (`lsp/pythoncompletionprovider.h/cpp`)
CompletionProvider for Python via Jedi helper process. Launches `python completion_helper.py` as QProcess, communicates via stdin/stdout JSON (actions: "complete"/"hover"/"signature"/"diagnostics"). Sends full code per request (no incremental sync needed). 500ms timeout fallback. Detects Jedi import failure and signals serverFailed for fallback chain. **Diagnostics**: `openDocument()` triggers immediate `sendDiagnosticsRequest()`; `updateText()` starts 500ms debounce timer → `onDiagnosticsDebounce()` → `sendDiagnosticsRequest()`. Sends base64-encoded code as single cell (`cellIndex=0`, action="diagnostics"). `processResponse()` parses diagnostic list from helper and emits `diagnosticsUpdated`. Unlike C++ clangd (push), Python diagnostics are pull-based.

### KeywordCompletionProvider (`editor/keywordcompletionprovider.h/cpp`)
Fallback CompletionProvider used when clangd/Jedi are unavailable. Filters language keywords (C++/Python) and document words from current editor text by prefix match. Hover and signature help return empty results (no-op).

### CompletionPopup (`editor/completionpopup.h/cpp`)
Floating QWidget (Qt::ToolTip, borderless) overlay for completion item selection. Dark theme (#252526 bg, #094771 selection). Custom CompletionItemDelegate draws icon + name + type tag per row. Bottom hint bar shows keyboard shortcuts. Signals itemSelected on Enter/Tab. Uses QListWidget internally. iconForType maps type strings to QIcon (function → f() icon, class → C:, variable → V:, etc.).

### HoverManager (`editor/hovermanager.h/cpp`)
QObject event filter installed on CodeEditor viewport. 400ms QTimer debounce on mouse hover; Ctrl modifier bypasses delay. Calls requestHover on the active CompletionProvider. Shows result in a QToolTip popup with signature (bold header) + doc + definition location. Auto-hides on mouse move or key press.

### SignatureHelpManager (`editor/signaturehelpmanager.h/cpp`)
Monitors CodeEditor::cursorPositionChanged. Detects unmatched `(` by scanning backwards with parenthesis counter. 200ms debounce before request. SignatureHelpPopup (inner QWidget subclass) displays: overload navigation (◀ 1/2 ▶), signature with active parameter highlighted in yellow, and doc text. Closes on `)`, Esc, or mouse click outside. Up/Down arrow navigates overloads.

### LanguageUtils (`core/languageutils.h/cpp`)
Singleton language registry. Currently cpp (extensions: cpp/hpp/cxx/cc/c/h/hxx/hh) → CppSyntaxHighlighter and python (py/pyw/pyx) → PythonSyntaxHighlighter. Adding a language = 1 map entry + 1 highlighter file + .pro entries.

### CppSyntaxHighlighter (`editor/cppsyntaxhighlighter.h/cpp`)
QSyntaxHighlighter. Dark theme: keywords #569CD6, preprocessor #C586C0, types #4EC9B0, strings #CE9178, numbers #B5CEA8, comments #6A9955. Multi-line comment block-state tracking.

### PythonSyntaxHighlighter (`editor/pythonsyntaxhighlighter.h/cpp`)
QSyntaxHighlighter. Dark theme colors. Supports f-strings and raw strings. Triple-quote (`"""`/`'''`) block-state tracking.

### BracketHighlighter (`editor/brackethighlighter.h/cpp`)
QSyntaxHighlighter for bracket matching. Highlights matching `{}()[]` pairs and provides companion `BracketCompletionFilter` for auto-pair skipping inside string/comment contexts.

### CompilerUtils (`runner/compilerutils.h`)
Header-only. Platform-specific compiler detection via `#ifdef`:
- **Windows**: g++ (MinGW via QStandardPaths) + MSVC cl.exe (VSCMD_VER env).
- **Linux**: g++ (QStandardPaths) + clang++ (QStandardPaths fallback).
- **macOS**: clang++ (Apple, QStandardPaths + `/usr/bin/clang++` fallback) + g++ (Homebrew via QStandardPaths).
Compile args: g++/clang `-std=c++17 -Wall -Wextra`, cl `/std:c++17 /W4 /EHsc`. Accepts both `gcc` and `clang` compiler IDs in `getCompileArgs()` and `getCompileOnlyArgs()`.

### ProcessRunner (`runner/processrunner.h/cpp`)
Two-phase compile→run via sequential QProcess. Methods: startCompile, startRun, startCompileAndRun (auto-transitions), startRunPython. writeInput(text) appends `\n`; writeRaw doesn't. isAcceptingInput() false during compile. Output raw (no .trimmed()). Signals: compileFinished, runFinished, processStarted, processStopped. **Note**: Main compile/run now routes through terminal commands (not ProcessRunner). ProcessRunner retained for MD code block stderr buffering in CodeBlockRunner.

### RunTerminalPanel (`panels/runterminalpanel.h/cpp`)
(New, replaces old OutputPanel) TerminalView-based run output widget. Stop/Clear buttons. `appendOutput(text, isStderr)` appends with ANSI red for stderr. `setStatus(text, isError)` shows green/red status. `setRunning(bool)` controls stop button enabled state. `setInputEnabled(bool)` controls local echo. Theme-aware styling via `refreshStyle()`. Owned by TerminalPanel as child widget.

### TerminalPanel (`panels/terminalpanel.h/cpp`)
Multi-tab PowerShell terminal widget. Uses `TerminalTabWidget` (QTabWidget with `StableWidthTabBar` that prevents text jitter on bold/normal state changes). Tab bar styled with active indicator, hover states, close-button icons from theme resources. Header bar (with +/× buttons) **removed** — now owned by BottomPanel header. Added `RunTerminalPanel* m_runTerminal` child widget (hidden by default, shown for compile/run output). New `openCommandTerminal(title, command, cwd, reuseKey)` method: creates/runs a command in a terminal tab with optional reuse per source file (`cppReuseKey`). `ensureTerminal()` now falls back to existing `TerminalView` tabs if current widget is not a terminal. Tab close via `closeTerminal()`. `setWorkingDirectoryProvider()` for dynamic CWD resolution.

### BottomPanel (`panels/bottompanel.h/cpp`)
Unified bottom panel (QSplitter bottom). 28px header bar with three tab buttons (诊断/终端/评测, bold for active) + [+] new terminal button + [×] close button (SVG icon, red hover). QStackedWidget: index 0 = diagnostics page (QScrollArea with two `DiagnosticSection` widgets), index 1 = TerminalPanel (multi-tab terminal + run output), index 2 = SubmitResultPanel (OpenJudge submission results). No longer contains OutputPanel. Header buttons use `QFontMetrics::horizontalAdvance` + 24px padding for fixed-width sizing. Stores `QList<SmdDiagnostic> m_diagnostics` and `CodeEditor *m_currentEditor`. `rebuildDiagnostics()` auto-hides sections with zero count, shows "无诊断信息" placeholder. `showSubmissionResult()` displays OpenJudge results in Judge tab with 1/3 height resize on first use. `showTerminalTab()`/`showJudgeTab()`/`showDiagnosticsTab()` tab navigation. `runTerminal()` accessor for CompileRunManager/CodeBlockRunner. `setWorkingDirectoryProvider()` for terminal CWD. Uses `TabButtonGroup` for tab switching with style provider.

### JudgeEngine (`judge/judgeengine.h/cpp`)
Discovers `.in`/`.out` pairs. Compile → warmup (empty stdin, discarded output — populates OS cache for stable first test result) → per-test execution. 1000ms timeout (TLE). 64MB memory limit with triple-capture monitoring. Platform-specific `captureMemory()`:
- **Windows**: `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` + `GetProcessMemoryInfo`.
- **Linux**: `/proc/<pid>/status` (VmRSS) + `waitid` WNOWAIT `ru_maxrss` via syscall for exited processes.
- **macOS**: `proc_pidinfo(PROC_PIDTASKINFO)` for `pti_resident_size` + `wait4` fallback.
`m_testHandled` prevents dual-fire between timeout and process finish. Output: trimmed line-by-line comparison. TestResult struct includes `inputData` (read from `.in` file) for ErrorJournal recording.

### JudgePanel (`panels/judgepanel.h/cpp`)
Folder selector + OpenJudge buttons + 5-column result table + detail view. Owns JudgeEngine. Emits runAllRequested, openJudgeRequested, submitToOpenJudgeRequested. In `onTestFinished()`, non-AC results call `ErrorJournal::instance().recordFailure()`.

### ErrorJournal (`ai/errorjournal.h/cpp`)
Singleton managing persistent error records from Judge failures. `ErrorRecord` struct: id (UUID), problemName, sourceFile, testFolder, testCaseName, statusCode, elapsedMs, memoryKb, inputData, actualOutput, expectedOutput, detail, aiAnalysis (async-filled), tags, timestamp, reviewed. JSON storage at `{executable_dir}/error_journal/records.json` (version 1). `recordFailure()` called from JudgePanel for WA/RE/TLE/MLE results. `requestAnalysis(id)` reads source code from disk, builds ErrorAnalysis prompt (via PromptTemplates), creates configured AiProvider, streams response into `aiAnalysis` field. Signals: `analysisReady(recordId)`, `recordsChanged()`.

### ErrorListPanel (`ai/errorlistpanel.h/cpp`)
QWidget with two views: list (filtered error records) and detail (expanded record info). Filter bar: status ComboBox (全部/WA/RE/TLE/MLE) + keyword search LineEdit. List items show status icon (AC green/RE red/TLE yellow/MLE purple), problem name, source file, timestamp, tags. Detail view: problem header (status/time/memory), file info, I/O comparison (input/expected/actual in monospace), AI analysis section (QTextBrowser with Markdown rendering), action buttons (重新分析/删除/已阅/返回列表). Delete-all, delete-one, mark-reviewed all go through ErrorJournal. Signals: errorClicked, deleteRecordRequested, deleteAllRequested.

### OpenJudgeWidget (`panels/openjudgewidget.h/cpp`)
QWidget embedded as a non-EditorWidget tab in TabManager. Three states: homework list → problem list → problem detail. Sample extraction via \<pre\> pairing. Auto-login via SettingsManager (base64-obfuscated). TabManager::openOpenJudge() creates/switches to the singleton tab; findOpenJudgeWidget() locates it. closeTab() removes directly without save prompts (qobject_cast<EditorWidget*> returns nullptr). When active, MainWindow disables save/save-as actions.

### Crawler (`judge/crawler.h/cpp`)
HTTP crawler for cxsjsx.openjudge.cn. QNetworkAccessManager + cookie jar. Login via JSON API. Submit posts raw source (percent-encoded, avoids `+`→space corruption), polls 30s for result.

### SubmitResultPanel (`panels/submissionpanel.h/cpp`)
Dark-themed result display. Color-coded status (AC green, WA/RE red, etc.) + time/memory + collapsible CE error log.

### AiPanel (`ai/aipanel.h/cpp`)
QDockWidget content widget for AI assistant. 340px default width. Title bar with three tab buttons ("AI 助手" / "错题本 (N)" / "历史对话") + "新对话" button + "清空对话" button (both chat only). QStackedWidget switches between ChatArea (index 0), ErrorListPanel (index 1), and AiHistoryListWidget (index 2). onTabSwitch() hides ActionBar and InputBar on error/history tabs, shows them on chat tab; refreshes history list when switching to history tab. updateErrorBadge() shows record count. Signals: sendMessage(text), actionTriggered(index), clearRequested(), newConversationRequested(), errorSelected(recordId), historyListVisibilityChanged(bool). Public API: addUserMessage, addAssistantMessage, appendToLastAssistant (streaming), flushPendingUpdates(), clearChat, setInputEnabled, setCurrentTab(index), errorListPanel(), historyListWidget(). Provides lastAssistantContent() and hasStreamingTarget() for response state tracking.

### ActionBar (`ai/actionbar.h/cpp`)
Dynamic QHBoxLayout of QPushButtons. setActions(QVector<AiAction>) clears old buttons and creates new ones with labels/tooltips from ActionInfo. Emits actionTriggered(AiAction). Context-dependent: Markdown mode shows 5 buttons (改进写作/总结笔记/提取标签/出题自测/翻译), Code mode shows 4 (解释代码/寻找Bug/添加注释/优化建议). Dark theme buttons (#3c3c3c bg, #ccc text, #0078d4 hover border).

### ChatArea (`ai/chatarea.h/cpp`)
QScrollArea with vertical QVBoxLayout of ChatBubble widgets. addMessage(role, text) creates new bubble. appendToLastMessage(text) appends to last assistant bubble for streaming. messageCount() and lastBubble() for state queries. Auto-scrolls to bottom on new content. clear() removes all bubbles.

### ChatBubble (`ai/chatbubble.h/cpp`)
QWidget per message. Two roles: User (right-aligned, blue bg #1a3a5c) and Assistant (left-aligned, gray bg #2d2d2d). Role label above bubble ("你" blue / "AI 助手" green). Uses QTextBrowser for HTML rendering. Lightweight Markdown→HTML converter (markdownToHtml): supports **bold**, *italic*, `inline code`, [links](url), `#`/`##` headings, `-`/`*` lists, numbered lists, fenced ```code blocks```, `---` horizontal rules. Code blocks styled with #1e1e1e bg. setText()/appendText() update and auto-size height via QTextDocument::size(). **Streaming optimization**: 80ms QTimer debounce in appendText() coalesces rapid SSE chunks into single updateContent() call. setUpdatesEnabled(false) around setHtml() prevents blank flash. updateBrowserHeight() extracted with >1px threshold to prevent infinite resize loops. flushUpdate() forces pending debounced update on streaming finished.

**Incremental HTML streaming**: `m_accumulatedHtml` caches the converted HTML so only the delta chunk (new text since last tick) is processed on each debounce tick via `processSimpleDelta()` (inline-only conversion: bold/italic/code), rather than running full `markdownToHtml()` on the entire accumulated text. `isStructuralDelta()` detects code block fences, headings, lists, and horizontal rules — when a structural marker is found in the delta, falls back to full `markdownToHtml()` for correctness. `m_inCodeBlock` tracks whether the cursor is inside a code block, forcing the full-rebuild path to prevent incorrect `<p>` wrapping of `<pre><code>` blocks. `flushUpdate()` forces one final full rebuild to correct incremental approximation errors (e.g. list-item text split across chunks). `setText()` resets all incremental state (`m_accumulatedHtml.clear()`, `m_lastProcessedLength = 0`, `m_fullRebuildNeeded = false`, `m_inCodeBlock = false`). Theme changes set `m_fullRebuildNeeded = true` to trigger full rebuild on next tick.

### AiContextManager (`ai/aicontextmanager.h/cpp`)
Static utility class. collectContext(EditorWidget*) returns ContextBundle: mode (Markdown/Code/Unknown), filePath, fileContent (full when no selection), selectedText, language (via LanguageUtils + extra map for html/css/js/go/rust etc.), cursorLine/Column, plus error analysis fields (errorStatusCode, actualOutput, expectedOutput, inputData, elapsedMs, memoryKb, errorDetail). currentEditorMode(editor) detects mode: SMD cells check active cell type, code files → Code, everything else → Markdown. languageForFile(ext) maps extensions to language strings (markdown, cpp, python, html, javascript, etc.).

### AiProviders (`ai/aiproviders.h/cpp`)
All AI provider classes in one unit. **AiProvider**: abstract QObject base class with Message struct, MessageRole enum, chatStream/cancel API, SSE frame buffer, 30s timeout. **AnthropicProvider**: POST `{endpoint}/messages`, x-api-key auth, event/data SSE dual parsing → content_block_delta→partialResponse. **OpenAiProvider** (DeepSeek-compatible): POST `{endpoint}/chat/completions`, Bearer auth, `data:` SSE lines → choices[0].delta.content→partialResponse, `[DONE]`→finished. **AiProviderFactory**: static factory, createProvider(Anthropic|OpenAiCompatible). Both providers guard against duplicate `finished()` via `m_finished` flag.

### PromptTemplates (`ai/prompttemplates.h`)
Header-only. Defines `AiAction` enum (11 values: ImproveWriting, SummarizeNote, ExtractTags, SelfTest, Translate, ExplainCode, FindBugs, AddComments, OptimizeCode, ErrorAnalysis, FreeChat) and `PromptBundle{systemPrompt, userPrompt}` struct. `actionsForMode(mode)` returns the appropriate action list per editor context (Markdown → 5 MD actions, Code → 4 code actions). All prompt text content externalized to `prompts.json` — loaded at runtime by `PromptManager`.

### PromptManager (`ai/promptmanager.h/cpp`)
QObject singleton (`PromptManager::instance()`). Manages loading, resolution and hot-reload of AI prompt templates.

**Load priority**: `{executable_dir}/prompts.json` → Qt resource `:/prompts/prompts.json` → C++ hardcoded defaults (`loadDefaults()`).

**Public API**:
- `buildPrompt(action, ctx, freeQuery)` → `PromptBundle{sytemPrompt, userPrompt}` — resolves the appropriate template with context, supporting selection-aware branching (with/without selected text).
- `actionLabel(action)` / `actionTooltip(action)` — UI metadata for action bar buttons.
- `reload()` — hot-reload from external file (fallback to resource), emits `promptsReloaded()`.

**Template placeholders**: `{fileContent}`, `{selectedText}`, `{language}`, `{extension}`, `{filePath}`, `{freeQuery}`, `{errorStatusCode}`, `{elapsedMs}`, `{memoryKb}`, `{inputData}`, `{expectedOutput}`, `{actualOutput}`, `{errorDetail}`, `{cursorLine}`, `{cursorColumn}`. Supported in both `userPrompt` and `userPromptWithSelection` fields.

**defaultQuery**: per-action fallback used when the resolved user prompt is empty — primarily for FreeChat's empty-input greeting.

**reload() behavior**: always tries external file first; on failure falls to resource. Always emits `promptsReloaded()`.

**Configuration file** (`ai/prompts.json`): JSON format with version field. Each action entry has systemPrompt, userPrompt (template when no selection), userPromptWithSelection (template when text selected, empty → use userPrompt), defaultQuery (optional fallback), label/tooltip (UI metadata).

### SettingsPanel AI Service page (`panels/settingspanel.cpp`)
Category "AI 服务" (index 6) in settings panel sidebar. Controls: API type ComboBox (Anthropic/OpenAI), endpoint LineEdit, API Key LineEdit (password echo), model LineEdit, max_tokens SpinBox (256-16384), system prompt TextEdit. **Prompt 模板** section: external path display (selectable text) + "重新加载 Prompt 模板" button (calls `PromptManager::reload()`, shows ✓ feedback for 2s). Signals aiSettingChanged(key, value). API Key stored via SettingsManager::setAiApiKey() (base64 obfuscation, same as OJ password).

### LoginDialog (`judge/logindialog.h/cpp`)
QDialog: username, password, auto-login checkbox, Login/Skip buttons.

### SettingsPanel (`panels/settingspanel.h/cpp`)
Floating overlay with dimming background. Category sidebar (now 7 pages including Shortcuts) + reset button. Shortcuts page uses interactive KeyRecorder widgets with conflict detection and runtime QAction rebinding. Drag-move, 8-direction edge resize (min 400x300). Toggle via Ctrl+, or toolbar. Persists to config.ini [settings_overrides].

### FlowLayout (`widgets/flowlayout.h/cpp`)
Custom QLayout implementing auto-wrapping. heightForWidth() for constrained containers. Used by breadcrumb bar.

### TabButtonGroup (`widgets/tabbuttongroup.h/cpp`)
QObject subclass encapsulating QPushButton → QStackedWidget tab switching. `addTab(button, index)` wires clicked → switch; optional `StyleProvider` callback for per-tab active/inactive stylesheets. Replaces duplicated tab-switching logic in BottomPanel and AiPanel.

### WindowDragHelper (`panels/windowdraghelper.h/cpp`)
Lightweight non-QObject value-type helper encapsulating mouse-press/move/release drag-to-move boilerplate. `handlePress(widget, event, titleBarHeight)`/`handleMove(widget, event)`/`handleRelease(event)`. Used by HelpPanel and SettingsPanel for consistent frameless dragging.

### StringUtils (`core/utilities.h`)
Header-only namespace. `sanitizeForPython(s)` normalizes line endings and lone surrogates; `completionKindToString(kind)` maps LSP CompletionItemKind codes (1-25) to human-readable strings. Shared by CppCompletionProvider, PythonCompletionProvider, and SmdLspManager.

### MessageRole (`ai/aiproviders.h`)
Standalone header: `enum class MessageRole { User, Assistant, System }`. Replaces separate role enums in `ai/aiproviders.h` and `ai/chatbubble.h` for unified AI message role typing.

### ScrollbarHider (`widgets/scrollbarhider.h/cpp`)
Utility that installs event filters on QAbstractScrollArea to hide scrollbars when not needed. Registered once per editor widget on tab switch via `QSet` cache to avoid duplicate installation.

### HelpPanel (`panels/helppanel.h/cpp`)
Floating overlay help panel, same pattern as SettingsPanel but simpler. Semi-transparent overlay (`rgba(0,0,0,128)`) covers the full window, panel centered on top. Title bar with "帮助" label + close button, drag-move only (no resize). Left: QListWidget (170px) with 14 category items storing section IDs in Qt::UserRole. Right: QTextBrowser loading HTML from `:/help/content` resource. Scroll sync: `showEvent` triggers `computeSectionPositions()` using `QTextDocument::find()` + `blockBoundingRect().y()` for pixel-accurate Y positions; `onScrollChanged` compares scrollbar pixel value against positions and auto-highlights matching category. `m_updatingCategory` guard prevents feedback loop. Toggled via toolbar [帮助] button or F1 shortcut, handled by MainWindow::toggleHelp().

### ThemeManager (`core/thememanager.h/cpp`)
Singleton (QObject) managing VS Code 2026 Dark/Light themes. Built-in `QMap<QString, QColor>` palettes for all UI components (editor, panel, activitybar, scrollbar, input, button, tab, overlay, treeview, separator, labels, accents, close button, slider, toggle, cell, chat, titlebar, menu, statusbar). Theme enum: `Dark=0, Light=1, System=2`. `setTheme()` resolves System via `detectSystemTheme()` (Windows registry → time-based fallback). 5-minute `QTimer` auto-refresh when in System mode. `themeChanged(Theme)` signal emitted on actual change; `applyCurrentTheme()` re-emits for full reload. Semantic color access via `color(key)`/`hex(key)`.

**QSS scope optimization**: `setStyleSheetTarget(QWidget*)` allows limiting `loadQss()` output to a specific widget tree (MainWindow) instead of `qApp->setStyleSheet()`. This avoids Qt re-styling every widget in the application when the theme changes. When target is null, falls back to `qApp->setStyleSheet()` for backward compatibility.

### CaptionBtn (anonymous namespace in `core/mainwindow.cpp`)
Custom `QPushButton` subclass for system-native title bar icons (minimize/maximize/restore/close). Uses `QApplication::style()->standardIcon(SP_TitleBarMinButton etc.)` for icons. `QPainter::fillRect` self-paints hover background for immediate visual response (no Qt style lag). Used in `setupCustomTitleBar()` for the frameless window's right-side window control buttons. Conditionally compiled with `#ifndef Q_OS_MACOS` — macOS uses native traffic light buttons.

### MacOSWindow (`core/macoswindow.h/mm`)
Obj-C++ bridge (`.mm` file) enabling macOS native window behavior through AppKit. Compiled only on Apple platforms via `target_sources` in CMakeLists.txt. `enableFullSizeContentView(QWindow*)` obtains the `NSView` from Qt's `winId()`, traverses to the `NSWindow`, and calls `setTitlebarAppearsTransparent:YES`, `NSWindowStyleMaskFullSizeContentView`, `NSWindowTitleHidden`. This preserves native traffic light buttons while allowing the custom toolbar to render behind them. Called from `MainWindow::showEvent()` via `QTimer::singleShot(0)` to ensure the native NSWindow exists.

### CompilerErrorParser (`runner/compilererrorparser.h`)
Header-only utility namespace. Parses compiler stderr output into `QList<SmdDiagnostic>` for MD preview code block diagnostics. Two functions: `parseCompileErrors()` handles g++ (`file:line:col: error/warning: message`) and MSVC (`file(line,col): error Cxxxx: message`) formats; `parsePythonTraceback()` extracts last `File "...", line N` position and final exception message. Called by `MainWindow::parseAndShowBlockDiagnostics()` after MD code block run completes.

### FileExplorerWidget Toolbar (`panels/fileexplorerwidget.cpp`)
Breadcrumb bar has a new toolbar below it: `QHBoxLayout` with auto-elided folder name label + collapse-all button (calls `collapseAll()` on QTreeView) + refresh button (re-sets `QFileSystemModel` root path). `updateFolderLabel()` uses `QFontMetrics::elidedText` to prevent overflow. `resizeEvent` triggers live elision update.

### Preview Tab System (`editor/tabmanager.h/cpp`)
Single-click in file tree → `openPreview(filePath)`: if file already in a permanent tab, switch to it; if in current preview tab, just switch; if preview tab has different content, replace via `loadFile`; else create new preview tab. Preview tabs rendered in italic by `CustomTabBar::paintEvent` (sets italic font on QPainter before `drawControl`). `promotePreviewToPermanent()` clears preview status and re-renders tab title in normal font. Auto-promotion: `connectEditorSignals` attaches `modificationChanged` → `promotePreviewToPermanent()` when preview editor content is modified. `openFile()` also auto-promotes if the file is currently in the preview tab.

### MD Code Block Diagnostics (`runner/codeblockrunner.h/cpp`, `editor/editorwidget.cpp`, `preview-template.html`)
When ▶ Run is clicked in markdown preview: `EditorWidget::runCodeBlockRequested(lang, code, blockIndex)` → `CodeBlockRunner::runCodeBlock()` saves temp file, buffers stderr through ProcessRunner. On compile/run finished (via `CompileRunManager::compileFinished/runFinished` signals): `parseAndShowBlockDiagnostics()` uses `CompilerErrorParser` to parse stderr, stores in per-file/block diagnostics cache, pushes to `BottomPanel::setDiagnostics()` and `EditorWidget::applyBlockDiagnostics()`. The latter serializes to JSON and calls JS `window.applyBlockDiagnostics()` which wraps `<code>` lines in `<span class="code-line">` and applies wavy underlines. Manual stop skips diagnostics. On tab switch, `CodeBlockRunner::loadDiagnosticsForCurrentTab()` restores cached diagnostics. Preview refresh filter: `extractCodeBlockContents()` snapshots code, only re-applies diagnostics for unchanged blocks.

## Naming Convention

- `[[文件名]]` (WikiLink syntax) — links by filename without path or extension
- `findWikiTarget` performs: exact match → parent-dir-first search → sibling-subtree search → full subtree search
- `BacklinkIndex::resolveTarget` performs: exact path under root → global index lookup by `completeBaseName` → shortest-path tiebreaker (no current-editor bias)
