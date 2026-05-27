# Settings Panel 重构方案

## 问题总结

1. **内容不全** — 大量 ConfigManager 中已有的默认值在设置面板中找不到对应 UI
2. **clangd/Python 路径不能配置** — 四个提供者都硬编码 `QStandardPaths::findExecutable`
3. **键名未对齐配置文件** — `editor.auto_save` vs `auto_save.enabled`，`output_panel.max_blocks` 缺 `syncFromSettings`
4. **颜色控件不完整** — 现有 6 色，缺失约 23 色
5. **UI 体验细节** — 字体列表无过滤、系统提示词每字符 emit、API Key 无显示/隐藏、重置后面板不刷新

---

## 架构变更

### 设置值覆盖流程（不变）

```
SettingsPanel emit signal
    → MainWindow 接收
        → SettingsManager::setSettingOverride(key, value)
        → 使用处通过 SettingsManager::value(key) 读取（先查 override，后回退 ConfigManager）
```

### 需要新增的配置键

在 `config.json` / `ConfigManager::buildDefaultConfig()` 中补充：

```json
{
  "tools": {
    "clangd": {
      "path": "",
      "args": "--fallback-style=Google"
    },
    "python": {
      "path": ""
    }
  }
}
```

同时在 `SettingsManager` / `ConfigManager` 中新增对应 accessor，在 `CppCompletionProvider`、`SmdLspManager`、`PythonCompletionProvider` 中修改查找逻辑：

```
if 用户配置了 path && 文件存在
    → 使用用户路径
else
    → 回退 QStandardPaths::findExecutable (现有行为)
```

### 键名修正

| 当前面板 emit key | 修正为 | 影响范围 |
|-------------------|--------|----------|
| `editor.auto_save` | `auto_save.enabled` | SettingsPanel + MainWindow 接收侧 + 使用侧 |

---

## 页面设计（共 5 页）

### 1. 编辑器

原 4 页合并（编辑器 + 输出面板 + 预览 + 搜索）+ 新增项。

**区块：编辑**

| 控件 | 类型 | 数据 key | 来源 |
|------|------|----------|------|
| 字体 | QComboBox（editable, 带自动补全） | `editor.font.family` | 现有 |
| 字号 | QSpinBox 8–24 | `editor.font.size` | 现有 |
| 缩进宽度 | QSpinBox 1–8 | `editor.indent_width` | 现有 |
| MD 缩进宽度 | QSpinBox 1–8 | `editor.markdown_indent_width` | 现有 |
| 默认缩放 | QSlider 50%–300% + QLineEdit | `editor.zoom.default` | 现有 |

**区块：自动保存**

| 控件 | 类型 | 数据 key | 来源 |
|------|------|----------|------|
| 自动保存 | ToggleSwitch | `auto_save.enabled` | 现有，**key 修正** |
| 保存间隔 | QSpinBox 1–300, suffix "秒", 默认 30 | `auto_save.interval_ms` | 新增 |

**区块：输出面板**

| 控件 | 类型 | 数据 key | 来源 |
|------|------|----------|------|
| 输出字体 | QComboBox（editable） | `output_panel.font.family` | 新增 |
| 输出字号 | QSpinBox 8–24 | `output_panel.font.size` | 迁入 |
| 最大行数 | QSpinBox 100–100000, step 500 | `output_panel.max_blocks` | 迁入，**补 sync** |

**区块：预览**

| 控件 | 类型 | 数据 key | 来源 |
|------|------|----------|------|
| 分屏防抖 | QSpinBox 100–2000, suffix "ms" | `preview.split_debounce_ms` | 迁入 |
| 分屏比例 | QSpinBox 30–70, suffix "%" | `preview.split_preview_ratio` | 迁入 |

**区块：搜索**

| 控件 | 类型 | 数据 key | 来源 |
|------|------|----------|------|
| 每文件匹配数 | QSpinBox 1–50 | `search_panel.max_per_file` | 迁入 |
| 最大结果总数 | QSpinBox 50–2000, step 50 | `search_panel.max_total_results` | 迁入 |
| 片段最大长度 | QSpinBox 50–500, step 10 | `search_panel.snippet_max_length` | 迁入 |

---

### 2. 外观

保持现有页面，补齐所有缺失颜色。

**区块：主题**

| 控件 | 数据 key | 备注 |
|------|----------|------|
| 主题选择 QComboBox | — | 现有 |
| 恢复主题按钮 | — | 现有 |
| 文件树行高 QSpinBox 24–32 | `editor.file_tree_item_height` | 现有 |

**区块：颜色** — 用可折叠分类（QToolButton checked ↔ QWidget visible）分组，减少页面高度：

- 编辑器色（现有 6 色）
- 语法高亮（新增 9 色）
- 输出面板（新增 4 色）
- 搜索高亮（新增 2 色）
- 预览（新增 2 色）
- Judge 状态（新增 8 色）

所有颜色控件统一用 `QPushButton(color swatch) + QLabel(hex)` 模式，与现有一致。

改进：颜色选择器对话框的初始值传**当前已覆盖的值**而非 cfg 默认值。

---

### 3. AI 服务

保持现有页面，小幅增强。

| 控件 | 数据 key | 变更 |
|------|----------|------|
| API 类型 QComboBox | `ai.provider_type` | 现有 |
| API 端点 QLineEdit | `ai.endpoint` | 现有 |
| API Key QLineEdit + 切换按钮 | `ai.api_key` | 新增显示/隐藏按钮 |
| 模型 QLineEdit | `ai.model` | 现有 |
| Max Tokens QSpinBox | `ai.max_tokens` | 现有 |
| Temperature QDoubleSpinBox 0–2, step 0.1 | `ai.temperature` | 新增 |
| 系统提示词 QTextEdit | `ai.system_prompt` | 新增 300ms 防抖 + 焦点离开才 emit |

系统提示词防抖：连接 `textChanged` → 启动 QTimer(300ms) singleShot → 超时后 emit。

---

### 4. 快捷键

保持现有 40 个快捷键。仅在视觉上增加分组标签：

- 通用 (17)
- CodeEditor (3)
- SmdEditor (7)
- SmdEditor 命令模式 (3)
- 输出面板 (2)

---

### 5. 工具（新增页面）

**区块：语言服务**

| 控件 | 数据 key | 说明 |
|------|----------|------|
| clangd 路径 | QLineEdit + Browse 按钮 | `tools.clangd.path` — 为空则走 PATH |
| clangd 参数 | QLineEdit | `tools.clangd.args` — 默认 `--fallback-style=Google` |
| Python 路径 | QLineEdit + Browse 按钮 | `tools.python.path` — 为空则走 PATH |

Browse 按钮：`QFileDialog::getOpenFileName()`。

**区块：编译器**

| 控件 | 数据 key | 说明 |
|------|----------|------|
| GCC/Clang flags | QLineEdit | `compiler.gxx_flags` — 以空格分隔 |
| MSVC flags | QLineEdit | `compiler.msvc_flags` — 以空格分隔 |

**区块：评测**

| 控件 | 数据 key | 说明 |
|------|----------|------|
| 时间限制 | QSpinBox 100–10000, step 100, suffix "ms" | `judge.time_limit_ms` — 默认 1000 |
| 内存限制 | QSpinBox 16–1024, step 16, suffix "MB" | `judge.memory_limit_kb / 1024` — 默认 64 |

**区块：OpenJudge**

| 控件 | 数据 key | 说明 |
|------|----------|------|
| 服务器 URL | QLineEdit | `open_judge.base_url` |
| 自动登录 | ToggleSwitch | `open_judge.auto_login`（新 key） |
| 用户名 | QLineEdit | `open_judge.username`（现有 API） |
| 密码 | QLineEdit + 切换按钮 | `open_judge.password`（现有 API） |

---

## 数据流与 Bug 修复

### syncFromSettings 补全

当前 `syncFromSettings` 遗漏了输出面板最大行数的同步。重构时确保每个 emit 信号的控件都有一个对应的 sync 读取。

### resetToDefaults 后刷新 UI

当前 `resetToDefaultsRequested` → `SettingsManager::clear()` 只清内存，不清面板控件。修复：在 reset 回调末尾重新调用 `syncFromSettings()`。

### 颜色选择器初始值

当前颜色选择器传入 cfg 默认值，不反映已覆盖的值。改为从 `SettingsManager::value(key)` 读取当前值。

### Key 路径修正

| 文件 | 位置 | 当前值 | 改为 |
|------|------|--------|------|
| `settingspanel.cpp` | createEditorPage | `editor.auto_save` | `auto_save.enabled` |
| `cppcompletionprovider.cpp` | startServer | `QStandardPaths::findExecutable("clangd")` | 读配置 → PATH 回退 |
| `smdlspmanager.cpp` | startCppServer | `QStandardPaths::findExecutable("clangd")` | 同上 |
| `pythoncompletionprovider.cpp` | startProcess | `QStandardPaths::findExecutable("python")` | 读配置 → PATH 回退 |
| `smdlspmanager.cpp` | startPythonProcess | `QStandardPaths::findExecutable("python")` | 同上 |

---

## Roadmap

### Step 1 — 数据层：ConfigManager 增加 tools 段
- 编辑 `configmanager.cpp`：在 `buildDefaultConfig()` 中加入 `tools` 对象（clangd.path / clangd.args / python.path）
- 编辑 `configmanager.h`：新增 `toolClangdPath()`、`toolClangdArgs()`、`toolPythonPath()` accessor
- 编辑 `config.json`：同步加入 `tools` 段
- 验证：单元测试或手动验证 `ConfigManager::instance().toolClangdPath()` 返回正确默认值

### Step 2 — 工具路径可配置：修改 4 个 Provider
- 编辑 `cppcompletionprovider.cpp`：`startServer()` 中先读 `tools.clangd.path`，非空且文件存在则用，否则 `findExecutable`
- 编辑 `smdlspmanager.cpp`：`startCppServer()` 同上
- 编辑 `pythoncompletionprovider.cpp`：`startProcess()` 中先读 `tools.python.path`，同上回退逻辑
- 编辑 `smdlspmanager.cpp`：`startPythonProcess()` 同上
- 验证：在 SettingsManager 中设 override → 重启 provider → 确认使用新路径

### Step 3 — 设置面板重构：页面合并 + Bug 修复
- 修改 `settingspanel.cpp`：
  - 将 category list 从 7 项改为 5 项（编辑器 / 外观 / AI 服务 / 快捷键 / 工具）
  - 重写 `createEditorPage()` — 合并原编辑器、输出面板、预览、搜索到同一个页面，用 section label 分区块
  - 修正 `editor.auto_save` → `auto_save.enabled`
  - 补 `syncFromSettings` 中缺失的输出面板最大行数同步
  - 修复 `resetToDefaults` 后调用 `syncFromSettings` 刷新 UI
- 删除 `createOutputPanelPage()`、`createPreviewPage()`、`createSearchPage()` 三个旧函数
- 同步删除 `settingspanel.h` 中对应的函数声明和控件成员（m_outputFontSizeSpin 等迁入 editor page）
- 验证：逐一检查 5 个 category 切换、各区块控件显示与现有功能一致

### Step 4 — 工具页面
- 新增 `createToolsPage()` 函数，包含：
  - 语言服务区块（clangd 路径 + Browse、clangd 参数、Python 路径 + Browse）
  - 编译器区块（GCC flags、MSVC flags QLineEdit）
  - 评测区块（时间限制、内存限制 QSpinBox）
  - OpenJudge 区块（服务器 URL、自动登录 Toggle、用户名/密码）
- 所有控件 emit 对应信号
- `syncFromSettings` 中同步工具页控件
- 验证：页面能打开、控件可编辑、值能持久化到 config.ini

### Step 5 — 外观页：补全颜色控件
- 新增 6 个可折叠颜色区域（QToolButton + QWidget visibility 切换）
- 新增 23 个颜色选择器控件
- 修复颜色选择器初始值读取当前值而非 cfg 默认值
- 各颜色 emit `appearanceSettingChanged(key, hex)`
- 验证：所有颜色展开/折叠正常、选色后编辑器立即生效

### Step 6 — AI 服务页面增强
- 新增 Temperature QDoubleSpinBox
- 为 API Key 添加显示/隐藏切换按钮（QPushButton 图标切换 QLineEdit::EchoMode）
- 系统提示词改用 300ms 防抖 QTimer + focusOutEvent 触发 emit
- 验证：Temperature 写入正确 key、Key 切换显示、提示词不每字符发射

### Step 7 — 快捷键页视觉分组
- 在现有 40 项中加入 5 个灰色 QLabel 分组标题
- 无需新增 emit/数据逻辑
- 验证：分组标题正确显示，快捷键录制不受影响

### Step 8 — 收尾：端到端验证
- 检查所有 5 页所有控件的 emit key 与 ConfigManager 路径完全一致
- 检查每个控件在 `syncFromSettings` 中都有对应读取
- 构建 + windeployqt → 全功能走查
