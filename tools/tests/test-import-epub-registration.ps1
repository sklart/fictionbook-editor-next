# Проверяет 32-битную COM-регистрацию ImportEPUB и базовые COM-зависимости,
# необходимые для импорта EPUB из установленного или собранного FBE.

[CmdletBinding()]
param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$testRoot = Join-Path $repoRoot "out\tests\import-epub-registration"
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
    const CLSID importEpubClsid = {0xD4B1B165, 0x4D93, 0x4F2D, {0x8C, 0x8A, 0x2D, 0x0C, 0x64, 0x94, 0x31, 0xA1}};
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

Write-Host "Smoke-тест 32-битной COM-регистрации ImportEPUB прошёл успешно."
