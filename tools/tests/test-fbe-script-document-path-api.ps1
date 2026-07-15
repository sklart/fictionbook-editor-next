<#
.SYNOPSIS
Проверяет контракт API пути к открытому документу для скриптов FBE.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$idl = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\fbe.idl')
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
    @{ Text = 'U::GetFullPathName'; Source = $helperSource; Name = 'нормализация полного пути' },
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

foreach ($member in $documentedMembers) {
    $section = [regex]::Match(
        $documentation,
        "(?ms)^### .*" + [regex]::Escape($member) + ".*?(?=^### |\z)"
    )
    if (-not $section.Success) {
        throw "В документации не найден раздел API: $member."
    }
    if ($section.Value -notmatch '```js') {
        throw "В разделе API отсутствует пример JavaScript: $member."
    }
}

Write-Host 'Проверка контракта, полноты и примеров документации API скриптов прошла успешно.'
