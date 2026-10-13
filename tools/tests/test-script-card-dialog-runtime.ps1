<# Runs the modeless script-card probe in an isolated portable FBE. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [ValidateRange(15, 120)][int]$TimeoutSeconds = 45, [string]$ArtifactDirectory)

$ErrorActionPreference = 'Stop'
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
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable', $document) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
        foreach($line in @('phase=script-card-dialog', 'missing=1', 'reused=1', 'rendered=1', 'structured=1', 'responsive=1', 'optional=1', 'hashes-hidden=1', 'origin-shown=1', 'origin-states=1', 'technical-absent=1', 'window-icons=1', 'bold-heading=1', 'copied-details=1', 'category=1', 'preview-delayed=1', 'preview-shown=1', 'preview-reusable=1', 'preview-link=1', 'preview-fallback=1', 'preview-scroll-hidden=1', 'uid=1', 'changed-warning=1', 'buttons=1', 'info-click=1', 'info-double-click-safe=1', 'context-opens=1', 'multi-card-disabled=1', 'run-count=0', 'result=pass')) {
            if($text -notmatch [regex]::Escape($line)) { throw "Script-card runtime scenario failed:`n$text" }
        }
        if($ArtifactDirectory) {
            foreach($name in @('script-card.bmp', 'script-tree-info.bmp', 'script-card-dark.bmp', 'script-tree-info-dark.bmp')) {
                if(-not (Test-Path -LiteralPath (Join-Path $ArtifactDirectory $name))) { throw "Missing script-card screenshot: $name" }
            }
        }
        if($exitCode -ne 0) { throw "Script-card process exit code ${exitCode}:`n$text" }
    } finally {
        $env:FBE_NEXT_TEST_MODE = $savedMode
        $env:FBE_NEXT_TEST_SCENARIO = $savedScenario
        $env:FBE_NEXT_TEST_ARTIFACT_DIR = $savedArtifactDirectory
    }
    $passed = $true
    Write-Host 'Script-card dialog runtime regression passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
