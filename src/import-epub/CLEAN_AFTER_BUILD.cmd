@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "ROOT=%~dp0"

echo Cleaning build artifacts in "%ROOT%"...

for %%D in (build out ipch .vs Debug Release x64 Win32 build_logs) do (
    if exist "%ROOT%%%D" rmdir /s /q "%ROOT%%%D"
)

del /q "%ROOT%*.user" 2>nul
del /q "%ROOT%*.suo" 2>nul
del /q "%ROOT%*.aps" 2>nul
del /q "%ROOT%*.VC.db" 2>nul
del /q "%ROOT%*.VC.opendb" 2>nul
del /q "%ROOT%*.log" 2>nul
del /q "%ROOT%*.lastbuildstate" 2>nul
del /q "%ROOT%*.plog" 2>nul

echo Done.
exit /b 0
