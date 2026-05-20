# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Run

### Prerequisites
- **Environment**: MUST run from "x64 Native Tools Command Prompt for VS 2022".
- **System**: Qt 6.11.0 + MSVC 2022.
- **Build Dir**: `./build/Desktop_Qt_6_11_0_MSVC2022_64_bit-Debug`
- **Output**: `./release/smart-markdown.exe`

### Commands
- **Configure**: `qmake.exe -r smart-markdown.pro` (re-run when QRC changes)
- **Build**: `jom.exe -f Makefile.Release -j22` (or `nmake` as fallback)
- **Clean**: `jom.exe -f Makefile.Release clean` (or `nmake clean`)
- **Deploy**: `windeployqt release/smart-markdown.exe --qmldir .`

## Code Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for full component map, data flows, and component details.

### Component Map (high-level)

```
main.cpp                     → QApplication + MainWindow bootstrap
MainWindow                   → frameless orchestrator, owns all widgets
  ├── ActivityBar            → 48px left vertical bar with SVG icon buttons: Search, AI, Settings, Export PDF, Judge
  ├── FileExplorerWidget     → QTreeView + QFileSystemModel, file tree (left, in splitter)
  ├── TabManager             → QTabWidget, owns EditorWidget tabs (center)
  │   └── EditorWidget       → QStackedWidget, 6 modes: MarkdownEdit/Preview/CodeEdit/SplitPreview/PdfView/SmdEdit
  │       ├── SmdEditor (in SmdEdit mode)
  │       │   ├── SmdCell             → QFrame, one cell (Markdown/C++/Python) with editor/view stack
  │       │   ├── SmdOutputWidget     → per-cell output display (stdout/stderr), auto-height max 15 lines
  │       │   ├── SmdDiagnosticsPanel → per-SMD error/warning panel, toggled via Ctrl+D in edit mode
  │       │   └── SmdLspManager       → shared LSP backend for code cells, virtual-document stitching
  │       └── CodeEditor (in CodeEdit mode)
  │           ├── CompletionProvider (abstract) ← CppCompletionProvider / PythonCompletionProvider / KeywordCompletionProvider
  │           │   └── diagnosticsUpdated() signal — all providers emit diagnostics via SmdDiagnostic struct
  │           ├── CompletionPopup              → floating list QWidget
  │           ├── HoverManager                 → 400ms delayed tooltip
  │           └── SignatureHelpManager         → function signature popup
  ├── RightPanelContainer    → unified right dock with tab bar: History / Outline / Tags / Backlinks
  ├── AiPanel                → AI assistant tab (right dock, tabbed with RightPanelContainer)
  │   ├── ActionBar          → dynamic context-sensitive action buttons (改进/总结/解释/…)
  │   ├── ChatArea           → scrollable message thread with ChatBubble items
  │   │   └── ChatBubble     → single user/assistant message with Markdown→HTML rendering
  │   └── InputBar           → free-text QLineEdit + send button
  ├── AiContextManager       → collects editor context (mode, file, selection, language)
  ├── AiProvider (abstract)  → LLM provider interface
  │   ├── AnthropicProvider  → Anthropic Messages API via SSE (content_block_delta)
  │   └── OpenAiProvider     → OpenAI-compatible API via SSE (data: [DONE])
  ├── AiProviderFactory      → factory: createProvider(type), typeFromString(name)
  ├── ErrorJournal           → singleton, persistent JSON storage of Judge failures at error_journal/records.json
  ├── ErrorListPanel         → error list UI (tabbed in AiPanel): status filter, keyword search, expand-to-detail
  ├── PromptTemplates        → header-only prompt builder per AiAction, actionsForMode()
  ├── SearchPanel            → full-text search (left dock, tabbed with Judge)
  ├── JudgePanel + JudgeEngine → local OJ-style judge (left dock, tabbed with Search)
  ├── OpenJudgeWindow        → separate QMainWindow for OpenJudge browsing + submission
  ├── BottomPanel            → unified bottom panel (Output + Diagnostics tabs), replaces standalone OutputPanel
  │   ├── OutputPanel        → terminal-style stdout/stderr (tab 0)
  │   └── DiagnosticsTab     → error/warning list (tab 1), per-file via CodeEditor diagnostics cache
  ├── SettingsPanel          → floating overlay settings panel (includes AI Service page)
  └── ProcessRunner          → compile→run QProcess pipeline
```

### Key Conventions (not obvious from source)

- **Toolbar = title bar**: Single QToolBar serves as frameless title bar. Layout: [文件▼] | drag area | [面板][预览][分屏][运行▼] | [min][max][close]
- **ActivityBar**: Always-visible 48px left bar. Search/AI top group, Settings/Export PDF/Judge bottom. Active panel = left border highlight (#0078D4). Export PDF only visible for .md files.
- **Right panel tabs**: History/Outline/Tags/Backlinks in a single QDockWidget with QStackedWidget. Toggled via toolbar [面板] button. Click-outside auto-hides.
- **Left dock tabbing**: Search and Judge share the left dock area via tabifyDockWidget. Mutually exclusive — showing one hides the other.

- **File → mode mapping**: `.pdf` → PdfView, `.smd` → SmdEditor (cell-based), code extensions (`.cpp`/`.py` etc.) → CodeEditor with syntax highlighting, everything else → MarkdownEdit (WikiLinkTextEdit with `[[wikilink]]` autocomplete).
- **SMD cells**: Jupyter-like dual mode (edit/command). `---smd:markdown|cpp|python` delimiters. Each cell auto-heights, code cells compile/run via temp files. Active cell border: blue (`#0078d4`) in edit mode, purple (`#C586C0`) in command mode. C++ cells auto-group by `main()` boundaries for per-group compilation; Python cells use a persistent process with shared namespace across the same file. `Ctrl+D` toggles the `SmdDiagnosticsPanel` for error/warning inspection.
- **WikiLinks**: `[[filename]]` → bidirectional links indexed by BacklinkIndex. Preview converts to `<a href="wikilink:...">`; click resolves via multi-level filename matching.
- **Tags**: `#tag` → TagIndex (bidirectional). Preview converts to `<a href="tag:...">`.
- **Async indexing**: File index/backlinks/tags rebuilt on worker thread with cancellation token + generation counter to reject stale results.
- **Split preview**: QSplitter with edit left + WebEngine right, 500ms debounce, mutually exclusive with full preview.
- **Preview code block run**: marked renderer adds ▶ Run button on fenced code blocks; clicks navigate `runblock:execute` → C++ intercepts → saves temp file → ProcessRunner compiles/runs.
- **Local Judge**: Compile → warmup → per-test-case execution with 1s timeout + 64MB memory limit. Line-by-line trimmed output comparison.
- **OpenJudge**: Crawler-based HTTP (cxsjsx.openjudge.cn) for homework browsing, auto-login, sample extraction, code submission with 30s status polling.
- **stdin in BottomPanel**: Terminal-mode event filter captures keystrokes via OutputPanel (now a child tab of BottomPanel), buffers input, sends line-by-line on Enter. Paste splits multi-line with 20ms timer.
- **Compile & Run**: F5/F6/F7 → auto-save unsaved to temp → ProcessRunner (g++ or MSVC for C/C++, python for .py).
- **Code Completion & Diagnostics**: Ctrl+I (IME-safe alternative to Ctrl+Space) triggers completion manually. Auto-trigger on `.`, `::`, `->`. C++ clangd via LspClient (JSON-RPC over QProcess), Python via Jedi helper script. Fallback to keyword + document-words when server unavailable. `EscNativeFilter` catches VK_ESCAPE at Windows message level to close popups when Qt::Tool window HWND routing interferes. All `CompletionProvider` subclasses emit `diagnosticsUpdated(QList<SmdDiagnostic>)` — C++ via clangd `textDocument/publishDiagnostics`, Python via Jedi `diagnostics` action (base64-encoded, 500ms debounce). `SmdDiagnostic` struct lives in standalone `smddiagnostic.h`, shared by `CodeEditor` (squiggly lines), `BottomPanel` (diagnostics tab), and `SmdDiagnosticsPanel` (SMD diagnostics).
- **Custom Shortcuts**: SettingsPanel Shortcuts page (index 5) uses interactive KeyRecorder widgets with click-to-record, conflict detection dialog (overwrite/cancel), and runtime QAction rebinding via SettingsManager overrides. Persisted to config.ini [settings_overrides]. Includes all 25+ actions. `toggle_diagnostics` (default `Ctrl+D`) toggles BottomPanel diagnostics tab in code editors / SmdDiagnosticsPanel in SMD editor.
- **Diagnostics Architecture**: Two diagnostic panels coexist: (1) `SmdDiagnosticsPanel` for SMD cells (cell-granularity, embedded in SmdEditor splitter), (2) `BottomPanel` DiagnosticsTab for standalone `.cpp`/`.py` files (flat-file, shown in bottom splitter). `SmdEditor::eventFilter` has a widget-hierarchy guard to prevent its QApplication-level filter from stealing shortcuts (e.g. Ctrl+D) meant for CodeEditor.

## Coding Standards
- **Qt Logic**: Always use the **new signal/slot syntax**: `connect(sender, &Sender::signal, receiver, &Receiver::slot)`.
- **I18n**: Wrap all user-visible strings in `tr()` for future translation support (e.g., `tr("Save File")`).
- **Naming**:
  - Private members should start with `m_` (e.g., `m_fileIndex`).
  - Use `camelCase` for methods and variables, `PascalCase` for classes.
- **Memory Management**: Rely on Qt's **parent-child system** for automatic memory cleanup. If a class is not a `QObject`, use `std::unique_ptr` where appropriate.
- **Header Guards**: Use `#ifndef FILENAME_H` style guards instead of `#pragma once`.

## UI Guidelines
- **Layouts**: Primarily use `QSplitter` for the main workspace and `QLayout` subclasses for toolbars/status bars.
- **Theming**: Consolidate visual styling in `setStyleSheet` calls. New widgets should match the dark-themed tab styles defined in `TabManager`.
- **Panel Behavior**: Side panels (History, Backlinks) must be hosted in `QDockWidget` and implement the **"click-outside-to-hide"** pattern via `MainWindow::eventFilter`.
- **Tree View**: Any changes to the File Explorer must respect the `NoGhostDelegate` to prevent text rendering artifacts during renaming.
