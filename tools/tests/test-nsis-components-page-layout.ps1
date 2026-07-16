<#
.SYNOPSIS
Проверяет читаемость описаний компонентов на странице NSIS.

.DESCRIPTION
Компактная страница компонентов оставляет широкое дерево выбора. Скрипт
контролирует увеличение поля описания и длину строк всех базовых локалей,
чтобы текст не обрезался внизу страницы.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$installerPath = Join-Path $repoRoot 'packaging\nsis\Installer\MakeInstaller.nsi'
$installerText = Get-Content -Raw -LiteralPath $installerPath

foreach ($fragment in @(
    '!define MUI_PAGE_CUSTOMFUNCTION_SHOW ComponentsPageShow',
    'Function ComponentsPageShow',
    'GetDlgItem $0 $HWNDPARENT 1043',
    'IntOp $4 $4 + 16'
)) {
    if ($installerText.IndexOf($fragment, [StringComparison]::Ordinal) -lt 0) {
        throw "Не настроена увеличенная область описания компонентов: $fragment"
    }
}

$expectedScriptsDescription = @{
    'English.nsh' = 'Useful community scripts.'
    'Russian.nsh' = 'Сборка полезных скриптов от сообщества.'
    'Ukrainian.nsh' = 'Збірка корисних скриптів спільноти.'
}

foreach ($entry in $expectedScriptsDescription.GetEnumerator()) {
    $path = Join-Path $repoRoot (Join-Path 'packaging\nsis\Installer\Localization' $entry.Key)
    $lines = Get-Content -LiteralPath $path
    $descriptions = @($lines | Where-Object { $_ -match '^LangString DESC_' })
    if ($descriptions.Count -eq 0) {
        throw "В $path не найдены описания компонентов."
    }

    foreach ($line in $descriptions) {
        if ($line -notmatch '^LangString\s+(?<name>\S+)\s+\$\{[^}]+\}\s+"(?<value>.*)"$') {
            throw "Некорректная строка описания компонентов в ${path}: $line"
        }
        if ($Matches.value.Length -gt 100) {
            throw "Описание $($Matches.name) в $path слишком длинное ($($Matches.value.Length) символов)."
        }
    }

    $scriptsLine = $descriptions | Where-Object { $_ -match '^LangString DESC_Scripts\s+' } | Select-Object -First 1
    if ($scriptsLine -notmatch [regex]::Escape('"' + $entry.Value + '"')) {
        throw "В $path не обновлено описание набора скриптов."
    }
}

Write-Host 'Проверка описаний страницы компонентов NSIS прошла успешно.'
