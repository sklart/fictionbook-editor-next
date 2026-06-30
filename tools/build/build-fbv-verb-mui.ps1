[CmdletBinding()]
param(
    [ValidateSet("Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$importVsDevEnvironmentScript = Join-Path $PSScriptRoot "Import-VsDevEnvironment.ps1"
$rcConfigPath = Join-Path $PSScriptRoot "fbv-verb-mui.rcconfig"
$outputRoot = Join-Path $repoRoot "out\$Configuration"
$workRoot = Join-Path $repoRoot "build\obj\fbv-verb-mui\$Configuration"
$muiModuleName = "FBVVerbResources.dll"
$muiVersionSuffix = "v2"

$languages = @(
    @{
        Name = "en-US"
        LangId = "0x0409"
        LanguageDirective = "LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US"
        Text = "Validate FB2 file"
    },
    @{
        Name = "ru-RU"
        LangId = "0x0419"
        LanguageDirective = "LANGUAGE LANG_RUSSIAN, SUBLANG_DEFAULT"
        Text = "Проверить FB2 на ошибки"
    },
    @{
        Name = "uk-UA"
        LangId = "0x0422"
        LanguageDirective = "LANGUAGE LANG_UKRAINIAN, SUBLANG_DEFAULT"
        Text = "Перевірити FB2 на помилки"
    },
    @{
        Name = "de-DE"
        LangId = "0x0407"
        LanguageDirective = "LANGUAGE LANG_GERMAN, SUBLANG_GERMAN"
        Text = "FB2-Datei prüfen"
    },
    @{
        Name = "fr-FR"
        LangId = "0x040C"
        LanguageDirective = "LANGUAGE LANG_FRENCH, SUBLANG_FRENCH"
        Text = "Vérifier le fichier FB2"
    },
    @{
        Name = "es-ES"
        LangId = "0x0C0A"
        LanguageDirective = "LANGUAGE LANG_SPANISH, SUBLANG_SPANISH_MODERN"
        Text = "Validar archivo FB2"
    },
    @{
        Name = "it-IT"
        LangId = "0x0410"
        LanguageDirective = "LANGUAGE LANG_ITALIAN, SUBLANG_ITALIAN"
        Text = "Verifica file FB2"
    },
    @{
        Name = "pl-PL"
        LangId = "0x0415"
        LanguageDirective = "LANGUAGE LANG_POLISH, SUBLANG_DEFAULT"
        Text = "Sprawdź plik FB2"
    },
    @{
        Name = "cs-CZ"
        LangId = "0x0405"
        LanguageDirective = "LANGUAGE LANG_CZECH, SUBLANG_DEFAULT"
        Text = "Zkontrolovat soubor FB2"
    },
    @{
        Name = "bg-BG"
        LangId = "0x0402"
        LanguageDirective = "LANGUAGE LANG_BULGARIAN, SUBLANG_DEFAULT"
        Text = "Провери FB2 файла"
    },
    @{
        Name = "pt-PT"
        LangId = "0x0816"
        LanguageDirective = "LANGUAGE LANG_PORTUGUESE, SUBLANG_PORTUGUESE"
        Text = "Validar ficheiro FB2"
    },
    @{
        Name = "nl-NL"
        LangId = "0x0413"
        LanguageDirective = "LANGUAGE LANG_DUTCH, SUBLANG_DUTCH"
        Text = "FB2-bestand valideren"
    }
)

function New-FbvVerbResourceSource {
    param(
        [Parameter(Mandatory)]
        [string]$RcPath,
        [Parameter(Mandatory)]
        [hashtable]$Language
    )

    $rcContent = @"
#include <winres.h>
#define IDS_SHELL_VALIDATE_VERB 109
$($Language.LanguageDirective)

STRINGTABLE
BEGIN
    IDS_SHELL_VALIDATE_VERB "$($Language.Text)"
END
"@

    [System.IO.File]::WriteAllText($RcPath, $rcContent, [System.Text.Encoding]::Unicode)
}

function Invoke-NativeTool {
    param(
        [Parameter(Mandatory)]
        [string]$FilePath,
        [Parameter(Mandatory)]
        [string[]]$ArgumentList
    )

    $output = & $FilePath @ArgumentList 2>&1
    $exitCode = $LASTEXITCODE

    if ($exitCode -ne 0) {
        if ($output) {
            $output | Write-Warning
        }
        throw "Команда завершилась с кодом ${exitCode}: $FilePath $($ArgumentList -join ' ')"
    }
}

& $importVsDevEnvironmentScript -Arch x86 -HostArch x64

$muirctPath = Get-ChildItem (Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin") -Recurse -Filter "muirct.exe" |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $muirctPath) {
    throw "Не найден muirct.exe."
}

$rcExePath = Get-Command rc.exe -ErrorAction Stop | Select-Object -ExpandProperty Source
$linkExePath = Get-Command link.exe -ErrorAction Stop | Select-Object -ExpandProperty Source

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
if (Test-Path -LiteralPath $workRoot) {
    Remove-Item -LiteralPath $workRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $workRoot -Force | Out-Null

$baseModulePath = Join-Path $outputRoot $muiModuleName
$discardModulePath = Join-Path $workRoot "discard.dll"

if (Test-Path -LiteralPath $baseModulePath) {
    Remove-Item -LiteralPath $baseModulePath -Force
}

foreach ($language in $languages) {
    $languageWorkDir = Join-Path $workRoot $language.Name
    $languageOutputDir = Join-Path $outputRoot $language.Name
    New-Item -ItemType Directory -Path $languageWorkDir -Force | Out-Null
    New-Item -ItemType Directory -Path $languageOutputDir -Force | Out-Null

    $sourceRcPath = Join-Path $languageWorkDir "FBVVerbResources.rc"
    $sourceResPath = Join-Path $languageWorkDir "FBVVerbResources.res"
    $sourceDllPath = Join-Path $languageWorkDir "FBVVerbResources.source.dll"
    $muiPath = Join-Path $languageOutputDir "$muiModuleName.mui"

    New-FbvVerbResourceSource -RcPath $sourceRcPath -Language $language

    Invoke-NativeTool -FilePath $rcExePath -ArgumentList @(
        "/nologo",
        "/fo$sourceResPath",
        $sourceRcPath
    )

    Invoke-NativeTool -FilePath $linkExePath -ArgumentList @(
        "/NOLOGO",
        "/DLL",
        "/NOENTRY",
        "/MACHINE:X86",
        "/OUT:$sourceDllPath",
        $sourceResPath
    )

    $lnOutputPath = if ($language.Name -eq "en-US") { $baseModulePath } else { $discardModulePath }

    Invoke-NativeTool -FilePath $muirctPath -ArgumentList @(
        "-q", $rcConfigPath,
        "-v", "2",
        "-x", $language.LangId,
        "-g", "0x0409",
        $sourceDllPath,
        $lnOutputPath,
        $muiPath
    )

    Invoke-NativeTool -FilePath $muirctPath -ArgumentList @(
        "-c", $baseModulePath,
        "-e", $muiPath
    )

    Write-Host "Подготовлен MUI-ресурс для $($language.Name): $muiPath"
}

Write-Host "Базовый MUI-модуль подготовлен: $baseModulePath"
Write-Host "Версионный suffix для indirect string: ;$muiVersionSuffix"
