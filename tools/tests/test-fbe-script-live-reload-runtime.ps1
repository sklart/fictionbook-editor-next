<# Runs the script reload and catalogue refresh probes in an isolated portable FBE. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [ValidateRange(30, 300)][int]$TimeoutSeconds = 120)

$ErrorActionPreference = 'Stop'
$FbeExe = (Resolve-Path -LiteralPath $FbeExe).Path
$exeDirectory = Split-Path -Parent $FbeExe
$portableIni = Join-Path $exeDirectory 'portable.ini'
$hadIni = Test-Path -LiteralPath $portableIni
$oldIni = if($hadIni) { Get-Content -LiteralPath $portableIni -Raw } else { $null }
$dataDirectory = [IO.Path]::GetFullPath((Join-Path $exeDirectory 'ScriptLiveReloadRuntime'))
if((Split-Path -Parent $dataDirectory) -ne [IO.Path]::GetFullPath($exeDirectory)) { throw 'Unsafe script runtime test directory.' }

function Invoke-Scenario([string]$scenario, [string[]]$required) {
    $env:FBE_NEXT_TEST_SCENARIO = $scenario
    $process = Start-Process -FilePath $FbeExe -WorkingDirectory $exeDirectory -ArgumentList @('--portable', $document) -PassThru
    if(-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force
        throw "Script runtime scenario timed out: $scenario"
    }
    $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
    foreach($line in $required) { if($text -notmatch [regex]::Escape($line)) { throw "Script runtime scenario $scenario failed:`n$text" } }
    if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^result=pass$') { throw "Script runtime scenario $scenario failed:`n$text" }
}

try {
    [IO.File]::WriteAllText($portableIni, "[Portable]`r`nDataPath=ScriptLiveReloadRuntime`r`n", [Text.UTF8Encoding]::new($false))
    if(Test-Path -LiteralPath $dataDirectory) { Remove-Item -LiteralPath $dataDirectory -Recurse -Force }
    $scripts = Join-Path $dataDirectory 'Scripts'
    New-Item -ItemType Directory -Path (Join-Path $scripts 'FolderA\FolderB') -Force | Out-Null
    foreach($path in @('Root.js', 'FolderA\Child.js', 'FolderA\FolderB\Deep.js')) {
        [IO.File]::WriteAllText((Join-Path $scripts $path), "function Run() {}`r`n", [Text.UTF8Encoding]::new($false))
    }
    $repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    Copy-Item -LiteralPath (Join-Path $repoRoot 'src\export-docx\res\ExportDOCX.ico') -Destination (Join-Path $scripts 'Root.ico')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'packaging\nsis\res\fbe-wizard.bmp') -Destination (Join-Path $scripts 'FolderA\Child.bmp')
    $document = Join-Path $dataDirectory 'script-runtime.fb2'
    [IO.File]::WriteAllText($document, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Script runtime</book-title><lang>ru</lang></title-info></description><body><section><title><p>Test</p></title><p>Test</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $report = Join-Path $dataDirectory 'Diagnostics\portable-state-report.txt'
    $savedMode = $env:FBE_NEXT_TEST_MODE; $savedScenario = $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        Invoke-Scenario 'script-live-reload-runtime' @('phase=script-live-reload', 'version-1=1', 'version-2=1')
        Invoke-Scenario 'script-catalog-refresh-runtime' @('phase=script-catalog-refresh', 'contents=1', 'added=1', 'removed=1', 'renamed=1', 'icon=1', 'uid=1', 'tree=1', 'document=1')
    } finally { $env:FBE_NEXT_TEST_MODE = $savedMode; $env:FBE_NEXT_TEST_SCENARIO = $savedScenario }
    Write-Host 'Script live reload runtime regression passed.'
} finally {
    if($hadIni) { [IO.File]::WriteAllText($portableIni, $oldIni, [Text.UTF8Encoding]::new($false)) } else { Remove-Item -LiteralPath $portableIni -Force -ErrorAction SilentlyContinue }
    if(Test-Path -LiteralPath $dataDirectory) { Remove-Item -LiteralPath $dataDirectory -Recurse -Force -ErrorAction SilentlyContinue }
}
