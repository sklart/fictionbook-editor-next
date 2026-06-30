#include <windows.h>
#include <unknwn.h>
#include <objidl.h>
#include <thumbcache.h>
#include <shlwapi.h>
#include <iostream>
#include <initguid.h>

// {4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}
DEFINE_GUID(CLSID_Fb2ThumbnailProvider,
0x4f99d1f0, 0x5d76, 0x4b9c, 0x9d, 0x3d, 0x9e, 0x6b, 0x8b, 0x4c, 0x7e, 0x31);

typedef HRESULT (STDAPICALLTYPE* DllGetClassObjectFn)(REFCLSID, REFIID, LPVOID*);

static int PrintFailure(const wchar_t* stage, HRESULT hr, int exitCode)
{
    std::wcerr << stage << L": HRESULT 0x" << std::hex << hr << std::dec << L"\n";
    return exitCode;
}

static int PrintWin32Failure(const wchar_t* stage, DWORD errorCode, int exitCode)
{
    std::wcerr << stage << L": Win32 0x" << std::hex << errorCode << std::dec << L"\n";
    return exitCode;
}

static HRESULT CreateReadOnlyStreamFromFile(const wchar_t* filePath, IStream** stream)
{
    if (stream == nullptr)
        return E_POINTER;

    *stream = nullptr;
    return ::SHCreateStreamOnFileEx(filePath, STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, stream);
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 3) {
        std::wcerr << L"Использование: fbshell-loader-smoke.exe <FBShell.dll> <валидный-fb2>\n";
        return 10;
    }

    HRESULT initHr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initHr))
        return PrintFailure(L"CoInitializeEx", initHr, 9);

    HMODULE module = ::LoadLibraryW(argv[1]);
    if (module == nullptr) {
        ::CoUninitialize();
        return PrintWin32Failure(L"LoadLibraryW", ::GetLastError(), 11);
    }

    FARPROC proc = ::GetProcAddress(module, "DllGetClassObject");
    if (proc == nullptr) {
        const int code = PrintWin32Failure(L"GetProcAddress(DllGetClassObject)", ::GetLastError(), 12);
        ::FreeLibrary(module);
        ::CoUninitialize();
        return code;
    }

    DllGetClassObjectFn dllGetClassObject = reinterpret_cast<DllGetClassObjectFn>(proc);

    IClassFactory* classFactory = nullptr;
    HRESULT hr = dllGetClassObject(CLSID_Fb2ThumbnailProvider, IID_IClassFactory, reinterpret_cast<void**>(&classFactory));
    if (FAILED(hr)) {
        ::FreeLibrary(module);
        ::CoUninitialize();
        return PrintFailure(L"DllGetClassObject", hr, 13);
    }

    IInitializeWithStream* initializeWithStream = nullptr;
    hr = classFactory->CreateInstance(nullptr, __uuidof(IInitializeWithStream), reinterpret_cast<void**>(&initializeWithStream));
    if (FAILED(hr)) {
        classFactory->Release();
        ::FreeLibrary(module);
        ::CoUninitialize();
        return PrintFailure(L"IClassFactory::CreateInstance(IInitializeWithStream)", hr, 14);
    }

    IThumbnailProvider* thumbnailProvider = nullptr;
    hr = initializeWithStream->QueryInterface(__uuidof(IThumbnailProvider), reinterpret_cast<void**>(&thumbnailProvider));
    if (FAILED(hr)) {
        initializeWithStream->Release();
        classFactory->Release();
        ::FreeLibrary(module);
        ::CoUninitialize();
        return PrintFailure(L"QueryInterface(IThumbnailProvider)", hr, 15);
    }

    IStream* stream = nullptr;
    hr = CreateReadOnlyStreamFromFile(argv[2], &stream);
    if (FAILED(hr)) {
        thumbnailProvider->Release();
        initializeWithStream->Release();
        classFactory->Release();
        ::FreeLibrary(module);
        ::CoUninitialize();
        return PrintFailure(L"SHCreateStreamOnFileEx", hr, 16);
    }

    hr = initializeWithStream->Initialize(stream, STGM_READ);
    if (FAILED(hr)) {
        stream->Release();
        thumbnailProvider->Release();
        initializeWithStream->Release();
        classFactory->Release();
        ::FreeLibrary(module);
        ::CoUninitialize();
        return PrintFailure(L"IInitializeWithStream::Initialize", hr, 17);
    }

    HBITMAP bitmap = nullptr;
    WTS_ALPHATYPE alphaType = WTSAT_UNKNOWN;
    hr = thumbnailProvider->GetThumbnail(256, &bitmap, &alphaType);
    if (FAILED(hr)) {
        stream->Release();
        thumbnailProvider->Release();
        initializeWithStream->Release();
        classFactory->Release();
        ::FreeLibrary(module);
        ::CoUninitialize();
        return PrintFailure(L"IThumbnailProvider::GetThumbnail", hr, 18);
    }

    if (bitmap != nullptr)
        ::DeleteObject(bitmap);
    stream->Release();
    thumbnailProvider->Release();
    initializeWithStream->Release();
    classFactory->Release();
    ::FreeLibrary(module);
    ::CoUninitialize();

    std::wcout << L"Loader smoke прошёл успешно.\n";
    return 0;
}
