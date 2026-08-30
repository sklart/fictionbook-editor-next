<#
.SYNOPSIS
Проверяет контракт настройки резервной .bak-копии при сохранении FB2.

.DESCRIPTION
Проверка подтверждает, что настройка FBE Next сохранена в Settings.xml,
по умолчанию включена и передаётся в атомарную замену файла.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$settingsHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\Settings.h')
$settingsSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\Settings.cpp')
$documentSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBDoc.cpp')
$commitSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\BackupFileCommit.h')
$dialogSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SettingsGeneralPage.cpp')

foreach ($contract in @(
    @{ Text = 'm_create_backup_file'; Source = $settingsHeader; Name = 'поле настройки' },
    @{ Text = 'GetCreateBackupFile'; Source = $settingsHeader; Name = 'getter настройки' },
    @{ Text = 'SetCreateBackupFile'; Source = $settingsHeader; Name = 'setter настройки' },
    @{ Text = 'CreateBackupFile'; Source = $settingsSource; Name = 'ключ Settings.xml' },
    @{ Text = 'm_create_backup_file\s*=\s*true'; Source = $settingsSource; Name = 'значение по умолчанию' },
    @{ Text = 'FbeBackupFileCommit::CommitSavedFile'; Source = $documentSource; Name = 'вызов атомарного сохранения' },
    @{ Text = 'CommitSavedFile\([^\)]*bool createBackupFile'; Source = $commitSource; Name = 'параметр атомарного сохранения' },
    @{ Text = '_Settings\.GetCreateBackupFile\(\)'; Source = $documentSource; Name = 'передача настройки в сохранение' },
    @{ Text = 'backupFilePath\s*=\s*createBackupFile'; Source = $commitSource; Name = 'сохранение без .bak' },
    @{ Text = 'ReplaceFile'; Source = $commitSource; Name = 'атомарная замена файла' },
    @{ Text = 'IDC_CREATE_BACKUP_FILE'; Source = $dialogSource; Name = 'контрол вкладки FBE Next' }
)) {
    if ($contract.Source -notmatch $contract.Text) {
        throw "Не найден обязательный контракт: $($contract.Name)."
    }
}
if ($commitSource -match 'DeleteFile\(backupFile') { throw 'Нельзя удалять существующий .bak до ReplaceFile.' }

Write-Host 'Проверка настройки резервной .bak-копии прошла успешно.'
