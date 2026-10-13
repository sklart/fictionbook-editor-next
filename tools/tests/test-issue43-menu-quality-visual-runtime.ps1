<# Short isolated smoke for plugin menu states and Quality Checker resizing. #>
[CmdletBinding()]
param([string]$FbeExe, [string]$ArtifactDirectory, [ValidateRange(15, 90)][int]$TimeoutSeconds = 45)

$ErrorActionPreference = 'Stop'
if(-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'issue43-menu-quality'
$passed = $false
try {
    $fixture = Join-Path $isolation.Root 'issue43.fb2'
    [IO.File]::WriteAllText($fixture, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Issue 43</book-title><lang>ru</lang></title-info></description><body><section><p>Test</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $oldMode,$oldScenario,$oldLocale = $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_UI_LOCALE
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'issue43-menu-quality-visual-runtime'
        foreach($locale in 'ru-RU','en-US') {
            $env:FBE_NEXT_UI_LOCALE = $locale
            $report = Join-Path $isolation.Root "issue43-$locale.txt"
            $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
            $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
            $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing>' }
            foreach($line in @('phase=issue43-menu-quality-visual','populated=1','empty-localized=1','layout=1','visual=1','result=pass')) {
                if($text -notmatch [regex]::Escape($line)) { throw "Issue 43 $locale failed: $text" }
            }
            if($exitCode -ne 0) { throw "Issue 43 $locale process exited $exitCode`: $text" }
            $bmp = "$report.bmp"
            if(-not (Test-Path -LiteralPath $bmp) -or (Get-Item -LiteralPath $bmp).Length -lt 1024) { throw "Missing Quality Checker BMP for $locale" }
            if($ArtifactDirectory) {
                New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
                Copy-Item -LiteralPath $bmp -Destination (Join-Path $ArtifactDirectory "issue43-quality-$locale.bmp") -Force
            }
        }
    } finally {
        $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_UI_LOCALE = $oldMode,$oldScenario,$oldLocale
    }
    $passed = $true
    Write-Host 'Issue 43 Import/Export menu and Quality Checker resize smoke passed (ru-RU, en-US).'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
