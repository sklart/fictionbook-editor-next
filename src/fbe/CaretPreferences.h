#pragma once

#include <windows.h>
#include <algorithm>

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

struct ResolvedCaretAppearance
{
	int width;
	COLORREF color;
	int period;
};

inline int SystemCaretWidth()
{
	DWORD width = 1;
	if(!::SystemParametersInfo(SPI_GETCARETWIDTH, 0, &width, 0) || width == 0) width = 1;
	return static_cast<int>((std::min)(width, static_cast<DWORD>(20)));
}

inline int SystemCaretPeriod()
{
	const UINT period = ::GetCaretBlinkTime();
	return period == INFINITE ? 0 : period == 0 ? 500 : static_cast<int>((std::min)(period, 5000U));
}

inline int CaretWidthPixels(const CaretPreferences& preferences, UINT dpi, int systemWidth)
{
	if(!dpi) dpi = 96;
	int width = systemWidth;
	if(preferences.mode == TextCaretMode::Enhanced)
		width = (std::max)(width, ::MulDiv(3, dpi, 96));
	else if(preferences.mode == TextCaretMode::Custom && preferences.width != 0)
		width = ::MulDiv(static_cast<int>(preferences.width), dpi, 96);
	return (std::min)(20, (std::max)(1, width));
}

inline COLORREF ContrastingCaretColor(COLORREF background)
{
	const int luminance = 299 * GetRValue(background) + 587 * GetGValue(background) + 114 * GetBValue(background);
	return luminance >= 128000 ? RGB(0, 0, 0) : RGB(255, 255, 255);
}

inline ResolvedCaretAppearance ResolveCaretAppearance(const CaretPreferences& preferences, UINT dpi,
	COLORREF background, COLORREF themeCaret, bool highContrast, int systemWidth, int systemPeriod)
{
	ResolvedCaretAppearance appearance = { CaretWidthPixels(preferences, dpi, systemWidth), themeCaret, systemPeriod };
	if(preferences.mode == TextCaretMode::Enhanced)
	{
		appearance.color = ContrastingCaretColor(background);
		appearance.period = 800;
	}
	else if(preferences.mode == TextCaretMode::Custom)
	{
		switch(preferences.color)
		{
		case TextCaretColor::Automatic: appearance.color = ContrastingCaretColor(background); break;
		case TextCaretColor::System: appearance.color = ::GetSysColor(COLOR_WINDOWTEXT); break;
		case TextCaretColor::Black: appearance.color = RGB(0, 0, 0); break;
		case TextCaretColor::White: appearance.color = RGB(255, 255, 255); break;
		case TextCaretColor::Custom: appearance.color = preferences.customColor; break;
		}
		switch(preferences.blink)
		{
		case TextCaretBlink::System: break;
		case TextCaretBlink::Slow: appearance.period = 800; break;
		case TextCaretBlink::Normal: appearance.period = 500; break;
		case TextCaretBlink::Fast: appearance.period = 300; break;
		case TextCaretBlink::Solid: appearance.period = 0; break;
		}
	}
	if(highContrast) appearance.color = ::GetSysColor(COLOR_WINDOWTEXT);
	return appearance;
}
