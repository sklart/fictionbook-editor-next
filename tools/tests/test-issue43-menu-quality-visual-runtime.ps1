<# Short isolated smoke for plugin menu states and Quality Checker resizing. #>
[CmdletBinding()]
param([string]$FbeExe, [string]$ArtifactDirectory, [ValidateRange(15, 90)][int]$TimeoutSeconds = 45)

$ErrorActionPreference = 'Stop'
if(-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'issue43-menu-quality'
$passed = $false
try {
    $catalog = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\..\localization\app-ui\catalog.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $menuCatalog = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\..\localization\app-ui\fbe-idr-mainframe-menu.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $captions = @{
        'import-caption' = 'fbe.plugin.import_epub.menu'
        'html-caption' = 'fbe.plugin.export_html.menu'
        'docx-caption' = 'fbe.plugin.export_docx.menu'
        'epub-caption' = 'fbe.plugin.export_epub.menu'
        'import-expected' = 'fbe.menu.idr_mainframe.plugins.none.import'
        'export-expected' = 'fbe.menu.idr_mainframe.plugins.none.export'
    }
    $fixture = Join-Path $isolation.Root 'issue43.fb2'
    [IO.File]::WriteAllText($fixture, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Issue 43</book-title><lang>ru</lang></title-info></description><body><section><p>Test</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $seed = New-IsolatedFbeSettingsSeed $isolation
    $settingsTemplate = [IO.File]::ReadAllText($seed).Replace('</Settings>', '<IntefaceLangID>0</IntefaceLangID></Settings>')
    $languagePattern = [regex]::new('<IntefaceLangID>[0-9]+</IntefaceLangID>')
    if(-not $languagePattern.IsMatch($settingsTemplate)) { throw 'The isolated settings template lacks the interface language.' }
    $settingsDirectory = Join-Path $isolation.DataPath 'Settings'
    New-Item -ItemType Directory -Path $settingsDirectory -Force | Out-Null
    $languageIds = @{ 'ru-RU' = 0xFBE002; 'en-US' = 0xFBE001 }
    $oldMode,$oldScenario,$oldLocale = $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_UI_LOCALE
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'issue43-menu-quality-visual-runtime'
        foreach($locale in 'ru-RU','en-US') {
            $env:FBE_NEXT_UI_LOCALE = $locale
            $settings = $languagePattern.Replace($settingsTemplate, '<IntefaceLangID>' + $languageIds[$locale] + '</IntefaceLangID>', 1)
            [IO.File]::WriteAllText((Join-Path $settingsDirectory 'Settings.xml'), $settings, [Text.UTF8Encoding]::new($false))
            $report = Join-Path $isolation.Root "issue43-$locale.txt"
            Set-IsolatedRuntimeStage $isolation "issue43-menu-quality/$locale"
            $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
            $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
            $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw -Encoding UTF8 } else { '<missing>' }
            $values = @{}
            foreach($line in ($text -split "`n")) {
                if($line -match '^([^=]+)=(.*)$') { $values[$Matches[1]] = $Matches[2].TrimEnd("`r") }
            }
            foreach($key in @('populated','plugin-count','plugin-ids','plugin-names','empty-menus','empty-ids','empty-texts','empty-disabled','empty-localized','layout','visual')) {
                if($values[$key] -ne '1') { throw "Issue 43 $locale $key failed: $text" }
            }
            if($values['phase'] -ne 'issue43-menu-quality-visual' -or $values['result'] -ne 'pass') { throw "Issue 43 $locale failed: $text" }
            foreach($entry in $captions.GetEnumerator()) {
                $source = if($entry.Key -like '*-expected') { $menuCatalog.strings } else { $catalog.seedStrings }
                $expected = $source.($entry.Value).translations.$locale
                if(-not $expected -or $values[$entry.Key] -cne $expected) {
                    throw "Issue 43 $locale $($entry.Key) differs from localization catalog: $text"
                }
            }
            if($values['import-actual'] -cne $values['import-expected'] -or $values['export-actual'] -cne $values['export-expected']) {
                throw "Issue 43 $locale empty menu captions differ: $text"
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
