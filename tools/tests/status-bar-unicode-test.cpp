#include "StatusBarUnicode.h"
static bool Expect(const wchar_t* value, int length, unsigned int expected)
{
	unsigned int codePoint = 0;
	return FBEStatusBar::FirstCodePoint(value, length, codePoint) && codePoint == expected;
}

int main()
{
	if (!Expect(L"A", 1, 0x0041) || !Expect(L"Ж", 1, 0x0416) ||
		!Expect(L"—", 1, 0x2014) || !Expect(L"€", 1, 0x20AC)) return 1;
	const wchar_t grin[] = { 0xD83D, 0xDE00 };
	const wchar_t gothic[] = { 0xD800, 0xDF48 };
	if (!Expect(grin, 2, 0x1F600) || !Expect(gothic, 2, 0x10348)) return 2;
	const wchar_t high[] = { 0xD800 };
	const wchar_t low[] = { 0xDC00 };
	const wchar_t invalid1[] = { 0xD800, L'A' };
	const wchar_t invalid2[] = { 0xDBFF, 0x0041 };
	unsigned int ignored = 0;
	if (FBEStatusBar::FirstCodePoint(high, 1, ignored) || FBEStatusBar::FirstCodePoint(low, 1, ignored) ||
		FBEStatusBar::FirstCodePoint(invalid1, 2, ignored) || FBEStatusBar::FirstCodePoint(invalid2, 2, ignored)) return 3;
	return 0;
}
