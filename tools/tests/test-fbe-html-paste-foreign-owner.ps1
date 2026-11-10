<# Native clipboard test. Caller must supply an empty, isolated clipboard and restore it externally. #>
[CmdletBinding()]
param([switch]$AllowClipboard)
$ErrorActionPreference = 'Stop'
if (-not $AllowClipboard) { throw 'Pass -AllowClipboard only for an empty, isolated clipboard.' }
if ([Threading.Thread]::CurrentThread.GetApartmentState() -ne [Threading.ApartmentState]::STA) { throw 'Requires PowerShell -STA.' }
Add-Type -AssemblyName System.Windows.Forms
$existing = [Windows.Forms.Clipboard]::GetDataObject()
if ($existing -and @($existing.GetFormats($false)).Count -ne 0) { throw 'Clipboard must be empty before this test.' }
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -PlatformToolset v143
$folder = Join-Path ([IO.Path]::GetTempPath()) ('fbe-foreign-owner-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $folder | Out-Null
$owner = $null; $probe = $null
try {
    $exe = Join-Path $folder 'probe.exe'
    & cl.exe /nologo /DUNICODE /D_UNICODE /source-charset:utf-8 /EHsc /std:c++17 /I (Join-Path $root 'src/fbe') /I (Join-Path $root 'third_party/wtl') "/Fo$folder\" (Join-Path $root 'tools/tests/html-paste-foreign-owner-test.cpp') (Join-Path $root 'src/fbe/clipboard/HtmlPasteSanitizer.cpp') /Fe$exe ole32.lib user32.lib gdi32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
    $ready = Join-Path $folder 'ready'; $done = Join-Path $folder 'done'
    $owner = Start-Process -FilePath $exe -ArgumentList @('owner', ('"' + $ready + '"'), ('"' + $done + '"')) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $folder 'owner.log') -RedirectStandardError (Join-Path $folder 'owner.err')
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while (!(Test-Path -LiteralPath $ready) -and [DateTime]::UtcNow -lt $deadline -and !$owner.HasExited) { Start-Sleep -Milliseconds 50 }
    if (!(Test-Path -LiteralPath $ready)) { throw 'Clipboard owner did not start.' }
    $probe = Start-Process -FilePath $exe -ArgumentList 'probe' -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $folder 'probe.log') -RedirectStandardError (Join-Path $folder 'probe.err')
    if (!$probe.WaitForExit(20000)) { Stop-Process -Id $probe.Id -Force; $probe.WaitForExit(); throw 'Clipboard probe timed out.' }
    $probe.Refresh()
    $output = Get-Content -Raw -LiteralPath (Join-Path $folder 'probe.log')
    Write-Host $output
    if ($probe.ExitCode -ne 0 -or $output -notmatch 'external_owner=1' -or $output -notmatch 'pre_read=1 pre_match=1' -or $output -notmatch 'marked=0 sequence_changed=0 owner_changed=0' -or $output -notmatch 'read=1 content=1' -or $output -notmatch 'marker=0') { throw "Foreign clipboard preservation failed: exit=$($probe.ExitCode)" }
    Write-Host 'Foreign clipboard preservation passed.'
} finally {
    if ($owner) {
        [IO.File]::WriteAllText((Join-Path $folder 'done'), 'done')
        if (!$owner.HasExited -and !$owner.WaitForExit(10000)) { Stop-Process -Id $owner.Id -Force; $owner.WaitForExit() }
    }
    Remove-Item -Recurse -Force -LiteralPath $folder -ErrorAction SilentlyContinue
}
