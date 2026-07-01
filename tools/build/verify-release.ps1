[CmdletBinding()]
param(
    [string]$Configuration = "Release",

    [ValidateSet("Modern", "Win7")]
    [string]$CompatibilityTarget = "Modern",

    [string]$PlatformToolset,

    [switch]$SkipUpdateManifest
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outputDir = Join-Path $repoRoot "out\$Configuration"
$versionHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\version.h")
$versionMatch = [regex]::Match(
    $versionHeader,
    '#define\s+FBE_VERSION_STRING\s+"(?<version>\d+\.\d+\.\d+)"'
)

if (-not $versionMatch.Success) {
    throw "Не найден FBE_VERSION_STRING."
}

$expectedVersion = $versionMatch.Groups["version"].Value
$requiredFiles = @(
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
    "FBShell.dll",
    "res_rus.dll",
    "res_ukr.dll",
    "Scintilla.dll",
    "Lexilla.dll"
)
$forbiddenFiles = @(
    "pcre.dll"
)

$requiredSymbols = @(
    "FBE.pdb",
    "FBV.pdb",
    "ExportHTML.pdb",
    "ExportDOCX.pdb",
    "ExportEPUB.pdb",
    "ImportEPUB.pdb",
    "ImportEPUBLunaSVG.pdb",
    "ExportDOCXBatch.pdb",
    "ExportEPUBBatch.pdb",
    "ImportEPUBBatch.pdb",
    "FBShell.pdb",
    "res_rus.pdb",
    "res_ukr.pdb"
)

& (Join-Path $PSScriptRoot "verify-runtime-binaries.ps1") -Directory $outputDir
& (Join-Path $repoRoot "tools\tests\test-source-safety.ps1")
if (-not $SkipUpdateManifest) {
    & (Join-Path $repoRoot "tools\tests\test-update-manifest.ps1")
}
& (Join-Path $repoRoot "tools\tests\test-spellcheck-dictionaries.ps1") -Configuration $Configuration
& (Join-Path $repoRoot "tools\tests\test-scintilla.ps1")
$pcre2TestArguments = @{
    Configuration = $Configuration
}
if ($PlatformToolset) {
    $pcre2TestArguments.PlatformToolset = $PlatformToolset
}
& (Join-Path $repoRoot "tools\tests\test-pcre2.ps1") @pcre2TestArguments
& (Join-Path $repoRoot "tools\tests\test-pcre2-wrapper.ps1") @pcre2TestArguments
& (Join-Path $repoRoot "tools\tests\test-pcre2-replace.ps1") @pcre2TestArguments
& (Join-Path $repoRoot "tools\tests\test-export-epub-cyrillic.ps1") -Configuration $Configuration
& (Join-Path $repoRoot "tools\tests\test-export-epub-xhtml11.ps1") -Configuration $Configuration
& (Join-Path $repoRoot "tools\tests\test-plugin-mojibake.ps1")
& (Join-Path $repoRoot "tools\tests\test-import-epub-registration.ps1") -Configuration $Configuration
if ($CompatibilityTarget -eq "Win7") {
    & (Join-Path $repoRoot "tools\tests\check-win7-imports.ps1") -Configuration $Configuration
}

function Test-BinarySecurityFlags {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [switch]$RequireControlFlowGuard
    )

    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 256 -or
        $bytes[0] -ne [byte][char]'M' -or
        $bytes[1] -ne [byte][char]'Z') {
        throw "Некорректный PE-файл: $Path"
    }

    $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
    if ($peOffset -lt 0 -or $peOffset + 96 -gt $bytes.Length -or
        [Text.Encoding]::ASCII.GetString($bytes, $peOffset, 4) -ne "PE`0`0") {
        throw "Некорректный PE-заголовок: $Path"
    }

    $optionalHeader = $peOffset + 24
    $magic = [BitConverter]::ToUInt16($bytes, $optionalHeader)
    if ($magic -ne 0x10b -and $magic -ne 0x20b) {
        throw "Неподдерживаемый optional header PE в $Path"
    }

    $dllCharacteristics = [BitConverter]::ToUInt16($bytes, $optionalHeader + 70)
    if (($dllCharacteristics -band 0x40) -eq 0) {
        throw "В $Path отсутствует флаг DYNAMIC_BASE (ASLR)."
    }
    if (($dllCharacteristics -band 0x100) -eq 0) {
        throw "В $Path отсутствует флаг NX_COMPAT (DEP)."
    }
    if ($RequireControlFlowGuard -and ($dllCharacteristics -band 0x4000) -eq 0) {
        throw "В $Path отсутствует флаг GUARD_CF (Control Flow Guard)."
    }
}

$controlFlowGuardFiles = @(
    "FBE.exe",
    "FBV.exe",
    "ExportHTML.dll",
    "ExportDOCX.dll",
    "ExportEPUB.dll",
    "FBShell.dll"
)

foreach ($name in $requiredFiles) {
    $path = Join-Path $outputDir $name

    if (-not (Test-Path -LiteralPath $path)) {
        throw "Отсутствует обязательный результат сборки: $path"
    }

    Test-BinarySecurityFlags -Path $path `
        -RequireControlFlowGuard:($name -in $controlFlowGuardFiles)
}

foreach ($name in $forbiddenFiles) {
    $path = Join-Path $outputDir $name
    if (Test-Path -LiteralPath $path) {
        throw "Устаревший runtime-файл не должен попадать в релиз: $path"
    }
}

foreach ($name in $requiredSymbols) {
    $path = Join-Path $outputDir $name
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Отсутствуют обязательные debug symbols: $path"
    }
    if ((Get-Item -LiteralPath $path).Length -eq 0) {
        throw "Файл debug symbols пуст: $path"
    }
}

foreach ($name in @("FBE.exe", "FBV.exe", "ExportHTML.dll", "ExportDOCX.dll", "ExportEPUB.dll", "ImportEPUB.dll", "ImportEPUBLunaSVG.dll", "ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe", "FBShell.dll", "res_rus.dll", "res_ukr.dll")) {
    $path = Join-Path $outputDir $name
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($path)

    if ($info.FileVersion -ne $expectedVersion) {
        throw "$name имеет версию файла '$($info.FileVersion)', ожидалась '$expectedVersion'."
    }

    if ($info.ProductVersion -ne $expectedVersion) {
        throw "$name имеет версию продукта '$($info.ProductVersion)', ожидалась '$expectedVersion'."
    }
}

$requiredFileDescriptions = @{
    "FBE.exe" = "FictionBook Editor Next"
    "FBV.exe" = "FictionBook Validator"
    "ExportHTML.dll" = "FictionBook Editor HTML export plugin"
    "ExportDOCX.dll" = "FictionBook Editor DOCX export plugin"
    "ExportEPUB.dll" = "FictionBook Editor EPUB export plugin"
    "ImportEPUB.dll" = "FictionBook Editor EPUB import plugin"
    "ImportEPUBLunaSVG.dll" = "FictionBook Editor EPUB SVG cover converter"
    "ExportDOCXBatch.exe" = "FictionBook Editor DOCX batch export utility"
    "ExportEPUBBatch.exe" = "FictionBook Editor EPUB batch export utility"
    "ImportEPUBBatch.exe" = "FictionBook Editor EPUB batch import utility"
    "FBShell.dll" = "FictionBook Editor shell property handler"
    "res_rus.dll" = "FictionBook Editor Russian resources"
    "res_ukr.dll" = "FictionBook Editor Ukrainian resources"
}

foreach ($entry in $requiredFileDescriptions.GetEnumerator()) {
    $path = Join-Path $outputDir $entry.Key
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($path)

    if ([string]::IsNullOrWhiteSpace($info.FileDescription)) {
        throw "У $($entry.Key) отсутствует метаданные FileDescription."
    }
    if ($info.FileDescription -ne $entry.Value) {
        throw "$($entry.Key) имеет FileDescription '$($info.FileDescription)', ожидалось '$($entry.Value)'."
    }
    if ([string]::IsNullOrWhiteSpace($info.ProductName)) {
        throw "У $($entry.Key) отсутствует метаданные ProductName."
    }
}

$editorVersions = @{
    "Scintilla.dll" = "5.6.3"
    "Lexilla.dll" = "5.5.0"
}
foreach ($name in $editorVersions.Keys) {
    $path = Join-Path $outputDir $name
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($path)
    if ($info.FileVersion -ne $editorVersions[$name]) {
        throw "$name имеет версию файла '$($info.FileVersion)', ожидалась '$($editorVersions[$name])'."
    }
}

$mtCandidates = @(
    (Get-Command mt.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1),
    (Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter mt.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x86\\mt\.exe$' } |
        Sort-Object FullName -Descending |
        Select-Object -ExpandProperty FullName -First 1)
) | Where-Object { $_ } | Select-Object -First 1

if (-not $mtCandidates) {
    throw "Не найден mt.exe; невозможно проверить встроенный manifest FBE.exe."
}

$manifestPath = Join-Path ([IO.Path]::GetTempPath()) "FBE-$PID.manifest"
try {
    & $mtCandidates -nologo "-inputresource:$outputDir\FBE.exe;#1" "-out:$manifestPath"
    if ($LASTEXITCODE -ne 0) {
        throw "mt.exe не смог извлечь manifest из FBE.exe."
    }

    [xml]$manifest = Get-Content -Raw -LiteralPath $manifestPath
    $dpiAware = $manifest.SelectSingleNode(
        "//*[local-name()='dpiAware' and namespace-uri()='http://schemas.microsoft.com/SMI/2005/WindowsSettings']"
    )
    $dpiAwareness = $manifest.SelectSingleNode(
        "//*[local-name()='dpiAwareness' and namespace-uri()='http://schemas.microsoft.com/SMI/2016/WindowsSettings']"
    )

    if (-not $dpiAware -or $dpiAware.InnerText.Trim() -ne "true/pm") {
        throw "FBE.exe не помечен legacy-fallback для per-monitor DPI."
    }
    if (-not $dpiAwareness -or $dpiAwareness.InnerText.Trim() -ne "PerMonitorV2,PerMonitor") {
        throw "FBE.exe не объявляет Per-Monitor V2 DPI awareness."
    }
}
finally {
    Remove-Item -LiteralPath $manifestPath -Force -ErrorAction SilentlyContinue
}

Write-Host "Проверка релиза для версии $expectedVersion прошла успешно."

