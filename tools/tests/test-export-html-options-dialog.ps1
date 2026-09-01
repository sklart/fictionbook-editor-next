$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw (Join-Path $root 'src\export-html\CustomFileSaveDialog.h')
$resource = Get-Content -Raw (Join-Path $root 'src\export-html\ExportHTML.rc')
foreach ($token in @('class CHtmlExportOptionsDialog : public CDialogImpl', 'COMMAND_ID_HANDLER(IDOK, OnOk)', 'COMMAND_ID_HANDLER(IDCANCEL, OnCancel)', 'SetStringValue(_T("CustomCss")', 'SetDWORDValue(_T("TOCDepth")')) { if (-not $source.Contains($token)) { throw "Standalone HTML options dialog is missing: $token" } }
if ($resource -notmatch 'IDD_HTML_EXPORT_OPTIONS DIALOGEX') { throw 'Standalone HTML options resource is missing.' }
Write-Host 'HTML export options dialog contract passed.'
