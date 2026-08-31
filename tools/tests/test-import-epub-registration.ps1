# Проверяет 32-битную COM-регистрацию ImportEPUB и базовые COM-зависимости,
# необходимые для импорта EPUB из установленного или собранного FBE.

[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [string]$DllPath
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$repoRootPath = $repoRoot.Path

if (-not $DllPath) {
    $DllPath = Join-Path $repoRootPath "out\$Configuration\Plugins\ImportEPUB.dll"
}

$DllPath = [IO.Path]::GetFullPath($DllPath)
if (-not (Test-Path -LiteralPath $DllPath)) {
    throw "ImportEPUB.dll не найден: $DllPath"
}

$regsvr32 = Join-Path $env:WINDIR "SysWOW64\regsvr32.exe"
if (-not (Test-Path -LiteralPath $regsvr32)) {
    $regsvr32 = Join-Path $env:WINDIR "System32\regsvr32.exe"
}
if (-not (Test-Path -LiteralPath $regsvr32)) {
    throw "regsvr32.exe не найден."
}

Write-Host "Регистрирую ImportEPUB для smoke-теста: $DllPath"
$registration = Start-Process -FilePath $regsvr32 -ArgumentList @('/s', $DllPath) -Wait -PassThru
if ($registration.ExitCode -ne 0) {
    throw "Регистрация ImportEPUB.dll завершилась с кодом $($registration.ExitCode)."
}

$importEpubClsid = '{3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}'
$perUserInprocPath = "Software\\Classes\\CLSID\\$importEpubClsid\\InprocServer32"
$currentUser32 = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
    [Microsoft.Win32.RegistryHive]::CurrentUser,
    [Microsoft.Win32.RegistryView]::Registry32)
try {
    $perUserInprocKey = $currentUser32.OpenSubKey($perUserInprocPath)
    if (-not $perUserInprocKey) {
        throw "После regsvr32 отсутствует 32-битная пользовательская COM-регистрация $perUserInprocPath."
    }
    try {
        $registeredDll = [string]$perUserInprocKey.GetValue('')
        if (-not [string]::Equals($registeredDll, $DllPath, [StringComparison]::OrdinalIgnoreCase)) {
            throw "32-битная пользовательская COM-регистрация ImportEPUB указывает на '$registeredDll', ожидалось '$DllPath'."
        }
    }
    finally {
        $perUserInprocKey.Dispose()
    }
}
finally {
    $currentUser32.Dispose()
}

# На повышенном токене COM игнорирует HKCU\\Software\\Classes. GitHub-hosted
# runner запускает job именно так, хотя продукт намеренно использует per-user
# регистрацию. Для smoke-активации временно дублируем уже проверенную запись в
# 32-битный HKLM и удаляем её в finally ниже.
$temporaryMachineRegistration = $false
$machineClasses32 = $null
$isElevated = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if ($isElevated) {
    $machineClasses32 = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        [Microsoft.Win32.RegistryHive]::LocalMachine,
        [Microsoft.Win32.RegistryView]::Registry32).CreateSubKey('Software\\Classes')
    $machineClassPath = "CLSID\\$importEpubClsid"
    if ($machineClasses32.OpenSubKey($machineClassPath)) {
        throw "Отказ от smoke-теста: в HKLM уже зарегистрирован CLSID ImportEPUB $importEpubClsid."
    }

    $machineClassKey = $machineClasses32.CreateSubKey($machineClassPath)
    $machineClassKey.SetValue('', 'FBE EPUB Import Plugin', [Microsoft.Win32.RegistryValueKind]::String)
    $machineInprocKey = $machineClassKey.CreateSubKey('InprocServer32')
    $machineInprocKey.SetValue('', $DllPath, [Microsoft.Win32.RegistryValueKind]::String)
    $machineInprocKey.SetValue('ThreadingModel', 'Apartment', [Microsoft.Win32.RegistryValueKind]::String)
    $machineInprocKey.Dispose()
    $machineClassKey.Dispose()
    $temporaryMachineRegistration = $true
}

$testRoot = Join-Path $repoRoot "out\tests\import-epub-registration"
try {
    New-Item -ItemType Directory -Force -Path $testRoot | Out-Null

$sourcePath = Join-Path $testRoot "import-epub-registration-smoke.cpp"
$exePath = Join-Path $testRoot "import-epub-registration-smoke.exe"

@'
#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shldisp.h>
#include <stdio.h>

static int TestClsid(const wchar_t* name, REFCLSID clsid)
{
    IUnknown* object = nullptr;
    HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_IUnknown, reinterpret_cast<void**>(&object));
    wprintf(L"%s: 0x%08X\n", name, static_cast<unsigned int>(hr));
    if (object)
        object->Release();
    return SUCCEEDED(hr) ? 0 : 1;
}

static int TestProgId(const wchar_t* progId)
{
    CLSID clsid = CLSID_NULL;
    HRESULT hr = CLSIDFromProgID(progId, &clsid);
    wprintf(L"CLSIDFromProgID(%s): 0x%08X\n", progId, static_cast<unsigned int>(hr));
    if (FAILED(hr))
        return 1;
    return TestClsid(progId, clsid);
}

int wmain()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        wprintf(L"CoInitializeEx: 0x%08X\n", static_cast<unsigned int>(hr));
        return 2;
    }

    int failed = 0;
    // CLSID FBE Next: не пересекается с ImportEPUB старого FBE.
    const CLSID importEpubClsid = {0x3C19F5A2, 0x2EC8, 0x4EC7, {0xB7, 0xA9, 0xF4, 0x91, 0x0B, 0x4C, 0xDD, 0x82}};
    failed += TestClsid(L"ImportEPUB plugin", importEpubClsid);
    failed += TestProgId(L"Msxml2.DOMDocument.6.0");
    failed += TestProgId(L"Msxml2.SAXXMLReader.6.0");
    failed += TestProgId(L"Msxml2.XMLSchemaCache.6.0");
    failed += TestProgId(L"Msxml2.MXXMLWriter.6.0");
    failed += TestClsid(L"CLSID_FileOpenDialog", CLSID_FileOpenDialog);
    failed += TestClsid(L"Shell.Application", CLSID_Shell);

    CoUninitialize();
    return failed == 0 ? 0 : 1;
}
'@ | Set-Content -LiteralPath $sourcePath -Encoding UTF8

    & (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64

    & cl.exe /nologo /EHsc /W3 "/Fe:$exePath" $sourcePath ole32.lib uuid.lib
    if ($LASTEXITCODE -ne 0) {
        throw "Сборка smoke-теста ImportEPUB завершилась с кодом $LASTEXITCODE."
    }

    & $exePath
    if ($LASTEXITCODE -ne 0) {
        throw "Smoke-тест 32-битной COM-регистрации ImportEPUB завершился с кодом $LASTEXITCODE."
    }
}
finally {
    if ($temporaryMachineRegistration) {
        $machineClasses32.DeleteSubKeyTree("CLSID\\$importEpubClsid", $false)
    }
    if ($machineClasses32) {
        $machineClasses32.Dispose()
    }
}

Write-Host "Smoke-тест 32-битной COM-регистрации ImportEPUB прошёл успешно."
