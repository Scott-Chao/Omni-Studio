import os
import json
import re

def generate_compilation_database():
    project_dir = os.getcwd().replace('\\', '/')
    
    # 1. 寻找存放编译参数的 Makefile
    # qmake 通常会生成 Makefile.Debug 和 Makefile.Release，我们优先解析 Debug 端的参数
    candidates = ['Makefile.Debug', 'Makefile.Release', 'Makefile']
    # 也搜索标准构建目录
    build_dirs = [
        'build/Desktop_Qt_6_11_0_MSVC2022_64_bit-Debug',
        'build/Desktop_Qt_6_11_0_MSVC2022_64_bit-Release',
        'build',
    ]
    makefile_path = None
    for c in candidates:
        if os.path.exists(c):
            makefile_path = c
            break
    if not makefile_path:
        for bd in build_dirs:
            for c in candidates:
                path = os.path.join(bd, c)
                if os.path.exists(path):
                    makefile_path = path
                    break
            if makefile_path:
                break

    if not makefile_path:
        print("[Error] 未找到 Makefile。请确保已经先运行了 qmake。")
        return

    print(f"[Info] 正在解析: {makefile_path}")

    # 2. 读取 Makefile 内容
    with open(makefile_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # 3. 提取 DEFINES (宏定义) 和 INCPATH (头文件包含路径)
    # 使用正则表达式处理换行符连接的情况（Windows Makefile 常用 \ 换行）
    defines_match = re.search(r'DEFINES\s*=\s*((?:.*\\\s*\n)*.*)', content)
    incpath_match = re.search(r'INCPATH\s*=\s*((?:.*\\\s*\n)*.*)', content)

    defines_raw = defines_match.group(1) if defines_match else ""
    incpath_raw = incpath_match.group(1) if incpath_match else ""

    # 清洗格式：去掉换行符连接用的反斜杠，合并成单行
    defines = " ".join([d.strip() for d in defines_raw.replace('\\\n', ' ').split() if d.strip()])
    
    # 关键 Pivot Point: 清洗路径。将 Windows 的 \\ 换成 /，
    # 并且把类似 -ID:\Qt\... 变成 clangd 喜欢的标准格式 -I D:/... (带空格)
    inc_parts = incpath_raw.replace('\\\n', ' ').replace('\\', '/').split()
    cleaned_inc_list = []
    for part in inc_parts:
        part = part.strip()
        if part.startswith('-I'):
            path_body = part[2:]
            # 补齐空格，例如 -I. 保持不变，-ID:/Qt 变成 -I D:/Qt
            if path_body.startswith(('A:','B:','C:','D:','E:','F:')):
                cleaned_inc_list.append(f"-I {path_body}")
            else:
                cleaned_inc_list.append(f"-I {path_body}")
        else:
            cleaned_inc_list.append(part)
    incpath = " ".join(cleaned_inc_list)

    # 4. 扫描项目中的所有 .cpp 源文件 (排除 MOC 生成文件)
    cpp_files = []
    for root, _, files in os.walk('.'):
        # 忽略编译输出目录，避免扫描到中间产物
        if any(p in root.replace('\\', '/') for p in ['/build', '/debug', '/release']):
            continue
        for file in files:
            if file.endswith('.cpp'):
                # 排除 Qt 自动生成的元对象源文件（这些不应该作为独立编译单元让 clangd 索引）
                if file.startswith('moc_') or file.startswith('qrc_'):
                    continue
                # 计算相对路径
                rel_path = os.path.relpath(os.path.join(root, file), start=os.getcwd()).replace('\\', '/')
                cpp_files.append(rel_path)

    # 5. 构建 json 字典结构
    compile_commands = []
    for cpp in cpp_files:
        # 组合成完整的 cl.exe 虚拟编译指令
        command = f"cl.exe /c {defines} {incpath} {cpp}"
        
        compile_commands.append({
            "directory": project_dir,
            "command": command,
            "file": cpp
        })

    # 6. 写入到根目录下的 compile_commands.json
    output_filename = "compile_commands.json"
    with open(output_filename, 'w', encoding='utf-8') as f:
        json.dump(compile_commands, f, indent=4)
        
    print(f"[Success] 成功为 {len(cpp_files)} 个源文件生成了本地专属编译数据库！")

if __name__ == "__main__":
    generate_compilation_database()