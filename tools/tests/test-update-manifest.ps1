[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

& (Join-Path $repoRoot "tools\version\sync-version.ps1") -ValidateUpdateManifest

Write-Host "Проверка update.xml прошла успешно."
