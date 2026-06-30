#include <windows.h>
#include <shobjidl.h>
#include <thumbcache.h>
#include <iostream>

static int PrintFailure(const wchar_t* stage, HRESULT hr, int exitCode)
{
    std::wcerr << stage << L": HRESULT 0x" << std::hex << hr << std::dec << L"\n";
    return exitCode;
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2) {
        std::wcerr << L"Использование: fb2-shell-thumbnail-prime-smoke.exe <fb2-файл>\n";
        return 10;
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
        256,
        static_cast<WTS_FLAGS>(WTS_FORCEEXTRACTION | WTS_EXTRACTINPROC),
        &sharedBitmap,
        &cacheFlags,
        &thumbnailId);
    if (FAILED(hr)) {
        thumbnailCache->Release();
        shellItem->Release();
        ::CoUninitialize();
        return PrintFailure(L"IThumbnailCache::GetThumbnail", hr, 14);
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
        std::wcerr << L"Не удалось прочитать размер возвращённого bitmap.\n";
        return 16;
    }

    std::wcout << L"Prime shell thumbnail smoke прошёл успешно: "
               << bitmapInfo.bmWidth << L"x" << bitmapInfo.bmHeight
               << L", cacheFlags=0x" << std::hex << static_cast<unsigned int>(cacheFlags) << std::dec << L".\n";

    if (bitmap != nullptr)
        ::DeleteObject(bitmap);
    sharedBitmap->Release();
    thumbnailCache->Release();
    shellItem->Release();
    ::CoUninitialize();
    return 0;
}
