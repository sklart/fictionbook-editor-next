#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include <thumbcache.h>
#include <objidl.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <initguid.h>

#include "..\..\src\fbshell\Fb2ThumbnailProvider.h"

// Дублируем CLSID локально, чтобы системный smoke-тест не зависел от линковки
// полноценной реализации provider и проверял именно COM-активацию через реестр.
DEFINE_GUID(CLSID_Fb2ThumbnailProvider,
0x4f99d1f0, 0x5d76, 0x4b9c, 0x9d, 0x3d, 0x9e, 0x6b, 0x8b, 0x4c, 0x7e, 0x31);

namespace {

bool ExpectTrue(const wchar_t* name, bool value)
{
    if (value)
        return true;

    std::wcerr << L"Проверка условия '" << name << L"' не прошла.\n";
    return false;
}

bool ExpectHResult(const wchar_t* name, HRESULT actual, HRESULT expected)
{
    if (actual == expected)
        return true;

    std::wcerr << L"Проверка HRESULT '" << name << L"' не прошла. Ожидалось 0x"
               << std::hex << expected << L", получено 0x" << actual << std::dec << L".\n";
    return false;
}

HRESULT ReadFileBytes(const wchar_t* filePath, std::vector<unsigned char>& bytes)
{
    bytes.clear();

    HANDLE fileHandle = ::CreateFileW(
        filePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE)
        return HRESULT_FROM_WIN32(::GetLastError());

    LARGE_INTEGER fileSize = {};
    if (!::GetFileSizeEx(fileHandle, &fileSize)) {
        const HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
        ::CloseHandle(fileHandle);
        return hr;
    }

    if (fileSize.QuadPart < 0 || fileSize.QuadPart > 32 * 1024 * 1024) {
        ::CloseHandle(fileHandle);
        return E_FAIL;
    }

    bytes.resize(static_cast<size_t>(fileSize.QuadPart));
    DWORD bytesRead = 0;
    const BOOL readOk = bytes.empty()
        ? TRUE
        : ::ReadFile(fileHandle, bytes.data(), static_cast<DWORD>(bytes.size()), &bytesRead, nullptr);
    const HRESULT result = (!readOk || bytesRead != bytes.size())
        ? HRESULT_FROM_WIN32(::GetLastError())
        : S_OK;
    ::CloseHandle(fileHandle);
    return result;
}

HRESULT CreateReadOnlyStreamFromFile(const wchar_t* filePath, IStream** stream)
{
    if (stream == nullptr)
        return E_POINTER;

    *stream = nullptr;

    std::vector<unsigned char> bytes;
    const HRESULT readHr = ReadFileBytes(filePath, bytes);
    if (FAILED(readHr))
        return readHr;

    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (memory == nullptr)
        return E_OUTOFMEMORY;

    void* buffer = ::GlobalLock(memory);
    if (buffer == nullptr) {
        const HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
        ::GlobalFree(memory);
        return hr;
    }

    if (!bytes.empty())
        std::memcpy(buffer, bytes.data(), bytes.size());
    ::GlobalUnlock(memory);

    const HRESULT hr = ::CreateStreamOnHGlobal(memory, TRUE, stream);
    if (FAILED(hr))
        ::GlobalFree(memory);
    return hr;
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2) {
        std::wcerr << L"Использование: fb2-thumbnail-provider-cocreate-smoke.exe <валидный-fb2>\n";
        return 10;
    }

    const HRESULT initHr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initHr)) {
        std::wcerr << L"Ошибка CoInitializeEx: 0x" << std::hex << initHr << std::dec << L"\n";
        return 11;
    }

    CComPtr<IInitializeWithStream> initializeWithStream;
    HRESULT hr = ::CoCreateInstance(
        CLSID_Fb2ThumbnailProvider,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&initializeWithStream));
    if (!ExpectHResult(L"CoCreateInstance.IInitializeWithStream", hr, S_OK)) {
        ::CoUninitialize();
        return 20;
    }

    CComPtr<IThumbnailProvider> thumbnailProvider;
    hr = initializeWithStream->QueryInterface(IID_PPV_ARGS(&thumbnailProvider));
    if (!ExpectHResult(L"QueryInterface.IThumbnailProvider", hr, S_OK)) {
        ::CoUninitialize();
        return 21;
    }

    CComPtr<IStream> stream;
    hr = CreateReadOnlyStreamFromFile(argv[1], &stream);
    if (!ExpectHResult(L"CreateReadOnlyStreamFromFile", hr, S_OK)) {
        ::CoUninitialize();
        return 22;
    }

    hr = initializeWithStream->Initialize(stream, STGM_READ);
    if (!ExpectHResult(L"Initialize.valid", hr, S_OK)) {
        ::CoUninitialize();
        return 23;
    }
    std::wcout << L"[checkpoint] Initialize -> S_OK\n";

    HBITMAP bitmap = nullptr;
    WTS_ALPHATYPE alphaType = WTSAT_UNKNOWN;
    hr = thumbnailProvider->GetThumbnail(256, &bitmap, &alphaType);
    if (!ExpectHResult(L"GetThumbnail.valid", hr, S_OK)) {
        ::CoUninitialize();
        return 24;
    }
    std::wcout << L"[checkpoint] GetThumbnail -> S_OK, bitmap=" << bitmap
               << L", alphaType=" << static_cast<int>(alphaType) << L"\n";
    if (!ExpectTrue(L"bitmap.valid.notNull", bitmap != nullptr)) {
        ::CoUninitialize();
        return 25;
    }
    std::wcout << L"[checkpoint] bitmap != nullptr\n";
    if (!ExpectTrue(L"alphaType.valid.known", alphaType == WTSAT_ARGB || alphaType == WTSAT_RGB)) {
        if (bitmap != nullptr)
            ::DeleteObject(bitmap);
        ::CoUninitialize();
        return 26;
    }
    std::wcout << L"[checkpoint] alphaType is valid\n";

    if (bitmap != nullptr) {
        std::wcout << L"[checkpoint] before DeleteObject(bitmap)\n";
        const BOOL deleteOk = ::DeleteObject(bitmap);
        std::wcout << L"[checkpoint] after DeleteObject(bitmap), result=" << deleteOk << L"\n";
    }

    std::wcout << L"[checkpoint] before releasing COM pointers\n";
    stream.Release();
    thumbnailProvider.Release();
    initializeWithStream.Release();
    std::wcout << L"[checkpoint] after releasing COM pointers\n";

    std::wcout << L"[checkpoint] before CoUninitialize\n";
    ::CoUninitialize();
    std::wcout << L"[checkpoint] after CoUninitialize\n";
    return 0;
}
