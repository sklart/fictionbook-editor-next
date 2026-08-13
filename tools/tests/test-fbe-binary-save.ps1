<#
Проверяет контракт выгрузки <binary>: оба UI-пути обязаны подтверждать замену
и использовать одну атомарную запись, не объявляя неполную запись успешной.
#>
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$helper = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\BinaryFileSave.h')
$external = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\ExternalHelper.h')
$view = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\FBEview.h')
$descriptionScript = Get-Content -Raw (Join-Path $repoRoot 'runtime\main.js')
$releaseVerification = Get-Content -Raw (Join-Path $repoRoot 'tools\build\verify-release.ps1')

function Assert-Contains([string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -notmatch $Pattern) { throw $Message }
}

Assert-Contains $descriptionScript 'window\.external\.SaveBinary\([^\r\n]+,\s*1\)' 'Кнопка «Сохранить» описания должна вызывать SaveBinary с диалогом.'
Assert-Contains $external 'OFN_OVERWRITEPROMPT' 'SaveBinary должен запрашивать подтверждение замены.'
Assert-Contains $view 'OFN_OVERWRITEPROMPT' 'Контекстное «Сохранить изображение как» должно запрашивать подтверждение замены.'
Assert-Contains $external 'BinaryFileSave::WriteAtomically\(file_name, data, byteCount, &error\)' 'SaveBinary должен использовать общую запись.'
Assert-Contains $view 'BinaryFileSave::WriteAtomically\(imgSaveDlg\.m_szFileName, bytes, byteCount, &error\)' 'Контекстное сохранение должно использовать общую запись.'
Assert-Contains $helper 'GetTempFileName' 'Запись должна начинаться с временного файла в каталоге назначения.'
Assert-Contains $helper 'CREATE_ALWAYS' 'Временный файл должен открываться с семантикой замены.'
Assert-Contains $helper 'MOVEFILE_REPLACE_EXISTING \| MOVEFILE_WRITE_THROUGH' 'Успешная запись должна заменять существующий файл.'
Assert-Contains $helper '!writeResult \|\| written == 0' 'Неполная или ошибочная WriteFile не должна считаться успехом.'
Assert-Contains $helper 'FlushFileBuffers' 'Перед заменой необходимо сбросить временный файл.'
Assert-Contains $external '\*ret = true;' 'SaveBinary должен возвращать успех только после успешной записи.'
Assert-Contains $external 'StartupTrace::Error\(L"binary-save", L"B510"' 'Ошибка SaveBinary должна попасть в диагностику.'
Assert-Contains $external 'modalResult == IDOK' 'Cancel в SaveBinary не должен трактоваться как ошибка.'
Assert-Contains $external 'ShowBinarySaveFailure' 'SaveBinary должен сообщать пользователю о реальной ошибке записи.'
Assert-Contains $view 'ShowBinarySaveFailure' 'Контекстное сохранение должно сообщать пользователю о реальной ошибке записи.'
Assert-Contains $view 'SafeArrayUnaccessData\(data\.parray\)' 'После доступа к SAFEARRAY блокировка должна сниматься.'
Assert-Contains $releaseVerification 'test-fbe-binary-save\.ps1' 'Статический binary-save тест должен входить в release verification.'
Assert-Contains $releaseVerification 'test-fbe-binary-save-runtime\.ps1' 'Runtime binary-save тест должен входить в release verification.'

Write-Host 'Binary image save contract passed.'
