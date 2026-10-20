[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
& (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion '14.44'
$api = & (Join-Path $PSScriptRoot 'ensure-fbe-api.ps1') -Configuration Release
$objects = Join-Path $root 'out/tests/plugin-settings'; New-Item -ItemType Directory -Force $objects | Out-Null
$harness = Join-Path $objects 'plugin-settings-runtime.exe'
& cl.exe /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE "/I$api" "/Fo:$objects\\" (Join-Path $PSScriptRoot 'plugin-settings-runtime-harness.cpp') (Join-Path $api 'FBE_i.c') /link ole32.lib oleaut32.lib shell32.lib user32.lib "/OUT:$harness"
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
    if($action -in @('dialogs','conversions')) { $arguments += ('"' + (Join-Path $directory 'Plugins') + '"') }
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
    Probe $iso.Root portable dialogs
    $profileA = Join-Path $iso.Root 'ProfileA/Settings/Plugins'
    $first = Profiles $profileA
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
