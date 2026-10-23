[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$suites = @(& (Join-Path $PSScriptRoot 'get-required-ci-runtime-suites.ps1'))
foreach ($suite in $suites) {
    $tokens=$null; $errors=$null
    $null=[Management.Automation.Language.Parser]::ParseInput($suite.Command,[ref]$tokens,[ref]$errors)
    if ($errors.Count) { throw "Invalid shared CI command: $($suite.Id)" }
}
$lifecycle=($suites | Where-Object Id -eq 'file-close').Command
foreach ($required in @('test-fbe-file-close-runtime.ps1','test-fbe-new-document-safety-runtime.ps1','-CleanupContext',"-Mode installed -Kind fbd -Theme dark","'zip','rar'")) {
    if (!$lifecycle.Contains($required)) { throw "Lifecycle matrix omits $required" }
}
$api=($suites | Where-Object Id -eq 'scripting15').Command
foreach ($required in @("'portable','installed'","'fb2','fbd'","'zip','rar'",'-InputEncoding utf-16','-Mode installed -DocumentType fbd -InputEncoding utf-16','test-fbe-scripting-examples.ps1')) {
    if (!$api.Contains($required)) { throw "API 1.5 matrix omits $required" }
}
$api14=($suites | Where-Object Id -eq 'scripting14').Command
foreach ($required in @("'portable','installed'","'dialogs','clipboard','status'",'test-status-bar-state.ps1')) {
    if (!$api14.Contains($required)) { throw "API 1.4 matrix omits $required" }
}
if (@($suites | Where-Object RequiresGitHubHosted).Count -ne 1 -or
    ($suites | Where-Object Id -eq 'clipboard-ole').Command -notmatch 'test-fbe-scripting-clipboard-ole-ci.ps1') { throw 'Isolated real OLE must remain mandatory.' }
# A missing producer cannot silently disappear from the catalog/local contour.
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('fbe-ci-matrix-'+[guid]::NewGuid().ToString('N')+'.yml')
try {
    $workflow=Get-Content (Join-Path $PSScriptRoot '../../.github/workflows/build.yml') -Raw
    [IO.File]::WriteAllText($fixture,$workflow.Replace('Run File Close lifecycle regression in isolated profiles','REMOVED producer'))
    $rejected=$false
    try { & (Join-Path $PSScriptRoot 'get-required-ci-runtime-suites.ps1') -WorkflowPath $fixture | Out-Null } catch { $rejected=$true }
    if (!$rejected) { throw 'Missing required producer was accepted.' }
} finally { Remove-Item -LiteralPath $fixture -ErrorAction SilentlyContinue }
Write-Host 'Shared CI/local runtime matrices: PASS (syntax, required cases, missing producer rejection).'
