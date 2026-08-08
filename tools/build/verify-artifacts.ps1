[CmdletBinding()]
param(
    [ValidateSet("Win32")]
    [string]$Platform = "Win32",

    [ValidateSet("Modern", "Win7", "All")]
    [string]$CompatibilityTarget = "Modern",

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

if ($CompatibilityTarget -eq "All" -and
    (Test-Path -LiteralPath (Join-Path $ArtifactsDirectory "Modern") -PathType Container) -and
    (Test-Path -LiteralPath (Join-Path $ArtifactsDirectory "Win7") -PathType Container)) {
    # Изолированные profiles имеют собственные SHA256SUMS, поэтому проверяем
    # каждый каталог отдельно, после чего сравниваем ожидаемо общие/разные DLL.
    & $PSCommandPath -Platform $Platform -CompatibilityTarget Modern `
        -ArtifactsDirectory (Join-Path $ArtifactsDirectory "Modern") -SkipInstaller:$SkipInstaller
    & $PSCommandPath -Platform $Platform -CompatibilityTarget Win7 `
        -ArtifactsDirectory (Join-Path $ArtifactsDirectory "Win7") -SkipInstaller:$SkipInstaller

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    function Get-ProfileZipEntrySha256 {
        param([string]$ArchivePath, [string]$EntryName)
        $archive = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
        try {
            $entry = $archive.GetEntry($EntryName)
            if ($null -eq $entry) { throw "В архиве отсутствует $EntryName" }
            $stream = $entry.Open()
            $sha256 = [Security.Cryptography.SHA256]::Create()
            try { return [BitConverter]::ToString($sha256.ComputeHash($stream)).Replace("-", "") }
            finally { $sha256.Dispose(); $stream.Dispose() }
        }
        finally { $archive.Dispose() }
    }

    $modernPortable = Join-Path $ArtifactsDirectory "Modern\FictionBookEditorNext-$version-$architecture-portable.zip"
    $win7Portable = Join-Path $ArtifactsDirectory "Win7\FictionBookEditorNext-$version-win7-$architecture-portable.zip"
    foreach ($name in @("FBE.exe", "FBV.exe", "ExportHTML.dll", "ExportDOCX.dll", "ExportEPUB.dll", "ImportEPUB.dll", "ImportEPUBLunaSVG.dll", "FBShell.dll", "FBShell64.dll", "Lang/ru-RU/res_rus.dll", "Lang/uk-UA/res_ukr.dll")) {
        if ((Get-ProfileZipEntrySha256 $modernPortable $name) -ne (Get-ProfileZipEntrySha256 $win7Portable $name)) {
            throw "Общий файл '$name' различается между Modern и Win7 portable-пакетами."
        }
    }
    foreach ($name in @("Scintilla.dll", "ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe")) {
        if ((Get-ProfileZipEntrySha256 $modernPortable $name) -eq (Get-ProfileZipEntrySha256 $win7Portable $name)) {
            throw "$name в Modern и Win7 portable-пакетах совпадает; Win7-вариант не был применён."
        }
    }
    Write-Host "Проверка изолированных Modern и Win7 артефактов прошла успешно."
    return
}

$checksumsName = "SHA256SUMS.txt"

$artifactProfiles = switch ($CompatibilityTarget) {
    "Modern" {
        @(@{ Prefix = ""; Label = "modern"; HasInstaller = -not $SkipInstaller })
    }
    "Win7" {
        @(@{ Prefix = "win7-"; Label = "win7"; HasInstaller = -not $SkipInstaller })
    }
    "All" {
        @(
            @{ Prefix = ""; Label = "modern"; HasInstaller = -not $SkipInstaller },
            @{ Prefix = "win7-"; Label = "win7"; HasInstaller = -not $SkipInstaller }
        )
    }
}

$expectedArtifacts = @($checksumsName)
foreach ($profile in $artifactProfiles) {
    $expectedArtifacts += "FictionBookEditorNext-$version-$($profile.Prefix)$architecture-portable.zip"
    $expectedArtifacts += "FictionBookEditorNext-$version-$($profile.Prefix)$architecture-symbols.zip"
    if ($profile.HasInstaller) {
        $expectedArtifacts += "FictionBookEditorNext-$version-$($profile.Prefix)$architecture-setup.exe"
    }
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

function Get-ZipEntrySha256 {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$EntryName
    )

    $archive = [IO.Compression.ZipFile]::OpenRead($Path)
    try {
        $entry = $archive.GetEntry($EntryName)
        if ($null -eq $entry) {
            throw "В архиве $(Split-Path -Leaf $Path) отсутствует файл '$EntryName'."
        }

        $stream = $entry.Open()
        $sha256 = [Security.Cryptography.SHA256]::Create()
        try {
            return [BitConverter]::ToString($sha256.ComputeHash($stream)).Replace("-", "")
        }
        finally {
            $sha256.Dispose()
            $stream.Dispose()
        }
    }
    finally {
        $archive.Dispose()
    }
}

$requiredPortableEntries = @(
    "FBE.exe",
    "FBV.exe",
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
    "Lang/ru-RU/res_rus.dll",
    "Lang/uk-UA/res_ukr.dll",
    "Scintilla.dll",
    "Lexilla.dll",
    "FictionBook.xsd",
    "copying.txt"
)

$portableEditorVersions = @{
    "Scintilla.dll" = "5.6.5"
    "Lexilla.dll" = "5.5.2"
}

$obsoletePortableEntries = @(
    "SciLexer.dll",
    "GdiPlus.dll",
    "gdiplus.cat",
    "gdiplus.manifest"
)

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

foreach ($profile in $artifactProfiles) {
    $portableName = "FictionBookEditorNext-$version-$($profile.Prefix)$architecture-portable.zip"
    $symbolsName = "FictionBookEditorNext-$version-$($profile.Prefix)$architecture-symbols.zip"
    $portablePath = Join-Path $ArtifactsDirectory $portableName
    $portableEntries = Get-ZipEntryNames -Path $portablePath
    foreach ($name in $requiredPortableEntries) {
        if ($portableEntries -notcontains $name) {
            throw "В архиве $portableName отсутствует обязательный файл '$name'."
        }
    }

    $portableVersionStage = Join-Path ([IO.Path]::GetTempPath()) "FBE-editor-versions-$PID-$($profile.Label)"
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

    foreach ($name in $obsoletePortableEntries) {
        if ($portableEntries -contains $name) {
            throw "В архиве $portableName найден устаревший приватный компонент GDI+: '$name'."
        }
    }

    $symbolEntries = @(Get-ZipEntryNames -Path (Join-Path $ArtifactsDirectory $symbolsName) | Sort-Object)
    if (Compare-Object $expectedSymbolEntries $symbolEntries) {
        throw "Архив $symbolsName не содержит ожидаемый набор PDB-файлов."
    }
}

if ($CompatibilityTarget -eq "All") {
    $modernPortablePath = Join-Path $ArtifactsDirectory "FictionBookEditorNext-$version-$architecture-portable.zip"
    $win7PortablePath = Join-Path $ArtifactsDirectory "FictionBookEditorNext-$version-win7-$architecture-portable.zip"

    # Общие бинарники должны происходить из одного явного modern-этапа, а не
    # из неявного инкрементального состояния второй сборки. Batch-конвертеры
    # исключены: они намеренно пересобираются с API-уровнем Windows 7.
    $commonPortableEntries = @(
        "FBE.exe", "FBV.exe", "ExportHTML.dll", "ExportDOCX.dll", "ExportEPUB.dll",
        "ImportEPUB.dll", "ImportEPUBLunaSVG.dll", "FBShell.dll", "FBShell64.dll",
        "Lang/ru-RU/res_rus.dll", "Lang/uk-UA/res_ukr.dll"
    )
    foreach ($name in $commonPortableEntries) {
        $modernHash = Get-ZipEntrySha256 -Path $modernPortablePath -EntryName $name
        $win7Hash = Get-ZipEntrySha256 -Path $win7PortablePath -EntryName $name
        if ($modernHash -ne $win7Hash) {
            throw "Общий файл '$name' различается между Modern и Win7 portable-пакетами."
        }
    }

    # Scintilla собирается отдельно с Win7-определением, поэтому архивы
    # обязаны содержать разные редакторские DLL, а не одну старую runtime-копию.
    $modernScintillaHash = Get-ZipEntrySha256 -Path $modernPortablePath -EntryName "Scintilla.dll"
    $win7ScintillaHash = Get-ZipEntrySha256 -Path $win7PortablePath -EntryName "Scintilla.dll"
    if ($modernScintillaHash -eq $win7ScintillaHash) {
        throw "Scintilla.dll в Modern и Win7 portable-пакетах совпадает; Win7-вариант не был применён."
    }

    foreach ($name in @("ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe")) {
        $modernHash = Get-ZipEntrySha256 -Path $modernPortablePath -EntryName $name
        $win7Hash = Get-ZipEntrySha256 -Path $win7PortablePath -EntryName $name
        if ($modernHash -eq $win7Hash) {
            throw "$name в Modern и Win7 portable-пакетах совпадает; Win7-вариант пакетного конвертера не был применён."
        }
    }
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
