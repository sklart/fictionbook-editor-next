[CmdletBinding()]
param(
    [string]$Configuration = "Release",

    [ValidateSet("Modern", "Win7")]
    [string]$CompatibilityTarget = "Modern",

    [string]$PlatformToolset,

    [string]$BatchOutputDirectory,

    [switch]$SkipUpdateManifest,

    # Исходники, словари и общие статические контракты проверяются один раз
    # на Modern-этапе; Win7 повторяет только проверки своих бинарников.
    [switch]$SkipCommonChecks
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outputDir = Join-Path $repoRoot "out\$Configuration"
$batchOutputDir = if ($BatchOutputDirectory) {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BatchOutputDirectory)
} else {
    $outputDir
}
$batchNames = @("ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe", "ExportDOCXBatch.pdb", "ExportEPUBBatch.pdb", "ImportEPUBBatch.pdb")
function Get-ReleaseOutputPath([string]$Name) {
    $directory = if ($Name -in $batchNames) { $batchOutputDir } else { $outputDir }
    return Join-Path $directory $Name
}
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
    "Scintilla.dll",
    "Lexilla.dll"
)
$forbiddenFiles = @(
    "pcre.dll",
    "res_rus.dll",
    "res_ukr.dll"
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
    "Lang\\ru-RU\\res_rus.pdb",
    "Lang\\uk-UA\\res_ukr.pdb"
)

if (-not $SkipCommonChecks) {
& (Join-Path $repoRoot "tools\tests\test-source-safety.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-line-number-margin.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-updateui-notification.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-scintilla-modern-features.ps1")
& (Join-Path $repoRoot "tools\tests\test-fb2-schema-metadata.ps1")
& (Join-Path $repoRoot "tools\tests\test-fb2-source-structural-context.ps1") -CompatibilityTarget Modern
& (Join-Path $repoRoot "tools\tests\test-fb2-source-autocomplete.ps1") -CompatibilityTarget Modern
& (Join-Path $repoRoot "tools\tests\test-source-eol-annotations.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-special-representations.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-allocate-lines.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-full-process-benchmark.ps1")
& (Join-Path $repoRoot "tools\tests\test-words-ownerdata-stress.ps1")
& (Join-Path $repoRoot "tools\tests\test-editor-runtime-fingerprint.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-body-source-selection-transfer.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-table-visual-mode.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-table-production-roundtrip.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
& (Join-Path $repoRoot "tools\tests\test-fbe-table-production-roundtrip.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -Huge
& (Join-Path $repoRoot "tools\tests\test-fbe-table-failure-safety.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
& (Join-Path $repoRoot "tools\tests\test-fbe-script-error-diagnostics.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-binary-serialization.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-binary-production-roundtrip.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
& (Join-Path $repoRoot "tools\tests\test-image-import-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-image-codec-build-contract.ps1")
$imageImportTestArguments = @{ Configuration = $Configuration }
if ($PlatformToolset) { $imageImportTestArguments.PlatformToolset = $PlatformToolset }
& (Join-Path $repoRoot "tools\tests\test-image-import-native.ps1") @imageImportTestArguments
& (Join-Path $repoRoot "tools\tests\test-archhandler-argv.ps1") -PlatformToolset $PlatformToolset
& (Join-Path $repoRoot "tools\tests\test-fb2-check-content-types-base64.ps1")
& (Join-Path $repoRoot "tools\tests\test-save-sections-safe-replacement.ps1")
& (Join-Path $repoRoot "tools\tests\test-hta-legacy-js.ps1")
if (-not $SkipUpdateManifest) {
    & (Join-Path $repoRoot "tools\tests\test-update-manifest.ps1")
}
& (Join-Path $repoRoot "tools\tests\test-spellcheck-dictionaries.ps1") -Configuration $Configuration
$pcre2TestArguments = @{
    Configuration = $Configuration
    UsePreparedPcre2 = $true
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
& (Join-Path $repoRoot "tools\tests\test-plugin-static-runtime.ps1")
& (Join-Path $repoRoot "tools\tests\test-plugin-per-user-registration.ps1")
# В исходниках ещё есть накопленный исторический набор строк, который будет
# переноситься в JSON-локализации поэтапно. В release-контуре аудит остаётся
# видимым, но не должен блокировать выпуск до фиксации отдельного эталона.
# Строгий режим -FailOnFindings используется в узких regression-fixture.
& (Join-Path $repoRoot "tools\localization\analyze-product-hardcoded-cyrillic.ps1")
& (Join-Path $repoRoot "tools\tests\test-product-hardcoded-cyrillic-audit.ps1")
& (Join-Path $repoRoot "tools\tests\test-release-notes-format.ps1")
& (Join-Path $repoRoot "tools\tests\test-plugin-localization-catalog.ps1")
& (Join-Path $repoRoot "tools\tests\test-app-localization-catalog.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-localization-resources.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbv-localization-resources.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-html-localization-resources.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-html-standalone.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-docx-localization-resources.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-epub-localization-resources.ps1")
& (Join-Path $repoRoot "tools\tests\test-import-epub-localization-resources.ps1")
& (Join-Path $repoRoot "tools\tests\test-localization-export.ps1")
& (Join-Path $repoRoot "tools\tests\test-localization-win32-resource-fragments.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-main-menu-catalog.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-main-menu-generated-resource.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-main-menu-connected-resource.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-secondary-menus.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-small-dialogs.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-rc-ui-literals-inventory.ps1")
& (Join-Path $repoRoot "tools\tests\test-localization-runtime-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-runtime-interface-language-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-next-isolation.ps1")
& (Join-Path $repoRoot "tools\tests\test-runtime-lang-export.ps1")
& (Join-Path $repoRoot "tools\tests\test-runtime-lang-output-layout.ps1") -Configuration $Configuration -OutputDirectory $outputDir
& (Join-Path $repoRoot "tools\tests\test-fbe-runtime-lang-overlay.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbv-runtime-lang-overlay.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-html-runtime-lang-overlay.ps1")
& (Join-Path $repoRoot "tools\tests\test-import-epub-runtime-lang-overlay.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-epub-runtime-lang-overlay.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-docx-runtime-lang-overlay.ps1")
& (Join-Path $repoRoot "tools\tests\test-language-packs-inventory.ps1")
& (Join-Path $repoRoot "tools\tests\test-nsis-language-pack-plan.ps1")
& (Join-Path $repoRoot "tools\tests\test-nsis-installer-language-fallbacks.ps1")
& (Join-Path $repoRoot "tools\tests\test-nsis-installer-catalog.ps1")
& (Join-Path $repoRoot "tools\tests\test-nsis-components-page-layout.ps1")
& (Join-Path $repoRoot "tools\tests\test-import-epub-registration.ps1") -Configuration $Configuration
}

& (Join-Path $repoRoot "tools\tests\test-scintilla.ps1") `
    -EditorRuntimeDirectory (Join-Path $repoRoot "out\editor-runtime\$CompatibilityTarget")

if ($CompatibilityTarget -eq "Win7") {
    $sharedWin7Files = @(
        "FBE.exe", "FBV.exe", "ExportHTML.dll", "ExportDOCX.dll", "ExportEPUB.dll",
        "ImportEPUB.dll", "ImportEPUBLunaSVG.dll", "FBShell.dll"
    )
    & (Join-Path $repoRoot "tools\tests\check-win7-imports.ps1") `
        -Configuration $Configuration `
        -OutputDirectory $outputDir `
        -IncludeNames $sharedWin7Files

    & (Join-Path $repoRoot "tools\tests\check-win7-imports.ps1") `
        -Configuration $Configuration `
        -OutputDirectory $batchOutputDir `
        -IncludeNames @("ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe")

    $win7EditorRuntimeDir = Join-Path $repoRoot "out\editor-runtime\Win7"
    & (Join-Path $repoRoot "tools\tests\check-win7-imports.ps1") `
        -Configuration $Configuration `
        -OutputDirectory $win7EditorRuntimeDir `
        -IncludeNames @("Scintilla.dll", "Lexilla.dll")
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

function Test-PeMachine {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][UInt16]$ExpectedMachine)

    $bytes = [IO.File]::ReadAllBytes($Path)
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
    $machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
    if ($machine -ne $ExpectedMachine) {
        throw "$Path имеет PE machine 0x$($machine.ToString('x4')); ожидалось 0x$($ExpectedMachine.ToString('x4'))."
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
    $path = Get-ReleaseOutputPath $name

    if (-not (Test-Path -LiteralPath $path)) {
        throw "Отсутствует обязательный результат сборки: $path"
    }

    Test-BinarySecurityFlags -Path $path `
        -RequireControlFlowGuard:($name -in $controlFlowGuardFiles)
}

foreach ($propertyHandler in @(
        @{ Platform = "Win32"; Machine = [UInt16]0x014c },
        @{ Platform = "x64"; Machine = [UInt16]0x8664 }
    )) {
        $directory = Join-Path $repoRoot "out\package\shell-build\$($propertyHandler.Platform)\$Configuration"
        $path = Join-Path $directory "FBShell.dll"
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Не найдена фактически поставляемая DLL property handler: $path"
        }
        Test-PeMachine -Path $path -ExpectedMachine $propertyHandler.Machine
        Test-BinarySecurityFlags -Path $path -RequireControlFlowGuard
        $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($path)
        if ($info.FileVersion -ne $expectedVersion -or $info.ProductVersion -ne $expectedVersion) {
            throw "$path имеет версии File='$($info.FileVersion)', Product='$($info.ProductVersion)'; ожидалось '$expectedVersion'."
        }
        if ($CompatibilityTarget -eq "Win7") {
            & (Join-Path $repoRoot "tools\tests\check-win7-imports.ps1") `
                -Configuration $Configuration `
                -OutputDirectory $directory `
                -IncludeNames @("FBShell.dll")
        }
    }

foreach ($name in @("Lang\\ru-RU\\res_rus.dll", "Lang\\uk-UA\\res_ukr.dll")) {
    $path = Get-ReleaseOutputPath $name
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Отсутствует обязательный результат сборки: $path"
    }

    Test-BinarySecurityFlags -Path $path
}

foreach ($name in $forbiddenFiles) {
    $path = Join-Path $outputDir $name
    if (Test-Path -LiteralPath $path) {
        throw "Устаревший runtime-файл не должен попадать в релиз: $path"
    }
}

foreach ($name in $requiredSymbols) {
    $path = Get-ReleaseOutputPath $name
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Отсутствуют обязательные debug symbols: $path"
    }
    if ((Get-Item -LiteralPath $path).Length -eq 0) {
        throw "Файл debug symbols пуст: $path"
    }
}

foreach ($name in @("FBE.exe", "FBV.exe", "ExportHTML.dll", "ExportDOCX.dll", "ExportEPUB.dll", "ImportEPUB.dll", "ImportEPUBLunaSVG.dll", "ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe", "FBShell.dll", "Lang\\ru-RU\\res_rus.dll", "Lang\\uk-UA\\res_ukr.dll")) {
    $path = Get-ReleaseOutputPath $name
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
    "Lang\\ru-RU\\res_rus.dll" = "FictionBook Editor Russian resources"
    "Lang\\uk-UA\\res_ukr.dll" = "FictionBook Editor Ukrainian resources"
}

foreach ($entry in $requiredFileDescriptions.GetEnumerator()) {
    $path = Get-ReleaseOutputPath $entry.Key
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
    "Scintilla.dll" = "5.6.5"
    "Lexilla.dll" = "5.5.2"
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
