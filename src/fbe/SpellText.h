#pragma once

#include <atlstr.h>
#include <atlconv.h>

// Small text operations shared by CSpeller and its native regression tests.
// They deliberately contain no Hunspell or UI state.
inline LPCWSTR FbeSpellAlphaExceptions()
{
	return L"'\x2019\x02BC\u0301";
}

inline CString FbeNormalizeDictionaryApostrophes(CString word)
{
	word.Replace(L"\x2019", L"'");
	word.Replace(L"\x02BC", L"'");
	return word;
}

inline CString FbeRestoreSourceApostropheStyle(const CString& source, CString replacement)
{
	wchar_t apostrophe = L'\0';
	if (source.Find(L"\x2019") >= 0)
		apostrophe = 0x2019;
	else if (source.Find(L"\x02BC") >= 0)
		apostrophe = 0x02BC;
	if (apostrophe)
		replacement.Replace(L"'", CString(apostrophe));
	return replacement;
}

inline CStringA FbeEncodeDictionaryWord(const CString& word, UINT codePage)
{
	USES_CONVERSION;
	return CStringA(CT2A(word, codePage));
}

inline CString FbeDecodeDictionaryWord(const char* word, UINT codePage)
{
	USES_CONVERSION;
	return CString(CA2CT(word, codePage));
}
