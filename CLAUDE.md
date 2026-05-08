# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Run
- **Build System**: qmake (Qt 6.11.0)
- **Compiler**: MSVC 2022 (Visual Studio 2022 Build Tools or Community)
- **Required Qt Components** (install via `D:\Qt\MaintenanceTool.exe`):
  - `qt.qt6.6110.win64_msvc2022_64` — Qt 6.11.0 MSVC 2022 base kit
  - `extensions.qtwebengine.6110.win64_msvc2022_64` — Qt WebEngine (Chromium-based web view)
  - `extensions.qtwebchannel.6110.win64_msvc2022_64` — Qt WebChannel (WebEngine dependency)
  - `extensions.qtposition.6110.win64_msvc2022_64` — Qt Positioning (WebEngine dependency)
  - `tools.qtcreator` — Includes jom.exe (parallel nmake replacement)
- **Qt Install Path**: `D:\Qt\6.11.0\msvc2022_64`
- **jom Path**: `D:\Qt\Tools\QtCreator\bin\jom\jom.exe`
- **Build Directory**: `./build/Desktop_Qt_6_11_0_MSVC2022_64_bit-Debug`
- **Commands** (all commands MUST run from **"x64 Native Tools Command Prompt for VS 2022"** — see Environment Notes below):
  - **Configure**: `qmake.exe -r smart-markdown.pro` ("-r" for recursive, needed when QRC resources change)
  - **Build**: `jom.exe -f Makefile.Release -j22` (parallel build, use thread count after `-j`)
  - **Build (alternative)**: `nmake` (single-threaded, use if jom unavailable)
  - **Clean**: `jom.exe -f Makefile.Release clean` or `nmake clean`
- **Output Executable**: `./release/smart-markdown.exe`
- **Deployment**: `windeployqt release/smart-markdown.exe --qmldir .` (copies all required DLLs for distribution)

## Environment Notes

### Critical: Launch Build Commands from VS Command Prompt
You MUST run qmake, jom/nmake, and windeployqt from the **"x64 Native Tools Command Prompt for VS 2022"** (available in Start Menu → Visual Studio 2022). This sets up the MSVC compiler (cl.exe) in PATH.

If you prefer to run from an ordinary terminal, first run:
```
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

Without the VS environment, qmake will fail with: `Project ERROR: msvc-version.conf loaded but QMAKE_MSC_VER isn't set`

### Qt DLLs at Runtime
When running `smart-markdown.exe` outside the VS command prompt (e.g., double-clicking), the Qt DLL directory must be in PATH:
```
set PATH=D:\Qt\6.11.0\msvc2022_64\bin;%PATH%
```
Or copy the Visual C++ redistributable DLLs (you may find them in `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\<version>\x64\Microsoft.VC143.CRT`).

### WebEngine Remote Debugging
The app sets `QTWEBENGINE_REMOTE_DEBUGGING=9222`. For debugging WebEngine issues, navigate to `http://localhost:9222` in a Chromium-based browser.

### Required Qt Binaries in PATH
Ensure `D:\Qt\6.11.0\msvc2022_64\bin` is in PATH for qmake and windeployqt to be found.

## Troubleshooting

### QMAKE_MSC_VER not set
**Symptom**: `Project ERROR: msvc-version.conf loaded but QMAKE_MSC_VER isn't set`
**Cause**: Running qmake from a regular terminal instead of the VS command prompt.
**Fix**: Use "x64 Native Tools Command Prompt for VS 2022".

### Unknown module(s) in QT: webenginewidgets
**Symptom**: qmake fails with "Unknown module(s) in QT: webenginewidgets"
**Cause**: Qt WebEngine extension not installed, or running in MinGW environment.
**Fix**: Qt WebEngine on Windows is MSVC-only. Install via Maintenance Tool under Qt 6.11.0 → MSVC 2022 64-bit → Qt WebEngine extension.

### Unknown module(s) in QT: webchannel / positioning
**Symptom**: qmake fails with "Unknown module(s) in QT: webchannel" or "positioning"
**Cause**: Missing WebEngine dependencies.
**Fix**: Install Qt WebChannel and Qt Positioning via Maintenance Tool.

### App hangs on startup with no window
**Symptom**: Application launches but no window appears.
**Cause**: `MainWindow::buildFileIndex()` or `BacklinkIndex::buildIndex()` scanning `QDir::homePath()` (tens of thousands of files) because `SettingsManager::lastFolderPath()` defaults to home directory when no `config.ini` exists.
**Fix**: Both methods have guard clauses (`rootPath == QDir::homePath()` returns early). This should no longer occur.

### LNK1104: cannot open release\smart-markdown.exe
**Symptom**: Linker fails because exe is in use.
**Fix**: Kill the running process first: `taskkill /f /im smart-markdown.exe`

### nmake doesn't support -j flag
**Symptom**: `nmake : fatal error U1065: invalid option 'j'`
**Fix**: nmake is single-threaded. Use `jom` for parallel builds: `jom -f Makefile.Release -j22`

## Code Architecture

The app is a single-window Markdown editor (Qt Widgets). All source files are at the project root. The architecture follows a flat component layout with `MainWindow` as the central coordinator.

### Component Map

```
main.cpp                  → QApplication + MainWindow bootstrap
MainWindow (mainwindow.*) → orchestrator: owns all widgets, routes signals/slots
  ├── FileExplorerWidget  → QTreeView + QFileSystemModel, file tree panel (left)
  ├── TabManager          → QTabWidget, manages EditorWidget tabs (center)
  │   └── EditorWidget    → QStackedWidget[QTextEdit | QWebEngineView], dual-mode editor
  ├── HistoryPanel        → QDockWidget + QListWidget, recent files (right, hidden by default)
  ├── BacklinkIndex       → QMap-based reverse index: target file → source files for `[[wikilinks]]`
  ├── BacklinksPanel      → QDockWidget + QListWidget, backlinks for current file (right, hidden by default)
  ├── SettingsManager     → QSettings wrapper, config.ini persistence
  └── TextFileUtils       → Utility (fileutils.h), 40+ text extension list & scan filters
```

### Key Data Flow

- **File opening**: `FileExplorerWidget::fileClicked` → `MainWindow::onFileSelected` → `TabManager::openFile` → returns `EditorWidget*`
- **Bidirectional links**: `[[文件名]]` syntax → regex in `EditorWidget::refreshPreview` converts to `<a href="wikilink:...">` tags → `PreviewPage::acceptNavigationRequest()` intercepts clicks on `wikilink:` scheme → emits `wikiLinkClicked` → `MainWindow::onWikiLinkClicked` → `MainWindow::findWikiTarget` (multi-level filename search) → opens or prompts create
- **File indexing**: `MainWindow::m_fileIndex` (QMap<filename_no_ext, QStringList<full_paths>>) — rebuilt on folder change, incrementally updated on rename/delete/move. Scans use `TextFileUtils::scanNameFilters()` for 40+ text extensions.
- **Backlinks**: `TabManager::currentChanged` → `MainWindow::refreshBacklinks` → `BacklinkIndex::backlinksFor(currentFilePath)` → `BacklinksPanel::showBacklinks`. Index updated via full rebuild (`buildIndex`) or per-file incremental (`rebuildFile` / `onFileRenamed` / `onFileDeleted`)
- **Rename + wiki link update**: `FileExplorerWidget::fileRenamed` → `MainWindow::onFileMovedOrRenamed` → `onFileRenamedInIndex` captures source files linking to old path, updates index, then `updateWikiLinksAfterRename` rewrites `[[oldName]]` → `[[newName]]` in each source file (using `replaceWikiLinkText` with exact wiki-link regex matching). Open editors with unsaved changes are handled by reading from `editor->toPlainText()` instead of disk.
- **Save**: `MainWindow::saveFile` → `EditorWidget::saveFile` (existing) or `EditorWidget::saveAsFile` (new = uses remembered save-as dir)
- **Zoom**: any zoom action → `EditorWidget::setZoomFactor` → `applyZoom` (adjusts QTextEdit fonts and QWebEngineView zoom factor) → emits `zoomFactorChanged` → `MainWindow::updateZoomLabel`

### Component Details

**EditorWidget** — Dual-mode (source/preview) editor. Preview uses `QWebEngineView` with an HTML template that loads marked.js for Markdown→HTML conversion, KaTeX for LaTeX math rendering, and Mermaid.js for diagram rendering. WikiLink detection happens during preview HTML generation via regex (`[[...]]` → `<a href="wikilink:...">`), intercepted by a custom `QWebEnginePage::acceptNavigationRequest()` override. Zoom factor range: 0.5–3.0, step 0.1. Has a 300ms debounce timer that auto-clears the modified flag if text reverts to original content.

**FileExplorerWidget** — Uses `QFileSystemModel` + `QSortFilterProxyModel` (custom `FileSortProxyModel` for folders-first sort). Custom `NoGhostDelegate` eliminates text ghosting during inline rename; if rename results in empty name, auto-restores original. Supports drag-drop file moving (within root directory only) with visual feedback (blue indicator bar). Delete key triggers removal even from context menu via `DeleteKeyFilter` event filter.

**TabManager** — Custom `CustomTabBar` (extends QTabBar) constrains drag bounds. `closeTab` shows a custom save prompt dialog. `closeAllTabs` is used for graceful shutdown. `updatePathsAfterMove` handles batch path updates after file tree drag-move operations.

**HistoryPanel** — Max 50 entries, auto-dedup by path. Persisted via `SettingsManager` under `History/recentFiles`. Persistence is deferred to program shutdown only (via `MainWindow::closeEvent`) to reduce disk I/O. `replacePath` rebuilds UI list after file moves. Automatically removes entries for deleted files on next access (shows warning dialog).

**BacklinkIndex** — Reverse index: absolute path of target file → list of source file paths that link to it via `[[wikilinks]]`. Also maintains a forward index (source → targets) for incremental updates. Uses the same recursive regex `\[\[((?:[^\[\]]|\[(?1)\])*)\]\]` as `EditorWidget::refreshPreview`. WikiLink target resolution follows deterministic rules (exact path under root for any known text extension → index lookup by `completeBaseName` → shortest-path tiebreaker), unlike `findWikiTarget` which uses current-editor-context disambiguation. Uses `TextFileUtils::scanNameFilters()` for file scanning and `TextFileUtils::textExtensions()` for target resolution, replacing hardcoded `.md`/`.txt` lists. Supports `buildIndex` (full scan) and incremental `rebuildFile`, `onFileRenamed`, `onFileDeleted`. `onFileRenamed` migrates both backlink keys and forward-link target references to keep `rebuildFile → removeFile` cleanup consistent.

**BacklinksPanel** — Display-only `QListWidget` (NoSelection) showing source files that link to the currently open file. Follows HistoryPanel pattern: embedded in `QDockWidget` (right side, default hidden), toolbar toggle (Ctrl+Shift+B), auto-hide on outside click. Shows "无反向链接" placeholder when empty. Minimum width 200px to prevent panel collapse.

**SettingsManager** — Writes `config.ini` next to the executable. Stores window geometry, splitter state, last open folder path, last save-as folder path, and recent files list.

**TextFileUtils** (`fileutils.h`) — Header-only utility providing `textExtensions()` (returns 40+ common text file extensions like `md`, `txt`, `cpp`, `py`, `js`, `json`, `xml`, `yaml`, `csv`, etc.) and `scanNameFilters()` (returns `"*." + ext` filters for `QDirIterator`). Used by `BacklinkIndex`, `EditorWidget`, `FileExplorerWidget`, and `MainWindow` to replace hardcoded `.md`/`.txt` file lists.

### Naming Convention

- `[[文件名]]` (WikiLink syntax) — links by filename without path or extension
- `findWikiTarget` performs: exact match → parent-dir-first search → sibling-subtree search → full subtree search
- `BacklinkIndex::resolveTarget` performs: exact path under root → global index lookup by `completeBaseName` → shortest-path tiebreaker (no current-editor bias)

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
