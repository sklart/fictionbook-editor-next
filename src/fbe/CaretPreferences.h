#pragma once

#include <windows.h>

enum class TextCaretMode : DWORD { System = 0, Enhanced = 1, Custom = 2 };
enum class TextCaretColor : DWORD { Automatic = 0, System = 1, Black = 2, White = 3, Custom = 4 };
enum class TextCaretBlink : DWORD { System = 0, Slow = 1, Normal = 2, Fast = 3, Solid = 4 };

// Width is in logical pixels; zero follows the system setting.
struct CaretPreferences
{
	TextCaretMode mode = TextCaretMode::System;
	DWORD width = 0;
	TextCaretColor color = TextCaretColor::Automatic;
	COLORREF customColor = RGB(0, 0, 0);
	TextCaretBlink blink = TextCaretBlink::System;
};

inline bool operator==(const CaretPreferences& left, const CaretPreferences& right)
{
	return left.mode == right.mode && left.width == right.width && left.color == right.color &&
		left.customColor == right.customColor && left.blink == right.blink;
}

inline bool operator!=(const CaretPreferences& left, const CaretPreferences& right) { return !(left == right); }
