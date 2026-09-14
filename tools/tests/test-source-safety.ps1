[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$utf8 = New-Object System.Text.UTF8Encoding($false, $true)

function Test-ApparentMojibake([string]$Text) {
    # UTF-8 decoded as a Western single-byte code page is normally visible
    # as a lead character (Ð, Ñ, Â or Ã) followed by a CP1252 continuation.
    if ($Text -match '(?:[ÐÑÂÃ][\u0080-\u00BF\u0178\u20AC\u201A-\u2022\u2026\u2030\u2122])') {
        return $true
    }

    # UTF-8 decoded as CP1251 and then saved as UTF-8 produces pairs beginning
    # with U+0420 or U+0421. A single pair can occur in legitimate text, so
    # require two pairs whose second character can only be a CP1251 decoding
    # of an UTF-8 continuation byte (0x80..0xBF).
    $cp1251Continuation = '[\u00A0-\u00BF\u0401-\u0407\u0409-\u040F\u0451-\u0457\u0459-\u045F\u0490-\u0491\u2018-\u2022\u2026\u2030\u2039\u20AC\u2122]'
    return ([regex]::Matches($Text, "[\u0420\u0421]$cp1251Continuation")).Count -ge 2
}

function ConvertFrom-CodePoints([int[]]$CodePoints) {
    return -join ($CodePoints | ForEach-Object { [char]$_ })
}

function Assert-MojibakeResult([string]$Text, [bool]$Expected, [string]$Description) {
    if ((Test-ApparentMojibake $Text) -ne $Expected) {
        throw "Mojibake regression failed: $Description"
    }
}

# Keep fixtures as code points so the source-safety scan does not match the
# intentionally malformed examples inside this test script.
Assert-MojibakeResult (ConvertFrom-CodePoints @(0x0420, 0x045F, 0x0421, 0x0402, 0x0420, 0x0451, 0x0420, 0x0406, 0x0420, 0x00B5, 0x0421, 0x201A)) $true `
    "CP1251-style greeting fixture"
Assert-MojibakeResult (ConvertFrom-CodePoints @(0x0420, 0x0459, 0x0420, 0x0455, 0x0420, 0x0491, 0x0420, 0x0451, 0x0421, 0x0402, 0x0420, 0x0455, 0x0420, 0x0406, 0x0420, 0x0454, 0x0420, 0x00B0)) $true `
    "CP1251-style encoding fixture"
Assert-MojibakeResult (ConvertFrom-CodePoints @(0x00D0, 0x0178, 0x00D1, 0x20AC, 0x00D0, 0x00B8, 0x00D0, 0x00B2, 0x00D0, 0x00B5, 0x00D1, 0x201A)) $true `
    "Western-style mojibake"
Assert-MojibakeResult (ConvertFrom-CodePoints @(0x0420, 0x0451)) $false "isolated CP1251-style pair"
Assert-MojibakeResult "Обычный русский текст" $false "ordinary Russian text"
Assert-MojibakeResult "Звичайний український текст" $false "ordinary Ukrainian text"
Assert-MojibakeResult "«Unicode — работает»" $false "ordinary Unicode punctuation"

function Read-SourceFile([string]$RelativePath) {
    $path = Join-Path $repoRoot $RelativePath
    $bytes = [System.IO.File]::ReadAllBytes($path)
    try {
        $text = $utf8.GetString($bytes)
    } catch [System.Text.DecoderFallbackException] {
        throw "Исходный файл должен быть корректным UTF-8: $RelativePath"
    }
    return $text
}

# Raw-byte guard for the first-party compilation and test inputs. Generated
# MIDL/localization output and third-party code intentionally stay outside this
# policy. ReadAllBytes plus a throwing decoder makes the result independent of
# the host ANSI code page in both Windows PowerShell 5.1 and PowerShell 7.
function Assert-FirstPartyUtf8 {
    $extensions = @('.c', '.cc', '.cpp', '.cxx', '.h', '.hpp', '.inl', '.ixx', '.ps1', '.psm1', '.psd1', '.props', '.targets', '.vcxproj', '.sln', '.idl', '.rc', '.rc2', '.cmd', '.bat')
    $paths = & git -c core.quotepath=false ls-files
    foreach ($relativePath in $paths) {
        $normalized = $relativePath.Replace('/', '\')
        if ($normalized -match '^(third_party|build|out)\\' -or
            $normalized -match '^src\\.*\\generated\\' -or
            $normalized -match '^src\\export-(docx|epub)\\Export(DOCX|EPUB)_i\.(c|h)$' -or
            $normalized -match '\.generated\.rc2$' -or
            $extensions -notcontains [System.IO.Path]::GetExtension($normalized).ToLowerInvariant()) {
            continue
        }
        $bytes = [System.IO.File]::ReadAllBytes((Join-Path $repoRoot $normalized))
        try {
            $text = $utf8.GetString($bytes)
        } catch [System.Text.DecoderFallbackException] {
            throw "First-party source is not valid UTF-8: $relativePath"
        }
        if ($text.IndexOf([char]0xFFFD) -ge 0) {
            throw "First-party source contains U+FFFD: $relativePath"
        }
        if (Test-ApparentMojibake $text) {
            throw "First-party source contains apparent UTF-8 mojibake: $relativePath"
        }
        $crlf = 0
        $bareLf = 0
        for ($index = 0; $index -lt $bytes.Length; ++$index) {
            if ($bytes[$index] -eq 0x0A) {
                if ($index -gt 0 -and $bytes[$index - 1] -eq 0x0D) { ++$crlf } else { ++$bareLf }
            }
        }
        if ($crlf -gt 0 -and $bareLf -gt 0) {
            throw "First-party source has mixed CRLF/LF line endings: $relativePath"
        }
    }
}

Assert-FirstPartyUtf8

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
Assert-Contains $mainFrame "m_view->SciFindNext(m_source,false,false)" `
    "Code-mode Replace must preserve the selected Up/Down direction"
Assert-NotContains $mainFrame "m_view->SciFindNext(m_source,true,false)" `
    "Code-mode Replace must not force the next search downward"

$scriptVisualResources = Read-SourceFile "src\fbe\scripts\ScriptVisualResources.cpp"
Assert-Contains $scriptVisualResources "GetFileAttributes(bitmapPath)" `
    "script bitmap lookup must use a handle-free file check"
Assert-Contains $scriptVisualResources "GetFileAttributes(iconPath)" `
    "script icon lookup must use a handle-free file check"
Assert-NotContains $scriptVisualResources "FindFirstFile" `
    "script visual lookup must not allocate search handles"

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
    Read-SourceFile "src\fbe\search\RegexBackend.cpp"
    Read-SourceFile "src\fbe\search\RegexBackendPcre2.cpp"
    Read-SourceFile "src\fbe\search\RegexPcre2MatchLoop.h"
) -join "`n"
Assert-Contains $appRegexSources "compileOptions |= PCRE2_UTF;" `
	"PCRE2 searches must stay in UTF mode / PCRE2-16 UTF mode"
Assert-Contains $appRegexSources "compileOptions |= PCRE2_MULTILINE;" `
    "PCRE2 multiline mode must be forwarded to the compiled pattern"
Assert-Contains $appRegexSources "if (!global)" `
	"regex wrapper must stop after the first match when Global is disabled"
Assert-Contains $appRegexSources "if (!pcre2_next_match(matchData, &offset, &globalOptions))" `
	"global regex matching must delegate offset advancement to PCRE2"
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

$spellerSource = Read-SourceFile "src\fbe\Speller.cpp"
Assert-MojibakeResult $spellerSource $false "Speller.cpp, including Tokens"
Assert-NotContains $spellerSource ([string][char]0xFFFD) `
    "Speller.cpp must not contain replacement characters"
Assert-Contains $spellerSource 'L" .,?\u2013!\u2014\u2026\r\n\t\"\u00AB\u00BB\u201C\u201D\u2018\u2019' `
    "Tokens must preserve the history-verified Unicode delimiter code points"
Assert-Contains $spellerSource 'checkWord.Replace(L"\u0451", L"\u0435")' `
    "Russian ё-to-е dictionary normalization must use explicit Unicode code points"
Assert-Contains $spellerSource "GetParagraphContainer" `
    "проверка орфографии должна нормализовать inline-выделение до абзаца"
Assert-Contains $spellerSource "GetNextParagraph(elem, m_fbw_body);" `
    "проверка видимой области должна обходить соседние DOM-абзацы"
Assert-NotContains $spellerSource "MSHTML::IHTMLElementCollectionPtr paras" `
    "проверка видимой области не должна собирать все абзацы длинного документа"
Assert-NotContains $spellerSource "getElementsByTagName" `
    "проверка видимой области не должна выполнять глобальный DOM-обход абзацев"
Assert-Contains $spellerSource "endElem || checked < fallbackParagraphLimit" `
    "граница видимой области должна отменять fallback-лимит абзацев"
Assert-NotContains $mainFrame "IsHTMLChanged()" `
    "обработчик изменения не должен пересчитывать все HTML-элементы документа"
Assert-NotContains $viewHeader "bool IsHTMLChanged()" `
    "редактор не должен хранить глобальный счётчик HTML-элементов для орфографии"

Write-Host "Проверки безопасной работы с исходниками прошли успешно."
