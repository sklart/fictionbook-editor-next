[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [ValidateSet(0,200,300,400,500)][int]$RefreshIterations = 0)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$FbeExe = (Resolve-Path -LiteralPath $FbeExe).Path
$work = Join-Path $repo ('out\tests\script-hotkeys-' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $work
$variables = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_HOTKEY_TEST_ROOT','FBE_NEXT_HOTKEY_TEST_PHASE','FBE_NEXT_HOTKEY_TEST_REFRESHES','FBE_NEXT_UI_LOCALE')
$saved = @{}; foreach ($name in $variables) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
try {
    foreach ($mode in 'installed','portable') {
        $directory = Join-Path $work $mode
        $null = New-Item -ItemType Directory -Path $directory
        Copy-Item -Path (Join-Path $repo 'runtime\*') -Destination $directory -Recurse -Force
        Copy-Item -LiteralPath $FbeExe -Destination (Join-Path $directory 'FBE.exe') -Force
        $builtLanguage = Join-Path (Split-Path -Parent $FbeExe) 'Lang'
        if (Test-Path -LiteralPath $builtLanguage) {
            $languageTarget = Join-Path $directory 'Lang'
            $null = New-Item -ItemType Directory -Path $languageTarget -Force
            Copy-Item -Path (Join-Path $builtLanguage '*') -Destination $languageTarget -Recurse -Force
        }
        if ($mode -eq 'portable') { [IO.File]::WriteAllText((Join-Path $directory 'portable.ini'), "[Portable]`r`nDataPath=Data`r`n") }
        $scripts = Join-Path $directory 'Data\Scripts'
        $null = New-Item -ItemType Directory -Path (Join-Path $scripts '01_Папка'),(Join-Path $scripts '02_Другая') -Force
        foreach ($path in @('01_Папка\Повтор.js','02_Другая\Повтор.js','Root.js','01_Папка\Длинное название команды для проверки колонки.js')) {
            [IO.File]::WriteAllText((Join-Path $scripts $path), "function Run() { window.external.SetStatusBarText('hotkey-last-ran'); }", [Text.UTF8Encoding]::new($false))
        }
        $document = Join-Path $directory 'fixture.fb2'
        [IO.File]::WriteAllText($document, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>Test</first-name><last-name>Author</last-name></author><book-title>Hotkeys</book-title><lang>ru</lang></title-info><document-info><author><first-name>Test</first-name><last-name>Author</last-name></author><id>hotkey-test</id><version>1.0</version></document-info></description><body><section><p>Test</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
        foreach ($phase in 'write','read') {
            if ($mode -eq 'portable' -and $phase -eq 'read') {
                $destination = Join-Path $work 'Portable moved with spaces'
                foreach ($path in @($directory,$destination)) {
                    if (-not [IO.Path]::GetFullPath($path).StartsWith($work + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe fixture move.' }
                }
                Move-Item -LiteralPath $directory -Destination $destination
                $directory = $destination
            }
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'script-hotkey-management-runtime'
            $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = Join-Path $directory 'Profile'
            $env:FBE_NEXT_HOTKEY_TEST_ROOT = (Join-Path $directory 'Data\Scripts') + '\'
            $env:FBE_NEXT_HOTKEY_TEST_PHASE = $phase
            $env:FBE_NEXT_HOTKEY_TEST_REFRESHES = [string]$RefreshIterations
            $env:FBE_NEXT_UI_LOCALE = 'ru-RU'
            $report = Join-Path $directory "report-$phase.txt"
            $process = Start-Process -FilePath (Join-Path $directory 'FBE.exe') -WorkingDirectory $directory -ArgumentList @("--$mode", '-b', ('"' + $report + '"'), ('"' + (Join-Path $directory 'fixture.fb2') + '"')) -PassThru -WindowStyle Hidden
            if (-not $process.WaitForExit($(if($RefreshIterations) { 600000 } else { 90000 }))) { Stop-Process -Id $process.Id -Force; throw "Hotkey $mode/$phase test timed out: $directory" }
            $text = if (Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<no report>' }
            if ($process.ExitCode -ne 0 -or $text -notmatch '(?m)^result=pass$') { throw "Hotkey $mode/$phase failed ($($process.ExitCode)):`n$text`nLogs: $directory" }
            Write-Host "Script hotkey management $mode/$phase runtime: PASS"
        }
    }
    Write-Host "Artifacts: $work"
} finally { foreach ($name in $variables) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) } }
