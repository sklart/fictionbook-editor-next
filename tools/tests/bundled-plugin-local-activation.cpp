#include <windows.h>
#include <objbase.h>
#include <iostream>
#include "../../src/fbe/FBE.h"

typedef HRESULT (STDAPICALLTYPE* DllGetClassObjectFn)(REFCLSID, REFIID, LPVOID*);

static int Failure(const wchar_t* stage, HRESULT code, int exitCode)
{
    std::wcerr << stage << L": 0x" << std::hex << code << std::dec << L"\n";
    return exitCode;
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 5) {
        std::wcerr << L"Использование: bundled-plugin-local-activation.exe <плагин.dll> <CLSID> <id> <Import|Export>\n";
        return 10;
    }

    CLSID clsid = {};
    HRESULT hr = ::CLSIDFromString(argv[2], &clsid);
    if (FAILED(hr)) return Failure(L"CLSIDFromString", hr, 11);

    HMODULE module = ::LoadLibraryW(argv[1]);
    if (module == NULL) return Failure(L"LoadLibraryW", HRESULT_FROM_WIN32(::GetLastError()), 12);
    FARPROC proc = ::GetProcAddress(module, "DllGetClassObject");
    if (proc == NULL) {
        ::FreeLibrary(module);
        return Failure(L"GetProcAddress(DllGetClassObject)", HRESULT_FROM_WIN32(::GetLastError()), 13);
    }

    DllGetClassObjectFn getClassObject = reinterpret_cast<DllGetClassObjectFn>(proc);
    IClassFactory* factory = NULL;
    hr = getClassObject(clsid, IID_IClassFactory, reinterpret_cast<void**>(&factory));
    if (FAILED(hr)) {
        ::FreeLibrary(module);
        return Failure(L"DllGetClassObject", hr, 14);
    }

    IUnknown* instance = NULL;
    hr = factory->CreateInstance(NULL, IID_IUnknown, reinterpret_cast<void**>(&instance));
    factory->Release();
    if (FAILED(hr)) {
        ::FreeLibrary(module);
        return Failure(L"IClassFactory::CreateInstance", hr, 15);
    }
    IFBEPluginInfo2* info = NULL;
    hr = instance->QueryInterface(IID_IFBEPluginInfo2, reinterpret_cast<void**>(&info));
    if (FAILED(hr)) { instance->Release(); ::FreeLibrary(module); return Failure(L"QueryInterface(IFBEPluginInfo2)", hr, 16); }
    BSTR id = NULL; ULONG apiVersion = 0;
    hr = info->GetPluginId(&id);
    if (SUCCEEDED(hr)) hr = info->GetApiVersion(&apiVersion);
    const bool validInfo = SUCCEEDED(hr) && id != NULL && wcscmp(id, argv[3]) == 0 && apiVersion == 2;
    ::SysFreeString(id); info->Release();
    if (!validInfo) { instance->Release(); ::FreeLibrary(module); return Failure(L"Plugin API metadata", FAILED(hr) ? hr : E_ACCESSDENIED, 17); }
    if (wcscmp(argv[4], L"Import") == 0) {
        IFBEImportPlugin2* api = NULL; hr = instance->QueryInterface(IID_IFBEImportPlugin2, reinterpret_cast<void**>(&api)); if (api) api->Release();
    } else if (wcscmp(argv[4], L"Export") == 0) {
        IFBEExportPlugin2* api = NULL; hr = instance->QueryInterface(IID_IFBEExportPlugin2, reinterpret_cast<void**>(&api)); if (api) api->Release();
    } else hr = E_INVALIDARG;
    if (FAILED(hr)) { instance->Release(); ::FreeLibrary(module); return Failure(L"QueryInterface(type-specific v2 API)", hr, 18); }
    instance->Release();
    ::FreeLibrary(module);
    std::wcout << L"Локальная активация прошла: " << argv[1] << L"\n";
    return 0;
}
