#pragma once

#include <cwctype>
#include <string>
#include <vector>

// Helpers shared by the Body/Source transfer code and its behavioural test.
// They deliberately operate on serialized XML only; DOM navigation remains the
// authority for selecting the XML body and the expected structural position.
namespace FBEBodySourceTransfer
{
struct XmlTextRange
{
	int start;
	int end;
};

inline bool IsVisibleWhitespace(wchar_t character)
{
	return iswspace(character) != 0 || character == L'\xA0';
}

inline bool DecodeXmlCharacterReference(const std::wstring& reference,
	std::wstring& decoded)
{
	decoded.clear();
	if (reference == L"&amp;") decoded = L"&";
	else if (reference == L"&lt;") decoded = L"<";
	else if (reference == L"&gt;") decoded = L">";
	else if (reference == L"&quot;") decoded = L"\"";
	else if (reference == L"&apos;") decoded = L"'";
	else if (reference == L"&nbsp;") decoded.assign(1, L'\xA0');
	else if (reference.size() >= 4 && reference[0] == L'&' && reference[1] == L'#' &&
		reference[reference.size() - 1] == L';')
	{
		unsigned long value = 0;
		size_t position = 2;
		int base = 10;
		if (position < reference.size() - 1 &&
			(reference[position] == L'x' || reference[position] == L'X'))
		{
			base = 16;
			++position;
		}
		if (position == reference.size() - 1) return false;
		for (; position < reference.size() - 1; ++position)
		{
			const wchar_t character = reference[position];
			int digit = -1;
			if (character >= L'0' && character <= L'9') digit = character - L'0';
			else if (base == 16 && character >= L'a' && character <= L'f') digit = character - L'a' + 10;
			else if (base == 16 && character >= L'A' && character <= L'F') digit = character - L'A' + 10;
			if (digit < 0 || digit >= base || value > (0x10FFFFUL - digit) / base) return false;
			value = value * base + digit;
		}
		if (value == 0 || value > 0x10FFFFUL || (value >= 0xD800UL && value <= 0xDFFFUL)) return false;
		if (value <= 0xFFFFUL) decoded.assign(1, static_cast<wchar_t>(value));
		else
		{
			value -= 0x10000UL;
			decoded.push_back(static_cast<wchar_t>(0xD800UL + (value >> 10)));
			decoded.push_back(static_cast<wchar_t>(0xDC00UL + (value & 0x3FF)));
		}
	}
	return !decoded.empty();
}

inline std::wstring NormalizeVisibleText(const std::wstring& text)
{
	std::wstring normalized;
	bool previousWhitespace = false;
	for (size_t index = 0; index < text.size(); ++index)
	{
		const bool whitespace = IsVisibleWhitespace(text[index]);
		if (whitespace)
		{
			if (!previousWhitespace) normalized.push_back(L' ');
		}
		else normalized.push_back(text[index]);
		previousWhitespace = whitespace;
	}
	return normalized;
}

struct MappedCharacter
{
	wchar_t character;
	int start;
	int end;
};

inline bool FindVisibleXmlTextRange(const std::wstring& sourceXml,
	const std::wstring& visibleText, int scopeStart, int scopeEnd,
	int expectedStart, XmlTextRange& result)
{
	result.start = result.end = -1;
	const std::wstring wanted = NormalizeVisibleText(visibleText);
	if (wanted.empty() || scopeStart < 0 || scopeEnd > static_cast<int>(sourceXml.size()) || scopeStart >= scopeEnd)
		return false;

	std::vector<MappedCharacter> source;
	bool previousWhitespace = false;
	for (int position = scopeStart; position < scopeEnd;)
	{
		if (sourceXml[position] == L'<')
		{
			const size_t tagEnd = sourceXml.find(L'>', static_cast<size_t>(position + 1));
			if (tagEnd == std::wstring::npos || tagEnd >= static_cast<size_t>(scopeEnd)) return false;
			std::wstring name = sourceXml.substr(position + 1, tagEnd - position - 1);
			const size_t first = name.find_first_not_of(L" \t\r\n");
			if (first != std::wstring::npos)
			{
				name.erase(0, first);
				if (!name.empty() && name[0] == L'/') name.erase(0, 1);
				const size_t nameEnd = name.find_first_of(L" \t\r\n/");
				if (nameEnd != std::wstring::npos) name.erase(nameEnd);
				if ((name == L"p" || name == L"title" || name == L"empty-line") &&
					!source.empty() && !previousWhitespace)
				{
					source.push_back({ L' ', position, static_cast<int>(tagEnd) + 1 });
					previousWhitespace = true;
				}
			}
			position = static_cast<int>(tagEnd) + 1;
			continue;
		}
		std::wstring decoded(1, sourceXml[position]);
		int nextPosition = position + 1;
		if (sourceXml[position] == L'&')
		{
			const size_t entityEnd = sourceXml.find(L';', static_cast<size_t>(position + 1));
			if (entityEnd != std::wstring::npos && entityEnd < static_cast<size_t>(scopeEnd))
			{
				const std::wstring entity = sourceXml.substr(position, entityEnd - position + 1);
				if (DecodeXmlCharacterReference(entity, decoded)) nextPosition = static_cast<int>(entityEnd) + 1;
			}
		}
		for (size_t characterIndex = 0; characterIndex < decoded.size(); ++characterIndex)
		{
			wchar_t character = decoded[characterIndex];
			const bool whitespace = IsVisibleWhitespace(character);
			if (whitespace)
			{
				if (previousWhitespace) continue;
				character = L' ';
			}
			source.push_back({ character, position, nextPosition });
			previousWhitespace = whitespace;
		}
		position = nextPosition;
	}

	int bestStart = -1;
	int bestEnd = -1;
	long bestDistance = 0;
	bool ambiguous = false;
	for (size_t start = 0; start + wanted.size() <= source.size(); ++start)
	{
		size_t offset = 0;
		while (offset < wanted.size() && source[start + offset].character == wanted[offset]) ++offset;
		if (offset != wanted.size()) continue;
		const int candidateStart = source[start].start;
		const int candidateEnd = source[start + wanted.size() - 1].end;
		if (expectedStart < 0)
		{
			if (bestStart >= 0) return false; // no structural position: never guess
			bestStart = candidateStart;
			bestEnd = candidateEnd;
			continue;
		}
		const long distance = candidateStart >= expectedStart
			? static_cast<long>(candidateStart - expectedStart)
			: static_cast<long>(expectedStart - candidateStart);
		if (bestStart < 0 || distance < bestDistance)
		{
			bestStart = candidateStart;
			bestEnd = candidateEnd;
			bestDistance = distance;
			ambiguous = false;
		}
		else if (distance == bestDistance) ambiguous = true;
	}
	if (bestStart < 0 || ambiguous) return false;
	result.start = bestStart;
	result.end = bestEnd;
	return true;
}
}
