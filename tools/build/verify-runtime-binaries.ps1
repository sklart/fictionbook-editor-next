[CmdletBinding()]
param(
    [string]$Directory
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$manifestPath = Join-Path $repoRoot "dependencies\runtime-binaries.json"
if (-not $Directory) {
    $Directory = Join-Path $repoRoot "runtime"
}
$Directory = (Resolve-Path -LiteralPath $Directory).Path

$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
if ($manifest.schemaVersion -ne 1 -or $manifest.architecture -ne "x86") {
    throw "Неподдерживаемый манифест runtime-бинарников."
}

foreach ($entry in $manifest.files) {
    $path = Join-Path $Directory $entry.name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Отсутствует зафиксированный runtime-бинарник: $path"
    }

    $item = Get-Item -LiteralPath $path
    $allowedSizes = @()
    if ($entry.allowedSizes) {
        $allowedSizes = @($entry.allowedSizes | ForEach-Object { [int64]$_ })
    }
    else {
        $allowedSizes = @([int64]$entry.size)
    }
    if ($allowedSizes -notcontains [int64]$item.Length) {
        throw "$($entry.name): размер $($item.Length), ожидался один из: $($allowedSizes -join ', ')."
    }

    $expectedHash = [string]$entry.sha256
    if (-not [string]::IsNullOrWhiteSpace($expectedHash)) {
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($hash -ne $expectedHash) {
            throw "$($entry.name): SHA-256 $hash, ожидался $expectedHash."
        }
    }

    $bytes = [IO.File]::ReadAllBytes($path)
    if ($bytes.Length -lt 256 -or
        $bytes[0] -ne [byte][char]'M' -or
        $bytes[1] -ne [byte][char]'Z') {
        throw "$($entry.name) не является корректным PE-файлом."
    }

    $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
    if ($peOffset -lt 0 -or $peOffset + 6 -gt $bytes.Length -or
        [Text.Encoding]::ASCII.GetString($bytes, $peOffset, 4) -ne "PE`0`0") {
        throw "$($entry.name) содержит некорректный PE-заголовок."
    }

    $machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
    if ($machine -ne 0x14c) {
        throw "$($entry.name) не является x86 PE-файлом."
    }

    if ($entry.importLibrary) {
        $importLibraryPath = Join-Path $repoRoot `
            ($entry.importLibrary.Replace("/", [IO.Path]::DirectorySeparatorChar))
        if (-not (Test-Path -LiteralPath $importLibraryPath -PathType Leaf)) {
            throw "Отсутствует зафиксированная import-библиотека: $importLibraryPath"
        }

        $importLibraryHash = (Get-FileHash -LiteralPath $importLibraryPath `
            -Algorithm SHA256).Hash
        if ($importLibraryHash -ne $entry.importLibrarySha256) {
            throw "$($entry.importLibrary): SHA-256 $importLibraryHash, ожидался $($entry.importLibrarySha256)."
        }
    }
}

Write-Host "Проверка runtime-бинарников для $Directory прошла успешно."
