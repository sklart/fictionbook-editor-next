<# Exercises real settings group painting in an isolated portable profile. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90, [ValidateSet('ru-RU','en-US','de-DE')][string]$Locale = 'ru-RU', [ValidateSet('sepia','navy','catppuccin-latte','catppuccin-mocha','solarized-light','solarized-dark','everforest-light-medium','dracula','catppuccin-frappe','high-contrast-light','high-contrast-dark','graphite','custom-probe','custom-light-probe')][string]$Palette = 'catppuccin-mocha', [switch]$ControlOnly)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'settings-group-paint'
$passed = $false
try {
    $settings = Join-Path $isolation.DataPath 'Settings\Settings.xml'
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $settings) | Out-Null
    $theme = if($Palette -in @('sepia','catppuccin-latte','solarized-light','everforest-light-medium','high-contrast-light','custom-light-probe')) { 1 } else { 2 }
    $paletteId = $Palette
    if($Palette -in @('custom-probe','custom-light-probe')) {
        $paletteId = 'custom:probe'
        $themes = Join-Path (Split-Path -Parent $settings) 'InterfaceThemes'
        New-Item -ItemType Directory -Force -Path $themes | Out-Null
        $base=if($Palette -eq 'custom-light-probe'){'light'}else{'dark'}
        $paletteJson = '{"schemaVersion":1,"id":"probe","name":"Pixel probe","base":"' + $base + '","colors":{"window":"#101820","control":"#325468","text":"#F5E6D0","border":"#6699AA","disabledText":"#9BA5B0","selectionBackground":"#AD6820","selectionText":"#FFFFFF","hover":"#176328","pressed":"#713A19","focus":"#FFE100"}}'
        [IO.File]::WriteAllText((Join-Path $themes 'probe.json'), $paletteJson, [Text.UTF8Encoding]::new($false))
    }
    $settingsXml = '<?xml version="1.0" encoding="utf-8"?><FBE><Settings ID="0"><DocumentTreeScripts>false</DocumentTreeScripts><InterfaceTheme>{0}</InterfaceTheme><InterfacePalette>{1}</InterfacePalette><TextCaretMode>0</TextCaretMode></Settings></FBE>' -f $theme,$paletteId
    [IO.File]::WriteAllText($settings, $settingsXml, [Text.UTF8Encoding]::new($false))
    $fixture = Join-Path $isolation.Root 'paint.fb2'
    $report = Join-Path $isolation.Root 'paint.txt'
    [IO.File]::WriteAllText($fixture, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Paint</book-title><lang>en</lang></title-info><document-info><id>paint-test</id><version>1</version></document-info></description><body><section><p>Paint test</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $originalHash = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
    $keys = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_PAINT_ONLY','FBE_NEXT_UI_LOCALE')
    $saved = @{}; foreach($key in $keys) { $saved[$key] = [Environment]::GetEnvironmentVariable($key) }
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='settings-dialog-runtime'; $env:FBE_NEXT_TEST_SETTINGS_PAINT_ONLY='1'; $env:FBE_NEXT_UI_LOCALE=$Locale
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable','-b',('"'+$report+'"'),('"'+$fixture+'"')) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'settings-group-paint' -Report $report -TimeoutSeconds $TimeoutSeconds
    } finally { foreach($key in $keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key]) } }
    $content = if(Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing>' }
    if($exitCode -ne 0 -or $content -notmatch '(?m)^button-states=31\r?$' -or $content -notmatch '(?m)^combo-states=1023\r?$' -or $content -notmatch '(?m)^editable-combo-states=127\r?$' -or $content -notmatch '(?m)^grouped-combo-states=15\r?$' -or $content -notmatch '(?m)^disabled-label-paint=1\r?$' -or $content -notmatch '(?m)^toggle-states=511\r?$' -or $content -notmatch '(?m)^spin-states=127\r?$' -or $content -notmatch '(?m)^hover-text=1\r?$' -or $content -notmatch '(?m)^layout=1\r?$' -or $content -notmatch '(?m)^source_unchanged=1\r?$') { throw "Settings controls paint regression: exit=$exitCode; $content" }
    if($Palette -in @('custom-probe','custom-light-probe')) {
        foreach($line in @('custom-active=1', 'control-color=6837298', 'hover-color=2646807', 'pressed-color=1653361')) {
            if($content -notmatch [regex]::Escape($line)) { throw "Custom probe palette was not applied: $content" }
        }
    }
    $areas = [regex]::Match($content, '(?m)^group-areas=(\d+) group-failures=(\d+)\r?$')
    if(-not $ControlOnly -and ($content -notmatch '(?m)^group-paint=1\r?$' -or $content -notmatch '(?m)^disabled-text=1\r?$' -or $content -notmatch '(?m)^checkbox-caption=1\r?$' -or $content -notmatch '(?m)^repaint-pages=9 repaint-failures=0\r?$' -or $content -notmatch '(?m)^hover-pages=9 hover-failures=0\r?$' -or -not $areas.Success -or [int]$areas.Groups[1].Value -lt 10 -or [int]$areas.Groups[2].Value -ne 0)) { throw "Settings group paint coverage failed: $content" }
    if((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $originalHash) { throw 'Settings paint test modified the FB2 fixture.' }
    $passed = $true
    Write-Host "Settings paint and document preservation: PASS ($Locale, $Palette, $($areas.Groups[1].Value) group regions)"
} finally { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed }
