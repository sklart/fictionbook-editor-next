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
$dictionarySources = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'runtime\dict\sources.json') | ConvertFrom-Json
$dictionaryArchiveHashes = [ordered]@{
    'dict/sources.json' = (Get-FileHash -LiteralPath (Join-Path $repoRoot 'runtime\dict\sources.json') -Algorithm SHA256).Hash
    'dict/de_DE.aff' = $dictionarySources.de_DE.affSha256
    'dict/de_DE.dic' = $dictionarySources.de_DE.dicSha256
    'dict/en_US.aff' = $dictionarySources.en_US.affSha256
    'dict/en_US.dic' = $dictionarySources.en_US.dicSha256
    'dict/ru_RU.aff' = $dictionarySources.ru_RU.affSha256
    'dict/ru_RU.dic' = $dictionarySources.ru_RU.dicSha256
    'dict/uk_UA.aff' = $dictionarySources.uk_UA.affSha256
    'dict/uk_UA.dic' = $dictionarySources.uk_UA.dicSha256
}

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
    foreach ($entry in $dictionaryArchiveHashes.GetEnumerator()) {
        $modernHash = Get-ProfileZipEntrySha256 $modernPortable $entry.Key
        $win7Hash = Get-ProfileZipEntrySha256 $win7Portable $entry.Key
        if ($modernHash -ne $entry.Value -or $win7Hash -ne $entry.Value) {
            throw "Словарь '$($entry.Key)' в portable-архиве не совпадает с runtime/dict/sources.json."
        }
        if ($modernHash -ne $win7Hash) {
            throw "Словарь '$($entry.Key)' различается между Modern и Win7 portable-пакетами."
        }
    }
    foreach ($name in @("FBE.exe", "FBV.exe", "ExportHTML.dll", "ExportDOCX.dll", "ExportEPUB.dll", "ImportEPUB.dll", "ImportEPUBLunaSVG.dll", "Lang/ru-RU/res_rus.dll", "Lang/uk-UA/res_ukr.dll")) {
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
    "LICENSE",
    "NOTICE",
    "THIRD-PARTY-NOTICES.md",
    "THIRD-PARTY-LICENSES/README.md",
    "THIRD-PARTY-LICENSES/WTL-MS-PL.txt",
    "THIRD-PARTY-LICENSES/Dictionary-en_US.txt",
    "THIRD-PARTY-LICENSES/Dictionary-de_DE.txt",
    "THIRD-PARTY-LICENSES/Dictionary-ru_RU.txt",
    "THIRD-PARTY-LICENSES/Dictionary-uk_UA.txt",
    "THIRD-PARTY-LICENSES/Scintilla-Lexilla.txt",
    "THIRD-PARTY-LICENSES/PCRE2.txt",
    "THIRD-PARTY-LICENSES/Hunspell.txt",
    "THIRD-PARTY-LICENSES/Hunspell-MySpell.txt",
    "THIRD-PARTY-LICENSES/libwebp.txt",
    "THIRD-PARTY-LICENSES/OpenJPEG.txt",
    "THIRD-PARTY-LICENSES/libheif.txt",
    "THIRD-PARTY-LICENSES/libde265.txt",
    "THIRD-PARTY-LICENSES/libaom.txt",
    "THIRD-PARTY-LICENSES/libaom-PATENTS.txt",
    "THIRD-PARTY-LICENSES/LunaSVG.txt",
    "THIRD-PARTY-LICENSES/PlutoVG.txt",
    "THIRD-PARTY-LICENSES/Theme-palettes-MIT.txt",
    "THIRD-PARTY-LICENSES/UAC.txt",
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
    "dict/en_US.aff",
    "dict/en_US.dic",
    "dict/de_DE.aff",
    "dict/de_DE.dic",
    "dict/ru_RU.aff",
    "dict/ru_RU.dic",
    "dict/uk_UA.aff",
    "dict/uk_UA.dic",
    "dict/sources.json",
    "portable.ini",
    "Data/Settings/.keep",
    "Data/Scripts/.keep",
    "Data/Dictionaries/.keep",
    "Data/Themes/.keep",
    "Data/Logs/.keep",
    "Data/Diagnostics/.keep",
    "Data/Recovery/.keep",
    "Data/Cache/.keep",
    "Data/Temp/.keep",
    "Lang/ru-RU/res_rus.dll",
    "Lang/uk-UA/res_ukr.dll",
    "Scintilla.dll",
    "Lexilla.dll",
    "FictionBook.xsd"
)

$portableEditorVersions = @{
    "Scintilla.dll" = "5.6.6"
    "Lexilla.dll" = "5.5.3"
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
    foreach ($entry in $dictionaryArchiveHashes.GetEnumerator()) {
        $actualHash = Get-ZipEntrySha256 -Path $portablePath -EntryName $entry.Key
        if ($actualHash -ne $entry.Value) {
            throw "Словарь '$($entry.Key)' в архиве $portableName не совпадает с runtime/dict/sources.json."
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
        "ImportEPUB.dll", "ImportEPUBLunaSVG.dll",
        "Lang/ru-RU/res_rus.dll", "Lang/uk-UA/res_ukr.dll"
    )
    $commonPortableEntries += @($dictionaryArchiveHashes.Keys)
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
