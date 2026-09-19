#pragma once

#include <cwctype>
#include <string>

struct FbeXmlDeclarationRange
{
	size_t start = std::wstring::npos;
	size_t end = std::wstring::npos;
};

inline bool FbeXmlDeclarationNameEquals(const std::wstring& name, const wchar_t* expected)
{
	const size_t expectedLength = std::char_traits<wchar_t>::length(expected);
	if(name.length() != expectedLength) return false;
	for(size_t index = 0; index < expectedLength; ++index)
		if(towlower(name[index]) != expected[index]) return false;
	return true;
}

// XML declaration is permitted only before all document content.  A UTF-16
// BOM is represented by U+FEFF after the Source editor has decoded its UTF-8
// buffer, so retain it while looking for the declaration.
inline bool FbeFindLeadingXmlDeclaration(const std::wstring& xml, FbeXmlDeclarationRange& result)
{
	result = FbeXmlDeclarationRange();
	size_t start = !xml.empty() && xml[0] == L'\xFEFF' ? 1 : 0;
	if(start + 5 >= xml.length() || xml[start] != L'<' || xml[start + 1] != L'?' ||
		towlower(xml[start + 2]) != L'x' || towlower(xml[start + 3]) != L'm' || towlower(xml[start + 4]) != L'l' ||
		!iswspace(xml[start + 5])) return false;
	const size_t end = xml.find(L"?>", start + 5);
	if(end == std::wstring::npos) return false;
	result.start = start;
	result.end = end;
	return true;
}

inline bool FbeFindXmlDeclarationAttribute(const std::wstring& xml, const FbeXmlDeclarationRange& declaration,
	const wchar_t* expectedName, size_t& valueStart, size_t& valueLength, size_t* attributeStart = NULL)
{
	valueStart = std::wstring::npos;
	valueLength = 0;
	if(attributeStart) *attributeStart = std::wstring::npos;
	size_t position = declaration.start + 5;
	while(position < declaration.end)
	{
		while(position < declaration.end && iswspace(xml[position])) ++position;
		if(position == declaration.end) break;
		const size_t nameStart = position;
		while(position < declaration.end && (iswalnum(xml[position]) || xml[position] == L'_' || xml[position] == L'-')) ++position;
		if(nameStart == position) return false;
		const std::wstring name = xml.substr(nameStart, position - nameStart);
		while(position < declaration.end && iswspace(xml[position])) ++position;
		if(position >= declaration.end || xml[position++] != L'=') return false;
		while(position < declaration.end && iswspace(xml[position])) ++position;
		if(position >= declaration.end || (xml[position] != L'\'' && xml[position] != L'"')) return false;
		const wchar_t quote = xml[position++];
		const size_t attributeValueStart = position;
		while(position < declaration.end && xml[position] != quote) ++position;
		if(position == declaration.end) return false;
		const size_t attributeValueLength = position - attributeValueStart;
		++position;
		if(FbeXmlDeclarationNameEquals(name, expectedName))
		{
			valueStart = attributeValueStart;
			valueLength = attributeValueLength;
			if(attributeStart) *attributeStart = nameStart;
			return true;
		}
	}
	return false;
}

inline std::wstring FbeExtractXmlDeclarationEncoding(const std::wstring& xml)
{
	FbeXmlDeclarationRange declaration;
	size_t valueStart = std::wstring::npos;
	size_t valueLength = 0;
	if(!FbeFindLeadingXmlDeclaration(xml, declaration) ||
		!FbeFindXmlDeclarationAttribute(xml, declaration, L"encoding", valueStart, valueLength) || valueLength == 0)
		return std::wstring();
	return xml.substr(valueStart, valueLength);
}

inline std::wstring FbeSetXmlDeclarationEncoding(const std::wstring& xml, const std::wstring& encoding)
{
	if(encoding.empty()) return xml;
	FbeXmlDeclarationRange declaration;
	if(FbeFindLeadingXmlDeclaration(xml, declaration))
	{
		size_t valueStart = std::wstring::npos;
		size_t valueLength = 0;
		if(FbeFindXmlDeclarationAttribute(xml, declaration, L"encoding", valueStart, valueLength))
		{
			std::wstring result(xml);
			result.replace(valueStart, valueLength, encoding);
			return result;
		}
		std::wstring result(xml);
		size_t standaloneStart = std::wstring::npos;
		if(FbeFindXmlDeclarationAttribute(xml, declaration, L"standalone", valueStart, valueLength, &standaloneStart))
			result.insert(standaloneStart, L"encoding=\"" + encoding + L"\" ");
		else
			result.insert(declaration.end, L" encoding=\"" + encoding + L"\"");
		return result;
	}

	const size_t insertionPoint = !xml.empty() && xml[0] == L'\xFEFF' ? 1 : 0;
	std::wstring result(xml);
	result.insert(insertionPoint, L"<?xml version=\"1.0\" encoding=\"" + encoding + L"\"?>\r\n");
	return result;
}
