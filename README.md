<div align="center">

# OmniStudio

[![Windows](https://img.shields.io/badge/Windows-0078D4?style=flat-square&logo=windows11&logoColor=white)](https://github.com/your-org/omni-studio)
[![macOS](https://img.shields.io/badge/macOS-000000?style=flat-square&logo=apple&logoColor=white)](https://github.com/your-org/omni-studio)
[![Linux](https://img.shields.io/badge/Linux-FCC624?style=flat-square&logo=linux&logoColor=black)](https://github.com/your-org/omni-studio)
[![Qt](https://img.shields.io/badge/Qt-6.11-41CD52?style=flat-square&logo=qt&logoColor=white)](https://www.qt.io)
[![License](https://img.shields.io/badge/License-GPLv3-181717?style=flat-square)](LICENSE)

</div>

**OmniStudio** 是一款整合 Markdown 笔记、代码编辑、AI 辅助与在线评测的桌面工具，让学习与编程工作流在同一个窗口中完成。

尤其适合北京大学信科课程的学习场景——学习《程序设计实习》等课程时，你不再需要在笔记软件、IDE、OJ 网页和 AI 聊天窗口之间反复切换。OmniStudio 把这些能力全部整合在一起：它是笔记软件，是代码编辑器，是 OJ 客户端，也是 AI 助教。

## 功能

### SMD 笔记本

受 Jupyter Notebook 启发的单元格编辑器。一个 `.smd` 文件包含 Markdown、C++、Python 三种单元格类型，支持混合编排。

- 用 Markdown 单元格写题解思路，C++ 单元格写代码，点击执行即可编译运行
- C++ 单元格按 `main()` 函数自动分组编译，Python 单元格通过持久化进程保留跨单元格命名空间
- 支持与 `.md` 格式双向无损转换

### OpenJudge 集成

北京大学《程序设计实习》等课程使用的在线评测平台，直接嵌入桌面：

- 题目浏览、登录凭据管理、代码提交
- 自动获取评测结果，错误用例 AI 辅助分析
- 支持本地错题记录与复盘

### AI 助手

侧边栏对话式 AI，实时互动：

- 多轮对话，流式输出
- 上下文感知：可基于当前编辑器内容提问
- 错题本：自动保存失败用例，可一键发送给 AI 分析
- 与本地评测引擎联动，分析运行时错误与逻辑错误

### 编辑器系统

- **多模式编辑器**：支持 Markdown 源码编辑、实时渲染预览、分屏编辑预览、代码编辑、PDF 阅读及 SMD 单元格编辑六种模式。
- **语法高亮引擎**：为 C/C++ 与 Python 提供关键字、类型、字符串、注释等分色彩色高亮。
- **数学公式渲染**：集成 KaTeX，支持行内与块级 LaTeX 公式渲染。
- **Mermaid 图表**：支持在 Markdown 中嵌入流程图、时序图、类图、甘特图等常用图表。
- **多文档界面 (MDI)**：标签页式文档管理，支持等宽与非等宽两种标签布局模式。

### 代码辅助

- **LSP 代码补全**：C++ 通过 clangd、Python 通过 Jedi 实现语言服务器协议补全，并提供关键字补全作为降级方案。
- **悬停提示**：光标悬停时显示类型签名、文档与定义位置等上下文信息。
- **诊断波浪线**：编译器与 LSP 产生的错误与警告以彩色波浪下划线标注于编辑器中，并汇总至底部诊断面板。

### 知识管理

- **双向链接**：支持 `[[文件名]]` 语法，在笔记间建立可跳转的引用关系。
- **反向链接面板**：展示当前文档被其他文件引用的来源列表。
- **标签系统**：支持 `#tag` 语法，提供标签索引与关联文件导航。
- **全文搜索**：在当前工作目录的所有文本文件中检索关键词，结果高亮并支持跳转。
- **大纲导航**：自动解析文档标题层级结构，点击标题跳转至对应位置。

### 编译、运行与评测

- **编译运行**：C/C++ 代码经编译后执行，Python 代码直接运行，输出显示于底部终端面板。
- **本地评测**：选取测试用例批量运行，输出 OJ 风格的逐行对比结果。
- **错题记录**：自动记录失败用例的输入、输出与期望结果，支持 AI 辅助分析。

### 用户界面与交互

- **无边框窗口**：自定义标题栏，支持拖拽移动与窗口缩放。
- **文件树**：提供面包屑导航、文件内联新建/重命名/删除及拖拽移动操作。
- **主题系统**：深色与浅色双主题，支持运行时即时切换。
- **设置面板**：悬浮遮罩式分类设置界面，涵盖编辑器、AI 服务、快捷键等配置项。

## 技术栈

| 层 | 选型 |
|---|---|
| 界面框架 | Qt 6.11 (QWidget) |
| 构建系统 | CMake 3.22+ / qmake |
| 代码补全 | LSP (clangd / Jedi) |
| 公式渲染 | KaTeX |
| 图表渲染 | Mermaid |
| AI 接口 | OpenAI 兼容 API |

## 构建与运行

### 源码构建

> **系统要求**：Qt 6.11+、CMake 3.22+、C++17 编译器

```bash
# macOS / Linux
./build.sh

# Windows (VS 2022 x64 Native Tools Command Prompt)
cmake --preset win32-release
cmake --build --preset win32-release
```

产物输出到 `./release/`。

## 许可证

[GPLv3](LICENSE)
