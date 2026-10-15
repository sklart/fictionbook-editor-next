@ECHO OFF
SETLOCAL
CHCP 65001 >NUL

SET "SCRIPT_DIR=%~dp0"
FOR %%I IN ("%SCRIPT_DIR%..\..\..") DO SET "REPO_ROOT=%%~fI"
SET "INSTALLER_DIR=%SCRIPT_DIR:~0,-1%"
SET "POWERSHELL_EXE=pwsh.exe"
WHERE /Q "%POWERSHELL_EXE%"
IF ERRORLEVEL 1 SET "POWERSHELL_EXE=powershell.exe"

"%POWERSHELL_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%\tools\build\build-installer.ps1" %*
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%
"%POWERSHELL_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%\tools\build\write-artifact-checksums.ps1" -ArtifactsDirectory "%REPO_ROOT%\out\artifacts"
EXIT /B %ERRORLEVEL%
