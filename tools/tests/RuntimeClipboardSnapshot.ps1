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
                [Windows.Forms.Clipboard]::Clear()
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
    [Windows.Forms.Clipboard]::Clear()
    return $snapshot
}

function Restore-FbeRuntimeClipboard {
    param([Parameter(Mandatory)]$Snapshot)
    if ($Snapshot.HadData) { [Windows.Forms.Clipboard]::SetDataObject($Snapshot.Data, $true, 10, 100) }
    else { [Windows.Forms.Clipboard]::Clear() }
}
