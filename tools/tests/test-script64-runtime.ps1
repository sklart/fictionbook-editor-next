[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [int]$TimeoutSeconds = 30,
    [string]$ArtifactDirectory = (Join-Path $PSScriptRoot '..\..\out\tests\script64-runtime'))
$ErrorActionPreference = 'Stop'
if([Threading.Thread]::CurrentThread.ApartmentState -ne 'STA') { throw 'Run this test with pwsh -STA.' }
Add-Type -AssemblyName System.Windows.Forms
$clipboardSnapshot = $null
$clipboardCaptured = $false
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'script64'
$passed = $false
$savedMode = $env:FBE_NEXT_TEST_MODE
$savedScenario = $env:FBE_NEXT_TEST_SCENARIO
$savedProbe = $env:FBE_NEXT_TEST_SCRIPT64_PATH
try {
    $sourcePath = Join-Path $root 'runtime\Scripts\21_Поиск и замена\64_Замена символов по списку из буфера обмена.js'
    $bytes = [IO.File]::ReadAllBytes($sourcePath)
    try { $source = [Text.UTF8Encoding]::new($false, $true).GetString($bytes) }
    catch { $source = [Text.Encoding]::GetEncoding(1251).GetString($bytes) }
    $source = $source.TrimStart([char]0xFEFF)
    $probe = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'script64-runtime-probe.js') -Raw
    $wrapper = "function Run(){`r`nvar candidate=(function(){`r`n" + $source + "`r`nreturn Run;})();`r`n" + $probe + "`r`n}`r`n"
    $probePath = Join-Path $isolation.Root 'script64-probe.js'
    [IO.File]::WriteAllText($probePath, $wrapper, [Text.UTF8Encoding]::new($true))
    $document = Join-Path $isolation.DataPath 'script64.fb2'
    [IO.File]::WriteAllText($document, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Script 64 test</book-title><lang>ru</lang></title-info></description><body><section><p>Test</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SCENARIO = 'script64-runtime'
    $env:FBE_NEXT_TEST_SCRIPT64_PATH = $probePath
    $report = Join-Path $isolation.DataPath 'Diagnostics\script64-report.txt'
    # Materialize every original format before FBE writes to the clipboard.
    # Retaining OleGetClipboard's live IDataObject alone is insufficient.
    $originalClipboard = [Windows.Forms.Clipboard]::GetDataObject()
    $clipboardSnapshot = [Windows.Forms.DataObject]::new()
    if($originalClipboard) {
        foreach($format in $originalClipboard.GetFormats($false)) {
            $value = $originalClipboard.GetData($format, $false)
            if($null -eq $value) { throw "Cannot capture clipboard format: $format" }
            if($value -is [IO.Stream]) {
                $position = $value.Position
                $value.Position = 0
                $copy = [IO.MemoryStream]::new()
                $value.CopyTo($copy)
                $value.Position = $position
                $copy.Position = 0
                $value = $copy
            } elseif($value -is [Drawing.Image]) { $value = $value.Clone() }
            $clipboardSnapshot.SetData($format, $false, $value)
        }
    }
    $clipboardCaptured = $true
    $process = Start-Process -FilePath $isolation.Exe -WindowStyle Hidden -WorkingDirectory $isolation.Root -ArgumentList @('--portable', $document) -PassThru
    $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'script64-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
    New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
    Copy-Item -LiteralPath $report -Destination (Join-Path $ArtifactDirectory 'script64-report.txt')
    $text = Get-Content -LiteralPath $report -Raw
    Write-Host $text
    if($exitCode -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw 'Script 64 runtime functional check failed; see saved report.' }
    $passed = $true
} finally {
    if($clipboardCaptured) {
        [Windows.Forms.Clipboard]::SetDataObject($clipboardSnapshot, $true, 10, 100)
        if(Test-Path -LiteralPath (Join-Path $ArtifactDirectory 'script64-report.txt')) {
            Add-Content -LiteralPath (Join-Path $ArtifactDirectory 'script64-report.txt') -Value 'clipboard-restored=1'
        }
    }
    $env:FBE_NEXT_TEST_MODE = $savedMode
    $env:FBE_NEXT_TEST_SCENARIO = $savedScenario
    $env:FBE_NEXT_TEST_SCRIPT64_PATH = $savedProbe
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
