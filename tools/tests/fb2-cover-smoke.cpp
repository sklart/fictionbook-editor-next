#include <windows.h>
#include <atlstr.h>
#include <iostream>
#include <vector>

#include "..\..\src\fbe\Fb2CoverImage.h"

namespace {

bool ExpectTrue(const wchar_t* name, bool value)
{
    if (value)
        return true;

    std::wcerr << L"Проверка условия '" << name << L"' не прошла.\n";
    return false;
}

bool ExpectEqual(const wchar_t* name, const CString& actual, const wchar_t* expected)
{
    if (actual == expected)
        return true;

    std::wcerr
        << L"Проверка поля '" << name << L"' не прошла. Ожидалось: '"
        << expected << L"', получено: '" << static_cast<const wchar_t*>(actual)
        << L"'.\n";
    return false;
}

bool ExpectPngSignature(const std::vector<unsigned char>& bytes)
{
    static const unsigned char kPngSignature[8] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
    };

    if (bytes.size() < sizeof(kPngSignature)) {
        std::wcerr << L"Байтов обложки меньше, чем нужно для сигнатуры PNG.\n";
        return false;
    }

    for (size_t index = 0; index < sizeof(kPngSignature); ++index) {
        if (bytes[index] != kPngSignature[index]) {
            std::wcerr << L"Сигнатура PNG у декодированной обложки не совпала.\n";
            return false;
        }
    }

    return true;
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 3) {
        std::wcerr << L"Использование: fb2-cover-smoke.exe <валидный-fb2> <битый-fb2>\n";
        return 10;
    }

    const HRESULT initHr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initHr)) {
        std::wcerr << L"Ошибка CoInitializeEx: 0x" << std::hex << initHr << std::dec << L"\n";
        return 11;
    }

    bool success = true;

    FB2CoverImage::CoverImage coverImage;
    CString errorMessage;
    if (!FB2CoverImage::TryRead(argv[1], coverImage, &errorMessage)) {
        std::wcerr << L"Не удалось прочитать валидную обложку FB2: "
                   << static_cast<const wchar_t*>(errorMessage) << L"\n";
        ::CoUninitialize();
        return 1;
    }

    success = ExpectEqual(L"href", coverImage.href, L"#cover.png") && success;
    success = ExpectEqual(L"binaryId", coverImage.binaryId, L"cover.png") && success;
    success = ExpectEqual(L"contentType", coverImage.contentType, L"image/png") && success;
    success = ExpectTrue(L"bytes.notEmpty", !coverImage.bytes.empty()) && success;
    success = ExpectPngSignature(coverImage.bytes) && success;

    FB2CoverImage::CoverImage brokenCoverImage;
    CString brokenErrorMessage;
    const bool brokenOk = FB2CoverImage::TryRead(argv[2], brokenCoverImage, &brokenErrorMessage);
    success = ExpectTrue(L"brokenCover.mustFail", !brokenOk) && success;
    success = ExpectTrue(L"brokenCover.errorMessage", !brokenErrorMessage.IsEmpty()) && success;
    success = ExpectTrue(L"brokenCover.bytesEmpty", brokenCoverImage.bytes.empty()) && success;

    if (!success) {
        ::CoUninitialize();
        return 2;
    }

    ::CoUninitialize();

    return 0;
}
