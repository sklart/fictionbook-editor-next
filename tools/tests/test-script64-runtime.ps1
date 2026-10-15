[CmdletBinding()]
param([string]$FbeExe, [int]$TimeoutSeconds = 30, [switch]$ParserSelfTest,
    [string]$ArtifactDirectory = (Join-Path $PSScriptRoot '..\..\out\tests\script64-runtime'))
$ErrorActionPreference = 'Stop'
function Read-Script64Report([string]$Text) {
    $entries = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    foreach($line in ($Text -split "`r?`n")) {
        if($line.Length -eq 0) { continue }
        $equals = $line.IndexOf('=')
        if($equals -lt 1) { throw "Malformed report line: $line" }
        $key = $line.Substring(0, $equals)
        if($entries.ContainsKey($key)) { throw "Duplicate report key: $key" }
        $entries.Add($key, $line.Substring($equals + 1))
    }
    foreach($key in @('harness-ran','parser-negative','undo','redo','undo-single','refusal-no-undo','exception-cleanup','ordinary','special',
        'backslash','quote','replacement-special','empty-selection','empty-list','invalid-list',
        'mixed-special','newlines','cyrillic','no-text','invalid-late','nodes','textarea','numeric-order','duplicates','cascade','utf16-reject')) {
        if(!$entries.ContainsKey($key) -or $entries[$key] -cne '1') { throw "Failed/missing check: $key" }
    }
    if(!$entries.ContainsKey('result') -or $entries['result'] -cne 'pass') { throw 'Runtime result is not pass.' }
    return $entries
}
if($ParserSelfTest) {
    $good = (@('harness-ran','parser-negative','undo','redo','undo-single','refusal-no-undo','exception-cleanup','ordinary','special',
        'backslash','quote','replacement-special','empty-selection','empty-list','invalid-list',
        'mixed-special','newlines','cyrillic','no-text','invalid-late','nodes','textarea','numeric-order','duplicates','cascade','utf16-reject') | ForEach-Object { "$_=1" }) -join "`n"
    $good += "`nresult=pass`n"
    $null = Read-Script64Report $good
    foreach($bad in @($good.Replace("`nspecial=1`n", "`nspecial=0`n"),
        $good.Replace("`nspecial=1`n", "`n"), ($good + "special=1`n"),
        $good.Replace("`nspecial=1`n", "`nspecial=10`n"))) {
        $rejected = $false
        try { $null = Read-Script64Report $bad } catch { $rejected = $true }
        if(!$rejected) { throw 'Report parser accepted an invalid synthetic result.' }
    }
    Write-Host 'Script 64 report parser: PASS (positive, collision, missing, duplicate, exact value).'
    return
}
if(!$FbeExe) { throw 'Specify -FbeExe or -ParserSelfTest.' }
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
    if($exitCode -ne 0) { throw "FBE exited with code $exitCode" }
    $null = Read-Script64Report $text
    $passed = $true
} finally {
    try {
        if($clipboardCaptured) {
            [Windows.Forms.Clipboard]::SetDataObject($clipboardSnapshot, $true, 10, 100)
            if(Test-Path -LiteralPath (Join-Path $ArtifactDirectory 'script64-report.txt')) {
                Add-Content -LiteralPath (Join-Path $ArtifactDirectory 'script64-report.txt') -Value 'clipboard-restored=1'
            }
        }
    } finally {
        $env:FBE_NEXT_TEST_MODE = $savedMode
        $env:FBE_NEXT_TEST_SCENARIO = $savedScenario
        $env:FBE_NEXT_TEST_SCRIPT64_PATH = $savedProbe
        Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
    }
}
