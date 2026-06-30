<#
.SYNOPSIS
Проверяет UI-строки плагинов и связанных компонентов на типичные признаки mojibake.

.DESCRIPTION
Скрипт просматривает исходники и ресурсные файлы плагинов ExportEPUB, ImportEPUB,
ExportDOCX, ExportHTML, а также FBV/FBE-строки, которые показываются пользователю.
Проверка ищет характерные фрагменты ошибочной UTF-8/Windows-1251 перекодировки
вроде "Рџ", "Ð", "╨" и "я╗┐". Документация и тестовые описания не сканируются,
потому что там такие строки могут встречаться как осознанные примеры.
#>

[CmdletBinding()]
param(
    [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}
else {
    $RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
}

$scanRoots = @(
    "src\export-epub",
    "src\import-epub",
    "src\export-docx",
    "src\export-html",
    "src\fbv",
    "src\fbe"
) | ForEach-Object { Join-Path $RepoRoot $_ } | Where-Object { Test-Path -LiteralPath $_ -PathType Container }

$extensions = @(".cpp", ".h", ".hpp", ".c", ".rc", ".rgs", ".idl")

function ConvertTo-Utf8AsWindows1251Text {
    param([string]$Text)

    $utf8Bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    return [Text.Encoding]::GetEncoding(1251).GetString($utf8Bytes)
}

$cyrillicAlphabet = "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя"
$markers = New-Object System.Collections.Generic.List[string]
foreach ($char in $cyrillicAlphabet.ToCharArray()) {
    $marker = ConvertTo-Utf8AsWindows1251Text ([string]$char)
    if ($marker.Length -gt 1 -and -not $markers.Contains($marker)) {
        $markers.Add($marker) | Out-Null
    }
}
foreach ($marker in @("Ð", "Ñ", "╨", "╤", "я╗┐")) {
    if (-not $markers.Contains($marker)) {
        $markers.Add($marker) | Out-Null
    }
}

$allowRules = @(
    @{
        PathPattern = "src\import-epub\ImportOptionsDialog.cpp"
        TextPattern = (ConvertTo-Utf8AsWindows1251Text "Аннотация")
        Reason = "В UI это намеренный пример строки, которую чинит опция восстановления кодировки."
    },
    @{
        PathPattern = "src\import-epub\EpubImport.cpp"
        TextPattern = (ConvertTo-Utf8AsWindows1251Text "Аннотация")
        Reason = "В коде это тестовый маркер эвристики восстановления mojibake."
    }
)

function Test-AllowListedLine {
    param(
        [string]$RelativePath,
        [string]$Text
    )

    foreach ($rule in $allowRules) {
        if ($RelativePath -ieq $rule.PathPattern -and $Text.Contains($rule.TextPattern)) {
            return $true
        }
    }

    return $false
}

$hits = New-Object System.Collections.Generic.List[object]

foreach ($root in $scanRoots) {
    Get-ChildItem -LiteralPath $root -Recurse -File |
        Where-Object {
            $extensions -contains $_.Extension.ToLowerInvariant() -and
            $_.FullName -notmatch '\\thirdparty\\'
        } |
        ForEach-Object {
            $file = $_
            $relative = [IO.Path]::GetRelativePath($RepoRoot, $file.FullName)
            $lineNumber = 0
            foreach ($line in Get-Content -LiteralPath $file.FullName -Encoding UTF8) {
                $lineNumber++
                foreach ($marker in $markers) {
                    if ($line.Contains($marker) -and -not (Test-AllowListedLine -RelativePath $relative -Text $line)) {
                        $hits.Add([pscustomobject]@{
                            File = $relative
                            Line = $lineNumber
                            Marker = $marker
                            Text = $line.Trim()
                        }) | Out-Null
                    }
                }
            }
        }
}

if ($hits.Count -gt 0) {
    $hits | Format-Table -AutoSize
    throw "Найдены возможные признаки mojibake в пользовательских строках плагинов: $($hits.Count)."
}

Write-Host "Проверка пользовательских строк плагинов на mojibake прошла успешно."
