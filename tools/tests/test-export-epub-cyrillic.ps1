<#
.SYNOPSIS
Проверяет, что ExportEPUB корректно экспортирует кириллические строки в EPUB без mojibake.

.DESCRIPTION
Скрипт вызывает экспортируемую функцию ExportEpubFileW из собранного ExportEPUB.dll,
создаёт EPUB из тестовой кириллической FB2-книги и проверяет content.opf, nav.xhtml,
toc.ncx и XHTML-главы на ожидаемые русские строки и типичные признаки двойной/ошибочной
кодировки вида Рђ/Рћ/С‚.
#>

[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [string]$DllPath = "",
    [string]$FixturePath = "",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

if ([Environment]::Is64BitProcess) {
    $x86PowerShell = Join-Path $env:WINDIR "SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
    if (Test-Path -LiteralPath $x86PowerShell) {
        $arguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath, "-Configuration", $Configuration)
        if (-not [string]::IsNullOrWhiteSpace($DllPath)) { $arguments += @("-DllPath", $DllPath) }
        if (-not [string]::IsNullOrWhiteSpace($FixturePath)) { $arguments += @("-FixturePath", $FixturePath) }
        if (-not [string]::IsNullOrWhiteSpace($OutputPath)) { $arguments += @("-OutputPath", $OutputPath) }
        & $x86PowerShell @arguments
        exit $LASTEXITCODE
    }
    throw "ExportEPUB.dll собирается как Win32, но 32-битный Windows PowerShell не найден: $x86PowerShell"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

if ([string]::IsNullOrWhiteSpace($DllPath)) {
    $DllPath = Join-Path $repoRoot "out\$Configuration\ExportEPUB.dll"
}
$DllPath = (Resolve-Path -LiteralPath $DllPath).Path

if ([string]::IsNullOrWhiteSpace($FixturePath)) {
    $FixturePath = Join-Path $PSScriptRoot "fb2-metadata-cyrillic-smoke.fb2"
}
$FixturePath = (Resolve-Path -LiteralPath $FixturePath).Path

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $testDir = Join-Path $repoRoot "out\tests\export-epub-cyrillic"
    New-Item -ItemType Directory -Path $testDir -Force | Out-Null
    $OutputPath = Join-Path $testDir "fb2-metadata-cyrillic-smoke.epub"
}
else {
    $parent = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
}

if (Test-Path -LiteralPath $OutputPath) {
    Remove-Item -LiteralPath $OutputPath -Force
}

$nativeSource = @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class ExportEpubNativeSmoke
{
    [UnmanagedFunctionPointer(CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    private delegate int ExportEpubFileWDelegate(
        string inputFb2Path,
        string outputEpubPath,
        int epubVersion,
        uint flags,
        StringBuilder errorBuffer,
        uint errorBufferChars);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr LoadLibraryW(string lpFileName);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr hModule);

    public static int Export(string dllPath, string inputFb2Path, string outputEpubPath, int epubVersion, uint flags, StringBuilder errorBuffer)
    {
        IntPtr module = LoadLibraryW(dllPath);
        if (module == IntPtr.Zero)
        {
            throw new InvalidOperationException("Не удалось загрузить DLL ExportEPUB: " + Marshal.GetLastWin32Error());
        }

        try
        {
            IntPtr proc = GetProcAddress(module, "ExportEpubFileW");
            if (proc == IntPtr.Zero)
            {
                throw new InvalidOperationException("В ExportEPUB.dll не найдена функция ExportEpubFileW.");
            }

            ExportEpubFileWDelegate fn = Marshal.GetDelegateForFunctionPointer<ExportEpubFileWDelegate>(proc);
            return fn(inputFb2Path, outputEpubPath, epubVersion, flags, errorBuffer, (uint)errorBuffer.Capacity);
        }
        finally
        {
            FreeLibrary(module);
        }
    }
}
"@

if (-not ("ExportEpubNativeSmoke" -as [type])) {
    Add-Type -TypeDefinition $nativeSource -Language CSharp
}

# Флаги совпадают с ExportEpubFileW: NCX fallback + cover page + annotation page.
$flags = 0x0001 -bor 0x0002 -bor 0x0004
$errorBuffer = [System.Text.StringBuilder]::new(4096)
$result = [ExportEpubNativeSmoke]::Export($DllPath, $FixturePath, $OutputPath, 3, [uint32]$flags, $errorBuffer)
if ($result -ne 0) {
    throw "ExportEpubFileW завершился с кодом $result. $($errorBuffer.ToString())"
}

if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
    throw "ExportEPUB не создал файл: $OutputPath"
}
if ((Get-Item -LiteralPath $OutputPath).Length -eq 0) {
    throw "ExportEPUB создал пустой файл: $OutputPath"
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($OutputPath)
try {
    $texts = @{}
    foreach ($entry in $zip.Entries) {
        if ($entry.FullName -match '\.(opf|xhtml|ncx)$') {
            $reader = [System.IO.StreamReader]::new($entry.Open(), [System.Text.Encoding]::UTF8, $true)
            try {
                $texts[$entry.FullName] = $reader.ReadToEnd()
            }
            finally {
                $reader.Dispose()
            }
        }
    }
}
finally {
    $zip.Dispose()
}

foreach ($required in @("OEBPS/content.opf", "OEBPS/nav.xhtml", "OEBPS/toc.ncx", "OEBPS/text/chapter_001.xhtml")) {
    if (-not $texts.ContainsKey($required)) {
        throw "В EPUB отсутствует ожидаемый XML-файл: $required"
    }
}

$expectedFragments = @(
    "<dc:title>Кириллический smoke test для shell-свойств</dc:title>",
    "<title>Кириллический smoke test для shell-свойств</title>",
    "<title>Аннотация</title>",
    "<h1>Аннотация</h1>",
    "<navLabel><text>Аннотация</text></navLabel>"
)

$combined = ($texts.GetEnumerator() | ForEach-Object { [string]$_.Value }) -join "`n"
foreach ($fragment in $expectedFragments) {
    if (-not $combined.Contains($fragment)) {
        throw "В EPUB не найден ожидаемый кириллический фрагмент: $fragment"
    }
}

$mojibakeMarkers = @("Рђ", "Рћ", "РЅ", "Р°", "С‚", "С†", "СЊ", "Ð", "Ñ")
foreach ($marker in $mojibakeMarkers) {
    if ($combined.Contains($marker)) {
        throw "В EPUB найден признак mojibake '$marker'."
    }
}

Write-Host "Smoke-тест ExportEPUB для кириллицы прошёл успешно."
Write-Host "  DLL: $DllPath"
Write-Host "  FB2: $FixturePath"
Write-Host "  EPUB: $OutputPath"
