[CmdletBinding()]
param(
    [ValidateSet("Win32")]
    [string]$Platform = "Win32",

    [string]$ArtifactsDirectory,

    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$versionHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\version.h")
$versionMatch = [regex]::Match(
    $versionHeader,
    '#define\s+FBE_VERSION_STRING\s+"(?<version>\d+\.\d+\.\d+)"'
)

if (-not $versionMatch.Success) {
    throw "В src\\version.h не найден FBE_VERSION_STRING."
}

$version = $versionMatch.Groups["version"].Value
$architecture = $Platform.ToLowerInvariant()
if (-not $ArtifactsDirectory) {
    $ArtifactsDirectory = Join-Path $repoRoot "out\artifacts"
}
$ArtifactsDirectory = (Resolve-Path -LiteralPath $ArtifactsDirectory).Path

$portableName = "FictionBookEditorNext-$version-$architecture-portable.zip"
$symbolsName = "FictionBookEditorNext-$version-$architecture-symbols.zip"
$setupName = "FictionBookEditorNext-$version-$architecture-setup.exe"
$checksumsName = "SHA256SUMS.txt"

$expectedArtifacts = @($portableName, $symbolsName, $checksumsName)
if (-not $SkipInstaller) {
    $expectedArtifacts += $setupName
}

$actualArtifacts = @(
    Get-ChildItem -LiteralPath $ArtifactsDirectory -File |
        Select-Object -ExpandProperty Name |
        Sort-Object
)
$expectedArtifacts = @($expectedArtifacts | Sort-Object)
if (Compare-Object $expectedArtifacts $actualArtifacts) {
    throw "Имена релизных артефактов не совпадают с ожидаемым набором."
}

Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-ZipEntryNames {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $archive = [IO.Compression.ZipFile]::OpenRead($Path)
    try {
        return @(
            $archive.Entries |
                Where-Object { -not [string]::IsNullOrEmpty($_.Name) } |
                Select-Object -ExpandProperty FullName
        )
    }
    finally {
        $archive.Dispose()
    }
}

$portablePath = Join-Path $ArtifactsDirectory $portableName
$portableEntries = Get-ZipEntryNames -Path $portablePath
$requiredPortableEntries = @(
    "FBE.exe",
    "ExportHTML.dll",
    "ExportDOCX.dll",
    "ExportEPUB.dll",
    "ImportEPUB.dll",
    "ImportEPUBLunaSVG.dll",
    "ExportDOCXBatch.exe",
    "ExportEPUBBatch.exe",
    "ImportEPUBBatch.exe",
    "FBE.Sequence.propdesc",
    "FBShell.dll",
    "FBShell64.dll",
    "res_rus.dll",
    "res_ukr.dll",
    "Scintilla.dll",
    "Lexilla.dll",
    "FictionBook.xsd",
    "copying.txt"
)
foreach ($name in $requiredPortableEntries) {
    if ($portableEntries -notcontains $name) {
        throw "В архиве $portableName отсутствует обязательный файл '$name'."
    }
}

$portableEditorVersions = @{
    "Scintilla.dll" = "5.6.3"
    "Lexilla.dll" = "5.5.0"
}
$portableVersionStage = Join-Path ([IO.Path]::GetTempPath()) "FBE-editor-versions-$PID"
New-Item -ItemType Directory -Path $portableVersionStage | Out-Null
try {
    $portableArchiveForVersions = [IO.Compression.ZipFile]::OpenRead($portablePath)
    try {
        foreach ($name in $portableEditorVersions.Keys) {
            $entry = $portableArchiveForVersions.GetEntry($name)
            $path = Join-Path $portableVersionStage $name
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $path, $true)
            $editorVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($path).FileVersion
            if ($editorVersion -ne $portableEditorVersions[$name]) {
                throw "В архиве $portableName файл $name имеет версию '$editorVersion', ожидалась '$($portableEditorVersions[$name])'."
            }
        }
    }
    finally {
        $portableArchiveForVersions.Dispose()
    }
}
finally {
    Remove-Item -LiteralPath $portableVersionStage -Recurse -Force -ErrorAction SilentlyContinue
}

if ($portableEntries | Where-Object { $_ -match '\.(pdb|lib|exp|obj)$' }) {
    throw "В архиве $portableName найдены файлы, которые должны оставаться только в разработческой среде."
}

$obsoletePortableEntries = @(
    "SciLexer.dll",
    "GdiPlus.dll",
    "gdiplus.cat",
    "gdiplus.manifest"
)
foreach ($name in $obsoletePortableEntries) {
    if ($portableEntries -contains $name) {
        throw "В архиве $portableName найден устаревший приватный компонент GDI+: '$name'."
    }
}

$runtimeManifest = Get-Content -Raw -LiteralPath `
    (Join-Path $repoRoot "dependencies\runtime-binaries.json") | ConvertFrom-Json
$portableArchive = [IO.Compression.ZipFile]::OpenRead($portablePath)
try {
    foreach ($entry in $runtimeManifest.files) {
        $zipEntry = $portableArchive.GetEntry($entry.name)
        if (-not $zipEntry) {
            throw "В архиве $portableName отсутствует зафиксированный runtime-файл '$($entry.name)'."
        }
        $allowedSizes = @($entry.size)
        if ($entry.PSObject.Properties.Name -contains "allowedSizes") {
            $allowedSizes = @($entry.allowedSizes)
        }
        if ($zipEntry.Length -notin $allowedSizes) {
            throw "В архиве $portableName файл '$($entry.name)' имеет неожиданный размер."
        }

        $expectedHash = [string]$entry.sha256
        if (-not [string]::IsNullOrWhiteSpace($expectedHash)) {
            $stream = $zipEntry.Open()
            try {
                $sha256 = [Security.Cryptography.SHA256]::Create()
                try {
                    $hash = [BitConverter]::ToString($sha256.ComputeHash($stream)).Replace("-", "")
                }
                finally {
                    $sha256.Dispose()
                }
            }
            finally {
                $stream.Dispose()
            }

            if ($hash -ne $expectedHash) {
                throw "В архиве $portableName файл '$($entry.name)' имеет неожиданный SHA-256."
            }
        }
    }
}
finally {
    $portableArchive.Dispose()
}

$symbolEntries = @(Get-ZipEntryNames -Path (Join-Path $ArtifactsDirectory $symbolsName) | Sort-Object)
$expectedSymbolEntries = @(
    "ExportHTML.pdb",
    "ExportDOCX.pdb",
    "ExportDOCXBatch.pdb",
    "ExportEPUB.pdb",
    "ExportEPUBBatch.pdb",
    "ImportEPUB.pdb",
    "ImportEPUBLunaSVG.pdb",
    "ImportEPUBBatch.pdb",
    "FBE.pdb",
    "FBShell.pdb",
    "FBShell.propertyhandler.win32.pdb",
    "FBShell.propertyhandler.x64.pdb",
    "FBShell.x64.pdb",
    "README.txt",
    "res_rus.pdb",
    "res_ukr.pdb"
) | Sort-Object
if (Compare-Object $expectedSymbolEntries $symbolEntries) {
    throw "Архив $symbolsName не содержит ожидаемый набор PDB-файлов."
}

$checksumPath = Join-Path $ArtifactsDirectory $checksumsName
$checksumEntries = @{}
foreach ($line in Get-Content -LiteralPath $checksumPath) {
    if ($line -notmatch '^(?<hash>[0-9A-Fa-f]{64})  (?<name>.+)$') {
        throw "Некорректная строка в SHA256SUMS.txt: $line"
    }
    if ($checksumEntries.ContainsKey($Matches.name)) {
        throw "Дублирующаяся запись контрольной суммы для '$($Matches.name)'."
    }
    $checksumEntries[$Matches.name] = $Matches.hash.ToUpperInvariant()
}

$hashedArtifactNames = @(
    $actualArtifacts |
        Where-Object { $_ -ne $checksumsName } |
        Sort-Object
)
$checksumNames = @($checksumEntries.Keys | Sort-Object)
if (Compare-Object $hashedArtifactNames $checksumNames) {
    throw "SHA256SUMS.txt не описывает полный набор артефактов."
}

foreach ($name in $hashedArtifactNames) {
    $actualHash = (Get-FileHash -LiteralPath (Join-Path $ArtifactsDirectory $name) `
        -Algorithm SHA256).Hash
    if ($actualHash -ne $checksumEntries[$name]) {
        throw "Контрольная сумма SHA-256 не совпадает для '$name'."
    }
}

Write-Host "Проверка релизных артефактов прошла успешно для версии $version."
