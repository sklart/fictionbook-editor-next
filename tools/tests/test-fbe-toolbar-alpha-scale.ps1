<#
.SYNOPSIS
Runs the production ARGB scaler on a synthetic magenta-key bitmap at all command-toolbar DPIs.
#>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90, [switch]$KeepArtifacts, [string]$ArtifactDirectory)
$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'toolbar-alpha-scale'
$passed = $false
try {
    if(-not $ArtifactDirectory) { $ArtifactDirectory = Join-Path $isolation.Root 'toolbar-images' }
    $ArtifactDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ArtifactDirectory); New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
    $fixture = Join-Path $isolation.Root 'alpha-scale.fb2'
    $report = Join-Path $isolation.Root 'alpha-scale.tsv'
    $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>alpha</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>alpha-scale-test</id><version>1.0</version></document-info></description><body><section><p>alpha scale</p></section></body></FictionBook>'
    Set-Content -LiteralPath $fixture -Value $xml -Encoding utf8
    $oldMode, $oldScenario, $oldArtifacts = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_ARTIFACT_DIR
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'toolbar-alpha-scale'; $env:FBE_NEXT_TEST_ARTIFACT_DIR = $ArtifactDirectory
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'toolbar-alpha-scale' -Report $report -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE alpha-scale scenario failed: exit $exitCode." }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario; $env:FBE_NEXT_TEST_ARTIFACT_DIR=$oldArtifacts }
    $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
    $expected = @{ 96 = 24; 120 = 30; 144 = 36; 168 = 42; 192 = 48 }
    if($rows.Count -ne $expected.Count) { throw "Expected $($expected.Count) alpha-scale rows, got $($rows.Count)." }
    if($rows[0].dark_supported -notin @('0','1') -or $rows[0].high_contrast -notin @('0','1')) { throw 'Missing platform or high-contrast state in alpha-scale report.' }
    foreach($dpi in $expected.Keys) {
        $row = @($rows | Where-Object dpi -eq $dpi)
        if($row.Count -ne 1 -or [int]$row[0].size -ne $expected[$dpi] -or [int]$row[0].transparent_corners -ne 1 -or [int]$row[0].visible_pixels -le 0 -or [int]$row[0].opaque_black_pixels -ne 0 -or [int]$row[0].magenta_pixels -ne 0 -or [int]$row[0].invalid_premultiplied_pixels -ne 0 -or [int]$row[0].max_color_error -gt 2 -or [int]$row[0].color_fidelity_passed -ne 1 -or [int]$row[0].standard_icons_passed -ne 1 -or [int]$row[0].standard_failure -ne 0 -or [int]$row[0].standard_intermediate_alpha_pixels -ne 0 -or [int]$row[0].standard_silhouette_passed -ne 1 -or [int]$row[0].standard_thin_line_width -le 0 -or [int]$row[0].standard_thin_line_width -gt [math]::Ceiling($expected[$dpi] / 24.0) -or [int]$row[0].standard_image_list_has_mask -ne 0 -or [int]$row[0].partial_alpha_passed -ne 1 -or [int]$row[0].real_icon_blend_passed -ne 1 -or [int]$row[0].passed -ne 1) { throw "ARGB alpha-scale contract failed at $dpi DPI." }
    }
    if($ArtifactDirectory) {
        foreach($dpi in 96,120,144,168,192) {
            $names = @('toolbar-scripts-light', 'toolbar-scripts-dark', 'toolbar-document-tree-light', 'toolbar-document-tree-dark')
            if($dpi -in 96,144,192) { $names += 'toolbar-standard' }
            foreach($name in $names) {
                if(-not (Test-Path -LiteralPath (Join-Path $ArtifactDirectory "$name-$dpi.bmp"))) { throw "Missing $name screenshot for $dpi DPI." }
            }
        }
        foreach($dpi in 96,120,144,192) {
            if(-not (Test-Path -LiteralPath (Join-Path $ArtifactDirectory "toolbar-32-light-$dpi.bmp"))) { throw "Missing 32px toolbar screenshot for $dpi DPI." }
        }
        $darkSheet = Join-Path $ArtifactDirectory 'toolbar-32-dark-96.bmp'
        if($rows[0].dark_supported -eq '1' -and $rows[0].high_contrast -eq '0') {
            if(-not (Test-Path -LiteralPath $darkSheet)) { throw 'Missing classic dark toolbar screenshot.' }
            $rc = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\..\src\fbe\FBE.rc') -Raw
            $toolbar = [regex]::Match($rc, '(?ms)^IDR_MAINFRAME TOOLBAR.*?^END').Value
            $buttons = @([regex]::Matches($toolbar, '(?m)^\s*BUTTON\s+(\S+)') | ForEach-Object { $_.Groups[1].Value })
            $subtitleIndex = [Array]::IndexOf($buttons, 'ID_STYLE_SUBTITLE')
            if($subtitleIndex -lt 0) { throw 'Subtitle command missing from the toolbar resource.' }
            Add-Type -AssemblyName System.Drawing
            $namedSheets = @(Get-ChildItem -LiteralPath $ArtifactDirectory -Filter 'toolbar-32-named-*-96.bmp')
            if($namedSheets.Count -ne 9) { throw "Expected nine named dark palette screenshots, got $($namedSheets.Count)." }
            foreach($imagePath in @($darkSheet) + @($namedSheets | ForEach-Object { $_.FullName })) {
              $bitmap = [Drawing.Bitmap]::new($imagePath)
              try {
                $luminance = { param($color) $values = @($color.R, $color.G, $color.B) | ForEach-Object { $s = $_ / 255.0; if($s -le 0.04045) { $s / 12.92 } else { [Math]::Pow(($s + 0.055) / 1.055, 2.4) } }; 0.2126 * $values[0] + 0.7152 * $values[1] + 0.0722 * $values[2] }
                $background = & $luminance ($bitmap.GetPixel($subtitleIndex * 32, 0))
                $greenPixels = 0; $bestContrast = 0.0
                for($y = 0; $y -lt 32; ++$y) { for($x = 0; $x -lt 32; ++$x) {
                    $color = $bitmap.GetPixel($subtitleIndex * 32 + $x, $y)
                    if($color.G -gt $color.R + 10 -and $color.G -gt $color.B + 10) {
                        ++$greenPixels; $value = & $luminance $color
                        $bestContrast = [Math]::Max($bestContrast, ([Math]::Max($value, $background) + 0.05) / ([Math]::Min($value, $background) + 0.05))
                    }
                } }
                if($greenPixels -lt 5 -or $bestContrast -lt 3.0) { throw "Dark subtitle icon is not recognizably green and readable in $imagePath (pixels=$greenPixels, contrast=$bestContrast)." }
              } finally { $bitmap.Dispose() }
            }
        }
    }
    $passed = $true
    Write-Host 'FBE toolbar ARGB scaling contract passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed -KeepArtifacts:$KeepArtifacts
}
