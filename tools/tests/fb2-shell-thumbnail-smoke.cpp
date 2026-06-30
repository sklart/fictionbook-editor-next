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
        std::wcerr << L"Использование: fb2-shell-thumbnail-smoke.exe <fb2-файл>\n";
        return 10;
    }

    const HRESULT initHr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initHr))
        return PrintFailure(L"CoInitializeEx", initHr, 11);

    IShellItemImageFactory* imageFactory = nullptr;
    HRESULT hr = ::SHCreateItemFromParsingName(
        argv[1],
        nullptr,
        IID_PPV_ARGS(&imageFactory));
    if (FAILED(hr)) {
        ::CoUninitialize();
        return PrintFailure(L"SHCreateItemFromParsingName(IShellItemImageFactory)", hr, 12);
    }

    SIZE size = {};
    size.cx = 256;
    size.cy = 256;
    HBITMAP bitmap = nullptr;
    hr = imageFactory->GetImage(size, SIIGBF_BIGGERSIZEOK | SIIGBF_THUMBNAILONLY, &bitmap);
    if (FAILED(hr)) {
        imageFactory->Release();
        ::CoUninitialize();
        return PrintFailure(L"IShellItemImageFactory::GetImage", hr, 13);
    }

    BITMAP bitmapInfo = {};
    if (::GetObject(bitmap, sizeof(bitmapInfo), &bitmapInfo) != sizeof(bitmapInfo)) {
        if (bitmap != nullptr)
            ::DeleteObject(bitmap);
        imageFactory->Release();
        ::CoUninitialize();
        std::wcerr << L"Не удалось прочитать размер возвращённого bitmap.\n";
        return 14;
    }

    std::wcout << L"Shell thumbnail smoke прошёл успешно: "
               << bitmapInfo.bmWidth << L"x" << bitmapInfo.bmHeight << L".\n";

    if (bitmap != nullptr)
        ::DeleteObject(bitmap);
    imageFactory->Release();
    ::CoUninitialize();
    return 0;
}
