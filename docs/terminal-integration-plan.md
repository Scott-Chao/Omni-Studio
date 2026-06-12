# 终端集成实施步骤

## 目标

在主界面左下角加入终端入口，点击后打开一个独立的底部终端面板。该面板暂不与现有输出/诊断底部面板合并，并支持创建多个 PowerShell 终端。新建终端默认工作目录使用当前文件树根目录。

## 实施步骤

1. 新增终端图标资源，并在 `resources/resources.qrc` 注册为 `:/icons/terminal`。
2. 扩展 `ActivityBar`，增加终端按钮、点击信号和激活态样式刷新。
3. 新增 `TerminalSession`，在 Windows 上使用 ConPTY 创建伪终端并启动 `powershell.exe -NoLogo`。
4. 新增 `TerminalView`，负责终端输出展示、键盘输入转义序列、复制粘贴、窗口尺寸变化通知。
5. 新增 `TerminalPanel`，作为独立底部面板管理多个终端标签，提供新建、关闭终端和关闭面板能力。
6. 在 `MainWindow` 中把 `TerminalPanel` 接入 `m_rightSplitter`，保持与现有 `BottomPanel` 独立。
7. 终端按钮点击时显示/隐藏终端面板；首次显示时自动创建终端。
8. 新建终端时读取 `FileExplorerWidget::rootPath()`，路径为空或不存在时回退到用户主目录。
9. 更新 CMake 和 qmake 工程文件，确保新增源码参与构建。

## 验证清单

- 点击左下角终端按钮后，底部出现独立终端面板。
- 第一次打开终端面板时自动启动 PowerShell。
- 点击 `+` 可以创建多个终端标签，关闭标签会终止对应会话。
- 在终端内执行 `$PWD`，默认路径应为当前文件树根目录。
- 支持普通命令输入、方向键历史、Tab、Ctrl+C、粘贴多行命令。
- 调整窗口或底部面板尺寸后，ConPTY 终端尺寸随视图更新。

## 当前实现边界

当前代码已经接入 Windows ConPTY，交互能力不再依赖普通 `QProcess` stdin/stdout 管道。终端渲染层实现了基础 ANSI/CSI 处理，能够覆盖常规 PowerShell 使用；若后续需要完整 TUI 程序兼容性，应将 `TerminalView` 的基础解析替换为成熟 VT 渲染组件。
