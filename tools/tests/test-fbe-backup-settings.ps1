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
$dialogSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SettingsNextDlg.cpp')

foreach ($contract in @(
    @{ Text = 'm_create_backup_file'; Source = $settingsHeader; Name = 'поле настройки' },
    @{ Text = 'GetCreateBackupFile'; Source = $settingsHeader; Name = 'getter настройки' },
    @{ Text = 'SetCreateBackupFile'; Source = $settingsHeader; Name = 'setter настройки' },
    @{ Text = 'CreateBackupFile'; Source = $settingsSource; Name = 'ключ Settings.xml' },
    @{ Text = 'm_create_backup_file\s*=\s*true'; Source = $settingsSource; Name = 'значение по умолчанию' },
    @{ Text = 'CommitSavedFile\([^\)]*bool createBackupFile\)'; Source = $documentSource; Name = 'параметр атомарного сохранения' },
    @{ Text = '_Settings\.GetCreateBackupFile\(\)'; Source = $documentSource; Name = 'передача настройки в сохранение' },
    @{ Text = 'backupFilePath\s*=\s*NULL'; Source = $documentSource; Name = 'сохранение без .bak' },
    @{ Text = 'IDC_CREATE_BACKUP_FILE'; Source = $dialogSource; Name = 'контрол вкладки FBE Next' }
)) {
    if ($contract.Source -notmatch $contract.Text) {
        throw "Не найден обязательный контракт: $($contract.Name)."
    }
}

Write-Host 'Проверка настройки резервной .bak-копии прошла успешно.'
