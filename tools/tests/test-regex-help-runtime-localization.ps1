[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
$labels = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'localization\regex-help-html.json') | ConvertFrom-Json
$launcher = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpLauncher.cpp')
foreach($key in @('fbe.regex_help.title','fbe.regex_help.open_failed')) {
    if(-not $launcher.Contains($key)) { throw "Launcher does not consume $key" }
    foreach($locale in $catalog.targetLanguages) {
        if([string]::IsNullOrWhiteSpace([string]$catalog.strings.$key.translations.$locale)) { throw "Missing $key/$locale" }
    }
}
foreach($locale in $catalog.targetLanguages) {
    foreach($key in @('design','source','example','note','warning','switch')) {
        if([string]::IsNullOrWhiteSpace([string]$labels.$locale.$key)) { throw "Missing HTML label $locale/$key" }
    }
}
if(@($labels.PSObject.Properties).Count -ne 12) { throw 'HTML label catalog must cover 12 locales.' }
Write-Host 'Regex Help runtime and HTML localization contract passed.'