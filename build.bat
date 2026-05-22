@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
set PATH=C:\Qt\Tools\QtCreator\bin\jom;C:\Qt\6.11.1\msvc2022_64\bin;%PATH%
cd /d "C:\Users\ALIENWARE\Desktop\QT\Smart-Markdown\build\Desktop_Qt_6_11_0_MSVC2022_64bit-Debug"
jom.exe -f Makefile.Release -j22
if %errorlevel% neq 0 (
  echo BUILD FAILED
  exit /b 1
)
echo BUILD OK
