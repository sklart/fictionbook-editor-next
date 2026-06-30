[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$singleByteEncoding = [System.Text.Encoding]::GetEncoding(1251)

function Read-SourceFile([string]$RelativePath) {
    $path = Join-Path $repoRoot $RelativePath
    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
        return [System.Text.Encoding]::Unicode.GetString($bytes)
    }
    return $singleByteEncoding.GetString($bytes)
}

function Assert-Contains(
    [string]$Text,
    [string]$Expected,
    [string]$Description
) {
    if (-not $Text.Contains($Expected)) {
        throw "Не выполнено требование по безопасной работе с исходниками: $Description"
    }
}

function Assert-NotContains(
    [string]$Text,
    [string]$Forbidden,
    [string]$Description
) {
    if ($Text.Contains($Forbidden)) {
        throw "Обнаружен запрещённый шаблон в исходниках: $Description"
    }
}

$utilsHeader = Read-SourceFile "src\fbe\utils\utils.h"
Assert-Contains $utilsHeader "type != REG_BINARY || len != sizeof(T)" `
    "binary registry values must have the exact requested size"
Assert-NotContains $utilsHeader "BYTE* buff = new BYTE[sizeof(T)]" `
    "QueryBV must not allocate an unchecked registry buffer"

$saveSources = @(
    Read-SourceFile "src\fbe\FBDoc.cpp"
    Read-SourceFile "src\fbe\mainfrm.cpp"
) -join "`n"

Assert-NotContains $saveSources "wchar_t str[MAX_PATH]" `
    "save paths must not be copied into fixed MAX_PATH buffers"

$helperCalls = ([regex]::Matches(
    $saveSources,
    "U::SetCurrentDirectoryToFile\(")).Count
if ($helperCalls -ne 3) {
    throw "Ожидалось 3 безопасных вызова helper-а для каталога сохранения, найдено: $helperCalls."
}

$elementDescriptor = Read-SourceFile "src\fbe\ElementDescriptor.cpp"
Assert-NotContains $elementDescriptor "wchar_t* picName = new wchar_t" `
    "tree icon paths must use CString ownership"
Assert-NotContains $elementDescriptor "FindClose(hPicture)" `
    "tree icon existence checks must not close invalid search handles"
Assert-Contains $elementDescriptor "GetFileAttributes(bitmapPath)" `
    "tree bitmap existence must use a handle-free file check"

$mainFrame = Read-SourceFile "src\fbe\mainfrm.cpp"
Assert-NotContains $mainFrame "new wchar_t[wcslen(fd.cFileName) + 1]" `
    "script names and picture paths must use CString ownership"
Assert-NotContains $mainFrame "FindClose(hPicture)" `
    "script picture checks must not close invalid search handles"
Assert-NotContains $mainFrame "if(found)" `
    "FindFirstFile results must be compared with INVALID_HANDLE_VALUE"

$scriptPictureCalls = ([regex]::Matches(
    $mainFrame,
    "LoadScriptPicture\((folder|script),")).Count
if ($scriptPictureCalls -ne 2) {
    throw "Ожидалось 2 вызова helper-а для картинок скриптов, найдено: $scriptPictureCalls."
}

$validSearchChecks = ([regex]::Matches(
    $mainFrame,
    "if\(found != INVALID_HANDLE_VALUE\)")).Count
if ($validSearchChecks -ne 2) {
    throw "Ожидалось 2 проверенных search-handle для скриптов, найдено: $validSearchChecks."
}

$descriptorManager = Read-SourceFile "src\fbe\ElementDescMnr.cpp"
Assert-Contains $descriptorManager "FindClose(found);" `
    "document-tree script enumeration must close its search handle"
Assert-NotContains $descriptorManager "CString fff =" `
    "document-tree script enumeration must not retain unused path locals"

$utilsSource = Read-SourceFile "src\fbe\utils\Utils.cpp"
Assert-Contains $utilsSource "capacity*=2;" `
    "module paths must grow beyond MAX_PATH when necessary"
Assert-Contains $utilsSource "CString name(tested);" `
    "script version checks must use CString ownership"
Assert-NotContains $utilsSource "wchar_t Name[MAX_PATH]" `
    "script version checks must not use fixed path buffers"
Assert-NotContains $utilsSource "exedir[p-1]!=_T('\')" `
    "path separator literals must be correctly escaped"

$appRegexSources = @(
    Read-SourceFile "src\fbe\apputils.cpp"
    Read-SourceFile "src\fbe\RegexBackend.cpp"
    Read-SourceFile "src\fbe\RegexBackendPcre2.cpp"
) -join "`n"
Assert-Contains $appRegexSources "compileOptions |= PCRE2_UTF;" `
    "PCRE2 searches must stay in UTF-8 mode"
Assert-Contains $appRegexSources "compileOptions |= PCRE2_MULTILINE;" `
    "PCRE2 multiline mode must be forwarded to the compiled pattern"
Assert-Contains $appRegexSources "if (!options.Global)" `
    "regex wrapper must stop after the first match when Global is disabled"
Assert-NotContains $appRegexSources "char dst[0xFFFF]" `
    "regex match extraction must not depend on a fixed-size temporary buffer"

$viewHeader = Read-SourceFile "src\fbe\FBEview.h"
Assert-Contains $viewHeader "bool		hasMatch;" `
    "FindReplaceOptions must explicitly track ownership of the saved regex match"
Assert-Contains $viewHeader "void ClearMatch()" `
    "FindReplaceOptions must provide centralized cleanup for the saved regex match"
Assert-Contains $viewHeader "~FindReplaceOptions() { ClearMatch(); }" `
    "FindReplaceOptions must release the saved regex match on destruction"

$viewSource = Read-SourceFile "src\fbe\FBEview.cpp"
Assert-Contains $viewSource "m_fo.ClearMatch();" `
    "Design-mode search/replace must clear the saved regex match before reuse"
Assert-Contains $viewSource "m_fo.match = new AU::IMatch2(*rm);" `
    "Design-mode selection must save an owned copy of the regex match"
Assert-Contains $viewSource "if (m_fo.hasMatch && m_fo.match)" `
    "Design-mode replacement must rely on the owned saved regex match"
Assert-NotContains $viewSource "m_fo.match=rm;" `
    "Design-mode replacement must not keep a borrowed regex-match pointer"

$modernPathSources = @(
    Read-SourceFile "src\fbe\FBE.cpp"
    Read-SourceFile "src\fbe\FBDoc.cpp"
    Read-SourceFile "src\fbe\mainfrm.cpp"
    Read-SourceFile "src\fbe\mainfrm.h"
    Read-SourceFile "src\fbe\Settings.cpp"
    Read-SourceFile "src\fbe\ExternalHelper.h"
) -join "`n"

Assert-NotContains $modernPathSources "GetModuleFileName(" `
    "application module paths must use U::GetModulePath"

$colorButton = Read-SourceFile "src\fbe\extras\ColorButton.cpp"
Assert-NotContains $colorButton "GetVersionEx" `
    "UI behavior must not depend on manifest-sensitive version detection"

Write-Host "Проверки безопасной работы с исходниками прошли успешно."
