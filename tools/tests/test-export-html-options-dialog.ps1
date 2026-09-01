$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw (Join-Path $root 'src\export-html\HtmlExportOptionsDialog.h')
$resource = Get-Content -Raw (Join-Path $root 'src\export-html\ExportHTML.rc')
$plugin = Get-Content -Raw (Join-Path $root 'src\export-html\ExportHTMLPlugin.cpp')
foreach ($token in @('class CHtmlExportOptionsDialog : public CDialogImpl', 'COMMAND_ID_HANDLER(IDOK, OnOk)', 'COMMAND_ID_HANDLER(IDCANCEL, OnCancel)', 'void Persist() const', 'SetStringValue(_T("CustomCss")')) { if (-not $source.Contains($token)) { throw "Standalone HTML options dialog is missing: $token" } }
foreach ($token in @('IDS_HTML_EXPORT_OPTIONS_TITLE', 'InitTooltips()', 'ModernFileDialog::Show', 'options.Persist()')) { if (($source + $resource + $plugin) -notmatch [regex]::Escape($token)) { throw "HTML modern options regression: $token" } }
if ($resource -notmatch 'IDD_HTML_EXPORT_OPTIONS DIALOGEX') { throw 'Standalone HTML options resource is missing.' }
Write-Host 'HTML export options dialog contract passed.'
