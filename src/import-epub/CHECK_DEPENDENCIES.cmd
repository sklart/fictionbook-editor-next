@echo off
setlocal EnableExtensions
chcp 65001 >nul
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0CHECK_DEPENDENCIES.ps1"
exit /b %ERRORLEVEL%
