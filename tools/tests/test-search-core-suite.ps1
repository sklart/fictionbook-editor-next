<# Post-build search and regular-expression regression suite. #>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [string]$PlatformToolset = 'v143',
    [Parameter(Mandatory)][string]$FbeExe
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Invoke-Test([string]$Name, [string[]]$Arguments = @()) {
    & (Join-Path $PSScriptRoot $Name) @Arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
Invoke-Test 'test-search-session.ps1'
Invoke-Test 'test-literal-search-mshtml-differential.ps1'
Invoke-Test 'test-search-document-adapter-mshtml.ps1'
Invoke-Test 'test-search-preset-store.ps1' @('-PlatformToolset',$PlatformToolset)
Invoke-Test 'test-search-preset-catalog.ps1' @('-Configuration',$Configuration,'-UsePreparedPcre2','-PlatformToolset',$PlatformToolset)
Invoke-Test 'test-search-preset-design-fixtures.ps1' @('-Configuration',$Configuration,'-PlatformToolset',$PlatformToolset)
Invoke-Test 'test-search-preset-source-scintilla.ps1' @('-PlatformToolset',$PlatformToolset)
Invoke-Test 'test-search-preset-preview.ps1'
Invoke-Test 'test-search-preset-live-refresh.ps1'
Invoke-Test 'test-search-templates-panel-contract.ps1'
Invoke-Test 'test-search-templates-pin-rendering.ps1'
Invoke-Test 'test-search-templates-open-runtime.ps1' @('-FbeExe',$FbeExe)
Invoke-Test 'test-regex-help-formatting.ps1'
Invoke-Test 'test-regex-help-markdown.ps1'
Invoke-Test 'test-regex-help-runtime.ps1' @('-FbeExe',$FbeExe)
Invoke-Test 'test-regex-help-escape.ps1'
Invoke-Test 'test-regex-quick-reference-runtime.ps1' @('-PlatformToolset',$PlatformToolset)
Invoke-Test 'test-regex-quick-reference-popup.ps1'
Invoke-Test 'test-regex-quick-reference-catalog.ps1'
Invoke-Test 'test-regex-quick-reference-insertion.ps1'
Invoke-Test 'test-search-preset-nbsp.ps1'
Invoke-Test 'test-fbe-find-replace-selection-seed.ps1'
Invoke-Test 'test-fbe-find-replace-ui-contract.ps1'
Invoke-Test 'test-pcre2-match-loop.ps1' @('-Configuration',$Configuration,'-UsePreparedPcre2','-PlatformToolset',$PlatformToolset)
Write-Host 'Search Core suite passed.'