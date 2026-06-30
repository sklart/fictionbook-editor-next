#include <windows.h>
#include <atlstr.h>
#include <iostream>
#include <vector>

#include "..\..\src\fbe\Fb2CoverImage.h"
#include "..\..\src\fbe\Fb2CoverThumbnail.h"
#include "..\..\src\fbe\atlimage.h"

ATL::CImage::CInitGDIPlus ATL::CImage::s_initGDIPlus;
ATL::CImage::CDCCache ATL::CImage::s_cache;

namespace {

bool ExpectTrue(const wchar_t* name, bool value)
{
    if (value)
        return true;

    std::wcerr << L"Проверка условия '" << name << L"' не прошла.\n";
    return false;
}

bool ExpectEqual(const wchar_t* name, int actual, int expected)
{
    if (actual == expected)
        return true;

    std::wcerr
        << L"Проверка числа '" << name << L"' не прошла. Ожидалось: "
        << expected << L", получено: " << actual << L".\n";
    return false;
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2) {
        std::wcerr << L"Использование: fb2-cover-thumbnail-smoke.exe <валидный-fb2>\n";
        return 10;
    }

    const HRESULT initHr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initHr)) {
        std::wcerr << L"Ошибка CoInitializeEx: 0x" << std::hex << initHr << std::dec << L"\n";
        return 11;
    }

    FB2CoverImage::CoverImage coverImage;
    CString errorMessage;
    if (!FB2CoverImage::TryRead(argv[1], coverImage, &errorMessage)) {
        std::wcerr << L"Не удалось прочитать обложку FB2: "
                   << static_cast<const wchar_t*>(errorMessage) << L"\n";
        ::CoUninitialize();
        return 1;
    }

    FB2CoverThumbnail::DecodedImage decodedImage;
    CString decodeError;
    if (!FB2CoverThumbnail::TryDecode(coverImage.bytes, decodedImage, &decodeError)) {
        std::wcerr << L"Не удалось декодировать обложку через системный image-стек: "
                   << static_cast<const wchar_t*>(decodeError) << L"\n";
        ::CoUninitialize();
        return 2;
    }

    bool success = true;
    success = ExpectTrue(L"bitmap.notNull", decodedImage.bitmap != nullptr) && success;
    success = ExpectEqual(L"width", decodedImage.width, 1) && success;
    success = ExpectEqual(L"height", decodedImage.height, 1) && success;

    std::vector<unsigned char> invalidBytes;
    invalidBytes.push_back(0x01);
    invalidBytes.push_back(0x02);
    invalidBytes.push_back(0x03);
    invalidBytes.push_back(0x04);

    FB2CoverThumbnail::DecodedImage invalidImage;
    CString invalidError;
    const bool invalidOk = FB2CoverThumbnail::TryDecode(invalidBytes, invalidImage, &invalidError);
    success = ExpectTrue(L"invalidImage.mustFail", !invalidOk) && success;
    success = ExpectTrue(L"invalidImage.errorMessage", !invalidError.IsEmpty()) && success;
    success = ExpectTrue(L"invalidImage.empty", invalidImage.IsEmpty()) && success;

    ::CoUninitialize();
    return success ? 0 : 3;
}
