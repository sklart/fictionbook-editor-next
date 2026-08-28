#pragma once

#include <cstddef>
#include <string>

namespace FBEStatusBar {
inline bool NextUtf8CodePoint(const std::string& text, std::size_t& offset, unsigned int& codePoint)
{
	if(offset >= text.size()) return false;
	const unsigned char first = static_cast<unsigned char>(text[offset++]);
	if(first < 0x80) { codePoint = first; return true; }
	int extra = first >= 0xF0 && first <= 0xF4 ? 3 : first >= 0xE0 && first <= 0xEF ? 2 : first >= 0xC2 && first <= 0xDF ? 1 : -1;
	if(extra < 0 || offset + static_cast<std::size_t>(extra) > text.size()) return false;
	codePoint = first & ((1u << (6 - extra)) - 1);
	for(int i = 0; i < extra; ++i) {
		const unsigned char next = static_cast<unsigned char>(text[offset++]);
		if((next & 0xC0) != 0x80) return false;
		codePoint = (codePoint << 6) | (next & 0x3F);
	}
	return !(codePoint >= 0xD800 && codePoint <= 0xDFFF) && codePoint <= 0x10FFFF;
}

inline bool IsUnicodeWordCodePoint(unsigned int codePoint)
{
	if(codePoint < 0x80) return (codePoint >= '0' && codePoint <= '9') || (codePoint >= 'A' && codePoint <= 'Z') || (codePoint >= 'a' && codePoint <= 'z') || codePoint == '_';
	if((codePoint >= 0x2000 && codePoint <= 0x206F) || (codePoint >= 0x2100 && codePoint <= 0x2BFF) || codePoint >= 0x1F000) return false;
	return true;
}

inline int CountUtf8Words(const std::string& text)
{
	int words = 0; bool inWord = false;
	for(std::size_t offset = 0; offset < text.size();) {
		unsigned int codePoint = 0;
		const bool word = NextUtf8CodePoint(text, offset, codePoint) && IsUnicodeWordCodePoint(codePoint);
		if(word && !inWord) ++words;
		inWord = word;
	}
	return words;
}

inline int SelectionLineCount(int startLine, int endLine, bool endAtLineStart)
{
	if(endAtLineStart && endLine > startLine) --endLine;
	return endLine - startLine + 1;
}
}
