@ECHO OFF
SETLOCAL
CHCP 65001 >NUL

SET "SCRIPT_DIR=%~dp0"
FOR %%I IN ("%SCRIPT_DIR%..\..\..") DO SET "REPO_ROOT=%%~fI"
SET "INSTALLER_DIR=%SCRIPT_DIR:~0,-1%"
SET "POWERSHELL_EXE=pwsh.exe"
WHERE /Q "%POWERSHELL_EXE%"
IF ERRORLEVEL 1 SET "POWERSHELL_EXE=powershell.exe"

"%POWERSHELL_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%\tools\version\sync-version.ps1" -ValidateUpdateManifest
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%

"%POWERSHELL_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%\tools\build\prepare-installer.ps1"
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%

"%POWERSHELL_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%\tools\localization\export-nsis-language-pack-plan.ps1"
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%

"%POWERSHELL_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%\tools\localization\export-nsis-installer-fallbacks.ps1"
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%

IF DEFINED FBE_MAKENSIS (
  SET "MAKENSIS=%FBE_MAKENSIS%"
) ELSE (
  SET "MAKENSIS=%ProgramFiles(x86)%\NSIS\Unicode\makensis.exe"
)
IF NOT EXIST "%MAKENSIS%" (
  ECHO ОШИБКА: не найден Unicode-вариант NSIS: "%MAKENSIS%"
  ECHO Установите NSIS Unicode или задайте FBE_MAKENSIS. Обычный makensis.exe не подходит: он искажает UTF-8 строки установщика.
  EXIT /B 1
)
SET "INPUTDIR=%REPO_ROOT%\out\package\FictionBookEditor"
SET "ARTIFACTSDIR=%REPO_ROOT%\out\artifacts"
SET "VERSION_NSH=%INSTALLER_DIR%\version.nsh"
SET "NSIS_SCRIPT=%INSTALLER_DIR%\MakeInstaller.nsi"
SET "NSIS_INCLUDE_DIR=%SCRIPT_DIR%..\NSIS"

IF NOT EXIST "%INPUTDIR%\" (
  ECHO ERROR: Katalog vhodnyh failov NSIS ne naiden: %INPUTDIR%
  ECHO Snachala zapustite .\tools\build\package-portable.ps1 ili .\tools\build\create-release.ps1.
  EXIT /B 1
)

IF NOT EXIST "%ARTIFACTSDIR%" MKDIR "%ARTIFACTSDIR%"

FOR /F "tokens=3 delims= " %%I IN ('findstr /b /c:"!define PRODUCT_VER_NUM" "%VERSION_NSH%"') DO SET "VERSION=%%~I"
IF NOT DEFINED VERSION EXIT /B 1

SET "OUTPUTFILE=%ARTIFACTSDIR%\FictionBookEditorNext-%VERSION%-win32-setup.exe"

FOR %%F IN ("%ARTIFACTSDIR%\FictionBook Editor Next Release *.exe") DO DEL /Q "%%~fF" 2>NUL

"%MAKENSIS%" /X"!addincludedir %NSIS_INCLUDE_DIR%" /X"!addplugindir /x86-unicode %NSIS_INCLUDE_DIR%" /DINPUTDIR="%INPUTDIR%" /DOUTPUTFILE="%OUTPUTFILE%" "%NSIS_SCRIPT%"
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%

"%POWERSHELL_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%\tools\build\write-artifact-checksums.ps1" -ArtifactsDirectory "%ARTIFACTSDIR%"
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%
