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
$scriptingApi = Get-Content -Raw (Join-Path $repoRoot 'docs\scripting-api.md')
$releaseVerification = Get-Content -Raw (Join-Path $repoRoot 'tools\build\verify-release.ps1')

function Assert-Contains([string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -notmatch $Pattern) { throw $Message }
}

Assert-Contains $descriptionScript 'window\.external\.SaveBinary\([^\r\n]+,\s*1\)' 'Кнопка «Сохранить» описания должна вызывать SaveBinary с диалогом.'
Assert-Contains $external 'OFN_OVERWRITEPROMPT' 'SaveBinary должен запрашивать подтверждение замены.'
Assert-Contains $view 'OFN_OVERWRITEPROMPT' 'Контекстное «Сохранить изображение как» должно запрашивать подтверждение замены.'
Assert-Contains $external 'BinaryFileSave::WriteAtomically\(file_name, data, byteCount, existingFilePolicy, &error\)' 'SaveBinary должен использовать общую запись.'
Assert-Contains $external 'prompt\s*\?\s*BinaryFileSave::ExistingFilePolicy::ReplaceExisting\s*:\s*BinaryFileSave::ExistingFilePolicy::FailIfExists' 'SaveBinary должен выбирать политику замены по prompt.'
Assert-Contains $external 'else\s*\{\s*CString message;[\s\S]{0,260}StartupTrace::Error\(L"binary-save", L"B510"' 'Любая ошибка SaveBinary должна попасть в диагностику.'
Assert-Contains $external 'if \(prompt\)\s*ShowBinarySaveFailure\(GetActiveWindow\(\), file_name, error\)' 'prompt=false не должен показывать ошибку сохранения.'
Assert-Contains $scriptingApi 'При `prompt = true` открывается стандартный диалог Save As' 'Документация должна описывать интерактивный SaveBinary.'
Assert-Contains $scriptingApi 'Если он уже существует, метод не\s*заменяет его и возвращает `false`' 'Документация должна запрещать замену при prompt=false.'
Assert-Contains $scriptingApi 'без показа диалога' 'Документация должна фиксировать non-interactive режим prompt=false.'
Assert-Contains $view 'BinaryFileSave::ExistingFilePolicy::ReplaceExisting' 'Контекстное сохранение должно явно разрешать замену.'
Assert-Contains $helper 'GetTempFileName' 'Запись должна начинаться с временного файла в каталоге назначения.'
Assert-Contains $helper 'CREATE_ALWAYS' 'Временный файл должен открываться с семантикой замены.'
Assert-Contains $helper 'enum class ExistingFilePolicy' 'Writer должен различать политики существующего файла.'
Assert-Contains $helper 'FailIfExists' 'Writer должен поддерживать отказ при существующем файле.'
Assert-Contains $helper 'ReplaceExisting' 'Writer должен поддерживать замену существующего файла.'
Assert-Contains $helper 'DWORD moveFlags = MOVEFILE_WRITE_THROUGH' 'Финальное перемещение должно сохранять write-through.'
Assert-Contains $helper 'existingFilePolicy == ExistingFilePolicy::ReplaceExisting' 'MOVEFILE_REPLACE_EXISTING должен использоваться только для явной политики замены.'
Assert-Contains $helper '!writeResult \|\| written == 0' 'Неполная или ошибочная WriteFile не должна считаться успехом.'
Assert-Contains $helper 'FlushFileBuffers' 'Перед заменой необходимо сбросить временный файл.'
Assert-Contains $external '\*ret = true;' 'SaveBinary должен возвращать успех только после успешной записи.'
Assert-Contains $external 'StartupTrace::Error\(L"binary-save", L"B510"' 'Ошибка SaveBinary должна попасть в диагностику.'
Assert-Contains $external 'modalResult == IDOK' 'Cancel в SaveBinary не должен трактоваться как ошибка.'
Assert-Contains $external 'ShowBinarySaveFailure' 'SaveBinary должен сообщать пользователю о реальной ошибке записи.'
Assert-Contains $view 'ShowBinarySaveFailure' 'Контекстное сохранение должно сообщать пользователю о реальной ошибке записи.'
Assert-Contains (Get-Content -Raw (Join-Path $repoRoot 'src\fbe\BinarySaveNotification.h')) 'Unknown Windows error' 'Fallback Win32-ошибки не должен дублировать числовой код.'
Assert-Contains $view 'SafeArrayUnaccessData\(data\.parray\)' 'После доступа к SAFEARRAY блокировка должна сниматься.'
Assert-Contains $releaseVerification 'test-fbe-binary-save\.ps1' 'Статический binary-save тест должен входить в release verification.'
Assert-Contains $releaseVerification 'test-fbe-binary-save-runtime\.ps1' 'Runtime binary-save тест должен входить в release verification.'

Write-Host 'Binary image save contract passed.'
