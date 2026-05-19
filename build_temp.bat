@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d "C:\Users\ALIENWARE\Desktop\QT\Smart-Markdown\build\Desktop_Qt_6_11_0_MSVC2022_64bit-Debug"
echo [QMAKE] Running qmake...
qmake.exe -r "C:\Users\ALIENWARE\Desktop\QT\Smart-Markdown\smart-markdown.pro" > C:\Users\ALIENWARE\Desktop\QT\Smart-Markdown\build_log.txt 2>&1
if %errorlevel% neq 0 (
  type C:\Users\ALIENWARE\Desktop\QT\Smart-Markdown\build_log.txt
  exit /b %errorlevel%
)
echo [BUILD] Running nmake...
nmake /f Makefile.Release >> C:\Users\ALIENWARE\Desktop\QT\Smart-Markdown\build_log.txt 2>&1
type C:\Users\ALIENWARE\Desktop\QT\Smart-Markdown\build_log.txt
exit /b %errorlevel%
