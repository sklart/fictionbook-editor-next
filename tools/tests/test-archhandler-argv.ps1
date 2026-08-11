<#
.SYNOPSIS
Проверяет реальный Windows argv, с которым ArchHandler запускает настроенную программу.
#>

[CmdletBinding()]
param(
    [string]$PlatformToolset
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset
& (Join-Path $repoRoot 'tools\build\build-archhandler.ps1') -PlatformToolset $PlatformToolset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$handlerDir = Join-Path $repoRoot 'out\archhandler\Win32\Release'

$testDir = Join-Path $repoRoot 'out\tests\archhandler-argv'
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
$receiver = Join-Path $testDir 'receiver.exe'
& cl.exe /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'archhandler-argv-receiver.cpp') "/Fe$receiver" /link /SUBSYSTEM:CONSOLE
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$registryRoot = 'HKCU:\Software\FictionBook Editor\ArchHandler'
$backupRoot = 'HKCU:\Software\FictionBook Editor\ArchHandler.__argv_test_backup'
Remove-Item -LiteralPath $backupRoot -Recurse -Force -ErrorAction SilentlyContinue
if (Test-Path -LiteralPath $registryRoot) {
    Copy-Item -LiteralPath $registryRoot -Destination $backupRoot -Recurse -Force
}

function Set-HandlerSettings([string]$Type, [string]$Parameters) {
    $key = Join-Path $registryRoot $Type
    New-Item -Path $key -Force | Out-Null
    New-ItemProperty -Path $key -Name ArchiveProgram -Value $receiver -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $key -Name ArchiveParameters -Value $Parameters -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $key -Name FB2Program -Value $receiver -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $key -Name FB2Parameters -Value $Parameters -PropertyType String -Force | Out-Null
}

function Invoke-HandlerCase([string]$HandlerName, [string]$Type, [string]$Archive, [string[]]$Expected) {
    $report = Join-Path $testDir ("argv-{0}.txt" -f [Guid]::NewGuid().ToString('N'))
    $previous = $env:ARCHHANDLER_TEST_OUTPUT
    $previousTestMode = $env:ARCHHANDLER_TEST_MODE
    try {
        $env:ARCHHANDLER_TEST_OUTPUT = $report
        $env:ARCHHANDLER_TEST_MODE = '1'
        & (Join-Path $handlerDir $HandlerName) --type $Type $Archive
        if ($LASTEXITCODE -ne 0) { throw "ArchHandler завершился с кодом $LASTEXITCODE для $Archive" }
        for ($attempt = 0; $attempt -lt 50 -and -not (Test-Path -LiteralPath $report); ++$attempt) { Start-Sleep -Milliseconds 100 }
        if (-not (Test-Path -LiteralPath $report)) { throw "Receiver не получил argv для $Archive" }
        $content = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($report))
        if ($content.EndsWith("`n")) { $content = $content.Substring(0, $content.Length - 1) }
        [string[]]$actual = $content.Split("`n") | ForEach-Object { $_.TrimEnd("`r") }
        if (@(Compare-Object -ReferenceObject $Expected -DifferenceObject $actual).Count -ne 0) {
            throw ("Неверный argv для {0}. Ожидалось: [{1}]; получено: [{2}]" -f $Archive, ($Expected -join ' | '), ($actual -join ' | '))
        }
    }
    finally {
        $env:ARCHHANDLER_TEST_OUTPUT = $previous
        $env:ARCHHANDLER_TEST_MODE = $previousTestMode
    }
}

try {
    Remove-Item -LiteralPath $registryRoot -Recurse -Force -ErrorAction SilentlyContinue
    Set-HandlerSettings zip '$1 --kind archive "$1" ""'
    Set-HandlerSettings rar '"$1" --kind archive'

    $normalZip = Join-Path $testDir 'роман.том.01 с пробелом.zip'
    $normalRar = Join-Path $testDir 'роман.том.02 с пробелом.rar'
    $fb2Zip = Join-Path $testDir 'книга.часть.fb2.zip'
    $fb2Rar = Join-Path $testDir 'книга.часть.fb2.rar'
    foreach ($archive in @($normalZip, $normalRar, $fb2Zip, $fb2Rar)) { [IO.File]::WriteAllBytes($archive, [byte[]](1,2,3)) }
    $trailingDirectory = Join-Path $testDir 'путь с пробелом'
    New-Item -ItemType Directory -Force -Path $trailingDirectory | Out-Null
    $trailingDirectory += '\'

    Invoke-HandlerCase 'ZipHandler.exe' 'zip' $normalZip @($normalZip, '--kind', 'archive', $normalZip, '')
    Invoke-HandlerCase 'RarHandler.exe' 'rar' $normalRar @($normalRar, '--kind', 'archive')
    Invoke-HandlerCase 'ZipHandler.exe' 'zip' $fb2Zip @($fb2Zip, '--kind', 'archive', $fb2Zip, '')
    Invoke-HandlerCase 'RarHandler.exe' 'rar' $fb2Rar @($fb2Rar, '--kind', 'archive')
    Invoke-HandlerCase 'ZipHandler.exe' 'zip' $trailingDirectory @($trailingDirectory, '--kind', 'archive', $trailingDirectory, '')

    $uncArchive = "\\localhost\C$" + $normalZip.Substring(2)
    Invoke-HandlerCase 'ZipHandler.exe' 'zip' $uncArchive @($uncArchive, '--kind', 'archive', $uncArchive, '')
    Write-Host 'ArchHandler Windows argv integration test passed.'
}
finally {
    Remove-Item -LiteralPath $registryRoot -Recurse -Force -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $backupRoot) {
        Move-Item -LiteralPath $backupRoot -Destination $registryRoot -Force
    }
}
