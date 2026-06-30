#include <windows.h>
#include <propkey.h>
#include <propsys.h>
#include <propvarutil.h>
#include <shobjidl.h>
#include <atlbase.h>
#include <atlstr.h>

#include <iostream>
#include <string>

namespace {

void WriteUtf8Line(const ATL::CString& text)
{
    const int utf8Length = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        text,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (utf8Length <= 1)
        return;

    std::string utf8(static_cast<size_t>(utf8Length - 1), '\0');
    ::WideCharToMultiByte(
        CP_UTF8,
        0,
        text,
        -1,
        utf8.data(),
        utf8Length,
        nullptr,
        nullptr);

    std::cout << utf8 << std::endl;
}

void QueryProperty(IPropertyStore* propertyStore, const wchar_t* canonicalName)
{
    PROPERTYKEY propertyKey = PKEY_Null;
    HRESULT hr = ::PSGetPropertyKeyFromName(canonicalName, &propertyKey);
    if (FAILED(hr)) {
        ATL::CString line;
        line.Format(L"%ls = <каноническое имя не зарегистрировано> (0x%08X)", canonicalName, static_cast<unsigned int>(hr));
        WriteUtf8Line(line);
        return;
    }

    PROPVARIANT value;
    ::PropVariantInit(&value);
    hr = propertyStore->GetValue(propertyKey, &value);
    if (FAILED(hr)) {
        ATL::CString line;
        line.Format(L"%ls = <ошибка GetValue> (0x%08X)", canonicalName, static_cast<unsigned int>(hr));
        WriteUtf8Line(line);
        ::PropVariantClear(&value);
        return;
    }

    PWSTR displayValue = nullptr;
    hr = ::PropVariantToStringAlloc(value, &displayValue);
    if (FAILED(hr)) {
        ATL::CString line;
        line.Format(L"%ls = <ошибка PropVariantToStringAlloc> (0x%08X)", canonicalName, static_cast<unsigned int>(hr));
        WriteUtf8Line(line);
        ::PropVariantClear(&value);
        return;
    }

    const wchar_t* safeValue = (displayValue != nullptr && *displayValue != L'\0') ? displayValue : L"<пусто>";
    ATL::CString line;
    line.Format(L"%ls = %ls", canonicalName, safeValue);
    WriteUtf8Line(line);
    ::CoTaskMemFree(displayValue);
    ::PropVariantClear(&value);
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2) {
        std::wcerr << L"Использование: fb2-shell-properties-query.exe <fb2-file>\n";
        return 10;
    }

    HRESULT hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        std::wcerr << L"Ошибка CoInitializeEx: 0x" << std::hex << hr << std::dec << L"\n";
        return 11;
    }

    CComPtr<IPropertyStore> propertyStore;
    hr = ::SHGetPropertyStoreFromParsingName(
        argv[1],
        nullptr,
        GPS_DEFAULT,
        IID_PPV_ARGS(&propertyStore));
    if (FAILED(hr)) {
        std::wcerr << L"Ошибка SHGetPropertyStoreFromParsingName: 0x"
                   << std::hex << hr << std::dec << L"\n";
        ::CoUninitialize();
        return 12;
    }

    const wchar_t* canonicalNames[] = {
        L"System.Author",
        L"System.Title",
        L"System.Language",
        L"FBE.Genre",
        L"FBE.Sequence",
        L"FBE.DocumentVersion",
        L"FBE.DocumentDate",
        L"FBE.Keywords",
        L"FBE.DocumentId"
    };

    for (const wchar_t* canonicalName : canonicalNames)
        QueryProperty(propertyStore, canonicalName);

    ::CoUninitialize();
    return 0;
}
