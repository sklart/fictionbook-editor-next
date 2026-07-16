param(
    [string[]] $Roots = @("src"),
    [switch] $FailOnFindings,
    [switch] $Detailed
)

<#
.SYNOPSIS
    Ищет кириллицу в строковых литералах C/C++ исходников продукта.
.DESCRIPTION
    Скрипт помогает постепенно выносить пользовательские строки в каталоги локализации.
    Он намеренно не считает кириллицу в комментариях ошибкой: русские комментарии в проекте допустимы.
    По умолчанию выводит компактную сводку и возвращает код 0; с -Detailed показывает конкретные строки, с -FailOnFindings завершает работу с ошибкой при найденных строках.
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$sourceExtensions = @(".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hh")
$generatedNamePattern = "\.generated\."
$cyrillicPattern = "[А-Яа-яЁёІіЇїЄєҐґ]"

function Get-SourceFiles {
    foreach ($root in $Roots) {
        $fullRoot = Join-Path $repoRoot $root
        if (-not (Test-Path -LiteralPath $fullRoot)) {
            continue
        }
        Get-ChildItem -LiteralPath $fullRoot -Recurse -File | Where-Object {
            $sourceExtensions -contains $_.Extension.ToLowerInvariant() -and
            $_.FullName -notmatch $generatedNamePattern -and
            $_.FullName -notmatch "\\third_party\\" -and
            $_.FullName -notmatch "\\out\\" -and
            $_.FullName -notmatch "\\build\\"
        }
    }
}

function Find-CyrillicStringLiterals {
    param(
        [Parameter(Mandatory = $true)] [string] $Path
    )

    $text = Get-Content -LiteralPath $Path -Raw
    $line = 1
    $column = 1
    $i = 0
    $inBlockComment = $false
    $length = $text.Length

    while ($i -lt $length) {
        $ch = $text[$i]
        $next = if ($i + 1 -lt $length) { $text[$i + 1] } else { [char]0 }

        if ($ch -eq "`n") {
            $line++
            $column = 1
            $i++
            continue
        }

        if ($inBlockComment) {
            if ($ch -eq '*' -and $next -eq '/') {
                $inBlockComment = $false
                $i += 2
                $column += 2
                continue
            }
            $i++
            $column++
            continue
        }

        if ($ch -eq '/' -and $next -eq '/') {
            while ($i -lt $length -and $text[$i] -ne "`n") {
                $i++
                $column++
            }
            continue
        }

        if ($ch -eq '/' -and $next -eq '*') {
            $inBlockComment = $true
            $i += 2
            $column += 2
            continue
        }

        # Символьные литералы не являются строками интерфейса. Пропускаем их
        # целиком, включая экранированные апострофы и двойные кавычки.
        if ($ch -eq "'") {
            $i++
            $column++
            $escaped = $false
            while ($i -lt $length) {
                $c = $text[$i]
                if ($c -eq "`n") {
                    $line++
                    $column = 1
                    $i++
                    $escaped = $false
                    continue
                }
                if ($escaped) {
                    $escaped = $false
                } elseif ($c -eq '\') {
                    $escaped = $true
                } elseif ($c -eq "'") {
                    $i++
                    $column++
                    break
                }
                $i++
                $column++
            }
            continue
        }

        $literalStart = $i
        $literalLine = $line
        $literalColumn = $column
        $prefix = ""

        if (($ch -eq 'L' -or $ch -eq 'u' -or $ch -eq 'U') -and $next -eq '"') {
            $prefix = [string]$ch
            $i++
            $column++
            $ch = $text[$i]
        } elseif ($ch -eq 'u' -and $next -eq '8' -and $i + 2 -lt $length -and $text[$i + 2] -eq '"') {
            $prefix = "u8"
            $i += 2
            $column += 2
            $ch = $text[$i]
        }

        if ($ch -eq '"') {
            $i++
            $column++
            $value = New-Object System.Text.StringBuilder
            $escaped = $false
            while ($i -lt $length) {
                $c = $text[$i]
                if ($c -eq "`n") {
                    $line++
                    $column = 1
                    $i++
                    $escaped = $false
                    continue
                }
                if ($escaped) {
                    [void]$value.Append($c)
                    $escaped = $false
                    $i++
                    $column++
                    continue
                }
                # В PowerShell строка '\\' содержит два символа. Здесь нужен
                # ровно один обратный слеш, иначе экранированная кавычка в C++
                # завершает literal раньше времени, и анализатор захватывает
                # последующие комментарии как часть строки.
                if ($c -eq '\') {
                    [void]$value.Append($c)
                    $escaped = $true
                    $i++
                    $column++
                    continue
                }
                if ($c -eq '"') {
                    $i++
                    $column++
                    break
                }
                [void]$value.Append($c)
                $i++
                $column++
            }

            $literal = $value.ToString()
            if ($literal -match $cyrillicPattern) {
                [pscustomobject]@{
                    File = $Path
                    Line = $literalLine
                    Column = $literalColumn
                    Prefix = $prefix
                    Text = $literal
                }
            }
            continue
        }

        $i++
        $column++
    }
}

$findings = @()
foreach ($file in Get-SourceFiles) {
    $findings += Find-CyrillicStringLiterals -Path $file.FullName
}

if ($findings.Count -eq 0) {
    Write-Host "Кириллица в C/C++ строковых литералах продукта не найдена."
    exit 0
}

Write-Host "Найдена кириллица в C/C++ строковых литералах продукта: $($findings.Count)"
Write-Host "Сводка по файлам:"
$findings |
    Group-Object File |
    Sort-Object -Property @{Expression="Count"; Descending=$true}, Name |
    Select-Object @{Name="Количество";Expression={$_.Count}}, @{Name="Файл";Expression={ Resolve-Path -LiteralPath $_.Name -Relative }} |
    Format-Table -AutoSize

if ($Detailed) {
    Write-Host ""
    Write-Host "Детальный список:"
    $findings |
        Sort-Object File, Line, Column |
        Select-Object @{Name="Файл";Expression={ Resolve-Path -LiteralPath $_.File -Relative }}, @{Name="Строка";Expression={$_.Line}}, @{Name="Колонка";Expression={$_.Column}}, @{Name="Текст";Expression={
            $s = $_.Text.Replace("`r", "\\r").Replace("`n", "\\n")
            if ($s.Length -gt 140) { $s.Substring(0, 137) + "..." } else { $s }
        }} |
        Format-Table -AutoSize -Wrap
} else {
    Write-Host "Для просмотра конкретных строк запустите скрипт с ключом -Detailed."
}

if ($FailOnFindings) {
    throw "В продуктовых C/C++ строковых литералах найдена кириллица: $($findings.Count)."
}



