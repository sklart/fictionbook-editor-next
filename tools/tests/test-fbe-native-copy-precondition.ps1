<# Verify that the real native-copy probe refuses a nonempty clipboard safely. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe)
$ErrorActionPreference = 'Stop'
if ([Threading.Thread]::CurrentThread.GetApartmentState() -ne [Threading.ApartmentState]::STA) { throw 'This clipboard test requires PowerShell -STA.' }
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition 'using System.Runtime.InteropServices; public static class FbeClipboardSequenceProbe { [DllImport("user32.dll")] public static extern uint GetClipboardSequenceNumber(); }'
$original = [Windows.Forms.Clipboard]::GetDataObject()
$snapshot = [Windows.Forms.DataObject]::new()
if ($original) {
    foreach ($format in $original.GetFormats($false)) {
        $value = $original.GetData($format, $false)
        if ($null -eq $value) { throw "Cannot capture clipboard format: $format" }
        if ($value -is [IO.Stream]) {
            if (-not $value.CanSeek) { throw "Cannot snapshot nonseekable clipboard format: $format" }
            $position=$value.Position; $value.Position=0; $copy=[IO.MemoryStream]::new(); $value.CopyTo($copy)
            $value.Position=$position; $copy.Position=0; $value=$copy
        } elseif ($value -is [Drawing.Image]) { $value=$value.Clone() }
        $snapshot.SetData($format, $false, $value)
    }
}
try {
    $marker='FBE native clipboard precondition fixture'
    [Windows.Forms.Clipboard]::SetText($marker)
    $before=[FbeClipboardSequenceProbe]::GetClipboardSequenceNumber()
    $rejected=$false
    try {
        & (Join-Path $PSScriptRoot 'test-fbe-native-copy-paste-runtime.ps1') -FbeExe $FbeExe -Cases char
    } catch {
        $message=$_.Exception.Message
        $rejected=$message -match 'exit=1\b' -and $message -match 'clipboard_precondition=nonempty-or-unavailable' -and $message -match 'result=FAIL'
    }
    $after=[FbeClipboardSequenceProbe]::GetClipboardSequenceNumber()
    if (-not $rejected) { throw 'Native-copy probe did not reject the nonempty clipboard safely.' }
    if ($before -ne $after -or [Windows.Forms.Clipboard]::GetText() -cne $marker) { throw 'Native-copy probe changed the protected clipboard.' }
    Write-Host 'Native-copy nonempty clipboard precondition passed; sequence and text unchanged.'
} finally {
    if ($original) { [Windows.Forms.Clipboard]::SetDataObject($snapshot,$true,10,100) }
    else { [Windows.Forms.Clipboard]::Clear() }
}
