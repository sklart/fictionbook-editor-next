$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dialog = Get-Content -Raw (Join-Path $root 'src\common\ModernFileDialog.h')
$images = Get-Content -Raw (Join-Path $root 'src\fbe\ImageImport.cpp')
$main = Get-Content -Raw (Join-Path $root 'src\fbe\mainfrm.cpp')
$view = Get-Content -Raw (Join-Path $root 'src\fbe\FBEview.cpp')

function Require([string]$text, [string]$pattern, [string]$message) { if ($text -notmatch $pattern) { throw $message } }
Require $dialog 'std::wstring title, okButtonLabel, fileNameLabel, defaultExtension' 'Request must own dialog strings.'
Require $dialog 'std::wstring initialFileName, initialFolder' 'Request must own initial paths.'
Require $images 'ImageImportFileTypes\(' 'Image filters need one shared source.'
Require $images 'L"\*\.jpg;\*\.jpeg;\*\.png;\*\.webp;\*\.jp2;\*\.j2k;\*\.bmp;\*\.gif;\*\.tif;\*\.tiff;\*\.avif;\*\.heic;\*\.heif"' 'Supported modern image spec is incomplete.'
Require $main 'filters\.push_back\(\{ type\.displayName\.GetString\(\), type\.wildcard\.GetString\(\) \}\)' 'Binary picker must use wildcard image specs.'
Require $view 'filters\.push_back\(\{ type\.displayName\.GetString\(\), type\.wildcard\.GetString\(\) \}\)' 'Image picker must use wildcard image specs.'
if ($main -match 'ImageImportFileFilter\(\).*COMDLG_FILTERSPEC' -or $view -match 'ImageImportFileFilter\(\).*COMDLG_FILTERSPEC') { throw 'Legacy NUL-delimited image filter passed to modern dialog.' }
Write-Host 'Modern file dialog contract passed.'
