# Architecture

## Component Map

```
main.cpp                  → QApplication + MainWindow bootstrap
MainWindow (mainwindow.*) → orchestrator: owns all widgets, routes signals/slots — frameless window with toolbar-as-title-bar, window drag/resize via nativeEvent & event()
  ├── ActivityBar         → 48px fixed left bar, 5 SVG icon buttons (Search/AI/Settings/Export PDF/Judge), active state with left border highlight (#0078D4)
  ├── CaptionBtn (anon ns)→ QPushButton subclass, system-native title bar icons (SP_TitleBarMin/Max/Normal/CloseButton), QPainter hover bg
  ├── FileExplorerWidget  → QTreeView + QFileSystemModel, file tree (in splitter, left of editor)
  ├── TabManager          → QTabWidget, owns EditorWidget tabs (center, right splitter top)
  │   └── EditorWidget    → QStackedWidget[WikiLinkTextEdit | QWebEngineView | CodeEditor | QSplitter(edit+preview) | QPdfView | SmdEditor], six-mode editor
  │       ├── WikiLinkTextEdit → QTextEdit subclass with QCompleter for [[wikilink]] autocomplete
  │       ├── CodeEditor  → QPlainTextEdit subclass with line numbers, syntax highlighting, auto-indent
  │       ├── SmdEditor   → QScrollArea-based cell editor for `.smd` files, Jupyter-like command/edit dual mode
  │       │   ├── SmdCell → QFrame subclass, one cell (Markdown/C++/Python) with editor/view stack
  │       │   ├── SmdOutputWidget → per-cell output display (stdout/stderr), 1000-line cap, auto-height
  │       │   ├── SmdDiagnosticsPanel → error/warning summary panel, toggled via Ctrl+D in edit mode
  │       │   └── SmdLspManager   → shared LSP backend for SMD code cells, virtual-document stitching per group. Diagnostics flow: SmdLspManager/CppCompletionProvider/PythonCompletionProvider → diagnosticsUpdated(QList<SmdDiagnostic>) → CodeEditor (squiggly) + BottomPanel/SmdDiagnosticsPanel (list)
  │       └── SmdFormat   → header-only namespace, parse/serialize `---smd:<type>` delimiter format
  ├── RightPanelContainer → Unified right QDockWidget with tab bar (History/Outline/Tags/Backlinks) + QStackedWidget. Toggled via toolbar [面板] button or Ctrl+Shift+E.
  ├── AiPanel             → AI assistant panel (right QDockWidget, tabbed with RightPanelContainer). Toggled via ActivityBar AI button or Ctrl+Shift+A.
  │   ├── ActionBar       → dynamic QHBoxLayout of action buttons, context-sensitive: Markdown actions (改进写作/总结笔记/提取标签/出题自测/翻译) or Code actions (解释代码/寻找Bug/添加注释/优化建议) based on AiContextManager::currentEditorMode()
  │   ├── ChatArea        → QScrollArea with vertical layout of ChatBubble items, auto-scrolls to bottom, supports streaming append
  │   │   └── ChatBubble  → QWidget with role label + QTextBrowser. User bubbles right-aligned blue, Assistant left-aligned gray. Lightweight Markdown→HTML converter for bold/italic/code/link/headings/lists/code blocks.
  │   └── InputBar        → QHBoxLayout: QLineEdit (text input) + QPushButton (发送). ReturnPressed / button click emit sendMessage.
  ├── ErrorJournal        → Singleton (QObject). ErrorRecord struct + JSON persistence to error_journal/records.json. recordFailure() called from JudgePanel::onTestFinished for non-AC results (WA/RE/TLE/MLE). requestAnalysis() builds ErrorAnalysis prompt and streams AI response back into ErrorRecord::aiAnalysis. Signals: analysisReady(recordId), recordsChanged().
  ├── ErrorListPanel      → QWidget with status filter (ComboBox: 全部/WA/RE/TLE/MLE), keyword search (LineEdit), scrollable error list (QListWidget), expand-to-detail view (QStackedWidget switching list↔detail). Detail shows problem info, I/O comparison, AI analysis Markdown rendering, action buttons (re-analyze/delete/review). Delete/delete-all/clear-all operations through ErrorJournal.
  ├── AiContextManager    → Static utility class. collectContext(EditorWidget*) returns ContextBundle{mode, filePath, fileContent, selectedText, language, cursorLine/Column, plus error analysis fields}. currentEditorMode() returns AiEditorMode::Markdown|Code|Unknown. Handles SMD cells, PDF view, code files.
  ├── PromptTemplates     → Header-only (prompttemplates.h). buildPrompt(action, ctx, freeQuery) returns PromptBundle{systemPrompt, userPrompt}. AiAction enum covers 10 actions + FreeChat (added ErrorAnalysis). actionsForMode(mode) maps editor mode to available action list. Metadata in actionInfos() provides display labels/tooltips.
  ├── AiProvider (abs)    → QObject abstract class. Signals: partialResponse(text), finished(), error(msg). Virtual: setApiKey, setModel, setSystemPrompt, setMaxTokens, chatStream(messages), cancel.
  │   ├── AnthropicProvider → POST {endpoint}/messages, x-api-key auth, SSE parse: event/`data:` dual lines → content_block_delta→`delta.text`. 30s timeout. Error codes: 401→invalid key, 429→rate limit.
  │   └── OpenAiProvider  → POST {endpoint}/chat/completions, Bearer auth, SSE parse: `data:` lines → choices[0].delta.content → partialResponse. `data: [DONE]` / finish_reason→finished. 30s timeout. Compatible with DeepSeek, OpenAI, etc.
  ├── AiProviderFactory   → Static factory: createProvider(Anthropic|OpenAiCompatible, parent), typeFromString("Anthropic"|"OpenAI"), availableProviders(). Used in settings page ComboBox + MainWindow::startAiRequest().
  ├── SearchPanel         → QDockWidget + QLineEdit + QListWidget, full-text search (left dock, tabbed with Judge)
  ├── JudgePanel          → QDockWidget + QTableWidget + JudgeEngine, local judge (left dock, tabbed with Search)
  │   └── JudgeEngine     → QObject managing compile→test QProcess pipeline, OJ-style results (AC/WA/RE/TLE/MLE)
  ├── BacklinkIndex       → QMap-based reverse index: target file → source files for `[[wikilinks]]`
  ├── TagIndex            → QMap-based bidirectional index: tag ↔ files for `#tag` syntax
  ├── ConfigManager       → Singleton, reads config.json for static configuration
  ├── SettingsManager     → QSettings wrapper, config.ini for runtime session state
  ├── LanguageUtils       → Extensible language registry: extension→highlighter factory map
  ├── ProcessRunner       → QObject managing compile→run QProcess pipeline
  ├── BottomPanel         → unified bottom panel (right splitter bottom) with two tabs: OutputPanel (terminal stdout/stderr) + DiagnosticsTab (error/warning list by DiagnosticSection). Manages provider connection lifecycle on tab switch.
  │   ├── OutputPanel     → dark-terminal output widget with stop/clear buttons
  │   └── DiagnosticSection → reusable QWidget (header with ▾/▸ toggle + count badge), shared by BottomPanel and SmdDiagnosticsPanel
  ├── SmdDiagnostic        → standalone struct header (`smddiagnostic.h`): cellIndex, startLine/Col, endLine/Col, message, severity (1=Error, 2=Warning). Shared by all diagnostic producers/consumers.
  ├── OpenJudgeWindow     → QMainWindow singleton, browse OpenJudge homework→problems→detail, submit code
  │   └── Crawler         → QNetworkAccessManager + QNetworkCookieJar, HTTP crawler for cxsjsx.openjudge.cn
  ├── SubmitResultPanel   → QWidget, dark-themed submission result display
  ├── LoginDialog         → QDialog, username/password + auto-login checkbox
  ├── KeyRecorder         → Click-to-record widget with Normal/Recording/Cleared states, conflict detection via ShortcutOverride event
  ├── SettingsPanel       → Floating overlay with dimming background, drag/resize, 7 category pages (including Shortcuts with KeyRecorder)
  ├── HelpPanel           → Floating overlay help panel, 14-category sidebar + QTextBrowser, scroll-sync, drag-move only, F1 toggle
  └── FlowLayout          → Custom QLayout subclass, auto-wrapping flow layout
```

## Key Data Flow

- **File opening**: FileExplorerWidget click → TabManager::openFile → EditorWidget::loadFile auto-detects mode by extension (.pdf→PdfView, .smd→SmdEdit, registered code ext→CodeEditor, else→MarkdownEdit).
- **SMD editing**: Cell widgets with header badge + editor/render stack + output area. Default edit mode (blue active border); Esc→command mode (purple active border, A/B insert cell, ↑/↓ navigate, Ctrl+K change cell language, Ctrl+Enter execute, Ctrl+Shift+Z un-render MD, Delete remove). Ctrl+D toggles diagnostics panel. C++ cells auto-group by main() boundaries for per-group compilation; Python cells use a persistent process with shared namespace across cells. Markdown cells toggle rendered view via Ctrl+Enter; code cells (C++/Python) execute via temp file → ProcessRunner → output below cell. Auto-height based on block count (max ~40 lines).
- **Code editing**: Enter auto-indents (copies leading ws, adds level on `{`, splits `{|}`). Tab inserts 4 spaces. Bracket auto-pair for `{}()[]""''` (skips in string/comment). Backspace removes empty pair or deletes to tab-stop.
- **WikiLinks**: `[[filename]]` → regex-converted to `<a href="wikilink:...">` in preview. Click → `acceptNavigationRequest` intercepts `wikilink:` scheme → multi-level filename search → opens existing or prompts create.
- **Preview code block run**: marked renderer adds ▶ button on code blocks. Click stores code in JS → navigates `runblock:execute` → C++ intercepts → saves temp file → ProcessRunner compiles/runs.
- **File indexing**: Async rebuild on folder change via worker thread with cancellation token + generation counter. Synchronous incremental updates on rename/delete/move.
- **Backlinks**: Built asynchronously from scratch. Forward + reverse index. Incremental ops (rebuildFile, rename, delete) remain synchronous.
- **Tags**: Extracted from `.md` files via Unicode-aware regex (skips headings and code blocks). Bidirectional index built async as Phase 3 of startup scan.
- **Rename**: FileExplorerWidget rename → update backlink index → rewrite `[[oldName]]→[[newName]]` in all linking files (reads from open editors if unsaved).
- **Split preview**: Ctrl+P toggles QSplitter with edit left + WebEngine right. 500ms debounce, content-diff guard. Mutually exclusive with full preview.
- **AI assistant (Phase 1)**: ActivityBar AI click or Ctrl+Shift+A → m_dockAi toggle. ActionBar button click → MainWindow::startAiRequest(action) → AiContextManager::collectContext(editor) → buildPrompt(action, ctx) → AiProviderFactory::createProvider(type) → provider.chatStream(messages) → SSE stream → partialResponse→appendToLastAssistant → finished→enable input. FreeChat via InputBar sends AiAction::FreeChat with text, historical messages preserved in m_aiHistory for multi-turn context (pruned at ~maxTokens*4 chars). Actions clear history for fresh context. Settings: API type/endpoint/key/model/max_tokens/system_prompt configurable via SettingsPanel AI Service page (index 6), persisted to config.ini via SettingsManager.
- **Compile & run**: F5/F6/F7 → auto-save unsaved to temp → ProcessRunner (g++/MSVC for C/C++, python for .py). stdin via OutputPanel (child of BottomPanel) event filter (echoes keystrokes, Enter sends line, paste splits multi-line with 20ms timer). Compilation blocks input; 50ms delay before enabling input on run start.
- **Diagnostics**: Two panels — `SmdDiagnosticsPanel` (SMD, per-cell, in SmdEditor splitter) and `BottomPanel` DiagnosticsTab (standalone `.cpp`/`.py`, in bottom splitter). All `CompletionProvider` subclasses emit `diagnosticsUpdated(QList<SmdDiagnostic>)`. C++: clangd `textDocument/publishDiagnostics` → `CppCompletionProvider::parseDiagnostics()`. Python: Jedi `diagnostics` action (base64-encoded code, 500ms debounce on text change). `CodeEditor` caches diagnostics via `setDiagnostics()` → `m_diagnostics`; `MainWindow::currentChanged` restores from cache on tab switch. Non-code-file tabs auto-hide BottomPanel; code-file tabs reconnect provider and load cached diagnostics.
- **Local Judge**: Compile → warmup → per-test execution (1s timeout, 64MB memory limit). Triple-capture memory monitoring. Line-by-line trimmed output comparison. AC/WA/TLE/MLE/RE color-coded table.
- **OpenJudge integration**: Crawler-based HTTP (cxsjsx.openjudge.cn). Homework browsing → problem detail → sample extraction (paired `<pre>` blocks) → cache to temp → inject into JudgePanel. Submission: POST raw source (percent-encoded, no base64) → poll 30s for result → show SubmitResultPanel.

## Component Details

### EditorWidget
Six-mode QStackedWidget. Page 0 `WikiLinkTextEdit`, page 1 full preview WebEngine, page 2 `CodeEditor`, page 3 QSplitter(edit+preview), page 4 QPdfView, page 5 `SmdEditor`. Modes are mutually exclusive: enabling split preview transfers `m_textEdit` between page 0 and page 3's splitter. PdfView and SmdEdit skip preview/wikilink operations. Preview pipeline shares marked.js + KaTeX + Mermaid across full and split modes. Code blocks are pre-highlighted in C++ (regex-based, matching highlighter colors), base64-encoded into custom fenced blocks. Zoom 0.5–3.0, step 0.1. 300ms debounce clears modified flag on text revert.

### PDF export
Temporary hidden QWebEngineView loads light-themed preview, waits for Mermaid async rendering via JS Promise polling, then calls `printToPdf()`.

### SmdEditor (`smdeditor.h/cpp`)
QScrollArea of SmdCell widgets in QVBoxLayout. Dual mode: edit (default, blue active border) and command (Esc, purple active border). Owns separate ProcessRunner for cell execution. C++ cells auto-group by `main()` boundaries — only cells in the same group are compiled together for each execution. Python cells use a persistent process (`python_executor.py`) that maintains a shared namespace across the same `.smd` file, enabling cross-cell variable reuse. `Ctrl+D` toggles `SmdDiagnosticsPanel` (public `toggleDiagnosticsPanel()`) for aggregated error/warning display. Temp files: `smd_cell_<PID>_<counter>.ext`. Modification tracking compares serialized content with original. Forwards `modificationChanged`/`fileLoaded`/`fileSaved` to EditorWidget. **eventFilter**: installed on QApplication (global, for FocusIn/MousePress cell activation), self, and top-level window. Keyboard shortcut entry has a widget-hierarchy guard — only processes shortcuts for widgets within this SmdEditor, preventing global filter from stealing shortcuts from CodeEditor.

### SmdCell (`smdcell.h/cpp`)
QFrame: header badge (MD blue/C++ green/Python yellow) + QStackedWidget(editor↔render) + hidden output area. Markdown uses QPlainTextEdit, code uses CodeEditor with highlighter. Auto-height via blockCount (min 1 line, max ~40, no internal scrollbar). Active cell borders: 2px blue (`#0078d4`) in edit mode, 2px purple (`#C586C0`) in command mode; inactive command-mode gray (`#3c3c3c`); edit mode transparent.

### SmdOutputWidget (`smdoutputwidget.h/cpp`)
Per-cell output display for SMD code cell execution. Dark terminal style (#1E1E1E bg), monospace font. 1000-line hard cap; auto-height limited to ~15 visible lines with internal scrollbar beyond that. stdout white, stderr red. Each SmdOutputWidget corresponds one-to-one with a SmdCell, managed by SmdEditor.

### SmdDiagnosticsPanel (`smddiagnosticspanel.h/cpp`)
QFrame-based diagnostics summary panel, toggled via Ctrl+D in edit mode. Embedded in SmdEditor's splitter, default hidden. Contains two `DiagnosticSection` widgets (Error red #F44747 / Warning yellow #CCA700) listing all LSP diagnostics from SmdLspManager. Each entry shows cell index and line number; click jumps to the corresponding cell and line. Supports debounced refresh (scheduleRefresh + 50ms timer). Shares `DiagnosticSection` component with `BottomPanel`'s diagnostics tab.

### SmdLspManager (`smdlspmanager.h/cpp`)
Shared LSP backend for SMD code cells. Manages per-language LSP client adapters (clangd for C++, Jedi for Python) with virtual-document stitching — groups related cells into a single virtual translation unit for clangd, or presents individual cells to Python LSP. Tracks cell order and content changes, translates `contentChanged` signals into `textDocument/didChange` notifications. Aggregates diagnostics (red/yellow squiggles) from all cells and forwards to SmdCell::setDiagnostics() and CodeEditor::setDiagnostics(). Created and destroyed on each file load; coordinates with SmdEditor for C++ program group detection.

### SmdFormat (`smdformat.h`)
Header-only. `---smd:<type>` delimiters. Parse splits on regex, trims leading/trailing blank lines. Serialize writes delimiter + content.

### FileExplorerWidget
QFileSystemModel + custom QSortFilterProxyModel (folders-first). NoGhostDelegate prevents text ghosting during inline rename (empty name auto-restores). Drag-drop moves within root only. Breadcrumb bar using FlowLayout.

### TabManager
CustomTabBar constrains drag bounds. Custom save prompt on close. batch updatePathsAfterMove for file tree drag operations.

### HistoryPanel
Max 50 entries, auto-dedup by path. Persisted at shutdown only (SettingsManager). Auto-removes deleted entries on next access. Hosted inside RightPanelContainer (right dock, tab 0).

### SearchPanel
Full-text via QDirIterator + scanning. 20 matches/file, 500 total. Gold (#FFD700) highlights on result click. Left dock (tabbed with JudgePanel). Persistent sidebar (no auto-hide).

### RightPanelContainer (`rightpanelcontainer.h/cpp`)
Unified right QDockWidget. Top 32px tab bar with 4 icon+text tabs (History/Outline/Tags/Backlinks), bottom QStackedWidget. Toggled via toolbar [面板] button or Ctrl+Shift+E. Click-outside auto-hides via MainWindow::eventFilter. Sub-panels: HistoryPanel (tab 0), OutlinePanel (tab 1), TagPanel (tab 2), BacklinksPanel (tab 3).

### OutlinePanel (`outlinepanel.h/cpp`)
Title outline panel hosted in RightPanelContainer (tab 1). Parses current document headings (`#` to `######`, skipping fenced code blocks) and displays them in an indented tree with hierarchical brightness (h1 brightest, h6 dimmest, h1/h2 bold). Clicking a heading jumps to the corresponding line in the editor. Auto-refreshes on tab switch and file save; clears for non-Markdown files.

### BacklinkIndex
Reverse index (target→sources) + forward index (source→targets) for `[[wikilinks]]`. Async full build via static `buildFromPath()`. Target resolution: exact path → global index by baseName → shortest-path tiebreaker. Unlike `findWikiTarget` (which disambiguates via current editor context), `resolveTarget` is purely deterministic with no editor bias. Incremental ops synchronous.

### BacklinksPanel
Display-only QListWidget (NoSelection). Hosted inside RightPanelContainer (right dock, tab 3). Shows placeholder when empty. Min width 200px.

### TagIndex
Non-QObject bidirectional index. Unicode-aware regex `(*UCP)#(?!\s)([\w-]+)`. Scans only `.md`/`.markdown`. Async Phase 3 of startup scan.

### TagPanel
Dual-mode: tag list ↔ files per tag. Back button to return. Hosted inside RightPanelContainer (right dock, tab 2).

### ConfigManager
Singleton, reads config.json. Dot-path resolution (e.g. `"editor.zoom.min"`). Built-in defaults for missing file. All static configuration.

### SettingsManager
Writes config.ini next to executable. Runtime state: geometry, recent files, OJ credentials (base64-obfuscated), user overrides. Singleton with override→ConfigManager→default chain.

### TextFileUtils (`fileutils.h`)
Header-only. 40+ text extension list + scan name filters.

### WikiLinkTextEdit (`wikilinktextedit.h/cpp`)
QTextEdit with QCompleter. `[[` triggers filename popup (case-insensitive prefix). `#` triggers tag autocomplete. Tab accepts, first item auto-selected.

### CodeEditor (`codeeditor.h/cpp`)
QPlainTextEdit with line numbers, auto-indent, bracket completion, search highlights, code completion. Dark theme Consolas 12pt. setLanguage() installs highlighter via LanguageUtils and creates a CompletionProvider (CppCompletionProvider / PythonCompletionProvider / KeywordCompletionProvider fallback). setDocumentUri() sets file identity for LSP diagnostics. Owns CompletionPopup, HoverManager, SignatureHelpManager. EscNativeFilter (Windows native event filter) catches VK_ESCAPE to close tool windows when Qt routing fails. **Diagnostics**: connects `provider->diagnosticsUpdated` → `setDiagnostics()` to cache `m_diagnostics` and draw squiggly underlines. `diagnostics()` getter exposes cache for BottomPanel restoration on tab switch. **Ctrl+D shortcut**: `m_toggleDiagnostics` QKeySequence; eventFilter handles ShortcutOverride (accept) + KeyPress (emit `diagnosticsToggleRequested()`); keyPressEvent has backup matchShortcut check.

### CompletionProvider (`completionprovider.h`)
Abstract QObject interface. Defines CompletionItem/HoverInfo/SignatureInfo structs. Pure virtual: requestCompletion, requestHover, requestSignatureHelp. Virtual no-op openDocument/updateText (overridden by LSP providers for text sync). Signals: completionReady, hoverReady, signatureHelpReady, **diagnosticsUpdated(QList<SmdDiagnostic>)**. Includes `smddiagnostic.h` for the shared `SmdDiagnostic` struct.

### LspClient (`lspclient.h/cpp`)
QProcess wrapper for LSP JSON-RPC 2.0 over stdin/stdout with Content-Length framing. start() launches server process. sendRequest/sendNotification with auto-incrementing message IDs. Signals: responseReceived(id, result), notificationReceived(method, params), serverError, serverStopped. Internal parseFrames() buffers and splits incoming frames from raw byte stream.

### CppCompletionProvider (`cppcompletionprovider.h/cpp`)
CompletionProvider for C/C++ via clangd LSP. Starts clangd with --fallback-style=Google (no compile_commands.json required). Sends initialize → initialized → didOpen → didChange → [completion|hover|signatureHelp]. Tracks pending requests with 500ms timeout QTimer. Parses LSP CompletionList/Hover/SignatureHelp responses into provider structs. Auto-restarts on crash. **Diagnostics**: `onNotificationReceived()` intercepts `textDocument/publishDiagnostics`, `parseDiagnostics()` converts LSP JSON ranges → `QList<SmdDiagnostic>` (cellIndex=0 for flat files), emits `diagnosticsUpdated`.

### PythonCompletionProvider (`pythoncompletionprovider.h/cpp`)
CompletionProvider for Python via Jedi helper process. Launches `python completion_helper.py` as QProcess, communicates via stdin/stdout JSON (actions: "complete"/"hover"/"signature"/"diagnostics"). Sends full code per request (no incremental sync needed). 500ms timeout fallback. Detects Jedi import failure and signals serverFailed for fallback chain. **Diagnostics**: `openDocument()` triggers immediate `sendDiagnosticsRequest()`; `updateText()` starts 500ms debounce timer → `onDiagnosticsDebounce()` → `sendDiagnosticsRequest()`. Sends base64-encoded code as single cell (`cellIndex=0`, action="diagnostics"). `processResponse()` parses diagnostic list from helper and emits `diagnosticsUpdated`. Unlike C++ clangd (push), Python diagnostics are pull-based.

### KeywordCompletionProvider (`keywordcompletionprovider.h/cpp`)
Fallback CompletionProvider used when clangd/Jedi are unavailable. Filters language keywords (C++/Python) and document words from current editor text by prefix match. Hover and signature help return empty results (no-op).

### CompletionPopup (`completionpopup.h/cpp`)
Floating QWidget (Qt::ToolTip, borderless) overlay for completion item selection. Dark theme (#252526 bg, #094771 selection). Custom CompletionItemDelegate draws icon + name + type tag per row. Bottom hint bar shows keyboard shortcuts. Signals itemSelected on Enter/Tab. Uses QListWidget internally. iconForType maps type strings to QIcon (function → f() icon, class → C:, variable → V:, etc.).

### HoverManager (`hovermanager.h/cpp`)
QObject event filter installed on CodeEditor viewport. 400ms QTimer debounce on mouse hover; Ctrl modifier bypasses delay. Calls requestHover on the active CompletionProvider. Shows result in a QToolTip popup with signature (bold header) + doc + definition location. Auto-hides on mouse move or key press.

### SignatureHelpManager (`signaturehelpmanager.h/cpp)
Monitors CodeEditor::cursorPositionChanged. Detects unmatched `(` by scanning backwards with parenthesis counter. 200ms debounce before request. SignatureHelpPopup (inner QWidget subclass) displays: overload navigation (◀ 1/2 ▶), signature with active parameter highlighted in yellow, and doc text. Closes on `)`, Esc, or mouse click outside. Up/Down arrow navigates overloads.

### LanguageUtils (`languageutils.h/cpp`)
Singleton language registry. Currently cpp (extensions: cpp/hpp/cxx/cc/c/h/hxx/hh) → CppSyntaxHighlighter and python (py/pyw/pyx) → PythonSyntaxHighlighter. Adding a language = 1 map entry + 1 highlighter file + .pro entries.

### CppSyntaxHighlighter
QSyntaxHighlighter. Dark theme: keywords #569CD6, preprocessor #C586C0, types #4EC9B0, strings #CE9178, numbers #B5CEA8, comments #6A9955. Multi-line comment block-state tracking.

### PythonSyntaxHighlighter
QSyntaxHighlighter. Dark theme colors. Supports f-strings and raw strings. Triple-quote (`"""`/`'''`) block-state tracking.

### CompilerUtils (`compilerutils.h`)
Header-only. Detects g++ (QStandardPaths), MSVC cl.exe (VSCMD_VER env), python (QStandardPaths). Compile args: g++ `-std=c++17 -Wall -Wextra`, cl `/std:c++17 /W4 /EHsc`.

### ProcessRunner (`processrunner.h/cpp`)
Two-phase compile→run via sequential QProcess. Methods: startCompile, startRun, startCompileAndRun (auto-transitions), startRunPython. writeInput(text) appends `\n`; writeRaw doesn't. isAcceptingInput() false during compile. Output raw (no .trimmed()). Signals: compileFinished, runFinished, processStarted, processStopped.

### OutputPanel (`outputpanel.h/cpp`)
Bottom QPlainTextEdit (Consolas 10pt, #1E1E1E bg). stdout white, stderr red. stdin terminal mode via eventFilter when process running. Compilation blocks all input. On stop: focus returns to editor. Now a child tab of `BottomPanel`; close button moved to BottomPanel header bar.

### BottomPanel (`bottompanel.h/cpp`)
Unified bottom panel (QSplitter bottom) replacing standalone OutputPanel. 28px header bar with tab buttons (输出/诊断) + close button. QStackedWidget: index 0 = OutputPanel, index 1 = diagnostics page (QScrollArea with two `DiagnosticSection` widgets for errors/warnings). Stores `QList<SmdDiagnostic> m_diagnostics` and `CodeEditor *m_currentEditor`. `rebuildDiagnostics()` auto-hides sections with zero count, shows "无诊断信息" placeholder when all empty. `setCurrentEditor()` tracks which editor diagnostics belong to. Uses `DiagnosticSection` from `smddiagnosticspanel.h` (shared with `SmdDiagnosticsPanel`).

### JudgeEngine (`judgeengine.h/cpp`)
Discovers `.in`/`.out` pairs. Compile → warmup (empty stdin, discarded output — populates OS cache for stable first test result) → per-test execution. 1000ms timeout (TLE). 64MB memory limit with triple-capture monitoring. m_testHandled prevents dual-fire between timeout and process finish. Output: trimmed line-by-line comparison. TestResult struct includes `inputData` (read from `.in` file) for ErrorJournal recording.

### JudgePanel (`judgepanel.h/cpp`)
Folder selector + OpenJudge buttons + 5-column result table + detail view. Owns JudgeEngine. Emits runAllRequested, openJudgeRequested, submitToOpenJudgeRequested. In `onTestFinished()`, non-AC results call `ErrorJournal::instance().recordFailure()`.

### ErrorJournal (`ai/errorjournal.h/cpp`)
Singleton managing persistent error records from Judge failures. `ErrorRecord` struct: id (UUID), problemName, sourceFile, testFolder, testCaseName, statusCode, elapsedMs, memoryKb, inputData, actualOutput, expectedOutput, detail, aiAnalysis (async-filled), tags, timestamp, reviewed. JSON storage at `{executable_dir}/error_journal/records.json` (version 1). `recordFailure()` called from JudgePanel for WA/RE/TLE/MLE results. `requestAnalysis(id)` reads source code from disk, builds ErrorAnalysis prompt (via PromptTemplates), creates configured AiProvider, streams response into `aiAnalysis` field. Signals: `analysisReady(recordId)`, `recordsChanged()`.

### ErrorListPanel (`ai/errorlistpanel.h/cpp`)
QWidget with two views: list (filtered error records) and detail (expanded record info). Filter bar: status ComboBox (全部/WA/RE/TLE/MLE) + keyword search LineEdit. List items show status icon (AC green/RE red/TLE yellow/MLE purple), problem name, source file, timestamp, tags. Detail view: problem header (status/time/memory), file info, I/O comparison (input/expected/actual in monospace), AI analysis section (QTextBrowser with Markdown rendering), action buttons (重新分析/删除/已阅/返回列表). Delete-all, delete-one, mark-reviewed all go through ErrorJournal. Signals: errorClicked, deleteRecordRequested, deleteAllRequested.

### OpenJudgeWindow (`openjudgewindow.h/cpp`)
Independent QMainWindow for browsing. Three states: homework list → problem list → problem detail. Sample extraction via \<pre\> pairing. Auto-login via SettingsManager (base64-obfuscated). Managed as QPointer singleton in MainWindow.

### Crawler (`crawler.h/cpp`)
HTTP crawler for cxsjsx.openjudge.cn. QNetworkAccessManager + cookie jar. Login via JSON API. Submit posts raw source (percent-encoded, avoids `+`→space corruption), polls 30s for result. Debug log to `crawler_debug.log`.

### SubmitResultPanel (`submissionpanel.h/cpp`)
Dark-themed result display. Color-coded status (AC green, WA/RE red, etc.) + time/memory + collapsible CE error log.

### AiPanel (`ai/aipanel.h/cpp`)
QDockWidget content widget for AI assistant. 340px default width. Title bar with two tab buttons ("AI 助手" / "错题本 (N)") + "清空对话" button (chat only). QStackedWidget switches between ChatArea (index 0) and ErrorListPanel (index 1). onTabSwitch() hides ActionBar and InputBar on error tab, shows them on chat tab. updateErrorBadge() shows record count. Signals: sendMessage(text), actionTriggered(index), clearRequested(), errorSelected(recordId). Public API: addUserMessage, addAssistantMessage, appendToLastAssistant (streaming), clearChat, setInputEnabled, setCurrentTab(index), errorListPanel(). Provides lastAssistantContent() and hasStreamingTarget() for response state tracking.

### ActionBar (`ai/actionbar.h/cpp`)
Dynamic QHBoxLayout of QPushButtons. setActions(QVector<AiAction>) clears old buttons and creates new ones with labels/tooltips from ActionInfo. Emits actionTriggered(AiAction). Context-dependent: Markdown mode shows 5 buttons (改进写作/总结笔记/提取标签/出题自测/翻译), Code mode shows 4 (解释代码/寻找Bug/添加注释/优化建议). Dark theme buttons (#3c3c3c bg, #ccc text, #0078d4 hover border).

### ChatArea (`ai/chatarea.h/cpp`)
QScrollArea with vertical QVBoxLayout of ChatBubble widgets. addMessage(role, text) creates new bubble. appendToLastMessage(text) appends to last assistant bubble for streaming. messageCount() and lastBubble() for state queries. Auto-scrolls to bottom on new content. clear() removes all bubbles.

### ChatBubble (`ai/chatbubble.h/cpp`)
QWidget per message. Two roles: User (right-aligned, blue bg #1a3a5c) and Assistant (left-aligned, gray bg #2d2d2d). Role label above bubble ("你" blue / "AI 助手" green). Uses QTextBrowser for HTML rendering. Lightweight Markdown→HTML converter (markdownToHtml): supports **bold**, *italic*, `inline code`, [links](url), `#`/`##` headings, `-`/`*` lists, numbered lists, fenced ```code blocks```, `---` horizontal rules. Code blocks styled with #1e1e1e bg. setText()/appendText() update and auto-size height via QTextDocument::size().

### AiContextManager (`ai/aicontextmanager.h/cpp`)
Static utility class. collectContext(EditorWidget*) returns ContextBundle: mode (Markdown/Code/Unknown), filePath, fileContent (full when no selection), selectedText, language (via LanguageUtils + extra map for html/css/js/go/rust etc.), cursorLine/Column, plus error analysis fields (errorStatusCode, actualOutput, expectedOutput, inputData, elapsedMs, memoryKb, errorDetail). currentEditorMode(editor) detects mode: SMD cells check active cell type, code files → Code, everything else → Markdown. languageForFile(ext) maps extensions to language strings (markdown, cpp, python, html, javascript, etc.).

### AiProvider (`ai/aiprovider.h`)
Abstract QObject base class. Message struct with User/Assistant/System roles and roleToJson() conversion. Interface: setApiKey, setModel, setSystemPrompt, setMaxTokens, chatStream(messages), cancel(). Signals: partialResponse(text), finished(), error(message).

### AnthropicProvider (`ai/anthropicprovider.h/cpp`)
POST to `{endpoint}/messages`. Headers: x-api-key, anthropic-version: 2023-06-01, application/json. Body: {model, max_tokens, stream:true, system, messages[{role, content:[{type:"text", text}]}]}. SSE parsing: event/data dual-line frames → content_block_delta→delta.text→partialResponse, message_stop→finished, error→error(). 30s timeout via QTimer. Error HTTP 401/403→invalid key, 429→rate limit.

### OpenAiProvider (`ai/openaiprovider.h/cpp`)
POST to `{endpoint}/chat/completions`. Headers: Bearer auth, application/json. Body: {model, max_tokens, stream:true, messages[{system, user, assistant}]} (OpenAI format). SSE parsing: data: lines → choices[0].delta.content→partialResponse, finish_reason or data: [DONE]→finished, error→error(). Default endpoint https://api.deepseek.com/v1. 30s timeout. Compatible with DeepSeek, OpenAI, etc.

### AiProviderFactory (`ai/aiproviderfactory.h/cpp`)
Static factory. enum ProviderType { Anthropic, OpenAiCompatible }. createProvider(type, parent) returns new AnthropicProvider or OpenAiProvider. typeFromString("Anthropic"/"OpenAI") maps settings panel strings. availableProviders() returns {"Anthropic", "OpenAI"}.

### PromptTemplates (`ai/prompttemplates.h`)
Header-only. buildPrompt(action, ctx, freeQuery) returns PromptBundle{systemPrompt, userPrompt}. 10 predefined AiActions: ImproveWriting, SummarizeNote, ExtractTags, SelfTest, Translate (Markdown); ExplainCode, FindBugs, AddComments, OptimizeCode (Code); ErrorAnalysis (Judge — no button, triggered by ErrorJournal); FreeChat (general). Each has tailored Chinese system prompt and user prompt template. ErrorAnalysis uses ctx.errorStatusCode/elapsedMs/inputData/expectedOutput/actualOutput/errorDetail fields for the prompt. actionsForMode(mode) returns appropriate action list per editor context. ActionInfo struct and actionInfos() supply display labels and tooltips for UI buttons.

### SettingsPanel AI Service page (`settingspanel.cpp`)
Category "AI 服务" (index 6) in settings panel sidebar. Controls: API type ComboBox (Anthropic/OpenAI), endpoint LineEdit, API Key LineEdit (password echo), model LineEdit, max_tokens SpinBox (256-16384), system prompt TextEdit. Signals aiSettingChanged(key, value). API Key stored via SettingsManager::setAiApiKey() (base64 obfuscation, same as OJ password).

### LoginDialog (`logindialog.h/cpp`)
QDialog: username, password, auto-login checkbox, Login/Skip buttons.

### SettingsPanel (`settingspanel.h/cpp`)
Floating overlay with dimming background. Category sidebar (now 7 pages including Shortcuts) + reset button. Shortcuts page uses interactive KeyRecorder widgets with conflict detection and runtime QAction rebinding. Drag-move, 8-direction edge resize (min 400x300). Toggle via Ctrl+, or toolbar. Persists to config.ini [settings_overrides].

### FlowLayout (`flowlayout.h/cpp`)
Custom QLayout implementing auto-wrapping. heightForWidth() for constrained containers. Used by breadcrumb bar.

### HelpPanel (`helppanel.h/cpp`)
Floating overlay help panel, same pattern as SettingsPanel but simpler. Semi-transparent overlay (`rgba(0,0,0,128)`) covers the full window, panel centered on top. Title bar with "帮助" label + close button, drag-move only (no resize). Left: QListWidget (170px) with 14 category items storing section IDs in Qt::UserRole. Right: QTextBrowser loading HTML from `:/help/content` resource. Scroll sync: `showEvent` triggers `computeSectionPositions()` using `QTextDocument::find()` + `blockBoundingRect().y()` for pixel-accurate Y positions; `onScrollChanged` compares scrollbar pixel value against positions and auto-highlights matching category. `m_updatingCategory` guard prevents feedback loop. Toggled via toolbar [帮助] button or F1 shortcut, handled by MainWindow::toggleHelp().

## Naming Convention

- `[[文件名]]` (WikiLink syntax) — links by filename without path or extension
- `findWikiTarget` performs: exact match → parent-dir-first search → sibling-subtree search → full subtree search
- `BacklinkIndex::resolveTarget` performs: exact path under root → global index lookup by `completeBaseName` → shortest-path tiebreaker (no current-editor bias)
