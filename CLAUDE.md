# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Run

### Prerequisites
- **System**: Qt 6.11.0 + MSVC 2022 (Windows) / Clang (macOS) / GCC (Linux).
- **CMake**: 3.22+, with Ninja (Windows) or Unix Makefiles (macOS/Linux).
- **Windows-only**: Can also use qmake from "x64 Native Tools Command Prompt for VS 2022".

### CMake (cross-platform, preferred for non-Windows)

Configure preset builds into `./build/{debug,release}`; output lands in `./release/`.

| Platform | Configure | Build |
|----------|-----------|-------|
| Windows  | `cmake --preset win32-debug` | `cmake --build --preset win32-debug` |
| macOS    | `cmake --preset macos-debug`  | `cmake --build --preset macos-debug` |
| Linux    | `cmake --preset linux-debug`  | `cmake --build --preset linux-debug` |

Use `*-release` preset for Release builds. Re-run configure when QRC changes.

**Convenience script (macOS / Linux)**: `./build.sh [debug|release|clean|rebuild]` — wraps CMake presets with platform auto-detection and Linux dependency checking.

### qmake (Windows-only)

Run from "x64 Native Tools Command Prompt for VS 2022".
- **Configure**: `qmake.exe -r omnistudio.pro` (re-run when QRC changes)
- **Build**: `jom.exe -f Makefile.Release -j22` (or `nmake` as fallback)
- **Clean**: `jom.exe -f Makefile.Release clean` (or `nmake clean`)
- **Deploy**: `windeployqt release/omnistudio.exe --qmldir .`

## Code Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for full component map, data flows, and component details.

## Workflow

1. **Do not compile**: I must not attempt to compile/build the project. The user compiles manually.
2. **Test plan required**: After each feature or refactor, I must provide a concrete test plan for the user to verify.
3. **No Co-Authored-By**: Commits must not include `Co-Authored-By: Claude ...` lines.

### Component Map (high-level)

```
CMakeLists.txt / CMakePresets.json / build.sh → CMake 构建系统（跨平台）
.github/workflows/{linux,macos,windows}-build.yml → CI（手动触发）

main.cpp                  → QApplication + MainWindow bootstrap
core/mainwindow.*        → frameless orchestrator (Win/Lin) / native title bar (macOS)
 ├── ai/airequesthandler.*    → AI request lifecycle (provider, streaming, history pruning)
 ├── index/indexmanager.*     → file index, backlinks, tags, async index build
 ├── core/crashrecoverymanager.* → stale recovery file cleanup
 ├── core/thememanager.*      → singleton, Dark/Light palettes, Windows registry auto-detect, **watchTheme() template** + **colorStyle()** helper
 ├── runner/compilerunmanager.* → compile/run/stop lifecycle, owns ProcessRunner, action enable/disable
 ├── runner/codeblockrunner.* → MD preview ▶  code block execution, stderr buffering, diagnostics
 ├── judge/openjudgemanager.* → OpenJudge tab creation, code submission, result display, error analysis
 ├── core/macoswindow.*     → macOS native window: transparent title bar + traffic light buttons (Obj-C++ via AppKit)
 ├── config/settingschangehandler.* → setting change application, ThemeManager color bridge, shortcut rebinding
 ├── panels/activitybar.*     → 48px left bar: Search/AI/Settings/Export PDF/Judge buttons
 ├── panels/fileexplorerwidget.* → QTreeView + QFileSystemModel, breadcrumb bar, toolbar
 ├── editor/tabmanager.*      → QTabWidget, owns EditorWidget tabs
 │   └── editor/editorwidget.* → 6 modes: MarkdownEdit/Preview/CodeEdit/SplitPreview/PdfView/SmdEdit
 │       ├── smd/smdeditor.*  → Jupyter-like cell-based editor (SmdCell + LSP + diagnostics)
 │       └── editor/codeeditor.* → syntax highlighting + CompletionProvider + LSP diagnostics
 ├── panels/rightpanelcontainer.* → unified right dock: History / Outline / Tags / Backlinks
 ├── ai/aipanel.*              → AI assistant (ActionBar, ChatArea, InputBar, HistoryList)
 ├── panels/searchpanel.*      → full-text search (left dock)
 ├── panels/judgepanel.* + judge/judgeengine.* → local OJ-style judge (left dock)
 ├── panels/bottompanel.*      → Output + Diagnostics tabs
 ├── panels/settingspanel.*    → floating settings overlay
 └── panels/helppanel.*        → floating help overlay, F1 toggle
```

### Key Conventions (not obvious from source)

- **Toolbar = title bar**: Single QToolBar serves as frameless title bar.
- **ActivityBar**: Always-visible 48px left bar. Export PDF hidden by default, shown by MainWindow on .md file open.
- **Preview tabs**: Single-click = temporary preview tab (italic title), reused on subsequent single-clicks. Double-click = permanent. Editing preview content auto-promotes.
- **File → mode mapping**: `.pdf` → PdfView, `.smd` → SmdEditor, code extensions → CodeEditor, everything else → MarkdownEdit.
- **SMD cells**: `---smd:markdown|cpp|python` delimiters. C++ cells auto-group by `main()` boundaries; Python uses persistent process with shared namespace.
- **WikiLinks**: `[[filename]]` → bidirectional links via BacklinkIndex. Preview resolves via multi-level filename matching.
- **Tags**: `#tag` → bidirectional TagIndex.
- **Async indexing**: Worker thread with cancellation token + generation counter to reject stale results.
- **QSS scoped to MainWindow**: `ThemeManager::setStyleSheetTarget()`, not `qApp->setStyleSheet()`.
- **QWebEngineView lazy**: Created on first preview demand, released when exiting preview mode.
- **Right panel auto-hide**: Uses `QApplication::focusChanged` + `QTimer::singleShot(0)` — no global event filter.
- **ThemeManager::watchTheme**: `CodeEditor::CodeEditor()` uses `ThemeManager::watchTheme(this, &CodeEditor::reloadColors)` — a template convenience that encapsulates `connect(&ThemeManager::instance(), &ThemeManager::themeChanged, ...)`, eliminating 30+ identical connection blocks across the project.
- **utilities.h**: Merged from `stringutils.h`/`fileutils.h`/`processutils.h`/`debuglog.h`. Contains `TextFileUtils` I/O helpers (`readTextFile`, `writeTextFile`, `readJsonFile`, `writeJsonFile`, `isSafeRootPath`) eliminating 20+ inline QFile/QTextStream/QJsonDocument boilerplate blocks, plus `StringUtils`/`ProcessUtils`/`debugLog` utilities.
- **TabButtonGroup**: Encapsulates a QPushButton → QStackedWidget tab switching pattern with an optional `StyleProvider` callback for per-tab active/inactive styling. Used by BottomPanel and AiPanel.
- **WindowDragHelper**: Value-type member (non-QObject) encapsulating press/move/release boilerplate for frameless panel dragging. Used by HelpPanel and SettingsPanel.
- **StringUtils/TextFileUtils/ProcessUtils**: Header-only namespaces in `utilities.h` providing string/process helpers and file I/O (`sanitizeForPython`, `readTextFile`, `writeTextFile`, `ProcessUtils::cleanup`, etc.).
- **MessageRole**: `enum class MessageRole { User, Assistant, System }` in `ai/aiproviders.h` (merged from standalone `messagerole.h`).
- **ChatBubble streaming**: 80ms debounce coalesces SSE chunks; incremental HTML with full rebuild on structural changes (code fences, headings, lists).
- **PromptManager**: `ai/promptmanager.h/cpp` — QObject singleton, loads prompts from `{exeDir}/prompts.json` (fallback to Qt resource `:/prompts/prompts.json`, then C++ hardcoded defaults `loadDefaults()`). Provides `buildPrompt(action, ctx, freeQuery)` replacing the old inline function, plus `actionLabel()`/`actionTooltip()` for UI metadata. `reload()` hot-reloads at runtime. Signal `promptsReloaded()` for UI sync. Template placeholders: `{fileContent}`, `{selectedText}`, `{language}`, `{extension}`, `{filePath}`, `{freeQuery}`, error fields, cursor fields. `defaultQuery` fallback for empty resolution (used by FreeChat).
- **Markdown indent**: Default 2 spaces, configurable via `editor.markdown_indent_width`. Code indent defaults to 4 — don't assume they're the same.
- **OpenJudge tab**: Non-file tab in TabManager — `closeTab()` removes directly without save prompts.
- **Cross-platform window**: `Q_OS_MACOS` keeps native title bar with traffic light buttons (`Window` flags); `Q_OS_WIN`/`Q_OS_LINUX` use `FramelessWindowHint` with custom TitleBarButton minimize/maximize/close. macOS reserves 70px left margin on toolbar spacer for traffic lights.
- **macOS menu bar**: `QMenuBar` added on macOS only — replaces `[文件 ▼]` toolbar button and window control buttons. Application/File/View/Tools/Help menus. Preferences item owns Ctrl+, shortcut.
- **macOS keyboard display**: ActivityBar tooltips use `QKeySequence::NativeText` to show `⌘B` etc. SmdCell hints use `nativeShortcutHint()` to convert `Ctrl+`→`⌘`, `Shift+`→`⇧`.
- **Compiler detection per platform**: `compilerutils.h` — Windows detects g++ (MinGW) + MSVC cl.exe; Linux detects g++ + clang++; macOS detects clang++ (Apple) + g++ (Homebrew). `getCompileArgs()` accepts `clang` ID alongside `gcc`.
- **Judge memory monitoring per platform**: `judgeengine.cpp` — Windows uses `OpenProcess` + `GetProcessMemoryInfo`; Linux uses `/proc/<pid>/status` (VmRSS) + `waitid` WNOWAIT; macOS uses `proc_pidinfo` (`PROC_PIDTASKINFO`) + `wait4` fallback.
- **Esc key handling per platform**: `codeeditor.cpp` — Windows uses `EscNativeFilter` (`qApp->installNativeEventFilter` catches `VK_ESCAPE`); Linux/macOS use `EscEventFilter` (Qt `installEventFilter` on `KeyPress`).
- **SVG icon tinting**: All `coloredSvgIcon()` helpers use `CompositionMode_DestinationIn` (fill blank pixmap with color, paint SVG as mask) — consistent across CoreGraphics and GDI.
- **macOS default font**: `ConfigManager` defaults to Menlo on macOS (fallback to system fixed font), Consolas on other platforms.

## Coding Standards
- **Qt Logic**: Use **new signal/slot syntax**: `connect(sender, &Sender::signal, receiver, &Receiver::slot)`.
- **I18n**: Wrap user-visible strings in `tr()`.
- **Naming**: `m_` for private members, `camelCase` for methods/variables, `PascalCase` for classes.
- **Memory Management**: Rely on Qt parent-child system. For non-QObject, use `std::unique_ptr`.
- **Header Guards**: Use `#ifndef FILENAME_H` style.

## UI Guidelines
- **Layouts**: Prefer `QSplitter` for workspace, `QLayout` for toolbars/status bars.
- **Theming**: Consolidate styling in `setStyleSheet`. New widgets match dark-themed tab styles from `TabManager`.
- **Panel Behavior**: Side panels in `QDockWidget` with click-outside-to-hide via `MainWindow::eventFilter`.
- **Tree View**: File Explorer must use `NoGhostDelegate` to prevent text rendering artifacts during rename.
