[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$about = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\AboutBox.cpp")
$download = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\extras\http_download.h")

if ($about -notmatch 'LRESULT\s+CAboutDlg::OnCloseCmd[\s\S]*?m_monitor\.reset\(\);[\s\S]*?AbandonAllDownload\(\);') {
    throw "Закрытие окна «О программе» должно передавать активные загрузки в фоновый reaper."
}
if ($download -notmatch 'void\s+AbandonAllDownload\(\)' -or
    $download -notmatch 'PCL_ThrowOwnership\(tasks\)' -or
    $download -notmatch 'FCHttpDownloadOrphanedTasks::DestroyProc') {
    throw "HTTP manager не содержит неблокирующую передачу активных загрузок."
}
if ($download -notmatch 'void\s+DisableCallbacks\(\)' -or
    $download -notmatch 'FCAutoCSec m_callback_lock') {
    throw "Перед передачей загрузки должны безопасно отключаться callback-и UI."
}

Write-Host "Проверка неблокирующего закрытия окна «О программе» прошла успешно."
