[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'), [switch]$BaselineDisabledForDiagnostic,
    [ValidateSet('modern','classic')][string]$DescriptionLayout = 'modern')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'new-edit-state'
$passed = $false
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_NEW_EDIT_OUTPUT','FBE_NEXT_TEST_NEW_EDIT_SCRIPT','FBE_NEXT_TEST_DISABLE_NEW_BASELINE','FBE_NEXT_TEST_DESCRIPTION_LAYOUT')
$saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
try {
    $settings = New-IsolatedFbeSettingsSeed -Isolation $isolation -StartupScriptsDirectory (Join-Path $isolation.Root 'empty-scripts')
    $settingsXml = [IO.File]::ReadAllText($settings, [Text.Encoding]::UTF8).Replace('</Settings>', "<DescriptionLayout>$DescriptionLayout</DescriptionLayout></Settings>")
    [IO.File]::WriteAllText($settings, $settingsXml, [Text.UTF8Encoding]::new($false))
    $fixture = Join-Path $isolation.Root 'original.fb2'
    $report = Join-Path $isolation.Root 'edit-state.txt'
    $output = Join-Path $isolation.Root 'new-saved.fb2'
    $image = Join-Path $isolation.Root 'new-test.png'
    Add-Type -AssemblyName System.Drawing
    $bitmap = [Drawing.Bitmap]::new(3, 2)
    try { $bitmap.Save($image, [Drawing.Imaging.ImageFormat]::Png) } finally { $bitmap.Dispose() }
    $script = Join-Path $isolation.Root 'add-new-binary.js'
    $jsImage = ConvertTo-Json -InputObject $image -Compress
    [IO.File]::WriteAllText($script, "function Run(){} function addNewBinary(){try{return String(window.external.AddBinaryFromFile($jsImage,'new-test.png'));}catch(e){return 'ERROR: '+String(e.number)+' '+String(e.description||e.message||e)+' '+String(window.external.GetLastScriptingError());}}", [Text.UTF8Encoding]::new($false))
    $content = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Fixture</book-title><lang>en</lang></title-info><document-info><id>fixture</id><version>1.0</version></document-info></description><body><section><p>Original fixture</p></section></body></FictionBook>'
    [IO.File]::WriteAllText($fixture, $content, [Text.UTF8Encoding]::new($false))
    $hash = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'new-document-edit-state-runtime'; $env:FBE_NEXT_TEST_NEW_EDIT_OUTPUT = $output; $env:FBE_NEXT_TEST_NEW_EDIT_SCRIPT = $script
    $env:FBE_NEXT_TEST_DISABLE_NEW_BASELINE = if ($BaselineDisabledForDiagnostic) { '1' } else { $null }
    $env:FBE_NEXT_TEST_DESCRIPTION_LAYOUT = $DescriptionLayout
    $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 90
    $result = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
    if ($code -ne 0 -or $result -notmatch 'result=PASS') { throw "New document edit-state test failed ($code): $result" }
    if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $hash) { throw 'Original fixture was modified.' }
    if (!(Test-Path -LiteralPath $output)) { throw 'Saved new document is missing.' }
    Write-Host $result
    $passed = $true
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
