@echo off
rem Runs the probe window once in fix mode and leaves the logs in logs\. Reads settings only; changes nothing in Windows.
rem Set GODOT_EXE to a Godot 4.7 executable first. Set IME_PROBE_MODE to observe or take for the other modes.
setlocal
cd /d "%~dp0"
if "%GODOT_EXE%"=="" (
	echo Set GODOT_EXE to a Godot 4.7 executable first.
	pause
	exit /b 1
)
if "%IME_PROBE_MODE%"=="" set "IME_PROBE_MODE=fix"
if not exist logs mkdir logs
reg query "HKCU\Software\Microsoft\Input\TSF\Tsf3Override\{A028AE76-01B1-46C2-99C4-ACD9858AE02F}" > logs\ime_settings.txt 2>&1
powershell -NoProfile -Command "(Get-WinUserLanguageList).InputMethodTips" >> logs\ime_settings.txt 2>&1
set "IME_PROBE_LOG=%CD%\logs\%IME_PROBE_MODE%_native.log"
set "IME_PROBE_GODOT_LOG=%CD%\logs\%IME_PROBE_MODE%_godot.log"
if exist "%IME_PROBE_LOG%" del "%IME_PROBE_LOG%"
"%GODOT_EXE%" --path "%CD%\project" --resolution 900x420
echo Logs are in %CD%\logs
pause
