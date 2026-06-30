<#
.SYNOPSIS
Проверяет регрессии ExportEPUB для EPUB 2 / XHTML 1.1.

.DESCRIPTION
Скрипт вызывает ExportEpubFileW из ExportEPUB.dll на специальной FB2-книге,
где есть прямой текст внутри annotation/cite/epigraph, file:// ссылка,
HTTP-ссылка с обратными слэшами и binary с переносами строк. Затем проверяет,
что EPUB не содержит запрещённых href, нормализует HTTP-ссылку, не вкладывает
блочные теги внутрь <a> и успешно кладёт декодированную обложку в архив.

Примечание: диагностические строки ниже намеренно оставлены ASCII-only, чтобы
сам smoke-тест не мог сломаться из-за локальной codepage PowerShell.
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
    throw "ExportEPUB.dll is Win32, but 32-bit Windows PowerShell was not found: $x86PowerShell"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

if ([string]::IsNullOrWhiteSpace($DllPath)) {
    $DllPath = Join-Path $repoRoot "out\$Configuration\ExportEPUB.dll"
}
$DllPath = (Resolve-Path -LiteralPath $DllPath).Path

if ([string]::IsNullOrWhiteSpace($FixturePath)) {
    $FixturePath = Join-Path $PSScriptRoot "fb2-export-epub-xhtml11-smoke.fb2"
}
$FixturePath = (Resolve-Path -LiteralPath $FixturePath).Path

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $testDir = Join-Path $repoRoot "out\tests\export-epub-xhtml11"
    New-Item -ItemType Directory -Path $testDir -Force | Out-Null
    $OutputPath = Join-Path $testDir "fb2-export-epub-xhtml11-smoke.epub"
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

public static class ExportEpubXhtml11Smoke
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
            throw new InvalidOperationException("ExportEPUB.dll load failed: " + Marshal.GetLastWin32Error());
        }

        try
        {
            IntPtr proc = GetProcAddress(module, "ExportEpubFileW");
            if (proc == IntPtr.Zero)
            {
                throw new InvalidOperationException("ExportEpubFileW was not found in ExportEPUB.dll.");
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

if (-not ("ExportEpubXhtml11Smoke" -as [type])) {
    Add-Type -TypeDefinition $nativeSource -Language CSharp
}

$flags = 0x0002 -bor 0x0004
$errorBuffer = [System.Text.StringBuilder]::new(4096)
$result = [ExportEpubXhtml11Smoke]::Export($DllPath, $FixturePath, $OutputPath, 2, [uint32]$flags, $errorBuffer)
if ($result -ne 0) {
    throw "ExportEpubFileW EPUB 2 failed with code $result. $($errorBuffer.ToString())"
}

if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
    throw "ExportEPUB did not create EPUB 2 file: $OutputPath"
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($OutputPath)
try {
    $texts = @{}
    $coverEntries = @()
    foreach ($entry in $zip.Entries) {
        if ($entry.FullName -match '\.(opf|xhtml|ncx|css)$') {
            $reader = [System.IO.StreamReader]::new($entry.Open(), [System.Text.Encoding]::UTF8, $true)
            try {
                $texts[$entry.FullName] = $reader.ReadToEnd()
            }
            finally {
                $reader.Dispose()
            }
        }
        if ($entry.FullName -match 'cover\.png$') {
            $coverEntries += $entry
        }
    }
}
finally {
    $zip.Dispose()
}

if ($coverEntries.Count -eq 0) {
    throw "cover.png was not stored in EPUB; base64 with whitespace may have failed."
}
if (($coverEntries | Where-Object { $_.Length -gt 0 }).Count -eq 0) {
    throw "cover.png entry is empty."
}

$combined = ($texts.GetEnumerator() | ForEach-Object { [string]$_.Value }) -join "`n"

if ($combined -match 'href\s*=\s*["'']file:') {
    throw "Forbidden file: href was found in EPUB."
}
if ($combined.Contains('file:///d:/xml/!new/index.html') -or $combined.Contains('file:///d:/xml/index.html')) {
    throw "Original local file:// link was preserved in EPUB."
}
if (-not $combined.Contains('http://www.kapustin.boom.ru/journal/arikaynen15.htm')) {
    throw "HTTP href with backslashes was not normalized."
}
if ($combined.Contains('http://www.kapustin.boom.ru\journal\arikaynen15.htm')) {
    throw "HTTP href with backslashes was preserved."
}
if ($combined -match '<a\b[^>]*>\s*<(?:div|blockquote|p)\b') {
    throw "Block element inside <a> was found."
}
if (-not $combined.Contains('inline-fragment')) {
    throw "p.inline-fragment was not found in EPUB 2 XHTML output."
}

Write-Host "ExportEPUB XHTML 1.1 / EPUB 2 smoke passed."
Write-Host "  DLL: $DllPath"
Write-Host "  FB2: $FixturePath"
Write-Host "  EPUB: $OutputPath"
