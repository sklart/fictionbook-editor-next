#define wmain ExistingImportRuntimeMain
#include "import-epub-v2-runtime-harness.cpp"
#undef wmain

int wmain(int argc, wchar_t** argv)
{
    if (argc != 4 || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    HMODULE module = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    GetClassObject get = module ? reinterpret_cast<GetClassObject>(GetProcAddress(module, "DllGetClassObject")) : nullptr;
    CLSID clsid = {}; if (!get || FAILED(CLSIDFromString(L"{3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}", &clsid))) return 2;
    IClassFactory* factory = nullptr; if (FAILED(get(clsid, IID_IClassFactory, reinterpret_cast<void**>(&factory)))) return 3;
    IFBEImportPlugin2* plugin = nullptr;
    const HRESULT created = factory->CreateInstance(nullptr, IID_IFBEImportPlugin2, reinterpret_cast<void**>(&plugin));
    factory->Release(); if (FAILED(created)) return 4;
    SetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", L"1");
    SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO", L"import-epub");
    SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH", argv[2]);
    Host* host = new Host; BSTR name = nullptr; IStream* stream = nullptr;
    const HRESULT imported = Call(plugin, host, &name, &stream);
    STATSTG stat = {}; bool ok = SUCCEEDED(imported) && stream && SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME)) && stat.cbSize.QuadPart > 0 && stat.cbSize.QuadPart < 1024 * 1024;
    std::vector<char> bytes(ok ? static_cast<size_t>(stat.cbSize.QuadPart) : 0); ULONG read = 0;
    LARGE_INTEGER start = {}; if (ok) ok = SUCCEEDED(stream->Seek(start, STREAM_SEEK_SET, nullptr)) && SUCCEEDED(stream->Read(bytes.data(), static_cast<ULONG>(bytes.size()), &read)) && read == bytes.size();
    HANDLE file = ok ? CreateFileW(argv[3], GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) : INVALID_HANDLE_VALUE;
    DWORD written = 0; if (file != INVALID_HANDLE_VALUE) { ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size(); CloseHandle(file); } else ok = false;
    if (stream) stream->Release(); SysFreeString(name); host->Release(); plugin->Release(); FreeLibrary(module); CoUninitialize();
    return ok ? 0 : 5;
}
