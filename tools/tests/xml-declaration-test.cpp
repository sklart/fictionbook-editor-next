#include <iostream>
#include <string>
#include "XmlDeclaration.h"

static bool ExpectEncoding(const wchar_t* xml, const wchar_t* expected)
{
    const std::wstring actual = FbeExtractXmlDeclarationEncoding(xml);
    if (actual == expected) return true;
    std::wcerr << L"Expected '" << expected << L"', got '" << actual << L"'\n";
    return false;
}

static bool ExpectSource(const wchar_t* xml, const wchar_t* encoding, const wchar_t* expected)
{
    const std::wstring actual = FbeSetXmlDeclarationEncoding(xml, encoding);
    if(actual == expected) return true;
    std::wcerr << L"Expected source '" << expected << L"', got '" << actual << L"'\n";
    return false;
}

int wmain()
{
    const std::wstring firstSource = FbeSetXmlDeclarationEncoding(
        L"<?xml version='1.0' standalone=\"yes\"?><x/>", L"windows-1251");
    const std::wstring secondSource = FbeSetXmlDeclarationEncoding(firstSource, L"utf-8");
    return ExpectEncoding(L"<?xml version=\"1.0\" encoding=\"utf-8\"?><x/>", L"utf-8") &&
        ExpectEncoding(L"<?xml version='1.0' encoding='windows-1251'?><x/>", L"windows-1251") &&
        ExpectEncoding(L"<?xml version='1.0' ENCODING = 'utf-8' standalone=\"yes\"?><x/>", L"utf-8") &&
        ExpectEncoding(L"\xFEFF<?xml standalone=\"yes\" encoding=\"windows-1251\"?><x/>", L"windows-1251") &&
        ExpectEncoding(L"<!-- <?xml version=\"1.0\" encoding=\"windows-1251\"?> --><x/>", L"") &&
        ExpectEncoding(L"<x><?xml version=\"1.0\" encoding=\"windows-1251\"?></x>", L"") &&
        ExpectEncoding(L"<x/>", L"") &&
        ExpectEncoding(L"<?xml version=\"1.0\" encoding=\"utf-8\"", L"") &&
        ExpectEncoding(L"<?xml version=\"1.0\"?><x/>", L"") &&
        ExpectEncoding(L"<?xml version=\"1.0\" encoding=utf-8?><x/>", L"") &&
        ExpectSource(L"<?xml version='1.1' standalone=\"yes\"?><x/>", L"windows-1251", L"<?xml version='1.1' encoding=\"windows-1251\" standalone=\"yes\"?><x/>") &&
        ExpectSource(L"<?xml version=\"1.0\" encoding='windows-1251' standalone='no'?><x/>", L"utf-8", L"<?xml version=\"1.0\" encoding='utf-8' standalone='no'?><x/>") &&
        ExpectSource(L"\xFEFF<x/>", L"utf-8", L"\xFEFF<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n<x/>") &&
        firstSource == L"<?xml version='1.0' encoding=\"windows-1251\" standalone=\"yes\"?><x/>" &&
        secondSource == L"<?xml version='1.0' encoding=\"utf-8\" standalone=\"yes\"?><x/>" ? 0 : 1;
}
