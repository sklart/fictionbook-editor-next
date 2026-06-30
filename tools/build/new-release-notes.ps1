# Создаёт человекочитаемые заметки к GitHub-релизу из CHANGELOG.md и добавляет ссылку на коммиты.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")
$changelogPath = Join-Path $repoRoot "CHANGELOG.md"

if (-not (Test-Path -LiteralPath $changelogPath -PathType Leaf)) {
    throw "Не найден CHANGELOG.md: $changelogPath"
}

$normalizedVersion = $Version
if ($normalizedVersion.StartsWith("v", [StringComparison]::OrdinalIgnoreCase)) {
    $normalizedVersion = $normalizedVersion.Substring(1)
}

$curatedNotesPath = Join-Path $repoRoot "docs\release-notes\$normalizedVersion.md"
if (Test-Path -LiteralPath $curatedNotesPath -PathType Leaf) {
    $body = Get-Content -LiteralPath $curatedNotesPath
}
else {
    $body = $null
}

if ($null -eq $body) {
    $lines = Get-Content -LiteralPath $changelogPath
    $start = $null
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^##\s+Unreleased\s*$') {
            $start = $i + 1
            break
        }
    }

    $body = @()
    if ($null -ne $start) {
        for ($i = $start; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match '^##\s+') {
                break
            }
            $body += $lines[$i]
        }
    }
}

if ($body.Count -eq 0) {
    $body = @(
        "### Что изменилось",
        "",
        "- Подготовлен релиз FictionBook Editor Next $normalizedVersion.",
        "- Подробности смотрите в списке коммитов ниже."
    )
}

$repository = $env:GITHUB_REPOSITORY
$currentTag = $env:GITHUB_REF_NAME
$commitsUrl = $null

if (-not [string]::IsNullOrWhiteSpace($repository) -and
    -not [string]::IsNullOrWhiteSpace($currentTag)) {
    $previousTag = $null
    try {
        $previousTag = (& git -C $repoRoot describe --tags --abbrev=0 "$currentTag^" 2>$null)
    }
    catch {
        $previousTag = $null
    }

    if ([string]::IsNullOrWhiteSpace($previousTag)) {
        $commitsUrl = "https://github.com/$repository/commits/$currentTag"
    }
    else {
        $commitsUrl = "https://github.com/$repository/compare/$previousTag...$currentTag"
    }
}

$notes = New-Object System.Collections.Generic.List[string]
$notes.Add("# FictionBook Editor Next $normalizedVersion")
$notes.Add("")
$notes.Add("Это пользовательское описание изменений. Оно намеренно написано не как список изменённых файлов, а как краткое объяснение того, что стало лучше в программе.")
$notes.Add("")
$notes.AddRange([string[]]$body)

if (-not [string]::IsNullOrWhiteSpace($commitsUrl)) {
    $notes.Add("")
    $notes.Add("### Коммиты")
    $notes.Add("")
    $notes.Add("- [Посмотреть изменения в GitHub]($commitsUrl)")
}

$outputDirectory = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}

Set-Content -LiteralPath $OutputPath -Value $notes -Encoding UTF8
Write-Host "Заметки к релизу подготовлены: $OutputPath"
$global:LASTEXITCODE = 0
exit 0
