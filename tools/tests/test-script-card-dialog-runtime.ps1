<# Runs the modeless script-card probe in an isolated portable FBE. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [ValidateRange(15, 120)][int]$TimeoutSeconds = 45, [string]$ArtifactDirectory)

$ErrorActionPreference = 'Stop'
$FbeExe = (Resolve-Path -LiteralPath $FbeExe).Path
$exeDirectory = Split-Path -Parent $FbeExe
$portableIni = Join-Path $exeDirectory 'portable.ini'
$hadIni = Test-Path -LiteralPath $portableIni
$oldIni = if($hadIni) { [IO.File]::ReadAllBytes($portableIni) } else { $null }
$dataDirectory = [IO.Path]::GetFullPath((Join-Path $exeDirectory 'ScriptCardDialogRuntime'))
if((Split-Path -Parent $dataDirectory) -ne [IO.Path]::GetFullPath($exeDirectory)) { throw 'Unsafe script-card test directory.' }

try {
    [IO.File]::WriteAllText($portableIni, "[Portable]`r`nDataPath=ScriptCardDialogRuntime`r`n", [Text.UTF8Encoding]::new($false))
    if(Test-Path -LiteralPath $dataDirectory) { Remove-Item -LiteralPath $dataDirectory -Recurse -Force }
    New-Item -ItemType Directory -Path $dataDirectory -Force | Out-Null
    $document = Join-Path $dataDirectory 'script-card-runtime.fb2'
    [IO.File]::WriteAllText($document, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Script card runtime</book-title><lang>ru</lang></title-info></description><body><section><title><p>Test</p></title><p>Test</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $report = Join-Path $dataDirectory 'Diagnostics\portable-state-report.txt'
    $savedMode = $env:FBE_NEXT_TEST_MODE
    $savedScenario = $env:FBE_NEXT_TEST_SCENARIO
    $savedArtifactDirectory = $env:FBE_NEXT_TEST_ARTIFACT_DIR
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'script-card-dialog-runtime'
        if($ArtifactDirectory) {
            $ArtifactDirectory = [IO.Path]::GetFullPath($ArtifactDirectory)
            New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
            $env:FBE_NEXT_TEST_ARTIFACT_DIR = $ArtifactDirectory
        } else {
            Remove-Item Env:FBE_NEXT_TEST_ARTIFACT_DIR -ErrorAction SilentlyContinue
        }
        $process = Start-Process -FilePath $FbeExe -WorkingDirectory $exeDirectory -ArgumentList @('--portable', $document) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-Process -Id $process.Id -Force
            throw 'Script-card runtime scenario timed out.'
        }
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
        foreach($line in @('phase=script-card-dialog', 'missing=1', 'reused=1', 'rendered=1', 'optional=1', 'uid=1', 'changed-warning=1', 'buttons=1', 'info-click=1', 'info-double-click-safe=1', 'context-opens=1', 'multi-card-disabled=1', 'run-count=0', 'result=pass')) {
            if($text -notmatch [regex]::Escape($line)) { throw "Script-card runtime scenario failed:`n$text" }
        }
        if($ArtifactDirectory) {
            foreach($name in @('script-card.bmp', 'script-tree-info.bmp', 'script-card-dark.bmp', 'script-tree-info-dark.bmp')) {
                if(-not (Test-Path -LiteralPath (Join-Path $ArtifactDirectory $name))) { throw "Missing script-card screenshot: $name" }
            }
        }
        if($process.ExitCode -ne 0) { throw "Script-card process exit code $($process.ExitCode):`n$text" }
    } finally {
        $env:FBE_NEXT_TEST_MODE = $savedMode
        $env:FBE_NEXT_TEST_SCENARIO = $savedScenario
        $env:FBE_NEXT_TEST_ARTIFACT_DIR = $savedArtifactDirectory
    }
    Write-Host 'Script-card dialog runtime regression passed.'
} finally {
    if($hadIni) { [IO.File]::WriteAllBytes($portableIni, $oldIni) } else { Remove-Item -LiteralPath $portableIni -Force -ErrorAction SilentlyContinue }
    if(Test-Path -LiteralPath $dataDirectory) { Remove-Item -LiteralPath $dataDirectory -Recurse -Force -ErrorAction SilentlyContinue }
}
