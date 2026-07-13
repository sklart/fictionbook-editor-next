<#
.SYNOPSIS
Строит инвентарь локализуемых UI-литералов FBE из Win32 .rc ресурсов.

.DESCRIPTION
Скрипт читает русскую и украинскую локализации FBE.rc в кодировке Windows-1251,
находит текстовые литералы в MENU и DIALOGEX ресурсах и сохраняет отчёт в JSON.
Он нужен как безопасный подготовительный этап перед переносом меню и диалогов FBE
в Weblate-friendly JSON: сначала фиксируем, какие строки ещё живут прямо в .rc,
потом переносим их с проверяемым покрытием.
#>
[CmdletBinding()]
param(
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repoRoot "out\localization\fbe-rc-ui-literals.json"
}

$files = @(
    @{ Language = "ru-RU"; Path = Join-Path $repoRoot "src\locales\res_rus\FBE.rc"; Encoding = [Text.Encoding]::GetEncoding(1251) },
    @{ Language = "uk-UA"; Path = Join-Path $repoRoot "src\locales\res_ukr\FBE.rc"; Encoding = [Text.Encoding]::GetEncoding(1251) }
)

$encoding = [Text.Encoding]::GetEncoding(1251)
$items = [Collections.Generic.List[object]]::new()

function ConvertFrom-RcLiteral {
    param([AllowEmptyString()][string]$Value)
    return $Value.Replace('""', '"')
}

function Add-Item {
    param(
        [Parameter(Mandatory)][string]$Language,
        [Parameter(Mandatory)][string]$RelativePath,
        [Parameter(Mandatory)][string]$Resource,
        [Parameter(Mandatory)][string]$ResourceType,
        [Parameter(Mandatory)][int]$Line,
        [Parameter(Mandatory)][string]$Kind,
        [AllowEmptyString()][string]$Text,
        [string]$TargetId
    )

    if ([string]::IsNullOrWhiteSpace($Text)) { return }
    if ($Text -eq "Static") { return }
    if ($Text -eq "DT" -or $Text -eq "CUSTOMIZE") { return }

    $items.Add([pscustomobject][ordered]@{
        language = $Language
        path = $RelativePath
        resource = $Resource
        resourceType = $ResourceType
        line = $Line
        kind = $Kind
        targetId = $TargetId
        text = $Text
    })
}

foreach ($file in $files) {
    $fullPath = [string]$file.Path
    $relativePath = $fullPath.Replace($repoRoot + "\", "")
    $text = [IO.File]::ReadAllText($fullPath, $file.Encoding)
    $lines = $text -split "`r?`n"

    $resource = $null
    $resourceType = $null

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $lineNumber = $i + 1
        $line = $lines[$i]

        if ($line -match '^\s*([A-Za-z0-9_]+)\s+(MENU|DIALOGEX)\b') {
            $resource = $Matches[1]
            $resourceType = $Matches[2]
            continue
        }

        if ($null -eq $resource) { continue }

        if ($line -match '^\s*([A-Za-z0-9_]+)\s+(MENU|DIALOGEX)\b') {
            $resource = $Matches[1]
            $resourceType = $Matches[2]
            continue
        }

        if ($line -match '^\s*(CAPTION)\s+"((?:[^"]|"")*)"') {
            Add-Item -Language $file.Language -RelativePath $relativePath -Resource $resource -ResourceType $resourceType -Line $lineNumber -Kind $Matches[1] -Text (ConvertFrom-RcLiteral $Matches[2])
            continue
        }

        if ($line -match '^\s*(POPUP)\s+"((?:[^"]|"")*)"') {
            Add-Item -Language $file.Language -RelativePath $relativePath -Resource $resource -ResourceType $resourceType -Line $lineNumber -Kind $Matches[1] -Text (ConvertFrom-RcLiteral $Matches[2])
            continue
        }

        if ($line -match '^\s*(MENUITEM)\s+"((?:[^"]|"")*)"\s*,\s*([^,\s]+)') {
            Add-Item -Language $file.Language -RelativePath $relativePath -Resource $resource -ResourceType $resourceType -Line $lineNumber -Kind $Matches[1] -TargetId $Matches[3] -Text (ConvertFrom-RcLiteral $Matches[2])
            continue
        }

        if ($line -match '^\s*(LTEXT|RTEXT|CTEXT|PUSHBUTTON|DEFPUSHBUTTON|GROUPBOX)\s+"((?:[^"]|"")*)"\s*,\s*([^,\s]+)') {
            Add-Item -Language $file.Language -RelativePath $relativePath -Resource $resource -ResourceType $resourceType -Line $lineNumber -Kind $Matches[1] -TargetId $Matches[3] -Text (ConvertFrom-RcLiteral $Matches[2])
            continue
        }

        if ($line -match '^\s*(CONTROL)\s+"((?:[^"]|"")*)"\s*,\s*([^,\s]+)') {
            Add-Item -Language $file.Language -RelativePath $relativePath -Resource $resource -ResourceType $resourceType -Line $lineNumber -Kind $Matches[1] -TargetId $Matches[3] -Text (ConvertFrom-RcLiteral $Matches[2])
            continue
        }
    }
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null

$byLanguage = @($items | Group-Object language | Sort-Object Name | ForEach-Object {
    [pscustomobject][ordered]@{ language = $_.Name; count = $_.Count }
})
$byResourceType = @($items | Group-Object resourceType | Sort-Object Name | ForEach-Object {
    [pscustomobject][ordered]@{ resourceType = $_.Name; count = $_.Count }
})
$byResource = @($items | Group-Object resource | Sort-Object Name | ForEach-Object {
    [pscustomobject][ordered]@{ resource = $_.Name; count = $_.Count }
})

$report = [ordered]@{
    generatedAt = (Get-Date).ToString("s")
    sourceEncoding = "windows-1251"
    purpose = "Инвентарь оставшихся локализуемых FBE MENU/DIALOGEX литералов перед переносом в JSON."
    totals = [ordered]@{
        items = $items.Count
        languages = $byLanguage.Count
        resources = $byResource.Count
    }
    byLanguage = $byLanguage
    byResourceType = $byResourceType
    byResource = $byResource
    items = $items
}

$utf8NoBom = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($OutputPath, ($report | ConvertTo-Json -Depth 8) + "`n", $utf8NoBom)

Write-Host "Инвентарь UI-литералов FBE подготовлен."
Write-Host "  Файл: $OutputPath"
Write-Host "  Литералов: $($items.Count)"
Write-Host "  Языков: $($byLanguage.Count)"
Write-Host "  Ресурсов: $($byResource.Count)"
$byResourceType | Format-Table -AutoSize


