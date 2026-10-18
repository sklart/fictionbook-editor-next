$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dialog = Get-Content -Raw (Join-Path $root 'src\common\ModernFileDialog.h')
$diagnostic = Get-Content -Raw (Join-Path $root 'src\common\DiagnosticLog.h')
$images = Get-Content -Raw (Join-Path $root 'src\fbe\ImageImport.cpp')
$main = Get-Content -Raw (Join-Path $root 'src\fbe\mainfrm.cpp')
$view = Get-Content -Raw (Join-Path $root 'src\fbe\FBEview.cpp')

function Require([string]$text, [string]$pattern, [string]$message) { if ($text -notmatch $pattern) { throw $message } }
Require $dialog 'std::wstring title, okButtonLabel, fileNameLabel, defaultExtension' 'Request must own dialog strings.'
Require $dialog 'std::wstring initialFileName, initialFolder' 'Request must own initial paths.'
Require $images 'ImageImportFileTypes\(' 'Image filters need one shared source.'
Require $images 'L"\*\.jpg;\*\.jpeg;\*\.png;\*\.webp;\*\.jp2;\*\.j2k;\*\.bmp;\*\.gif;\*\.tif;\*\.tiff;\*\.avif;\*\.heic;\*\.heif"' 'Supported modern image spec is incomplete.'
$pickerPattern = '\b\w+\.push_back\(\{\s*(\w+)\.displayName\.GetString\(\),\s*\1\.wildcard\.GetString\(\)\s*\}\)'
Require $main $pickerPattern 'Binary picker must use wildcard image specs.'
Require $view $pickerPattern 'Image picker must use wildcard image specs.'
Require 'arbitraryEntries.push_back({ renamedImageType.displayName.GetString(), renamedImageType.wildcard.GetString() })' $pickerPattern 'Picker contract must accept arbitrary local names.'
if ('arbitraryEntries.push_back({ firstType.displayName.GetString(), secondType.wildcard.GetString() })' -match $pickerPattern) { throw 'Picker contract must require the wildcard from the same image type.' }
if ($main -match 'ImageImportFileFilter\(\).*COMDLG_FILTERSPEC' -or $view -match 'ImageImportFileFilter\(\).*COMDLG_FILTERSPEC') { throw 'Legacy NUL-delimited image filter passed to modern dialog.' }
if ($dialog -match 'std::wofstream|modern-file-dialog\.log') { throw 'ModernFileDialog must not own a separate file logger.' }
Require $dialog 'DiagnosticLog\.h' 'ModernFileDialog must include the common diagnostic API.'
Require $diagnostic 'SHCreateDirectoryExW' 'Common diagnostics must create the directory recursively.'
Require $diagnostic 'hr=0x' 'Common diagnostics must record HRESULT.'
Require $diagnostic 'source=%s; code=%s; hr=0x%08lX; details=omitted' 'Common diagnostics must record structured identifiers and omit arbitrary details.'
$signature = [regex]::Match($diagnostic, 'inline\s+void\s+HResult\(const wchar_t\*\s+(\w+),\s*const wchar_t\*\s+(\w+),\s*HRESULT\s+\w+,\s*const wchar_t\*\s+\w+\)')
if (-not $signature.Success) { throw 'Common diagnostic API must accept category/code identifiers and HRESULT.' }
foreach($field in 1,2) { Require $diagnostic ('FbeDiagnosticPrivacy::Identifier\(' + [regex]::Escape($signature.Groups[$field].Value) + '\b') 'Diagnostic source/code must pass the identifier allowlist.' }
if ($diagnostic -match 'operation=' ) { throw 'Common diagnostics must not persist arbitrary operation strings.' }
if ($diagnostic -match 'FAILED\(::SHCreateDirectoryExW') { throw 'SHCreateDirectoryExW returns a Win32 code, not HRESULT.' }
$creation = [regex]::Match($diagnostic, 'const\s+int\s+(\w+)\s*=\s*::SHCreateDirectoryExW\([^;]+;')
if (-not $creation.Success) { throw 'Directory creation must inspect the Win32 return code explicitly.' }
$status = [regex]::Escape($creation.Groups[1].Value)
Require $diagnostic "$status\s*!=\s*ERROR_SUCCESS\s*&&\s*$status\s*!=\s*ERROR_ALREADY_EXISTS\s*&&\s*$status\s*!=\s*ERROR_FILE_EXISTS" 'Directory creation must accept all successful Win32 return codes.'
Write-Host 'Modern file dialog contract passed.'
