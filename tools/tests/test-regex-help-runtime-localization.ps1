[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
$dialog = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpDialog.cpp')
$parser = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpMarkdown.cpp')
$runtimeLocalization = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'src\fbe\RuntimeLocalization.cpp')
if ($runtimeLocalization -notmatch '\{ IDD_REGEX_HELP, IDC_REGEX_HELP_CLOSE, L"fbe\.dialog\.idd_regex_help\.close" \}') { throw 'Regex Help Close button is not bound to its resource ID.' }
if ($dialog -notmatch 'FbeApplyRuntimeDialogLocalization\(m_hWnd, IDD_REGEX_HELP\)') { throw 'Regex Help does not apply runtime dialog localization.' }
foreach ($key in @('fbe.regex_help.design.caption', 'fbe.regex_help.source.caption', 'fbe.dialog.idd_regex_help.close')) {
    foreach ($language in $catalog.targetLanguages) {
        if ([string]::IsNullOrWhiteSpace([string]$catalog.strings.$key.translations.$language)) { throw "Missing $key translation for $language." }
    }
}
if ($parser -notmatch 'GetPreferredRuntimeLocaleName' -or $parser -notmatch 'HelpPathForLocale\(L"en-US"') { throw 'Markdown help must resolve the runtime locale and then en-US.' }
if ($parser -notmatch 'Help file was not found\.') { throw 'Markdown help lacks the emergency fallback message.' }
if ($dialog -match 'BuildHelpBlocks|AddQuickReferenceSyntax|fbe\.regex_help\.(body|example)') { throw 'Long help prose remains embedded in the native dialog.' }
$obsolete = @($catalog.strings.psobject.Properties | Where-Object { $_.Name -match '^fbe\.regex_help\.(body|example)\.' })
if ($obsolete.Count -ne 0) { throw 'Long Regex Help body/example strings must not remain in the localization catalog.' }
$expected = @{ 'en-US' = 'Close'; 'ru-RU' = 'Закрыть' }
foreach ($language in $expected.Keys) { if ($catalog.strings.'fbe.dialog.idd_regex_help.close'.translations.$language -ne $expected[$language]) { throw "Incorrect catalog close caption for $language." } }
$output = Join-Path ([IO.Path]::GetTempPath()) ('fbe-regex-help-lang-' + $PID)
try {
    & (Join-Path $root 'tools\localization\export-runtime-lang.ps1') -RepositoryRoot $root -OutputDirectory $output -Clean | Out-Host
    foreach ($language in $expected.Keys) {
        $runtime = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $output $language 'fbe.json') | ConvertFrom-Json -AsHashtable
        if ($runtime.strings['fbe.dialog.idd_regex_help.close'] -ne $expected[$language]) { throw "Runtime close caption was not exported for $language." }
    }
}
finally { if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Recurse -Force } }
Write-Host 'Regex Help runtime localization contract passed.'
