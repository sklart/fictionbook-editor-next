<#
.SYNOPSIS
Проверяет контракт API пути к открытому документу для скриптов FBE.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$idl = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\contracts\fbe.idl')
$helperHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ExternalHelper.h')
$helperSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ExternalHelper.cpp')
$viewSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.cpp')
$documentSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBDoc.cpp')
$documentation = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'docs\scripting-api.md')

$documentedMembers = @(
    'BeginUndoUnit', 'EndUndoUnit', 'inflateBlock', 'GenrePopup', 'GetStylePath',
    'GetBinarySize', 'InflateParagraphs', 'GetUUID', 'MsgBox', 'AskYesNo',
    'SaveBinary', 'GetExtendedStyle', 'DescShowElement', 'DescShowMenu',
    'IsFastMode', 'SetStyleEx', 'GetImageDimsByPath', 'GetImageDimsByData',
    'GetNBSP', 'GetViewWidth', 'GetViewHeight', 'GetProgramVersion', 'InputBox',
    'GetModalResult', 'SetStatusBarText', 'GetDocumentFilePath',
    'GetDocumentFileName', 'GetDocumentDirectory'
)

foreach ($contract in @(
    @{ Text = 'id\(26\).*GetDocumentFilePath'; Source = $idl; Name = 'COM-ID полного пути' },
    @{ Text = 'id\(27\).*GetDocumentFileName'; Source = $idl; Name = 'COM-ID имени файла' },
    @{ Text = 'id\(28\).*GetDocumentDirectory'; Source = $idl; Name = 'COM-ID каталога' },
    @{ Text = 'SetDocumentFilePathSource'; Source = $helperHeader; Name = 'передача состояния документа в helper' },
    @{ Text = 'm_document_namevalid'; Source = $helperSource; Name = 'защита несохранённого документа' },
    @{ Text = '::GetFullPathName\(\*filename, 0, NULL, NULL\)'; Source = $helperSource; Name = 'безопасная нормализация полного пути' },
    @{ Text = 'return written > 0 && written < required \? CString\(path.data\(\)\) : CString\(\)'; Source = $helperSource; Name = 'пустой результат при ошибке нормализации' },
    @{ Text = 'path\.Mid\(separator \+ 1\)'; Source = $helperSource; Name = 'получение имени файла без ограничения MAX_PATH' },
    @{ Text = 'SetDocumentFilePathSource\(m_document_filename, m_document_namevalid\)'; Source = $viewSource; Name = 'привязка helper к представлению' },
    @{ Text = 'SetDocumentFilePathSource\(&m_filename, &m_namevalid\)'; Source = $documentSource; Name = 'привязка представления к документу' },
    @{ Text = 'GetDocumentFilePath\(\)'; Source = $documentation; Name = 'документация полного пути' },
    @{ Text = 'GetDocumentFileName\(\)'; Source = $documentation; Name = 'документация имени файла' },
    @{ Text = 'GetDocumentDirectory\(\)'; Source = $documentation; Name = 'документация каталога' }
)) {
    if ($contract.Source -notmatch $contract.Text) {
        throw "Не найден обязательный контракт: $($contract.Name)."
    }
}

function Get-ApiDocumentationSection([string]$Text, [string]$Member) {
    # Mentions in later prose are not headings; a heading cannot span lines.
    $pattern = '(?ms)^### [^\r\n]*\b' + [regex]::Escape($Member) + '\b[^\r\n]*\r?\n.*?(?=^### |\z)'
    return [regex]::Match($Text, $pattern)
}
function Test-JavaScriptExample([string]$Section) {
    return $Section -match '(?m)^```(?:js|javascript)[ \t]*\r?$'
}

# Examples must belong to their own method, with either valid fence spelling.
$sectionFixture = @'
### `First()`
No example here.
### `Second()`
```javascript
Second();
```
### `Third()`
```js
Third();
```
A later mention of First must not borrow the Third example.
'@
$firstSection = Get-ApiDocumentationSection $sectionFixture 'First'
if(-not $firstSection.Success -or (Test-JavaScriptExample $firstSection.Value) -or
   -not (Test-JavaScriptExample (Get-ApiDocumentationSection $sectionFixture 'Second').Value) -or
   -not (Test-JavaScriptExample (Get-ApiDocumentationSection $sectionFixture 'Third').Value) -or
   (Get-ApiDocumentationSection $sectionFixture 'FirstExtra').Success) {
    throw 'Разбор разделов API должен сохранять границы методов и распознавать js/javascript.'
}

foreach ($member in $documentedMembers) {
    $section = Get-ApiDocumentationSection $documentation $member
    if (-not $section.Success) {
        throw "В документации не найден раздел API: $member."
    }
    if (-not (Test-JavaScriptExample $section.Value)) {
        throw "В разделе API отсутствует пример JavaScript: $member."
    }
}

Write-Host 'Проверка контракта, полноты и примеров документации API скриптов прошла успешно.'
