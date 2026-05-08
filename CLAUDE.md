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

The app is a single-window Markdown editor (Qt Widgets). All source files are at the project root. The architecture follows a flat component layout with `MainWindow` as the central coordinator.

### Component Map

```
main.cpp                  → QApplication + MainWindow bootstrap
MainWindow (mainwindow.*) → orchestrator: owns all widgets, routes signals/slots
  ├── FileExplorerWidget  → QTreeView + QFileSystemModel, file tree panel (left)
  ├── TabManager          → QTabWidget, manages EditorWidget tabs (center)
  │   └── EditorWidget    → QStackedWidget[QTextEdit | QWebEngineView], dual-mode editor
  ├── HistoryPanel        → QDockWidget + QListWidget, recent files (right, hidden by default)
  ├── SearchPanel         → QDockWidget + QLineEdit + QListWidget, full-text search (left, hidden by default)
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
- **Full-text search**: `SearchPanel::performSearch` (300ms debounce on text input) → `QDirIterator` + `TextFileUtils::scanNameFilters` scans all text files → `QTextStream::readLine` streaming match per line → `SearchPanel::resultClicked` → `MainWindow::onSearchResultClicked` → `TabManager::openFile` → `EditorWidget::scrollToLine` (finds line via `QTextBlock`, highlights all matches with `setExtraSelections` gold background)

### Component Details

**EditorWidget** — Dual-mode (source/preview) editor. Preview uses `QWebEngineView` with an HTML template that loads marked.js for Markdown→HTML conversion, KaTeX for LaTeX math rendering, and Mermaid.js for diagram rendering. WikiLink detection happens during preview HTML generation via regex (`[[...]]` → `<a href="wikilink:...">`), intercepted by a custom `QWebEnginePage::acceptNavigationRequest()` override. Zoom factor range: 0.5–3.0, step 0.1. Has a 300ms debounce timer that auto-clears the modified flag if text reverts to original content.

**FileExplorerWidget** — Uses `QFileSystemModel` + `QSortFilterProxyModel` (custom `FileSortProxyModel` for folders-first sort). Custom `NoGhostDelegate` eliminates text ghosting during inline rename; if rename results in empty name, auto-restores original. Supports drag-drop file moving (within root directory only) with visual feedback (blue indicator bar). Delete key triggers removal even from context menu via `DeleteKeyFilter` event filter.

**TabManager** — Custom `CustomTabBar` (extends QTabBar) constrains drag bounds. `closeTab` shows a custom save prompt dialog. `closeAllTabs` is used for graceful shutdown. `updatePathsAfterMove` handles batch path updates after file tree drag-move operations.

**HistoryPanel** — Max 50 entries, auto-dedup by path. Persisted via `SettingsManager` under `History/recentFiles`. Persistence is deferred to program shutdown only (via `MainWindow::closeEvent`) to reduce disk I/O. `replacePath` rebuilds UI list after file moves. Automatically removes entries for deleted files on next access (shows warning dialog).

**SearchPanel** — Full-text search panel in a left-side `QDockWidget` (toggle: `Ctrl+Shift+F`). Contains `QLineEdit` for keyword input (300ms debounce), `QListWidget` for results (filename + line number + context snippet), and a `QLabel` status bar. On search, uses `QDirIterator` + `TextFileUtils::scanNameFilters()` to recursively scan all text files, reads each file line-by-line via `QTextStream`, and performs case-insensitive `QString::contains` matching. Results are capped at 20 matches per file and 500 total. On result click, emits `resultClicked(filePath, lineNumber, searchText)` → `MainWindow` opens file and calls `EditorWidget::scrollToLine`, which highlights all matching occurrences with gold (`#FFD700`) background via `QTextEdit::setExtraSelections`. Unlike History/Backlinks panels, SearchPanel does NOT auto-hide on outside click (persistent sidebar behavior).

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
