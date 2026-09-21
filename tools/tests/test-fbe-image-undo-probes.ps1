<#
.SYNOPSIS
Runs isolated real-MSHTML undo probes for image import components.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [ValidateSet('binary', 'binary-fill', 'image', 'image-inline', 'image-no-url', 'plain-block', 'plain-block-auto', 'plain-block-markup')]
    [string]$Probe = 'binary',
    [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-image-undo-probe-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
$savedEnvironment = @{}
foreach ($name in 'FBE_NEXT_TEST_MODE', 'FBE_NEXT_TEST_SCENARIO', 'FBE_NEXT_TEST_IMAGE_PATH', 'FBE_NEXT_TEST_IMAGE_PROBE', 'FBE_NEXT_TEST_IMAGE_BINARY_ID') { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    Add-Type -AssemblyName System.Drawing
    $imagePath = Join-Path $directory 'undo-probe.jpg'
    $bitmap = [Drawing.Bitmap]::new(2, 2)
    try { $bitmap.SetPixel(0, 0, [Drawing.Color]::Blue); $bitmap.Save($imagePath, [Drawing.Imaging.ImageFormat]::Jpeg) } finally { $bitmap.Dispose() }
    $base64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes($imagePath))
    $fixture = Join-Path $directory 'fixture.fb2'
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Undo probe</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>undo-probe</id><version>1.0</version></document-info></description><body><section><p>probe paragraph</p></section></body><binary id="existing-image" content-type="image/jpeg">$base64</binary></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $report = Join-Path $directory ($Probe + '.tsv')
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SCENARIO = 'image-undo-probe'
    $env:FBE_NEXT_TEST_IMAGE_PATH = $imagePath
    $env:FBE_NEXT_TEST_IMAGE_PROBE = $Probe
    $env:FBE_NEXT_TEST_IMAGE_BINARY_ID = 'existing-image'
    $process = Start-Process -FilePath $FbeExe -WorkingDirectory $directory -ArgumentList @('-b', $report, $fixture) -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "${Probe}: FBE не завершился." }
    $phases = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { 'report unavailable' }
    if ($process.ExitCode -ne 0) { throw "${Probe}: FBE завершился с $($process.ExitCode). Последняя фаза: $phases" }
    if ($phases -notmatch 'undo-complete') { throw "${Probe}: Undo не завершился. Фазы: $phases" }
    if (($Probe -match '^image' -and $Probe -ne 'image-no-url') -or $Probe -eq 'plain-block' -or $Probe -eq 'plain-block-auto' -or $Probe -eq 'plain-block-markup') { if ($phases -notmatch 'redo-complete') { throw "${Probe}: Redo не завершился. Фазы: $phases" } }
    Write-Host "${Probe}: real-MSHTML undo probe passed."
}
finally {
    foreach ($name in $savedEnvironment.Keys) { if ($null -eq $savedEnvironment[$name]) { Remove-Item -Path "Env:$name" -ErrorAction SilentlyContinue } else { Set-Item -Path "Env:$name" -Value $savedEnvironment[$name] } }
    Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
}
