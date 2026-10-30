[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
& (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion '14.44'
$api = & (Join-Path $PSScriptRoot 'ensure-fbe-api.ps1') -Configuration Release
$objects = Join-Path $root 'out/tests/plugin-settings'; New-Item -ItemType Directory -Force $objects | Out-Null
$harness = Join-Path $objects 'plugin-settings-runtime.exe'
& cl.exe /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE "/I$api" "/Fo:$objects\\" (Join-Path $PSScriptRoot 'plugin-settings-runtime-harness.cpp') (Join-Path $api 'FBE_i.c') /link /MANIFEST:EMBED ("/MANIFESTINPUT:" + (Join-Path $root "src/fbe/res/FBE.exe.manifest")) ole32.lib oleaut32.lib shell32.lib user32.lib gdi32.lib gdiplus.lib comctl32.lib "/OUT:$harness"
if($LASTEXITCODE -ne 0) { throw 'Plug-in settings harness did not compile.' }
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'plugin-settings'
$passed = $false
$registryFixture = 'Software\FBEPluginSettingsTests\' + [guid]::NewGuid().ToString('N')
$oldFixture = $env:FBE_NEXT_TEST_REGISTRY_ROOT
$env:FBE_NEXT_TEST_REGISTRY_ROOT = $registryFixture
$names = @('export-html','export-docx','import-epub','export-epub')
$legacy = @('PluginSettings\ExportHTML','ExportDOCX','ImportEPUB','Plugins\ExportEPUB')
$base = 'Software\FBETeam\FictionBook Editor Next\'
function Snapshot {
    $text = & reg.exe query 'HKCU\Software\FBETeam\FictionBook Editor Next' /s 2>&1
    if($LASTEXITCODE -eq 1) { return '<absent>' }
    if($LASTEXITCODE -ne 0) { throw 'Could not snapshot registry.' }
    return ($text -join "`n")
}
function Probe([string]$directory, [string]$mode, [string]$action) {
    $exe = Join-Path $directory 'plugin-settings-runtime.exe'
    $log = Join-Path $directory "settings-$mode-$action.log"
    $arguments = @("--$mode", $action)
    if($action -in @('dialogs','conversions') -or $action.StartsWith('ui-')) { $arguments += ('"' + (Join-Path $directory 'Plugins') + '"') }
    $process = Start-Process $exe -ArgumentList $arguments -WorkingDirectory $directory -WindowStyle Hidden -PassThru -RedirectStandardOutput $log -RedirectStandardError ($log + '.stderr')
    if(!$process.WaitForExit(30000)) { Stop-Process -Id $process.Id -Force; throw "Settings probe timeout: $log" }
    $process.WaitForExit(); $process.Refresh()
    if($process.ExitCode -ne 0) { throw "Settings probe failed ($($process.ExitCode)): $log`n$(Get-Content ($log + '.stderr') -Raw)" }
    Get-Content $log | Write-Host
}
function Profiles([string]$directory) {
    $result = @{}
    foreach($name in $names) {
        $path = Join-Path $directory "$name.json"
        $data = Get-Content $path -Raw | ConvertFrom-Json
        if($data.schemaVersion -ne 1 -or !$data.values) { throw "Invalid saved profile: $path" }
        $result[$name] = [IO.File]::ReadAllText($path)
    }
    return $result
}
try {
    $before = Snapshot
    Copy-Item $harness (Join-Path $iso.Root 'plugin-settings-runtime.exe')
    [IO.File]::WriteAllText((Join-Path $iso.Root 'portable.ini'), "[Portable]`r`nDataPath=ProfileA`r`n")
    $assets = Join-Path $iso.Root 'ProfileA/Settings/Plugins/Templates'; New-Item -ItemType Directory -Force $assets | Out-Null
    $xsl = Join-Path $assets 'custom.xsl'; $css = Join-Path $assets 'custom.css'
    [IO.File]::WriteAllText($xsl, '<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"/>')
    [IO.File]::WriteAllText($css, 'body { color: black; }')
    $unicodeFont = 'Unicode ' + [char]0x0416
    $seed = @{schemaVersion=1;values=@{UseCustomTemplate=1;Template=$xsl;CustomCss=$css;CustomFontFamily=$unicodeFont}} | ConvertTo-Json
    [IO.File]::WriteAllText((Join-Path $assets '../export-html.json'), $seed, [Text.UTF8Encoding]::new($false))
    foreach($action in @('ui-position-default','ui-position-move','ui-position-verify')) { Probe $iso.Root portable $action }
    Probe $iso.Root portable dialogs
    $profileA = Join-Path $iso.Root 'ProfileA/Settings/Plugins'
    $first = Profiles $profileA
    $importSettings = $first['import-epub'] | ConvertFrom-Json
    if($null -eq $importSettings.values.SeparatorSubtitles -or $importSettings.values.SeparatorSubtitles -ne 0) { throw 'SeparatorSubtitles must be persisted and disabled by default.' }
    if($null -eq $importSettings.values.CreateTitleBlock -or $importSettings.values.CreateTitleBlock -ne 0) { throw 'CreateTitleBlock must be persisted and disabled by default.' }
    foreach($field in @('UnifyImageNames','SeparateCoverName','UnifyNoteNumbers','BlankBeforeImages','BlankAfterImages')) {
        if($null -eq $importSettings.values.$field -or $importSettings.values.$field -ne 0) { throw "$field must be persisted and disabled by default." }
    }
    foreach($pair in @(@('ui-import-unify-images','UnifyImageNames'),@('ui-import-cover-name','SeparateCoverName'),@('ui-import-unify-notes','UnifyNoteNumbers'),@('ui-import-blank-before','BlankBeforeImages'),@('ui-import-blank-after','BlankAfterImages'))) {
        Probe $iso.Root portable $pair[0]
        $enabled = Get-Content (Join-Path $profileA 'import-epub.json') -Raw | ConvertFrom-Json
        if($enabled.values.($pair[1]) -ne 1) { throw "Option $($pair[1]) was not enabled through the dialog." }
        Probe $iso.Root portable dialogs
        $reloaded = Get-Content (Join-Path $profileA 'import-epub.json') -Raw | ConvertFrom-Json
        if($reloaded.values.($pair[1]) -ne 1) { throw "Option $($pair[1]) did not survive reloading." }
        Probe $iso.Root portable $pair[0]
    }
    Probe $iso.Root portable ui-import-title
    $titleEnabled = Get-Content (Join-Path $profileA 'import-epub.json') -Raw | ConvertFrom-Json
    if($titleEnabled.values.CreateTitleBlock -ne 1) { throw 'The title block option was not enabled through the dialog.' }
    Probe $iso.Root portable dialogs
    $titleReloaded = Get-Content (Join-Path $profileA 'import-epub.json') -Raw | ConvertFrom-Json
    if($titleReloaded.values.CreateTitleBlock -ne 1) { throw 'The title block option was not retained after reopening.' }
    Probe $iso.Root portable ui-import-title
    $titleDisabled = Get-Content (Join-Path $profileA 'import-epub.json') -Raw | ConvertFrom-Json
    if($titleDisabled.values.CreateTitleBlock -ne 0) { throw 'The title block option was not disabled through the dialog.' }
    Probe $iso.Root portable ui-import-separator
    $enabledSettings = Get-Content (Join-Path $profileA 'import-epub.json') -Raw | ConvertFrom-Json
    if($enabledSettings.values.SeparatorSubtitles -ne 1) { throw 'The separator option was not enabled through the dialog.' }
    Probe $iso.Root portable dialogs
    $reloadedSettings = Get-Content (Join-Path $profileA 'import-epub.json') -Raw | ConvertFrom-Json
    if($reloadedSettings.values.SeparatorSubtitles -ne 1) { throw 'The separator option was not retained after reopening.' }
    Probe $iso.Root portable ui-import-separator
    $disabledSettings = Get-Content (Join-Path $profileA 'import-epub.json') -Raw | ConvertFrom-Json
    if($disabledSettings.values.SeparatorSubtitles -ne 0) { throw 'The separator option was not disabled through the dialog.' }
    foreach($entry in @(@('export-epub','IncludeCoverPage'), @('export-docx','ExportImages'), @('import-epub','ImportImages'))) {
        $settings = $first[$entry[0]] | ConvertFrom-Json
        if($settings.values.($entry[1]) -ne 1) { throw "Default setting was lost by the dialog: $($entry -join '/')" }
    }
    $html = $first['export-html'] | ConvertFrom-Json
    if($html.values.Template -notlike 'ProfileA*' -or $html.values.CustomCss -notlike 'ProfileA*' -or $html.values.CustomFontFamily -ne $unicodeFont) { throw 'HTML XSL/CSS paths or Unicode settings were not preserved.' }
    # Every persisted field must survive loading and saving through its actual
    # settings dialog in another process, not only through the shared codec.
    Probe $iso.Root portable dialogs
    $again = Profiles $profileA
    foreach($name in $names) { if($first[$name] -cne $again[$name]) { throw "Settings changed after restart: $name" } }
    $moved = Join-Path $iso.Root 'Moved'; New-Item -ItemType Directory $moved | Out-Null
    Get-ChildItem $iso.Root -Force | Where-Object Name -ne 'Moved' | Copy-Item -Destination $moved -Recurse -Force
    Probe $moved portable dialogs
    $relocated = Profiles (Join-Path $moved 'ProfileA/Settings/Plugins')
    foreach($name in $names) { if($first[$name] -cne $relocated[$name]) { throw "Settings changed after relocation: $name" } }
    $field = @('IncludeToc','ExportImages','ImportImages','IncludeTitlePage')
    for($i=0; $i -lt $names.Count; ++$i) {
        $path = Join-Path $moved "ProfileA/Settings/Plugins/$($names[$i]).json"
        $data = Get-Content $path -Raw | ConvertFrom-Json
        $data.values.($field[$i]) = 1 - [int]$data.values.($field[$i])
        [IO.File]::WriteAllText($path, ($data | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
    }
    Probe $moved portable dialogs
    $different = Profiles (Join-Path $moved 'ProfileA/Settings/Plugins')
    $unchanged = Profiles $profileA
    foreach($name in $names) {
        if($first[$name] -cne $unchanged[$name] -or $first[$name] -ceq $different[$name]) { throw "Portable copies are not isolated: $name" }
    }
    Probe $iso.Root portable ui-save
    $edited = Profiles $profileA
    foreach($name in $names) { if($edited[$name] -ceq $first[$name]) { throw "UI edits were not saved: $name" } }
    Probe $iso.Root portable dialogs
    $reloaded = Profiles $profileA
    foreach($name in $names) { if($edited[$name] -cne $reloaded[$name]) { throw "UI edits were lost after restart: $name" } }
    foreach($action in @('ui-cancel','ui-defaults-cancel','ui-readonly')) {
        Probe $iso.Root portable $action
        $after = Profiles $profileA
        foreach($name in $names) { if($edited[$name] -cne $after[$name]) { throw "Cancelled/failed dialog changed the profile: $action/$name" } }
    }
    Probe $iso.Root portable ui-conflict
    $newer = Profiles $profileA
    foreach($name in $names) { if($newer[$name] -ceq $edited[$name]) { throw "The peer instance did not save: $name" } }
    Probe $iso.Root portable ui-corrupt
    foreach($name in $names) {
        $path = Join-Path $profileA "$name.json"
        if([IO.File]::ReadAllText($path) -cne '{bad') { throw "A damaged profile was overwritten: $name" }
        [IO.File]::WriteAllText($path,$newer[$name],[Text.UTF8Encoding]::new($false))
    }
    foreach($action in @('ui-default','ui-compat','ui-rich')) {
        Probe $iso.Root portable $action
        $expected = @{IncludeNcxFallbackInEpub3=1;IncludeCoverPage=1;IncludeAnnotationPage=1;IncludeTitlePage=0;WriteDiagnosticLog=0;
            ShowSummaryAfterExport=0;OpenFileAfterExport=0;OpenFolderAfterExport=0;ShowPreflightWarnings=1;CssJustifyText=1;
            CssFirstLineIndent=0;CssHyphenate=0;IncludeCoverInToc=1;IncludeAnnotationInToc=1;IncludeNotesInToc=1;IncludeNoteBacklinks=1;
            UseFirstImageAsCoverWhenMissing=1;RemoveUnusedImages=0;WarnLargeXhtmlKb=512;MaxTocDepth=6}
        if($action -eq 'ui-compat') { $expected.CssJustifyText=0; $expected.WarnLargeXhtmlKb=384; $expected.MaxTocDepth=4 }
        if($action -eq 'ui-rich') { $expected.ShowSummaryAfterExport=1; $expected.IncludeTitlePage=1; $expected.CssHyphenate=1; $expected.RemoveUnusedImages=1; $expected.MaxTocDepth=8 }
        $actual = Get-Content (Join-Path $profileA 'export-epub.json') -Raw | ConvertFrom-Json
        foreach($fieldName in $expected.Keys) { if($actual.values.$fieldName -ne $expected[$fieldName]) { throw "Preset failed: $action/$fieldName" } }
    }
    $beforeFileDialog = Profiles $profileA
    Probe $iso.Root portable ui-file-save
    $afterFileDialog = Profiles $profileA
    if($beforeFileDialog['import-epub'] -ceq $afterFileDialog['import-epub']) { throw 'Import settings opened from the file dialog did not save.' }
    foreach($action in @('ui-file-cancel','ui-file-readonly')) {
        Probe $iso.Root portable $action
        $after = Profiles $profileA
        foreach($name in $names) { if($afterFileDialog[$name] -cne $after[$name]) { throw "File dialog changed the profile after cancellation/failure: $action/$name" } }
    }
    # Restore the baseline before the legacy compatibility and conversion cases.
    foreach($name in $names) { [IO.File]::WriteAllText((Join-Path $profileA "$name.json"),$first[$name],[Text.UTF8Encoding]::new($false)) }
    Write-Host 'All tabs/edit/save/cancel/defaults/presets/conflict between two processes/error retention: PASS.'
    Probe $iso.Root portable store
    Probe $iso.Root portable conversions
    if((Snapshot) -cne $before) { throw 'Portable plug-ins changed the Installed registry profile.' }
    # Redirect HKCU only inside the native Installed test process. Seed the old
    # key layout and verify it is retained, without touching the user's keys.
    $view = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser,[Microsoft.Win32.RegistryView]::Registry32)
    try {
        for($i=0; $i -lt $names.Count; ++$i) {
            $key = $view.CreateSubKey("$registryFixture\$base$($legacy[$i])")
            try { $key.SetValue($field[$i], 0, [Microsoft.Win32.RegistryValueKind]::DWord) } finally { $key.Dispose() }
        }
        foreach($action in @('ui-position-default','ui-position-move','ui-position-verify')) { Probe $iso.Root installed $action }
        Probe $iso.Root installed dialogs
        Probe $iso.Root installed dialogs
        for($i=0; $i -lt $names.Count; ++$i) {
            $key = $view.OpenSubKey("$registryFixture\$base$($legacy[$i])")
            try { if($key.GetValue($field[$i], -1) -ne 0 -or $key.GetValueNames().Count -lt 10) { throw "Installed legacy settings were reset: $($names[$i])" } } finally { $key.Dispose() }
        }
        $afterInstalled = Profiles $profileA
        foreach($name in $names) { if($first[$name] -cne $afterInstalled[$name]) { throw "Installed changed Portable: $name" } }
        $migration = Join-Path $iso.Root 'Migration'; New-Item -ItemType Directory $migration | Out-Null
        [IO.File]::WriteAllText((Join-Path $migration 'portable.ini'), "[Portable]`r`nDataPath=ImportedData`r`n")
        $migrate = Join-Path $root 'tools/settings/import-plugin-settings.ps1'
        & $migrate -PortableDirectory $migration -RegistryRoot "$registryFixture\$base"
        $imported = Profiles (Join-Path $migration 'ImportedData/Settings/Plugins')
        $refused = $false
        try { & $migrate -PortableDirectory $migration -RegistryRoot "$registryFixture\$base" } catch { $refused = $true }
        if(!$refused) { throw 'Migration overwrote existing profiles without consent.' }
        $afterRefusal = Profiles (Join-Path $migration 'ImportedData/Settings/Plugins')
        foreach($name in $names) { if($imported[$name] -cne $afterRefusal[$name]) { throw "Migration damaged existing profile: $name" } }
        & $migrate -PortableDirectory $migration -RegistryRoot "$registryFixture\$base" -ReplaceExisting
        Write-Host 'Explicit migration/custom DataPath/no implicit overwrite: PASS.'
    } finally { $view.Dispose() }
    if((Snapshot) -cne $before) { throw 'Installed fixture escaped its redirected registry root.' }
    Write-Host 'Plug-in settings persistence/restart/relocation/two copies/Installed compatibility: PASS.'
    $passed = $true
} finally {
    $env:FBE_NEXT_TEST_REGISTRY_ROOT = $oldFixture
    $view = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser,[Microsoft.Win32.RegistryView]::Registry32)
    try { $view.DeleteSubKeyTree($registryFixture, $false) } finally { $view.Dispose() }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
