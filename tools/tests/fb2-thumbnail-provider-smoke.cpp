#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>
#include <iostream>
#include <vector>
#include <cstring>

#include "..\..\src\fbshell\Fb2ThumbnailProvider.h"

CComModule _Module;

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

bool ExpectEqualInt(const wchar_t* name, int actual, int expected)
{
    if (actual == expected)
        return true;

    std::wcerr << L"Проверка числа '" << name << L"' не прошла. Ожидалось "
               << expected << L", получено " << actual << L".\n";
    return false;
}

bool ExpectLessOrEqualInt(const wchar_t* name, int actual, int expectedMax)
{
    if (actual <= expectedMax)
        return true;

    std::wcerr << L"Проверка числа '" << name << L"' не прошла. Ожидалось значение <= "
               << expectedMax << L", получено " << actual << L".\n";
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

bool GetBitmapSize(HBITMAP bitmap, int& width, int& height)
{
    width = 0;
    height = 0;

    BITMAP bitmapInfo = {};
    if (::GetObject(bitmap, sizeof(bitmapInfo), &bitmapInfo) != sizeof(bitmapInfo))
        return false;

    width = bitmapInfo.bmWidth;
    height = bitmapInfo.bmHeight;
    return true;
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    const int requestedEdge = 256;

    if (argc != 7) {
        std::wcerr << L"Использование: fb2-thumbnail-provider-smoke.exe <png-fb2> <jpeg-fb2> <bmp-fb2> <битый-fb2> <без-coverpage-fb2> <без-binary-fb2>\n";
        return 10;
    }

    const HRESULT initHr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initHr)) {
        std::wcerr << L"Ошибка CoInitializeEx: 0x" << std::hex << initHr << std::dec << L"\n";
        return 11;
    }

    bool success = true;

    CComObject<Fb2ThumbnailProvider>* providerWithoutInit = nullptr;
    HRESULT hr = CComObject<Fb2ThumbnailProvider>::CreateInstance(&providerWithoutInit);
    if (FAILED(hr) || providerWithoutInit == nullptr) {
        std::wcerr << L"Не удалось создать экземпляр Fb2ThumbnailProvider: 0x"
                   << std::hex << hr << std::dec << L"\n";
        ::CoUninitialize();
        return 12;
    }

    HBITMAP bitmap = nullptr;
    WTS_ALPHATYPE alphaType = WTSAT_UNKNOWN;
    hr = providerWithoutInit->Initialize(nullptr, STGM_READ);
    success = ExpectHResult(L"Initialize.nullStream", hr, E_POINTER) && success;
    hr = providerWithoutInit->GetThumbnail(requestedEdge, nullptr, &alphaType);
    success = ExpectHResult(L"GetThumbnail.nullBitmap", hr, E_POINTER) && success;
    bitmap = nullptr;
    hr = providerWithoutInit->GetThumbnail(requestedEdge, &bitmap, nullptr);
    success = ExpectHResult(L"GetThumbnail.nullAlphaType", hr, E_POINTER) && success;
    hr = providerWithoutInit->GetThumbnail(requestedEdge, &bitmap, &alphaType);
    success = ExpectHResult(L"GetThumbnail.beforeInitialize", hr, E_UNEXPECTED) && success;
    success = ExpectTrue(L"bitmap.beforeInitialize.null", bitmap == nullptr) && success;
    delete providerWithoutInit;

    const wchar_t* validFixtures[3] = { argv[1], argv[2], argv[3] };
    const wchar_t* validLabels[3] = { L"png", L"jpeg", L"bmp" };
    for (int i = 0; i < 3; ++i) {
        CComPtr<IStream> validStream;
        hr = CreateReadOnlyStreamFromFile(validFixtures[i], &validStream);
        if (FAILED(hr)) {
            std::wcerr << L"Не удалось открыть валидный fixture как IStream (" << validLabels[i] << L"): 0x"
                       << std::hex << hr << std::dec << L"\n";
            ::CoUninitialize();
            return 13 + i;
        }

        CComObject<Fb2ThumbnailProvider>* validProvider = nullptr;
        hr = CComObject<Fb2ThumbnailProvider>::CreateInstance(&validProvider);
        if (FAILED(hr) || validProvider == nullptr) {
            std::wcerr << L"Не удалось создать provider для валидного файла (" << validLabels[i] << L"): 0x"
                       << std::hex << hr << std::dec << L"\n";
            ::CoUninitialize();
            return 20 + i;
        }

        ATL::CString initializeName;
        initializeName.Format(L"Initialize.valid.%s", validLabels[i]);
        hr = validProvider->Initialize(validStream, STGM_READ);
        success = ExpectHResult(initializeName, hr, S_OK) && success;

        ATL::CString initializeSecondName;
        initializeSecondName.Format(L"Initialize.secondCall.%s", validLabels[i]);
        hr = validProvider->Initialize(validStream, STGM_READ);
        success = ExpectHResult(initializeSecondName, hr, HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED)) && success;

        bitmap = nullptr;
        alphaType = WTSAT_UNKNOWN;
        ATL::CString getThumbnailName;
        getThumbnailName.Format(L"GetThumbnail.valid.%s", validLabels[i]);
        hr = validProvider->GetThumbnail(requestedEdge, &bitmap, &alphaType);
        success = ExpectHResult(getThumbnailName, hr, S_OK) && success;

        ATL::CString notNullName;
        notNullName.Format(L"bitmap.valid.notNull.%s", validLabels[i]);
        success = ExpectTrue(notNullName, bitmap != nullptr) && success;

        ATL::CString alphaName;
        alphaName.Format(L"alphaType.valid.known.%s", validLabels[i]);
        success = ExpectTrue(alphaName, alphaType == WTSAT_RGB) && success;

        int width = 0;
        int height = 0;
        ATL::CString sizeReadableName;
        sizeReadableName.Format(L"bitmap.valid.sizeReadable.%s", validLabels[i]);
        success = ExpectTrue(sizeReadableName, bitmap != nullptr && GetBitmapSize(bitmap, width, height)) && success;
        std::wcout << L"[thumbnail-provider-smoke] " << validLabels[i]
                   << L": " << width << L"x" << height
                   << L", requested=" << requestedEdge << L"\n";

        ATL::CString widthMaxName;
        widthMaxName.Format(L"bitmap.valid.widthMax.%s", validLabels[i]);
        success = ExpectLessOrEqualInt(widthMaxName, width, requestedEdge) && success;

        ATL::CString heightMaxName;
        heightMaxName.Format(L"bitmap.valid.heightMax.%s", validLabels[i]);
        success = ExpectLessOrEqualInt(heightMaxName, height, requestedEdge) && success;

        ATL::CString edgeName;
        edgeName.Format(L"bitmap.valid.maxEdge.%s", validLabels[i]);
        const int actualMaxEdge = width > height ? width : height;
        success = ExpectEqualInt(edgeName, actualMaxEdge, requestedEdge) && success;

        if (bitmap != nullptr)
            ::DeleteObject(bitmap);
        delete validProvider;
    }

    CComPtr<IStream> brokenStream;
    hr = CreateReadOnlyStreamFromFile(argv[4], &brokenStream);
    if (FAILED(hr)) {
        std::wcerr << L"Не удалось открыть битый fixture как IStream: 0x"
                   << std::hex << hr << std::dec << L"\n";
        ::CoUninitialize();
        return 15;
    }

    CComObject<Fb2ThumbnailProvider>* brokenProvider = nullptr;
    hr = CComObject<Fb2ThumbnailProvider>::CreateInstance(&brokenProvider);
    if (FAILED(hr) || brokenProvider == nullptr) {
        std::wcerr << L"Не удалось создать provider для битого файла: 0x"
                   << std::hex << hr << std::dec << L"\n";
        ::CoUninitialize();
        return 16;
    }

    hr = brokenProvider->Initialize(brokenStream, STGM_READ);
    success = ExpectHResult(L"Initialize.broken", hr, S_OK) && success;

    bitmap = nullptr;
    alphaType = WTSAT_UNKNOWN;
    hr = brokenProvider->GetThumbnail(requestedEdge, &bitmap, &alphaType);
    success = ExpectHResult(L"GetThumbnail.broken", hr, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT)) && success;
    success = ExpectTrue(L"bitmap.broken.null", bitmap == nullptr) && success;
    delete brokenProvider;

    CComPtr<IStream> missingCoverStream;
    hr = CreateReadOnlyStreamFromFile(argv[5], &missingCoverStream);
    if (FAILED(hr)) {
        std::wcerr << L"Не удалось открыть fixture без coverpage как IStream: 0x"
                   << std::hex << hr << std::dec << L"\n";
        ::CoUninitialize();
        return 17;
    }

    CComObject<Fb2ThumbnailProvider>* missingCoverProvider = nullptr;
    hr = CComObject<Fb2ThumbnailProvider>::CreateInstance(&missingCoverProvider);
    if (FAILED(hr) || missingCoverProvider == nullptr) {
        std::wcerr << L"Не удалось создать provider для файла без coverpage: 0x"
                   << std::hex << hr << std::dec << L"\n";
        ::CoUninitialize();
        return 18;
    }

    hr = missingCoverProvider->Initialize(missingCoverStream, STGM_READ);
    success = ExpectHResult(L"Initialize.missingCoverpage", hr, S_OK) && success;
    bitmap = nullptr;
    alphaType = WTSAT_UNKNOWN;
    hr = missingCoverProvider->GetThumbnail(requestedEdge, &bitmap, &alphaType);
    success = ExpectHResult(L"GetThumbnail.missingCoverpage", hr, HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) && success;
    success = ExpectTrue(L"bitmap.missingCoverpage.null", bitmap == nullptr) && success;
    delete missingCoverProvider;

    CComPtr<IStream> missingBinaryStream;
    hr = CreateReadOnlyStreamFromFile(argv[6], &missingBinaryStream);
    if (FAILED(hr)) {
        std::wcerr << L"Не удалось открыть fixture без binary как IStream: 0x"
                   << std::hex << hr << std::dec << L"\n";
        ::CoUninitialize();
        return 19;
    }

    CComObject<Fb2ThumbnailProvider>* missingBinaryProvider = nullptr;
    hr = CComObject<Fb2ThumbnailProvider>::CreateInstance(&missingBinaryProvider);
    if (FAILED(hr) || missingBinaryProvider == nullptr) {
        std::wcerr << L"Не удалось создать provider для файла без binary: 0x"
                   << std::hex << hr << std::dec << L"\n";
        ::CoUninitialize();
        return 20;
    }

    hr = missingBinaryProvider->Initialize(missingBinaryStream, STGM_READ);
    success = ExpectHResult(L"Initialize.missingBinary", hr, S_OK) && success;
    bitmap = nullptr;
    alphaType = WTSAT_UNKNOWN;
    hr = missingBinaryProvider->GetThumbnail(requestedEdge, &bitmap, &alphaType);
    success = ExpectHResult(L"GetThumbnail.missingBinary", hr, HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) && success;
    success = ExpectTrue(L"bitmap.missingBinary.null", bitmap == nullptr) && success;
    delete missingBinaryProvider;

    ::CoUninitialize();
    return success ? 0 : 3;
}

