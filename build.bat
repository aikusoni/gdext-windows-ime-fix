@echo off
rem Build both extensions with the x64 Visual C++ toolchain.
setlocal
if not defined VCVARS64 set "VCVARS64=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS64%" >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
if not exist project\bin mkdir project\bin
cl /nologo /utf-8 /W4 /O1 /MT /LD /Fobuild\ /Febuild\ime_probe.dll src\ime_probe.c user32.lib imm32.lib || exit /b 1
copy /y build\ime_probe.dll project\bin\ime_probe.dll >nul || exit /b 1
cl /nologo /utf-8 /W4 /O1 /MT /LD /Fobuild\ /Febuild\windows_ime_fix.dll src\ime_fix.c user32.lib imm32.lib || exit /b 1
copy /y windows_ime_fix.gdextension build\ >nul || exit /b 1
echo IME_PROBE_BUILD_OK
