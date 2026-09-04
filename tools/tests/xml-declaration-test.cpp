#include <iostream>
#include <string>
#include "XmlDeclaration.h"

static bool Expect(const wchar_t* xml, const wchar_t* expected)
{
    const std::wstring actual = FbeExtractXmlDeclarationEncoding(xml);
    if (actual == expected) return true;
    std::wcerr << L"Expected '" << expected << L"', got '" << actual << L"'\n";
    return false;
}

int wmain()
{
    return Expect(L"<?xml version=\"1.0\" encoding=\"utf-8\"?><x/>", L"utf-8") &&
        Expect(L"<?xml version='1.0' encoding='windows-1251'?><x/>", L"windows-1251") &&
        Expect(L"<?xml version='1.0' ENCODING = 'utf-8' standalone=\"yes\"?><x/>", L"utf-8") &&
        Expect(L"<?xml standalone=\"yes\" encoding=\"windows-1251\"?><x/>", L"windows-1251") &&
        Expect(L"<x/>", L"") &&
        Expect(L"<?xml version=\"1.0\" encoding=\"utf-8\"", L"") &&
        Expect(L"<?xml version=\"1.0\"?><x/>", L"") &&
        Expect(L"<?xml version=\"1.0\" encoding=utf-8?><x/>", L"") ? 0 : 1;
}
