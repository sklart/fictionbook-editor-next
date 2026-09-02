$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw (Join-Path $root 'src\import-epub\ImportEPUBPlugin.cpp')
if ($source.Contains('GetActiveWindow()')) { throw 'Import EPUB settings owner must not use GetActiveWindow().' }
if ($source -notmatch 'IOleWindow[\s\S]*GetWindow\(&owner\)') { throw 'Import EPUB settings must use the active file-dialog HWND.' }
if ($source -notmatch 'HWND owner = m_owner') { throw 'Import EPUB owner fallback is missing.' }
if ($source -notmatch 'ShowImportOptionsDialog\(owner, edited\)') { throw 'Import EPUB settings dialog call is missing.' }
Write-Host 'Import EPUB modern file dialog owner contract passed.'
