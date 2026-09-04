#pragma once

#include <cwctype>
#include <string>

inline std::wstring FbeExtractXmlDeclarationEncoding(const std::wstring& xml)
{
	const size_t declarationStart = xml.find(L"<?xml");
	if (declarationStart == std::wstring::npos) return std::wstring();
	const size_t declarationEnd = xml.find(L"?>", declarationStart + 5);
	if (declarationEnd == std::wstring::npos) return std::wstring();

	size_t position = declarationStart + 5;
	while (position < declarationEnd)
	{
		while (position < declarationEnd && iswspace(xml[position])) ++position;
		const size_t nameStart = position;
		while (position < declarationEnd && (iswalnum(xml[position]) || xml[position] == L'_' || xml[position] == L'-')) ++position;
		if (nameStart == position) return std::wstring();
		std::wstring name = xml.substr(nameStart, position - nameStart);
		for (size_t i = 0; i < name.length(); ++i) name[i] = static_cast<wchar_t>(towlower(name[i]));
		while (position < declarationEnd && iswspace(xml[position])) ++position;
		if (position >= declarationEnd || xml[position++] != L'=') return std::wstring();
		while (position < declarationEnd && iswspace(xml[position])) ++position;
		if (position >= declarationEnd || (xml[position] != L'\'' && xml[position] != L'"')) return std::wstring();
		const wchar_t quote = xml[position++];
		const size_t valueStart = position;
		while (position < declarationEnd && xml[position] != quote) ++position;
		if (position == declarationEnd || valueStart == position) return std::wstring();
		const std::wstring value = xml.substr(valueStart, position++ - valueStart);
		if (name == L"encoding") return value;
	}
	return std::wstring();
}
