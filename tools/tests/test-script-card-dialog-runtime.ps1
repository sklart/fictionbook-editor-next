#Requires -Version 7.0
<# Runs the modeless script-card probe in an isolated portable FBE. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [ValidateRange(15, 120)][int]$TimeoutSeconds = 45, [string]$ArtifactDirectory,
      [switch]$DpiDiagnosticOnly, [switch]$AllowClipboardMutation)

$ErrorActionPreference = 'Stop'
if (-not $DpiDiagnosticOnly -and -not $AllowClipboardMutation) {
    throw 'Full card test changes the system clipboard; use a private clipboard session or explicitly opt in after preserving it.'
}
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'script-card-dialog'
$dataDirectory = $isolation.DataPath
$passed = $false

try {
    $repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    & (Join-Path $repoRoot 'tools\localization\export-runtime-lang.ps1') -RepositoryRoot $repoRoot -OutputDirectory (Join-Path $isolation.Root 'Lang') -Clean | Out-Null
    $document = Join-Path $dataDirectory 'script-card-runtime.fb2'
    [IO.File]::WriteAllText($document, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Script card runtime</book-title><lang>ru</lang></title-info></description><body><section><title><p>Test</p></title><p>Test</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $report = Join-Path $dataDirectory 'Diagnostics\portable-state-report.txt'
    $savedMode = $env:FBE_NEXT_TEST_MODE
    $savedScenario = $env:FBE_NEXT_TEST_SCENARIO
    $savedArtifactDirectory = $env:FBE_NEXT_TEST_ARTIFACT_DIR
    $savedSkipClipboard = $env:FBE_NEXT_TEST_SKIP_CLIPBOARD
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'script-card-dialog-runtime'
        $env:FBE_NEXT_TEST_SKIP_CLIPBOARD = if ($DpiDiagnosticOnly) { '1' } else { $null }
        if($ArtifactDirectory) {
            $ArtifactDirectory = [IO.Path]::GetFullPath($ArtifactDirectory)
            New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
            $env:FBE_NEXT_TEST_ARTIFACT_DIR = $ArtifactDirectory
        } else {
            Remove-Item Env:FBE_NEXT_TEST_ARTIFACT_DIR -ErrorAction SilentlyContinue
        }
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable', $document) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
        if($ArtifactDirectory -and (Test-Path -LiteralPath $report)) {
            Copy-Item -LiteralPath $report -Destination (Join-Path $ArtifactDirectory 'portable-state-report.txt') -Force
        }
        $required = @('phase=script-card-dialog', 'missing=1', 'reused=1', 'rendered=1', 'structured=1', 'responsive=1', 'optional=1', 'characteristic-layout=1', 'characteristic-dpi-matrix=1', 'standard-values=1', 'cleaning-values=1', 'thumb-position=1', 'long-settings-fit=1', 'hashes-hidden=1', 'origin-hidden=1', 'origin-states-hidden=1', 'technical-absent=1', 'window-icons=1', 'bold-heading=1', 'category=1', 'category-formatted=1', 'preview-delayed=1', 'preview-shown=1', 'preview-reusable=1', 'preview-bridge=1', 'preview-bridge-timer=1', 'preview-bridge-exit=1', 'preview-pointer-link=1', 'preview-pointer-exit=1', 'preview-pointer-crossing=1', 'preview-deactivated=1', 'preview-crossing=1', 'preview-switch=1', 'preview-surface=1', 'preview-link=1', 'preview-fallback=1', 'preview-scroll-hidden=1', 'uid=1', 'changed-warning=1', 'buttons=1', 'info-click=1', 'info-selection=1', 'info-double-click-safe=1', 'context-opens=1', 'multi-card-disabled=1', 'comment-content=1', 'comment-narrow=1', 'comment-settings=1', 'comment-dark=1', 'comment-warning=1', 'comment-tooltip=1', 'comment-wide=1', 'comment-changed-dark=1', 'run-count=0')
        foreach($line in $required) {
            if($text -notmatch ('(?m)^' + [regex]::Escape($line) + '\r?$')) { throw "Script-card check '$line' failed:`n$text" }
        }
        if($ArtifactDirectory) {
            foreach($name in @('script-card.bmp', 'script-tree-info.bmp', 'script-card-dark.bmp', 'script-tree-info-dark.bmp', 'comment-card-narrow.bmp', 'comment-card-settings.bmp', 'comment-card-settings-dark.bmp', 'comment-card-changed.bmp', 'comment-card-changed-wide.bmp', 'comment-card-changed-dark.bmp', 'cleaning-card-settings.bmp')) {
                if(-not (Test-Path -LiteralPath (Join-Path $ArtifactDirectory $name))) { throw "Missing script-card screenshot: $name" }
            }
        }
        if($DpiDiagnosticOnly) {
            if($text -notmatch '(?m)^clipboard-skipped=1\r?$' -or $text -notmatch '(?m)^phase=script-card-dialog\r?$' -or
               $text -notmatch '(?m)^result=fail\r?$') { throw "DPI diagnostic incomplete:`n$text" }
            $passed = $true
            Write-Host 'Card checks excluding clipboard: PASS; full card test remains NOT RUN because clipboard was skipped.'
            return
        }
        if($text -notmatch '(?m)^copied-details=1\r?$' -or $text -notmatch '(?m)^result=pass\r?$') { throw "Script-card runtime scenario failed:`n$text" }
        if($text -notmatch '(?m)^clipboard-restored=1\r?$') { throw "Clipboard restoration failed:`n$text" }
        if($exitCode -ne 0) { throw "Script-card process exit code ${exitCode}:`n$text" }
    } finally {
        $env:FBE_NEXT_TEST_MODE = $savedMode
        $env:FBE_NEXT_TEST_SCENARIO = $savedScenario
        $env:FBE_NEXT_TEST_ARTIFACT_DIR = $savedArtifactDirectory
        $env:FBE_NEXT_TEST_SKIP_CLIPBOARD = $savedSkipClipboard
    }
    $passed = $true
    Write-Host 'Script-card dialog runtime regression passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
