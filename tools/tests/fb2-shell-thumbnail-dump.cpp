#include <windows.h>
#include <shobjidl.h>
#include <thumbcache.h>
#include <string>
#include <vector>
#include <iostream>
#include <cstring>

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

static bool SaveBitmapAsBmp(HBITMAP bitmap, const wchar_t* outputPath)
{
    if (bitmap == nullptr || outputPath == nullptr)
        return false;

    BITMAP bitmapInfo = {};
    if (::GetObject(bitmap, sizeof(bitmapInfo), &bitmapInfo) != sizeof(bitmapInfo))
        return false;

    HDC screenDc = ::GetDC(nullptr);
    if (screenDc == nullptr)
        return false;

    HDC sourceDc = ::CreateCompatibleDC(screenDc);
    HDC targetDc = ::CreateCompatibleDC(screenDc);
    if (sourceDc == nullptr || targetDc == nullptr) {
        if (sourceDc != nullptr)
            ::DeleteDC(sourceDc);
        if (targetDc != nullptr)
            ::DeleteDC(targetDc);
        ::ReleaseDC(nullptr, screenDc);
        return false;
    }

    BITMAPINFOHEADER dibHeader = {};
    dibHeader.biSize = sizeof(dibHeader);
    dibHeader.biWidth = bitmapInfo.bmWidth;
    dibHeader.biHeight = -bitmapInfo.bmHeight;
    dibHeader.biPlanes = 1;
    dibHeader.biBitCount = 24;
    dibHeader.biCompression = BI_RGB;

    BITMAPINFO dibInfo = {};
    dibInfo.bmiHeader = dibHeader;

    void* targetBits = nullptr;
    HBITMAP targetBitmap = ::CreateDIBSection(screenDc, &dibInfo, DIB_RGB_COLORS, &targetBits, nullptr, 0);
    if (targetBitmap == nullptr || targetBits == nullptr) {
        ::DeleteDC(sourceDc);
        ::DeleteDC(targetDc);
        ::ReleaseDC(nullptr, screenDc);
        return false;
    }

    HGDIOBJ oldSourceBitmap = ::SelectObject(sourceDc, bitmap);
    HGDIOBJ oldTargetBitmap = ::SelectObject(targetDc, targetBitmap);

    RECT targetRect = { 0, 0, bitmapInfo.bmWidth, bitmapInfo.bmHeight };
    HBRUSH backgroundBrush = ::CreateSolidBrush(RGB(255, 255, 255));
    if (backgroundBrush == nullptr || ::FillRect(targetDc, &targetRect, backgroundBrush) == 0) {
        if (backgroundBrush != nullptr)
            ::DeleteObject(backgroundBrush);
        ::SelectObject(sourceDc, oldSourceBitmap);
        ::SelectObject(targetDc, oldTargetBitmap);
        ::DeleteObject(targetBitmap);
        ::DeleteDC(sourceDc);
        ::DeleteDC(targetDc);
        ::ReleaseDC(nullptr, screenDc);
        return false;
    }
    ::DeleteObject(backgroundBrush);

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    BOOL blitOk = ::GdiAlphaBlend(
        targetDc,
        0,
        0,
        bitmapInfo.bmWidth,
        bitmapInfo.bmHeight,
        sourceDc,
        0,
        0,
        bitmapInfo.bmWidth,
        bitmapInfo.bmHeight,
        blend);

    if (!blitOk) {
        blitOk = ::BitBlt(
            targetDc,
            0,
            0,
            bitmapInfo.bmWidth,
            bitmapInfo.bmHeight,
            sourceDc,
            0,
            0,
            SRCCOPY);
    }

    ::SelectObject(sourceDc, oldSourceBitmap);
    ::SelectObject(targetDc, oldTargetBitmap);
    ::DeleteDC(sourceDc);
    ::DeleteDC(targetDc);
    ::ReleaseDC(nullptr, screenDc);

    if (!blitOk) {
        ::DeleteObject(targetBitmap);
        return false;
    }

    const size_t stride = static_cast<size_t>(((bitmapInfo.bmWidth * 24) + 31) / 32) * 4;
    const size_t pixelBytes = stride * static_cast<size_t>(bitmapInfo.bmHeight);
    std::vector<BYTE> pixels(pixelBytes);
    std::memcpy(pixels.data(), targetBits, pixelBytes);
    ::DeleteObject(targetBitmap);

    BITMAPFILEHEADER fileHeader = {};
    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = static_cast<DWORD>(fileHeader.bfOffBits + pixelBytes);

    HANDLE fileHandle = ::CreateFileW(
        outputPath,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE)
        return false;

    DWORD bytesWritten = 0;
    const bool ok =
        ::WriteFile(fileHandle, &fileHeader, sizeof(fileHeader), &bytesWritten, nullptr) != FALSE &&
        bytesWritten == sizeof(fileHeader) &&
        ::WriteFile(fileHandle, &dibHeader, sizeof(dibHeader), &bytesWritten, nullptr) != FALSE &&
        bytesWritten == sizeof(dibHeader) &&
        ::WriteFile(fileHandle, pixels.data(), static_cast<DWORD>(pixels.size()), &bytesWritten, nullptr) != FALSE &&
        bytesWritten == pixels.size();

    ::CloseHandle(fileHandle);
    return ok;
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 5) {
        std::wcerr << L"Использование: fb2-shell-thumbnail-dump.exe <fb2-файл> <output-bmp> <size> <flags>\n";
        return 10;
    }

    const int requestedSize = _wtoi(argv[3]);
    if (requestedSize <= 0) {
        std::wcerr << L"Некорректный размер thumbnail: " << argv[3] << L"\n";
        return 18;
    }

    const int requestedFlags = _wtoi(argv[4]);
    if (requestedFlags < 0) {
        std::wcerr << L"Некорректные флаги thumbnail cache: " << argv[4] << L"\n";
        return 19;
    }

    const HRESULT initHr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initHr))
        return PrintFailure(L"CoInitializeEx", initHr, 11);

    IShellItem* shellItem = nullptr;
    HRESULT hr = ::SHCreateItemFromParsingName(argv[1], nullptr, IID_PPV_ARGS(&shellItem));
    if (FAILED(hr)) {
        ::CoUninitialize();
        return PrintFailure(L"SHCreateItemFromParsingName(IShellItem)", hr, 12);
    }

    IThumbnailCache* thumbnailCache = nullptr;
    hr = ::CoCreateInstance(CLSID_LocalThumbnailCache, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&thumbnailCache));
    if (FAILED(hr)) {
        shellItem->Release();
        ::CoUninitialize();
        return PrintFailure(L"CoCreateInstance(CLSID_LocalThumbnailCache)", hr, 13);
    }

    ISharedBitmap* sharedBitmap = nullptr;
    WTS_CACHEFLAGS cacheFlags = WTS_DEFAULT;
    WTS_THUMBNAILID thumbnailId = {};
    hr = thumbnailCache->GetThumbnail(
        shellItem,
        requestedSize,
        static_cast<WTS_FLAGS>(requestedFlags),
        &sharedBitmap,
        &cacheFlags,
        &thumbnailId);
    if (FAILED(hr)) {
        thumbnailCache->Release();
        shellItem->Release();
        ::CoUninitialize();
        return PrintFailure(L"IThumbnailCache::GetThumbnail(WTS_EXTRACT)", hr, 14);
    }

    HBITMAP bitmap = nullptr;
    hr = sharedBitmap->GetSharedBitmap(&bitmap);
    if (FAILED(hr)) {
        sharedBitmap->Release();
        thumbnailCache->Release();
        shellItem->Release();
        ::CoUninitialize();
        return PrintFailure(L"ISharedBitmap::GetSharedBitmap", hr, 15);
    }

    BITMAP bitmapInfo = {};
    if (::GetObject(bitmap, sizeof(bitmapInfo), &bitmapInfo) != sizeof(bitmapInfo)) {
        if (bitmap != nullptr)
            ::DeleteObject(bitmap);
        sharedBitmap->Release();
        thumbnailCache->Release();
        shellItem->Release();
        ::CoUninitialize();
        std::wcerr << L"Не удалось прочитать размер bitmap.\n";
        return 16;
    }

    if (!SaveBitmapAsBmp(bitmap, argv[2])) {
        if (bitmap != nullptr)
            ::DeleteObject(bitmap);
        sharedBitmap->Release();
        thumbnailCache->Release();
        shellItem->Release();
        ::CoUninitialize();
        return PrintWin32Failure(L"SaveBitmapAsBmp", ::GetLastError(), 17);
    }

    std::wcout << L"Дамп thumbnail сохранён: " << argv[2]
               << L" (" << bitmapInfo.bmWidth << L"x" << bitmapInfo.bmHeight
               << L", requested=" << requestedSize
               << L", requestedFlags=0x" << std::hex << requestedFlags << std::dec
               << L", cacheFlags=0x" << std::hex << static_cast<unsigned int>(cacheFlags) << std::dec << L")\n";

    if (bitmap != nullptr)
        ::DeleteObject(bitmap);
    sharedBitmap->Release();
    thumbnailCache->Release();
    shellItem->Release();
    ::CoUninitialize();
    return 0;
}
