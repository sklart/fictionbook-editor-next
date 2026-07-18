# Проверяет, что Modern/Win7 release-поток повторно собирает DLL редактора и
# пакетные конвертеры, а остальные общие компоненты и MUI/property handler
# используются явно.
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Get-Text {
    param([string]$RelativePath)
    return Get-Content -Raw -LiteralPath (Join-Path $repoRoot $RelativePath)
}

function Assert-Contains {
    param([string]$Text, [string]$Expected, [string]$Description)
    if (-not $Text.Contains($Expected)) {
        throw "${Description}: не найдено '$Expected'."
    }
}

$workflow = Get-Text ".github\workflows\build.yml"
$build = Get-Text "tools\build\build.ps1"
$scintilla = Get-Text "tools\build\build-scintilla.ps1"
$release = Get-Text "tools\build\create-release.ps1"
$portable = Get-Text "tools\build\package-portable.ps1"
$artifacts = Get-Text "tools\build\verify-artifacts.ps1"
$verifyRelease = Get-Text "tools\build\verify-release.ps1"

Assert-Contains $workflow "-EditorRuntimeOnly" "Win7-этап workflow"
Assert-Contains $workflow "-BatchConvertersOnly" "Win7-этап пакетных конвертеров workflow"
Assert-Contains $workflow "SkipPropertyHandlerBuild = `$true" "Win7-релиз workflow"
Assert-Contains $workflow "SkipFbvVerbMuiBuild = `$true" "Win7-релиз workflow"
Assert-Contains $build "[switch]`$EditorRuntimeOnly" "build.ps1"
Assert-Contains $build "Собраны только целевые DLL редактора" "build.ps1"
Assert-Contains $build "[switch]`$BatchConvertersOnly" "build.ps1"
Assert-Contains $build "Собраны только пакетные конвертеры" "build.ps1"
Assert-Contains $scintilla "[string]`$OutputDirectory" "build-scintilla.ps1"
Assert-Contains $scintilla "out\editor-runtime" "build-scintilla.ps1"
Assert-Contains $release "[switch]`$SkipPropertyHandlerBuild" "create-release.ps1"
Assert-Contains $release "[switch]`$SkipFbvVerbMuiBuild" "create-release.ps1"
Assert-Contains $release "[switch]`$Prerelease" "create-release.ps1"
Assert-Contains $release "-EditorRuntimeDirectory `$editorRuntimeDirectory" "create-release.ps1"
Assert-Contains $portable "[string]`$EditorRuntimeDirectory" "package-portable.ps1"
Assert-Contains $portable "[switch]`$SkipFbvVerbMuiBuild" "package-portable.ps1"
Assert-Contains $portable '"Scintilla.dll", "Lexilla.dll"' "package-portable.ps1"
Assert-Contains $artifacts "Get-ZipEntrySha256" "verify-artifacts.ps1"
Assert-Contains $artifacts "Общий файл" "verify-artifacts.ps1"
Assert-Contains $artifacts "Win7-вариант не был применён" "verify-artifacts.ps1"
Assert-Contains $artifacts 'foreach ($name in @("ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe"))' "verify-artifacts.ps1"
Assert-Contains $verifyRelease 'analyze-product-hardcoded-cyrillic.ps1")' "verify-release.ps1"
if ($verifyRelease -match 'analyze-product-hardcoded-cyrillic\.ps1"\)\s+-FailOnFindings') {
    throw "Релизный контур не должен блокироваться накопленным набором кириллических строк; строгая проверка допустима только для отдельных фикстур."
}

$win7Imports = Get-Text "tools\tests\check-win7-imports.ps1"
Assert-Contains $win7Imports '"CreateFile2"' "check-win7-imports.ps1"

$runtimeOnlyIndex = $build.IndexOf("if (`$EditorRuntimeOnly)")
$pcre2Index = $build.IndexOf("Подготовка PCRE2")
if ($runtimeOnlyIndex -lt 0 -or $pcre2Index -lt 0 -or $runtimeOnlyIndex -gt $pcre2Index) {
    throw "Режим EditorRuntimeOnly обязан завершаться до сборки PCRE2 и общих компонентов."
}

Write-Host "Проверка исключения повторных сборок Modern/Win7 прошла успешно."
