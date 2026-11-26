[CmdletBinding()]
param([string]$FbeExe)
$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '../../out/Release/FBE.exe' }
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'This clipboard-mutating regression requires a disposable GitHub-hosted runner.'
}
if ([Threading.Thread]::CurrentThread.GetApartmentState() -ne [Threading.ApartmentState]::STA) { throw 'PowerShell -STA is required.' }
Add-Type -AssemblyName System.Windows.Forms
$expectedText = 'Clipboard ' + [char]0x0416 + ' ' + [char]0x03a9
$expectedHtml = '<html><body><!--StartFragment--><b>Clipboard HTML</b><!--EndFragment--></body></html>'
$data = [Windows.Forms.DataObject]::new()
$data.SetData([Windows.Forms.DataFormats]::UnicodeText, $false, $expectedText)
$data.SetData([Windows.Forms.DataFormats]::Html, $false, $expectedHtml)
[Windows.Forms.Clipboard]::SetDataObject($data, $true, 10, 100)
try {
    & (Join-Path $PSScriptRoot 'test-fbe-clipboard-paste-runtime.ps1') -FbeExe $FbeExe
    $restored = [Windows.Forms.Clipboard]::GetDataObject()
    if (-not $restored -or $restored.GetData([Windows.Forms.DataFormats]::UnicodeText, $false) -cne $expectedText -or
        $restored.GetData([Windows.Forms.DataFormats]::Html, $false) -cne $expectedHtml) {
        $textValue = if ($restored) { $restored.GetData([Windows.Forms.DataFormats]::UnicodeText, $false) } else { $null }
        $htmlValue = if ($restored) { $restored.GetData([Windows.Forms.DataFormats]::Html, $false) } else { $null }
        throw "Normalizer paste did not restore clipboard data: text=[$textValue], html=[$htmlValue]."
    }
    & (Join-Path $PSScriptRoot 'test-fbe-clipboard-title-paste-runtime.ps1') -FbeExe $FbeExe
    & (Join-Path $PSScriptRoot 'test-fbe-html-paste-runtime.ps1') -FbeExe $FbeExe -RealPaste -Cases word
    [Windows.Forms.Clipboard]::SetDataObject($data, $true, 10, 100)
    & (Join-Path $PSScriptRoot 'test-fbe-html-paste-runtime.ps1') -FbeExe $FbeExe -RealPaste -Cases @('word','queue-reload')
    $restored = [Windows.Forms.Clipboard]::GetDataObject()
    if (-not $restored -or $restored.GetData([Windows.Forms.DataFormats]::UnicodeText, $false) -cne $expectedText -or
        $restored.GetData([Windows.Forms.DataFormats]::Html, $false) -cne $expectedHtml) {
        throw 'The original Unicode or HTML clipboard data was not restored.'
    }
    Write-Host 'Clipboard sequence and Unicode/HTML restoration passed.'
} finally { [Windows.Forms.Clipboard]::Clear() }
