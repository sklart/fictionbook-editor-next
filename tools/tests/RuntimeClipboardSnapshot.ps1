function Clear-FbeRuntimeClipboard {
    if (-not ('FbeRuntimeClipboardNative' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class FbeRuntimeClipboardNative {
    [DllImport("user32.dll", SetLastError=true)] public static extern bool OpenClipboard(IntPtr owner);
    [DllImport("user32.dll", SetLastError=true)] public static extern bool EmptyClipboard();
    [DllImport("user32.dll", SetLastError=true)] public static extern bool CloseClipboard();
}
'@
    }
    [Windows.Forms.Clipboard]::Clear()
    $opened = $false
    for ($attempt = 0; $attempt -lt 10; $attempt++) {
        if ([FbeRuntimeClipboardNative]::OpenClipboard([IntPtr]::Zero)) { $opened = $true; break }
        Start-Sleep -Milliseconds 20
    }
    if (-not $opened) { throw 'Cannot open clipboard for test cleanup.' }
    try {
        if (-not [FbeRuntimeClipboardNative]::EmptyClipboard()) { throw 'Cannot empty clipboard for test cleanup.' }
    } finally {
        if (-not [FbeRuntimeClipboardNative]::CloseClipboard()) { throw 'Cannot close clipboard after test cleanup.' }
    }
}

function Save-FbeRuntimeClipboard {
    if ([Threading.Thread]::CurrentThread.GetApartmentState() -ne [Threading.ApartmentState]::STA) { throw 'Clipboard test requires PowerShell -STA.' }
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    $original = [Windows.Forms.Clipboard]::GetDataObject()
    $copy = [Windows.Forms.DataObject]::new()
    if ($original) {
        foreach ($format in $original.GetFormats($false)) {
            $value = $original.GetData($format, $false)
            if ($null -eq $value) {
                # A disposable hosted runner may retain an unrenderable delayed
                # format from an earlier test. Never discard such data on a
                # developer desktop, where it may belong to the user.
                if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
                    throw "Cannot capture clipboard format: $format"
                }
                Write-Warning "Discarding unavailable clipboard format on disposable hosted runner: $format"
                Clear-FbeRuntimeClipboard
                return [pscustomobject]@{ HadData = $false; Data = $copy }
            }
            if ($value -is [IO.Stream]) {
                $position = $value.Position; $value.Position = 0
                $stream = [IO.MemoryStream]::new(); $value.CopyTo($stream); $value.Position = $position; $stream.Position = 0
                $value = $stream
            } elseif ($value -is [Drawing.Image]) { $value = $value.Clone() }
            $copy.SetData($format, $false, $value)
        }
    }
    $snapshot = [pscustomobject]@{ HadData = $null -ne $original; Data = $copy }
    Clear-FbeRuntimeClipboard
    return $snapshot
}

function Restore-FbeRuntimeClipboard {
    param([Parameter(Mandatory)]$Snapshot)
    if ($Snapshot.HadData) { [Windows.Forms.Clipboard]::SetDataObject($Snapshot.Data, $true, 10, 100) }
    else { Clear-FbeRuntimeClipboard }
}
