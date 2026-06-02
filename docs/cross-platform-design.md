# Cross-Platform Support Design

## Overview

Support Linux and macOS while maintaining zero regression on Windows. Use a phased approach:
CMake migration → Linux support → macOS support.

## Roadmap

```
Step 1: CMakeLists.txt  ──  新建 CMake 构建系统，Windows 保留 .pro
          ↓
Step 2: qmake → CMake  ──  验证 CMake 在 Windows 上编译通过且零功能变化
          ↓
Step 3: Linux 窗口+键盘 ──  无边框窗口 + Esc 键盘事件适配
          ↓
Step 4: Linux 模块适配 ──  评测内存监控 + 编译器检测 + 主题自动检测
          ↓
Step 5: Linux 集成验证 ──  CMake 在 Linux 上全量编译 + 功能测试
          ↓
Step 6: macOS 适配    ──  无边框窗口 + 内存监控 + 编译器 + 主题检测
          ↓
Step 7: macOS 集成验证 ──  CMake 在 macOS 上全量编译 + 功能测试
```

Each step is a standalone commit. Steps 1–2 are Windows-only safe; Steps 3–6 add platform branches without touching Windows code paths.

## Constraints

- Windows existing behavior must NOT change. All Windows code paths remain guarded by `#ifdef Q_OS_WIN`.
- Acceptable feature parity with graceful degradation where platform limits apply.
- Keyboard events (Esc handling) and Judge memory monitoring are required on all platforms.

## Phase 1: Build System (qmake → CMake)

Retain `smart-markdown.pro` for Windows users. Add `CMakeLists.txt` at project root.

**CMake structure:**

```cmake
cmake_minimum_required(VERSION 3.22)
project(SmartMarkdown LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets Network Pdf PdfWidgets)

# Source files — same list as .pro
set(SOURCES ...)
set(HEADERS ...)

qt_add_executable(SmartMarkdown ${SOURCES} ${HEADERS})
qt_add_resources(SmartMarkdown resources/resources.qrc)

target_link_libraries(SmartMarkdown PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::WebEngineWidgets Qt6::Network Qt6::Pdf Qt6::PdfWidgets
)

if(WIN32)
    target_link_libraries(SmartMarkdown PRIVATE user32 dwmapi)
    target_compile_definitions(SmartMarkdown PRIVATE ...)
elseif(APPLE)
    target_link_libraries(SmartMarkdown PRIVATE "-framework System")
elseif(UNIX)
    find_package(XCB REQUIRED)
    target_link_libraries(SmartMarkdown PRIVATE xcb)
endif()
```

**Windows path preserved**: All `win32:` / `win32-msvc*:` conditions in `.pro` go to `if(WIN32)` blocks in CMake.

## Phase 2: Linux Support

### Frameless Window (mainwindow.cpp)

```cpp
#ifdef Q_OS_WIN
    // existing nativeEvent (WM_NCHITTEST, DWM corner rounding) — unchanged
#elif defined(Q_OS_LINUX)
    // X11: xcb edge detection, Qt windowHandle()->startSystemResize fallback
    // Wayland: Qt startSystemMove/startSystemResize only (protocol restriction)
#endif
```

- Use `windowHandle()->startSystemMove()` / `startSystemResize()` for window drag/resize
- For X11: detect `$XDG_SESSION_TYPE` → `"x11"` for xcb-based edge detection
- For Wayland: Qt abstraction as Wayland protocol forbids client-side window management

### Keyboard Event (codeeditor.cpp)

Current `EscNativeFilter` uses Windows `VK_ESCAPE` via `QAbstractNativeEventFilter`. Replace with Qt-level `QShortcut(Qt::Key_Escape)` on non-Windows platforms. Windows keeps the native event filter approach.

**Implementation**: Split `EscNativeFilter` — Windows `#ifdef Q_OS_WIN` keeps native event filter; Linux/macOS uses `eventFilter` on parent widget with `QEvent::KeyPress`.

### Memory Monitoring (judgeengine.cpp)

```cpp
#ifdef Q_OS_WIN
    // existing psapi.h GetProcessMemoryInfo — unchanged
#elif defined(Q_OS_LINUX)
    // Read /proc/[pid]/status VmRSS line with QFile, poll-based
#elif defined(Q_OS_MACOS)
    // task_info from <mach/task_info.h>
#endif
```

- Linux: `/proc/[pid]/status` → parse `VmRSS:` line → kB value. Simple `QFile` read, no extra deps.
- Windows `#ifdef` + `<psapi.h>` stays. `m_peakMemoryKb` field is already abstract.

### Compiler Detection (compilerutils.h)

```cpp
// Existing MSVC + g++ detection stays Windows-only (#ifdef Q_OS_WIN)
// Linux: QStandardPaths::findExecutable("g++") primary, "clang++" fallback
// macOS: QStandardPaths::findExecutable("clang++") + xcrun fallback
```

### Theme Detection (thememanager.cpp)

- Windows: registry auto-detect (current, unchanged)
- Linux: `gsettings get org.gnome.desktop.interface color-scheme` via QProcess
- macOS: `defaults read -g AppleInterfaceStyle` via QProcess

CMake condition: `if(WIN32)` / `elseif(UNIX)` / `elseif(APPLE)` for linker flags.

## Phase 3: macOS Support

- Frameless window: `windowHandle()->startSystemMove/startSystemResize` works well on macOS via Qt
- `NSEvent` monitoring via `QAbstractNativeEventFilter` for corner cases
- Compiler: detect Clang via `xcrun --find clang++`
- Memory: `task_info` via `<mach/task_info.h>`, link `-framework System`
- Theme: `defaults read -g AppleInterfaceStyle`
- Need macOS build machine (Xcode) for testing — requires CI setup or local Mac hardware

## Files Modified

| File | Change |
|------|--------|
| `CMakeLists.txt` (new) | Full CMake build, replaces qmake for CI/non-Windows |
| `smart-markdown.pro` | Unchanged, kept for Windows users |
| `core/mainwindow.cpp` | Add `#elif Q_OS_LINUX` and `#elif Q_OS_MACOS` branches |
| `editor/codeeditor.cpp` | Refactor `EscNativeFilter` to use Qt-level event on non-Windows |
| `judge/judgeengine.cpp` | Add Linux `/proc/pid/status` and macOS `task_info` branches |
| `runner/compilerutils.h` | Add Linux g++/clang++ detection, macOS xcrun clang++ detection |
| `core/thememanager.cpp` | Add Linux gsettings and macOS defaults detection |
| `smart-markdown.pro` | No changes needed |

## Risk & Mitigation

- **Linux frameless window complexity**: X11 + Wayland fragmentation. Mitigation: use Qt's built-in `startSystemResize()` as common fallback.
- **macOS testing**: Requires Apple hardware. Mitigation: Phase 3 deferred until CI or device available.
- **CMake migration breakage**: Existing `.pro` stays; Windows developers unaffected. CMake validated in CI.

## Test Plan

1. Windows: full compile (qmake + MSVC), verify frameless window behavior, judge, keyboard Esc — no regression
2. Linux: CMake build (g++/Clang), verify window drag/resize, judge memory monitoring, compiler detection, theme detection
3. macOS: CMake build (Clang + Xcode), verify same features
