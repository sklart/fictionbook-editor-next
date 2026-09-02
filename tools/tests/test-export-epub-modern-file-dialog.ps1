$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw (Join-Path $root 'src\export-epub\ExportEPUBPlugin.cpp')
$resource = Get-Content -Raw (Join-Path $root 'src\export-epub\ExportEPUB.rc')
function Require([string]$pattern, [string]$message) { if ($source -notmatch $pattern) { throw $message } }
Require 'ModernFileDialog::Show' 'ExportEPUB does not use ModernFileDialog.'
Require 'STDMETHOD\(OnTypeChange\)' 'ExportEPUB must handle selected filter changes.'
Require 'm_version = VersionFromFilterIndex\(index\)' 'EPUB type-change must update export version.'
Require 'version = VersionFromFilterIndex\(result\.filterIndex\)' 'Accepted filter must determine final EPUB version.'
foreach ($legacy in @('OPENFILENAME', 'OFN_ENABLEHOOK', 'OFN_ENABLETEMPLATE', 'SaveDialogHookProc')) { if ($source.Contains($legacy)) { throw "Legacy EPUB dialog token remains: $legacy" } }
if ($resource.Contains('IDD_SAVE_DIALOG_EXTRA')) { throw 'Unused EPUB save-dialog resource remains.' }
if ($source -notmatch 'fileDialogWindow->GetWindow\(&owner\)') { throw 'EPUB settings dialog must use the active file-dialog owner.' }
if ($source -notmatch 'Outcome::Cancelled' -or $source -notmatch 'Outcome::Failed' -or $source -notmatch 'LogFailure\(L"Export EPUB save dialog"') { throw 'EPUB must distinguish Cancelled and Failed file-dialog outcomes.' }
if ($source.Contains('ModernFileDialog::TraceFailure')) { throw 'EPUB dialog errors must not use the temporary dialog logger.' }
Write-Host 'ExportEPUB modern file dialog contract passed.'
