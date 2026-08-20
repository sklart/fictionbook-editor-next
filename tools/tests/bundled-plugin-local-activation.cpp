#include <windows.h>
#include <objbase.h>
#include <iostream>

typedef HRESULT (STDAPICALLTYPE* DllGetClassObjectFn)(REFCLSID, REFIID, LPVOID*);

static int Failure(const wchar_t* stage, HRESULT code, int exitCode)
{
    std::wcerr << stage << L": 0x" << std::hex << code << std::dec << L"\n";
    return exitCode;
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 3) {
        std::wcerr << L"Использование: bundled-plugin-local-activation.exe <плагин.dll> <CLSID>\n";
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
    instance->Release();
    ::FreeLibrary(module);
    std::wcout << L"Локальная активация прошла: " << argv[1] << L"\n";
    return 0;
}
