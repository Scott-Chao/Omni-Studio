# UI 主题与窗口框架优化设计

**目标**: 将 OmniStudio 的 UI 对齐到 VS Code 的布局质量和 Obsidian 的可主题化能力，同时保持架构轻量和可扩展。

---

## 1. 概要

OmniStudio 当前有 30+ 个文件包含硬编码的 `setStyleSheet()` 颜色值，没有运行时主题切换，窗口标题栏使用系统标准图标。本规范引入：

- `ThemeManager` 单例 — 集中管理所有颜色 token
- 现代自定义标题栏 — 自绘 SVG 窗口按钮
- Fusion 样式 — 跨平台渲染一致性
- 全局 QSS 系统 — 消除重复的滚动条/菜单/提示样式
- 亮色/暗色双主题 — VS Code Dark+ 和 Obsidian Light

**范围**: 仅限视觉样式。不修改功能逻辑，不新增 UI 组件（标题栏按钮和主题管理器除外）。

---

## 2. 架构

### 2.1 ThemeManager（QObject 单例）

```
ThemeManager (新增: thememanager.h/cpp)
├── loadTheme(name)              — 从 qrc 或 user_data/themes/ 加载主题
├── color(token) → QColor         — 主查询方法，支持用户覆盖
├── setOverride(token, color)     — 用户通过设置面板自定义
├── clearOverrides()              — 重置当前主题的所有覆盖
├── availableThemes() → QStringList
├── currentThemeName() → QString
├── currentThemeType() → enum { Dark, Light }
├── loadQss()                     — 加载 global.qss，替换 %%token%%，通过 qApp 应用
└── signal: themeChanged()
```

**主题生命周期**:
1. `main.cpp` 在 `QApplication` 创建后初始化 `ThemeManager`
2. `ConfigManager` 存储活动主题名称在 `appearance.theme`
3. `ThemeManager` 加载主题 JSON，从 `SettingsManager` 合并用户覆盖
4. 发出 `themeChanged()` 信号，`MainWindow` 传播刷新到所有子组件

**设计决策**:
- 主题文件是纯 JSON（~80 个 color token），通过 qrc 嵌入
- 用户覆盖存储在 `config.ini [theme_overrides]`，通过 `SettingsManager` 管理
- 向后兼容：迁移期间保留现有 `ConfigManager::appearance.colors` 作为回退，所有消费者迁移完成后移除

### 2.2 主题 JSON 格式

```json
{
  "name": "VS Code Dark+",
  "type": "dark",
  "colors": {
    "workbench.background":   "#1E1E1E",
    "workbench.foreground":   "#CCCCCC",
    "titleBar.background":    "#3C3C3C",
    "titleBar.foreground":   "#CCCCCC",
    "activityBar.background": "#333337",
    "activityBar.activeBorder": "#0078D4",
    "activityBar.foreground": "#CCCCCC",
    "sideBar.background":     "#252526",
    "sideBar.foreground":    "#CCCCCC",
    "sideBar.border":        "#3C3C3C",
    "tab.activeBackground":   "#1E1E1E",
    "tab.activeForeground":  "#FFFFFF",
    "tab.inactiveBackground": "#2D2D2D",
    "tab.inactiveForeground":"#969696",
    "tab.border":            "#252526",
    "input.background":       "#3C3C3C",
    "input.foreground":      "#CCCCCC",
    "input.border":          "#555555",
    "button.background":      "#0E639C",
    "button.foreground":     "#FFFFFF",
    "button.hoverBackground":"#1177BB",
    "scrollbarSlider.background":     "#424242",
    "scrollbarSlider.hoverBackground":"#555555",
    "list.hoverBackground":   "#2A2D2E",
    "list.activeBackground": "#094771",
    "menu.background":        "#2D2D2D",
    "menu.foreground":       "#CCCCCC",
    "menu.selectionBackground":"#094771",
    "menu.separatorColor":   "#555555",
    "panel.border":          "#3C3C3C",
    "dropdown.background":    "#3C3C3C",
    "dropdown.border":       "#555555",
    "badge.background":       "#0078D4",
    "badge.foreground":      "#FFFFFF",
    "diagnostics.error":     "#F44747",
    "diagnostics.warning":   "#CCA700",
    "settings.numberInputBackground": "#3C3C3C",
    "settings.textInputBackground":   "#3C3C3C",
    "titleBar.buttonCloseHover":  "#c42b1c",
    "titleBar.buttonHover":       "#3a3a3a",
    "foreground": "#CCCCCC",
    "background": "#1E1E1E"
  }
}
```

### 2.3 内置主题

| 主题名称 | 类型 | 来源 |
|---------|------|------|
| `VS Code Dark+` | 暗色 | 当前硬编码颜色的正式化 |
| `Obsidian Light` | 亮色 | 新亮色主题，柔和背景 + 低饱和强调色 |

主题文件作为 qrc 资源存储：`:/themes/dark-vscode.json`，`:/themes/light-obsidian.json`

---

## 3. 窗口框架

### 3.1 当前状态

- `Qt::FramelessWindowHint` + `nativeEvent` WM_NCHITTEST 边缘缩放（10px 边距）
- `QToolBar` 充当标题栏 — 包含 [文件▼] 间隔 [帮助] [面板] [预览] [分屏] [运行▼] [最小化][最大化][关闭]
- `CaptionBtn`（匿名 namespace QPushButton 子类）使用 `QStyle::standardIcon()` 获取系统图标
- 无圆角、无阴影、无 DWM 模糊

### 3.2 优化设计

**标题栏结构保持不变**：仍使用 `QToolBar` 作为标题栏（运行良好且与 Qt 布局系统自然集成）。改动：

1. **TitleBarButton**（替换 CaptionBtn）：新增 `titlebarbutton.h/cpp`
   - 自绘 SVG 图标（最小化/最大化/恢复/关闭），存储在 qrc 中
   - 悬停状态：关闭 → 红色 (#c42b1c)，其他 → 深灰 (#3a3a3a)
   - 颜色通过 `ThemeManager` 获取，可自定义
   - 默认透明背景，无边框

2. **窗口圆角**（仅非最大化时）：
   - `setAttribute(Qt::WA_TranslucentBackground)` + Fusion
   - 主窗口 QSS `border-radius: 8px`
   - 最大化时通过 `nativeEvent` WM_NCCALCSIZE 或 `showEvent()` 移除圆角
   - 阴影通过 `QGraphicsDropShadowEffect` 或 DWM blur 实现

3. **标题栏背景**：与标签栏和编辑器区分：
   - 背景：`ThemeManager::color("titleBar.background")` (#3C3C3C)
   - 实际按钮的不可拖拽区域保持不变

---

## 4. Fusion 样式 + 全局 QSS

### 4.1 Fusion

```cpp
// main.cpp
QApplication app(argc, argv);
app.setStyle(QStyleFactory::create("Fusion"));
```

Fusion 提供：
- 跨 Windows 版本的一致控件渲染
- 统一的 QPushButton/QComboBox/QScrollBar 外观
- 通过 QSS 正确的暗色主题支持（Windows 原生样式忽略许多控件的 QSS）
- 更好的 QPalette 集成，用于未通过 QSS 明确设置的颜色

### 4.2 全局 QSS（`global.qss`）

作为 qrc 资源加载，由 `ThemeManager::loadQss()` 处理：
- 扫描 `%%token.name%%` 占位符
- 替换为 `ThemeManager::color("token.name").name()`
- 调用 `qApp->setStyleSheet(processed)`

消除重复样式：
- 滚动条（目前在 codeeditor.cpp, editorwidget.cpp, activitybar.cpp, tabmanager.cpp 等多个文件中）
- QToolTip
- QMenu（在 `mainwindow.cpp` 中为文件菜单和运行菜单重复定义）
- QScrollArea
- QSplitter handle

### 4.3 间距 Token

```cpp
namespace ThemeSpacing {
    constexpr int micro  = 4;   // 图标与文字间隙
    constexpr int tight  = 8;   // 按钮内部 padding
    constexpr int base   = 12;  // 面板内部 padding
    constexpr int loose  = 16;  // 容器间距
    constexpr int wide   = 24;  // 区块间距
}
```

所有新代码使用这些常量。现有硬编码值逐步迁移。

---

## 5. 组件视觉优化

### 5.1 ActivityBar

- 保持 48px 宽度，`#333337` 背景
- 激活按钮：2px 左边框使用 `activityBar.activeBorder`，`#2d2d2d` 背景
- 非激活按钮：透明左边框，透明背景
- 悬停：`#2d2d2d` 背景
- 图标：22x22 SVG，`:icons/` qrc 前缀

### 5.2 标签页（TabManager）

- 标签高度：32px（比当前 ~28px + padding 更紧凑）
- 激活标签：`tab.activeBackground` (#1E1E1E) + `tab.activeForeground` (#FFFFFF)
- 非激活标签：`tab.inactiveBackground` (#2D2D2D) + `tab.inactiveForeground` (#969696)
- 圆角：左上和右上 8px
- 关闭按钮：自定义 SVG，仅悬停时显示
- 底部边缘：激活标签与编辑器区域连续，无分隔线

### 5.3 滚动条（统一）

- 宽度：10px（悬停时 14px）
- 滑块：`scrollbarSlider.background` → 悬停时 `scrollbarSlider.hoverBackground`
- 圆角：5px
- 无箭头按钮
- 背景透明（覆盖在内容上，VS Code 风格）

### 5.4 侧边栏面板

- 行高：28px（FileExplorer, History, Backlinks, Tags）
- 悬停：`list.hoverBackground` (#2A2D2E)
- 激活：`list.activeBackground` (#094771)
- 侧边栏与编辑器之间的边框：1px `sideBar.border` (#3C3C3C)

---

## 6. 设置面板集成

### 6.1 主题选择器

- 在"外观"设置页面添加 ComboBox
- 选项来自 `ThemeManager::availableThemes()`
- 选择后发出 `ThemeManager::loadTheme(name)` 信号
- 选择持久化到 `config.ini [appearance] theme=<name>`

### 6.2 颜色覆盖

- 保留当前颜色选择器列表，但使用 `ThemeManager` token 而非直接 `ConfigManager` 查询
- 用户修改的 token 存储在 `config.ini [theme_overrides] token=<hex>`
- "恢复主题默认值"按钮重置为当前主题默认值

---

## 7. 迁移路径

### 第一阶段：基础（预估变更文件数）
1. 创建 `ThemeManager`（新文件：2）— 2 文件
2. 添加内置主题（新文件：2 JSON）— 2 文件
3. 创建 `global.qss` — 1 文件
4. 修改 `main.cpp` 添加 Fusion + ThemeManager 初始化 — 1 文件
5. 更新 `omnistudio.pro` 添加新文件 + 主题 QRC — 1 文件

### 第二阶段：标题栏
6. 创建 `TitleBarButton`（新文件：2）— 2 文件
7. 在 `mainwindow.cpp` 中替换 CaptionBtn — 1 文件
8. 添加窗口圆角 + 阴影 — 1 文件

### 第三阶段：迁移（逐组件采用主题）
文件逐步迁移，每个文件：
- 将硬编码 hex 替换为 `ThemeManager::color()` 调用
- 添加 `ThemeManager::instance().onThemeChanged(...)` 连接以支持动态刷新
- 移除已被 `global.qss` 覆盖的本地样式条目

需要迁移的关键文件（约 25-30 个）：
`activitybar.cpp`, `tabmanager.cpp`, `editorwidget.cpp`, `codeeditor.cpp`, `fileexplorerwidget.cpp`, `settingspanel.cpp`, `helppanel.cpp`, `bottompanel.cpp`, `outputpanel.cpp`, `rightpanelcontainer.cpp`, `smdcell.cpp`, `smdeditor.cpp`, `smddiagnosticspanel.cpp`, `smdoutputwidget.cpp`, `searchpanel.cpp`, `judgepanel.cpp`, `openjudgewindow.cpp`, `submissionpanel.cpp`, `completionpopup.cpp`, `historypanel.cpp`, `tagpanel.cpp`, `backlinkspanel.cpp`, `signaturehelpmanager.cpp`, `hovermanager.cpp`, `chatarea.cpp`, `chatbubble.cpp`, `actionbar.cpp`, `aipanel.cpp`, `errorlistpanel.cpp`, `scrollbarhider.cpp`

总计：**约 12 个新文件，约 30 个修改文件**

---

## 8. Roadmap（可测试步骤）

每个 Step 完成后均可编译运行并验证效果。

### Step 1: ThemeManager + 主题 JSON
- 新建 `thememanager.h/cpp`，实现基础框架：`loadTheme()`, `color()`, `availableThemes()`, `themeChanged()` 信号
- 新建 `resources/themes/dark-vscode.json`（完整 token 列表）
- 新建 `resources/themes/light-obsidian.json`
- 更新 `omnistudio.pro` 添加新文件
- **验证**：写一个临时 main 输出 `ThemeManager::instance().color("workbench.background").name()` → 打印 `#1E1E1E`

### Step 2: Fusion 样式 + 全局 QSS
- `main.cpp` 添加 `QApplication::setStyle(QStyleFactory::create("Fusion"))`
- 新建 `resources/global.qss`（滚动条、QToolTip、QMenu 等全局样式，用 `%%token%%` 占位符）
- `ThemeManager::loadQss()` 实现占位符替换 + `qApp->setStyleSheet()`
- 在 `main.cpp` 初始化后调用
- **验证**：运行程序，检查滚动条样式是否统一、QMenu 是否应用暗色背景

### Step 3: 标题栏按钮翻新
- 新建 `titlebarbutton.h/cpp`：自绘 SVG 窗口图标（最小化/最大化/关闭），hover 变色
- `mainwindow.cpp`：用 `TitleBarButton` 替换 `CaptionBtn`
- **验证**：运行程序，检查标题栏三个按钮外观是否正确，hover 是否变色，点击功能正常

### Step 4: 窗口圆角 + 阴影
- `mainwindow.cpp`：非最大化时添加 8px 圆角 + 阴影效果
- 最大化时移除圆角
- **验证**：运行程序，非最大化时检查圆角和阴影；最大化时检查直角

### Step 5: ActivityBar 迁移
- `activitybar.cpp`：硬编码颜色 → `ThemeManager::color()`，响应 `themeChanged()` 刷新
- **验证**：切换主题 → ActivityBar 背景和按钮颜色随之变化

### Step 6: TabManager 迁移
- `tabmanager.cpp` `mainwindow.cpp` 中标签页相关 QSS → `ThemeManager::color()`
- 标签高度统一为 32px，关闭按钮自定义 SVG
- **验证**：切换主题 → 标签页颜色变化；关闭按钮 hover 可见

### Step 7: EditorWidget + CodeEditor 迁移
- `editorwidget.cpp` `codeeditor.cpp`：编辑器背景/前景/选择色 → `ThemeManager::color()`
- **验证**：切换主题 → 编辑器颜色变化；语法高亮颜色切换

### Step 8: 侧边栏面板迁移
- `fileexplorerwidget.cpp` `searchpanel.cpp` `judgepanel.cpp` `rightpanelcontainer.cpp`
- 行高统一 28px，hover/active 颜色 → `ThemeManager::color()`
- **验证**：文件树 hover 高亮；切换主题 → 侧边栏颜色变化

### Step 9: 底部面板 + 输出面板迁移
- `bottompanel.cpp` `outputpanel.cpp`
- **验证**：输出面板背景/前景色随主题变化

### Step 10: SMD 编辑器组件迁移
- `smdcell.cpp` `smdeditor.cpp` `smddiagnosticspanel.cpp` `smdoutputwidget.cpp`
- **验证**：SMD 单元格边框色、输出面板色随主题变化

### Step 11: AI 面板组件迁移
- `aipanel.cpp` `chatarea.cpp` `chatbubble.cpp` `actionbar.cpp` `errorlistpanel.cpp`
- **验证**：AI 面板、聊天气泡颜色随主题变化

### Step 12: 剩余小部件迁移
- `settingspanel.cpp` `helppanel.cpp` `openjudgewindow.cpp` `submissionpanel.cpp` `completionpopup.cpp`
- `historypanel.cpp` `tagpanel.cpp` `backlinkspanel.cpp` `scrollbarhider.cpp`
- **验证**：所有面板颜色正确，无残留硬编码暗色值

### Step 13: 设置面板主题选择器
- `settingspanel.cpp` "外观"页顶部添加主题 ComboBox
- 选择后调用 `ThemeManager::loadTheme(name)`，触发全局刷新
- 添加"恢复主题默认值"按钮
- **验证**：下拉切换主题 → 全 UI 实时切换；重启后保留选择

### Step 14: Obsidian Light 主题调试
- 运行 Obsidian Light 主题，逐个面板检查颜色对比度是否合适
- 调整 `light-obsidian.json` 中的颜色值直到视觉效果满意
- **验证**：亮色主题下所有文字可读，无过曝或过暗区域

---

## 9. 不在此范围

- 动画和过渡（用户明确排除）
- 组件行为或布局逻辑更改
- 社区主题加载（架构上已为此预留，但不实现）

---

## 10. 未决问题

- **窗口阴影方案**：`QGraphicsDropShadowEffect` vs DWM `WM_NCCALCSIZE` blur？QGraphicsDropShadowEffect 在 Windows 无边框窗口上有已知问题。DWM 方案更优但更复杂。
- **主题覆盖粒度**：覆盖应该是按主题（同一 token 在不同主题中有不同值）还是全局？按主题更灵活但增加了复杂性。

两者在实施阶段通过实验解决。

---

## 11. Spec 自查

- **占位符**：无。所有部分已填写完整。
- **内部一致性**：主题 token 名称在所有部分中保持一致。迁移路径与架构匹配。
- **范围检查**：仅关注视觉样式。无功能蔓延。
- **歧义检查**：窗口阴影方案列为未决问题，将在实施中解决。所有颜色 token 名称明确。
