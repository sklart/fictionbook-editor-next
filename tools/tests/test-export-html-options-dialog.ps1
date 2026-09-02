$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw (Join-Path $root 'src\export-html\HtmlExportOptionsDialog.h')
$resource = Get-Content -Raw (Join-Path $root 'src\export-html\ExportHTML.rc')
$plugin = Get-Content -Raw (Join-Path $root 'src\export-html\ExportHTMLPlugin.cpp')
$allSource = $source + $resource + $plugin
foreach ($token in @('class CHtmlExportOptionsDialog : public CDialogImpl', 'COMMAND_ID_HANDLER(IDOK, OnOk)', 'COMMAND_ID_HANDLER(IDCANCEL, OnCancel)', 'void Persist() const', 'SetStringValue(_T("CustomCss")')) { if (-not $source.Contains($token)) { throw "Standalone HTML options dialog is missing: $token" } }
foreach ($token in @('IDS_HTML_EXPORT_OPTIONS_TITLE', 'InitTooltips()', 'ModernFileDialog::Show', 'options.Persist()')) { if ($allSource -notmatch [regex]::Escape($token)) { throw "HTML modern options regression: $token" } }
if ($resource -notmatch 'IDD_HTML_EXPORT_OPTIONS DIALOGEX') { throw 'Standalone HTML options resource is missing.' }
foreach ($token in @('BuildHtmlModernFileTypes', 'IDS_SAVE_FILE_FILTER', 'IDS_OPEN_TEMPLATE_FILTER', 'IDS_OPEN_CSS_FILTER', 'Outcome::Failed', 'FbeDiagnostic::HResult')) { if ($allSource -notmatch [regex]::Escape($token)) { throw "HTML modern dialog regression: $token" } }
if ($allSource.Contains('OutputDebugStringW')) { throw 'Modern file dialog errors must use persistent logging.' }
foreach ($caption in @('HTML with external images', 'XSL files (*.xsl)', 'CSS files (*.css)', 'All files (*.*)')) { if ($allSource.Contains($caption)) { throw "HTML modern dialog contains a hardcoded filter caption: $caption" } }
if ($plugin -notmatch 'Outcome::Cancelled[\s\S]*Outcome::Failed[\s\S]*options\.Persist\(\)') { throw 'HTML settings must persist only after an accepted save dialog.' }
if ($source -match 'OnInitDialog[^{]*\{[^}]*ResolveExportHtmlTemplate\(_Settings\)') { throw 'HTML options OnInitDialog must not reload persistent settings.' }
if ($plugin -notmatch 'options\.LoadSettings\(\);[\s\S]*ModernFileDialog::Show') { throw 'HTML options must load settings once before opening Save dialog.' }
if ($source -notmatch 'OnCancel[\s\S]*EndDialog\(IDCANCEL\)') { throw 'Nested HTML options Cancel must leave the current object unchanged.' }
Write-Host 'HTML export options dialog contract passed.'
