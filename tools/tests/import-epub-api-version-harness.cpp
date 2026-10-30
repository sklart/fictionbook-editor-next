#include <windows.h>
#include <oleauto.h>
#include <cstdio>
#include "../../src/import-epub/ImportEPUBApi.h"

using Build = HRESULT (WINAPI*)(LPCWSTR, const ImportEpubOptionsV1*, BSTR*, ImportEpubRuntimeStatsV1*, BSTR*);

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3 || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    HMODULE dll = LoadLibraryW(argv[1]);
    Build build = dll ? reinterpret_cast<Build>(GetProcAddress(dll, "ImportEPUB_BuildFb2XmlW")) : nullptr;
    if (!build) return 2;
    ImportEpubOptionsV1 options = {};
    options.cbSize = sizeof(options); options.version = IMPORT_EPUB_API_VERSION;
    options.importImages = options.importNotes = options.preserveLinks = options.validateResult = 1;
    BSTR output = nullptr, error = nullptr;
    HRESULT hr = build(argv[2], &options, &output, nullptr, &error);
    const bool v1 = SUCCEEDED(hr) && output != nullptr;
    SysFreeString(output); SysFreeString(error); output = error = nullptr;
    SYSTEM_INFO system = {}; GetSystemInfo(&system);
    BYTE* pages = static_cast<BYTE*>(VirtualAlloc(nullptr, system.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!pages || sizeof(options) > system.dwPageSize) return 3;
    DWORD oldProtect = 0;
    if (!VirtualProtect(pages + system.dwPageSize, system.dwPageSize, PAGE_NOACCESS, &oldProtect)) return 4;
    ImportEpubOptionsV1* guarded = reinterpret_cast<ImportEpubOptionsV1*>(pages + system.dwPageSize - sizeof(options));
    *guarded = options;
    hr = build(argv[2], guarded, &output, nullptr, &error);
    const bool guardedV1 = SUCCEEDED(hr) && output != nullptr;
    SysFreeString(output); SysFreeString(error); output = error = nullptr;
    VirtualFree(pages, 0, MEM_RELEASE);
    ImportEpubOptionsV2 extended = {};
    extended.base = options; extended.base.version = IMPORT_EPUB_API_VERSION_2;
    extended.base.cbSize = sizeof(ImportEpubOptionsV1); extended.separatorSubtitles = 1;
    const bool truncated = build(argv[2], &extended.base, &output, nullptr, &error) == E_INVALIDARG && !output && !error;
    extended.base.cbSize = sizeof(extended); extended.base.version = 999;
    const bool unknown = build(argv[2], &extended.base, &output, nullptr, &error) == E_INVALIDARG && !output && !error;
    extended.base.version = IMPORT_EPUB_API_VERSION_2;
    hr = build(argv[2], &extended.base, &output, nullptr, &error);
    const bool v2 = SUCCEEDED(hr) && output != nullptr;
    SysFreeString(output); SysFreeString(error);
    std::printf("v1=%d guarded-v1=%d truncated-v2=%d unknown-version=%d v2=%d\n", v1, guardedV1, truncated, unknown, v2);
    FreeLibrary(dll); CoUninitialize();
    return v1 && guardedV1 && truncated && unknown && v2 ? 0 : 1;
}
