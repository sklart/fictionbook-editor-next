#pragma once

namespace FBEStatusBar {

inline bool FirstCodePoint(const wchar_t* text, int length, unsigned int& codePoint)
{
	if (!text || length <= 0)
		return false;
	const unsigned int first = static_cast<unsigned int>(text[0]);
	if (first >= 0xD800 && first <= 0xDBFF)
	{
		if (length < 2)
			return false;
		const unsigned int second = static_cast<unsigned int>(text[1]);
		if (second < 0xDC00 || second > 0xDFFF)
			return false;
		codePoint = 0x10000 + ((first - 0xD800) << 10) + second - 0xDC00;
		return true;
	}
	if (first >= 0xDC00 && first <= 0xDFFF)
		return false;
	codePoint = first;
	return true;
}

} // namespace FBEStatusBar
