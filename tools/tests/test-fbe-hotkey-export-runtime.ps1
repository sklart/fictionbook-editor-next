<# Exercises the production hotkey TXT formatter through a real portable FBE process. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [ValidateRange(30, 180)][int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-hotkey-export-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    Copy-Item -Path (Join-Path (Split-Path $FbeExe -Parent) '*') -Destination $root -Recurse -Force
    [IO.File]::WriteAllText((Join-Path $root 'portable.ini'), "[Portable]`r`nDataPath=Data`r`n", [Text.UTF8Encoding]::new($false))
    $document = Join-Path $root 'hotkeys.fb2'
    [IO.File]::WriteAllText($document, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Hotkeys</book-title><lang>en</lang></title-info></description><body><section><p>Hotkeys</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $savedMode,$savedScenario,$savedLocale,$savedExportLocale = $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_UI_LOCALE,$env:FBE_NEXT_TEST_HOTKEY_EXPORT_LOCALE
    try {
        foreach($locale in 'ru-RU','en-US') {
            $report = Join-Path $root 'Data\Diagnostics\portable-state-report.txt'
            $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='hotkey-export-runtime'; $env:FBE_NEXT_UI_LOCALE=$locale; $env:FBE_NEXT_TEST_HOTKEY_EXPORT_LOCALE=$locale
            $process=Start-Process -FilePath (Join-Path $root 'FBE.exe') -WorkingDirectory $root -ArgumentList @('--portable','-b',$report,$document) -PassThru
            if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "Hotkey export runtime timed out for $locale." }
            $text=if(Test-Path -LiteralPath $report){Get-Content -LiteralPath $report -Raw}else{''}
            foreach($line in "locale=$locale",'header=1','localized-names=1','scripts-plugins=1','special-keys=1','tabs=1','no-empty-groups=1','result=pass') {
                if($process.ExitCode -ne 0 -or $text -notmatch ('(?m)^' + [regex]::Escape($line) + '$')) { throw "Hotkey export runtime failed for ${locale}:`n$text" }
            }
        }
    } finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_UI_LOCALE,$env:FBE_NEXT_TEST_HOTKEY_EXPORT_LOCALE=$savedMode,$savedScenario,$savedLocale,$savedExportLocale }
    Write-Host 'FBE hotkey TXT export localization runtime passed.'
} finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
