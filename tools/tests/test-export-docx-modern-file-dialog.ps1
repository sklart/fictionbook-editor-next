$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw (Join-Path $root 'src\export-docx\ExportDOCXPlugin.cpp')
$resource = Get-Content -Raw (Join-Path $root 'src\export-docx\ExportDOCX.rc')
if ($source -notmatch 'ModernFileDialog::Show') { throw 'ExportDOCX does not use ModernFileDialog.' }
if ($source -notmatch 'AddPushButton\(IDC_FILEDLG_SETTINGS') { throw 'ExportDOCX settings button was not migrated.' }
foreach ($legacy in @('CDocxSaveDialog', 'OFN_ENABLEHOOK', 'OFN_ENABLETEMPLATE', 'OPENFILENAME')) {
    if ($source.Contains($legacy)) { throw "ExportDOCX still contains legacy file-dialog token: $legacy" }
}
if ($resource.Contains('IDD_DOCX_FILE_OPTIONS')) { throw 'Unused DOCX file-dialog resource remains.' }
Write-Host 'ExportDOCX modern file dialog contract passed.'
