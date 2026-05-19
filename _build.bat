@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >/dev/null 2>&1
cd /d "C:\Users\ALIENWARE\Desktop\QT\Smart-Markdown\build\Desktop_Qt_6_11_0_MSVC2022_64bit-Debug"
nmake -f Makefile.Debug > build_out.txt 2>&1
