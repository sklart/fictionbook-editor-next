[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'This clipboard-mutating regression requires a disposable GitHub-hosted runner.'
}
if ([Threading.Thread]::CurrentThread.GetApartmentState() -ne [Threading.ApartmentState]::STA) { throw 'PowerShell -STA is required.' }
Add-Type -AssemblyName System.Windows.Forms
& (Join-Path $PSScriptRoot 'test-fbe-clipboard-paste-runtime.ps1') -FbeExe $FbeExe
& (Join-Path $PSScriptRoot 'test-fbe-clipboard-title-paste-runtime.ps1') -FbeExe $FbeExe
& (Join-Path $PSScriptRoot 'test-fbe-html-paste-runtime.ps1') -FbeExe $FbeExe -RealPaste -Cases word
# A second run proves preservation of both nonempty text and HTML formats.
$expectedText = "Clipboard Ж Ω"
$expectedHtml = '<html><body><!--StartFragment--><b>Clipboard Ж Ω</b><!--EndFragment--></body></html>'
$data = [Windows.Forms.DataObject]::new()
$data.SetData([Windows.Forms.DataFormats]::UnicodeText, $false, $expectedText)
$data.SetData([Windows.Forms.DataFormats]::Html, $false, $expectedHtml)
[Windows.Forms.Clipboard]::SetDataObject($data, $true, 10, 100)
try {
    & (Join-Path $PSScriptRoot 'test-fbe-html-paste-runtime.ps1') -FbeExe $FbeExe -RealPaste -Cases @('word','queue-reload')
    $restored = [Windows.Forms.Clipboard]::GetDataObject()
    if (-not $restored -or $restored.GetData([Windows.Forms.DataFormats]::UnicodeText, $false) -cne $expectedText -or
        $restored.GetData([Windows.Forms.DataFormats]::Html, $false) -cne $expectedHtml) {
        throw 'The original Unicode or HTML clipboard data was not restored.'
    }
    Write-Host 'Clipboard sequence and Unicode/HTML restoration passed.'
} finally { [Windows.Forms.Clipboard]::Clear() }
