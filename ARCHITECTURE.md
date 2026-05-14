# Architecture

## Component Map

```
main.cpp                  → QApplication + MainWindow bootstrap
MainWindow (mainwindow.*) → orchestrator: owns all widgets, routes signals/slots — frameless window with toolbar-as-title-bar, window drag/resize via nativeEvent & event()
  ├── ActivityBar         → 48px fixed left bar, 4 SVG icon buttons (Search/Settings/Export PDF/Judge), active state with left border highlight (#0078D4)
  ├── CaptionBtn (anon ns)→ QPushButton subclass, system-native title bar icons (SP_TitleBarMin/Max/Normal/CloseButton), QPainter hover bg
  ├── FileExplorerWidget  → QTreeView + QFileSystemModel, file tree (in splitter, left of editor)
  ├── TabManager          → QTabWidget, owns EditorWidget tabs (center, right splitter top)
  │   └── EditorWidget    → QStackedWidget[WikiLinkTextEdit | QWebEngineView | CodeEditor | QSplitter(edit+preview) | QPdfView | SmdEditor], six-mode editor
  │       ├── WikiLinkTextEdit → QTextEdit subclass with QCompleter for [[wikilink]] autocomplete
  │       ├── CodeEditor  → QPlainTextEdit subclass with line numbers, syntax highlighting, auto-indent
  │       ├── SmdEditor   → QScrollArea-based cell editor for `.smd` files, Jupyter-like command/edit dual mode
  │       │   └── SmdCell → QFrame subclass, one cell (Markdown/C++/Python) with editor/view stack + output area
  │       └── SmdFormat   → header-only namespace, parse/serialize `---smd:<type>` delimiter format
  ├── RightPanelContainer → Unified right QDockWidget with tab bar (History/Outline/Tags/Backlinks) + QStackedWidget. Toggled via toolbar [面板] button or Ctrl+Shift+E.
  ├── SearchPanel         → QDockWidget + QLineEdit + QListWidget, full-text search (left dock, tabbed with Judge)
  ├── JudgePanel          → QDockWidget + QTableWidget + JudgeEngine, local judge (left dock, tabbed with Search)
  │   └── JudgeEngine     → QObject managing compile→test QProcess pipeline, OJ-style results (AC/WA/RE/TLE/MLE)
  ├── BacklinkIndex       → QMap-based reverse index: target file → source files for `[[wikilinks]]`
  ├── TagIndex            → QMap-based bidirectional index: tag ↔ files for `#tag` syntax
  ├── ConfigManager       → Singleton, reads config.json for static configuration
  ├── SettingsManager     → QSettings wrapper, config.ini for runtime session state
  ├── LanguageUtils       → Extensible language registry: extension→highlighter factory map
  ├── ProcessRunner       → QObject managing compile→run QProcess pipeline
  ├── OutputPanel         → Bottom QDockWidget (right splitter bottom), dark-terminal output with stop/clear buttons
  ├── OpenJudgeWindow     → QMainWindow singleton, browse OpenJudge homework→problems→detail, submit code
  │   └── Crawler         → QNetworkAccessManager + QNetworkCookieJar, HTTP crawler for cxsjsx.openjudge.cn
  ├── SubmitResultPanel   → QWidget, dark-themed submission result display
  ├── LoginDialog         → QDialog, username/password + auto-login checkbox
  ├── SettingsPanel       → Floating overlay with dimming background, drag/resize
  └── FlowLayout          → Custom QLayout subclass, auto-wrapping flow layout
```

## Key Data Flow

- **File opening**: FileExplorerWidget click → TabManager::openFile → EditorWidget::loadFile auto-detects mode by extension (.pdf→PdfView, .smd→SmdEdit, registered code ext→CodeEditor, else→MarkdownEdit).
- **SMD editing**: Cell widgets with header badge + editor/render stack + output area. Default edit mode; Esc→command mode (A/B insert cell, ↑/↓ navigate, Ctrl+K change cell language, Ctrl+Enter execute, Ctrl+Shift+Z un-render MD, Delete remove, Ctrl+D duplicate). Markdown cells toggle rendered view via Ctrl+Enter; code cells (C++/Python) execute via temp file → ProcessRunner → output below cell. Auto-height based on block count (max ~40 lines).
- **Code editing**: Enter auto-indents (copies leading ws, adds level on `{`, splits `{|}`). Tab inserts 4 spaces. Bracket auto-pair for `{}()[]""''` (skips in string/comment). Backspace removes empty pair or deletes to tab-stop.
- **WikiLinks**: `[[filename]]` → regex-converted to `<a href="wikilink:...">` in preview. Click → `acceptNavigationRequest` intercepts `wikilink:` scheme → multi-level filename search → opens existing or prompts create.
- **Preview code block run**: marked renderer adds ▶ button on code blocks. Click stores code in JS → navigates `runblock:execute` → C++ intercepts → saves temp file → ProcessRunner compiles/runs.
- **File indexing**: Async rebuild on folder change via worker thread with cancellation token + generation counter. Synchronous incremental updates on rename/delete/move.
- **Backlinks**: Built asynchronously from scratch. Forward + reverse index. Incremental ops (rebuildFile, rename, delete) remain synchronous.
- **Tags**: Extracted from `.md` files via Unicode-aware regex (skips headings and code blocks). Bidirectional index built async as Phase 3 of startup scan.
- **Rename**: FileExplorerWidget rename → update backlink index → rewrite `[[oldName]]→[[newName]]` in all linking files (reads from open editors if unsaved).
- **Split preview**: Ctrl+P toggles QSplitter with edit left + WebEngine right. 500ms debounce, content-diff guard. Mutually exclusive with full preview.
- **Compile & run**: F5/F6/F7 → auto-save unsaved to temp → ProcessRunner (g++/MSVC for C/C++, python for .py). stdin via OutputPanel event filter (echoes keystrokes, Enter sends line, paste splits multi-line with 20ms timer). Compilation blocks input; 50ms delay before enabling input on run start.
- **Local Judge**: Compile → warmup → per-test execution (1s timeout, 64MB memory limit). Triple-capture memory monitoring. Line-by-line trimmed output comparison. AC/WA/TLE/MLE/RE color-coded table.
- **OpenJudge integration**: Crawler-based HTTP (cxsjsx.openjudge.cn). Homework browsing → problem detail → sample extraction (paired `<pre>` blocks) → cache to temp → inject into JudgePanel. Submission: POST raw source (percent-encoded, no base64) → poll 30s for result → show SubmitResultPanel.

## Component Details

### EditorWidget
Six-mode QStackedWidget. Page 0 `WikiLinkTextEdit`, page 1 full preview WebEngine, page 2 `CodeEditor`, page 3 QSplitter(edit+preview), page 4 QPdfView, page 5 `SmdEditor`. Modes are mutually exclusive: enabling split preview transfers `m_textEdit` between page 0 and page 3's splitter. PdfView and SmdEdit skip preview/wikilink operations. Preview pipeline shares marked.js + KaTeX + Mermaid across full and split modes. Code blocks are pre-highlighted in C++ (regex-based, matching highlighter colors), base64-encoded into custom fenced blocks. Zoom 0.5–3.0, step 0.1. 300ms debounce clears modified flag on text revert.

### PDF export
Temporary hidden QWebEngineView loads light-themed preview, waits for Mermaid async rendering via JS Promise polling, then calls `printToPdf()`.

### SmdEditor (`smdeditor.h/cpp`)
QScrollArea of SmdCell widgets in QVBoxLayout. Dual mode: edit (default) and command (Esc). Owns separate ProcessRunner for cell execution. Temp files: `smd_cell_<PID>_<counter>.ext`. Modification tracking compares serialized content with original. Forwards `modificationChanged`/`fileLoaded`/`fileSaved` to EditorWidget.

### SmdCell (`smdcell.h/cpp`)
QFrame: header badge (MD blue/C++ green/Python yellow) + QStackedWidget(editor↔render) + hidden output area. Markdown uses QPlainTextEdit, code uses CodeEditor with highlighter. Auto-height via blockCount (min 1 line, max ~40, no internal scrollbar). Active cell blue border; inactive command-mode gray; edit mode transparent.

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
QPlainTextEdit with line numbers, auto-indent, bracket completion, search highlights. Dark theme Consolas 12pt. setLanguage() installs highlighter via LanguageUtils.

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
Bottom dock QPlainTextEdit (Consolas 10pt, #1E1E1E bg). stdout white, stderr red. stdin terminal mode via eventFilter when process running. Compilation blocks all input. On stop: focus returns to editor.

### JudgeEngine (`judgeengine.h/cpp`)
Discovers `.in`/`.out` pairs. Compile → warmup (empty stdin, discarded output — populates OS cache for stable first test result) → per-test execution. 1000ms timeout (TLE). 64MB memory limit with triple-capture monitoring. m_testHandled prevents dual-fire between timeout and process finish. Output: trimmed line-by-line comparison.

### JudgePanel (`judgepanel.h/cpp`)
Folder selector + OpenJudge buttons + 5-column result table + detail view. Owns JudgeEngine. Emits runAllRequested, openJudgeRequested, submitToOpenJudgeRequested.

### OpenJudgeWindow (`openjudgewindow.h/cpp`)
Independent QMainWindow for browsing. Three states: homework list → problem list → problem detail. Sample extraction via \<pre\> pairing. Auto-login via SettingsManager (base64-obfuscated). Managed as QPointer singleton in MainWindow.

### Crawler (`crawler.h/cpp`)
HTTP crawler for cxsjsx.openjudge.cn. QNetworkAccessManager + cookie jar. Login via JSON API. Submit posts raw source (percent-encoded, avoids `+`→space corruption), polls 30s for result. Debug log to `crawler_debug.log`.

### SubmitResultPanel (`submissionpanel.h/cpp`)
Dark-themed result display. Color-coded status (AC green, WA/RE red, etc.) + time/memory + collapsible CE error log.

### LoginDialog (`logindialog.h/cpp`)
QDialog: username, password, auto-login checkbox, Login/Skip buttons.

### SettingsPanel (`settingspanel.h/cpp`)
Floating overlay with dimming background. Category sidebar (6 pages) + reset button. Drag-move, 8-direction edge resize (min 400x300). Toggle via Ctrl+, or toolbar. Persists to config.ini [settings_overrides].

### FlowLayout (`flowlayout.h/cpp`)
Custom QLayout implementing auto-wrapping. heightForWidth() for constrained containers. Used by breadcrumb bar.

## Naming Convention

- `[[文件名]]` (WikiLink syntax) — links by filename without path or extension
- `findWikiTarget` performs: exact match → parent-dir-first search → sibling-subtree search → full subtree search
- `BacklinkIndex::resolveTarget` performs: exact path under root → global index lookup by `completeBaseName` → shortest-path tiebreaker (no current-editor bias)
