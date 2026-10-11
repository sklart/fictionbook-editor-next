#include "stdafx.h"
#include "UiMetrics.h"
#include <vector>

namespace
{
struct FontPair
{
	UINT dpi;
	HFONT dialog;
	HFONT menu;
};

std::vector<FontPair> s_fontPairs;

const FontPair& FontsForDpi(UINT dpi)
{
	for(const FontPair& fonts : s_fontPairs)
		if(fonts.dpi == dpi) return fonts;

	NONCLIENTMETRICS metrics = {};
	metrics.cbSize = sizeof(metrics);
	typedef BOOL (WINAPI* SystemParametersInfoForDpiProc)(UINT, UINT, PVOID, UINT, UINT);
	HMODULE user32 = ::GetModuleHandle(L"user32.dll");
	SystemParametersInfoForDpiProc systemParametersInfoForDpi = user32
		? reinterpret_cast<SystemParametersInfoForDpiProc>(::GetProcAddress(user32, "SystemParametersInfoForDpi")) : NULL;
	const BOOL metricsRead = systemParametersInfoForDpi != NULL
		? systemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi)
		: ::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
	LOGFONTW dialogFont = {};
	LOGFONTW menuFont = {};
	if(metricsRead)
	{
		dialogFont = metrics.lfMessageFont;
		menuFont = metrics.lfMenuFont;
	}
	else
	{
		HFONT defaultFont = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
		if(defaultFont != NULL) ::GetObjectW(defaultFont, sizeof(dialogFont), &dialogFont);
		menuFont = dialogFont;
	}
	s_fontPairs.push_back({ dpi, ::CreateFontIndirectW(&dialogFont), ::CreateFontIndirectW(&menuFont) });
	return s_fontPairs.back();
}
}

HFONT UiMetrics::s_dialogFont = NULL;
HFONT UiMetrics::s_menuFont = NULL;
UINT UiMetrics::s_dpi = 96;

UINT UiMetrics::GetDpi(HWND window)
{
	typedef UINT (WINAPI* GetDpiForWindowProc)(HWND);
	HMODULE user32 = ::GetModuleHandle(L"user32.dll");
	GetDpiForWindowProc getDpiForWindow = user32
		? reinterpret_cast<GetDpiForWindowProc>(::GetProcAddress(user32, "GetDpiForWindow")) : NULL;
	if(getDpiForWindow && window != NULL)
	{
		const UINT dpi = getDpiForWindow(window);
		if(dpi) return dpi;
	}

	HDC dc = ::GetDC(window);
	const UINT dpi = dc ? static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSX)) : 96;
	if(dc) ::ReleaseDC(window, dc);
	return dpi ? dpi : 96;
}

void UiMetrics::EnsureFonts()
{
	if(s_dialogFont != NULL && s_menuFont != NULL)
		return;
	const FontPair& fonts = FontsForDpi(s_dpi);
	s_dialogFont = fonts.dialog;
	s_menuFont = fonts.menu;
}

void UiMetrics::UpdateForWindow(HWND window)
{
	s_dpi = GetDpi(window);
	s_dialogFont = NULL;
	s_menuFont = NULL;
	EnsureFonts();
}

void UiMetrics::Shutdown()
{
	for(const FontPair& fonts : s_fontPairs)
	{
		if(fonts.dialog != NULL) ::DeleteObject(fonts.dialog);
		if(fonts.menu != NULL) ::DeleteObject(fonts.menu);
	}
	s_fontPairs.clear();
	s_dialogFont = NULL;
	s_menuFont = NULL;
}

HFONT UiMetrics::DialogFont()
{
	EnsureFonts();
	return s_dialogFont != NULL ? s_dialogFont : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
}

HFONT UiMetrics::MenuFont()
{
	EnsureFonts();
	return s_menuFont != NULL ? s_menuFont : DialogFont();
}

HFONT UiMetrics::DialogFontForWindow(HWND window)
{
	const FontPair& fonts = FontsForDpi(GetDpi(window));
	return fonts.dialog != NULL ? fonts.dialog : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
}

HFONT UiMetrics::MenuFontForWindow(HWND window)
{
	const FontPair& fonts = FontsForDpi(GetDpi(window));
	return fonts.menu != NULL ? fonts.menu : DialogFontForWindow(window);
}

int UiMetrics::Scale(int px)
{
	return ::MulDiv(px, static_cast<int>(s_dpi ? s_dpi : 96), 96);
}

int UiMetrics::SmallGap() { return Scale(4); }
int UiMetrics::NormalGap() { return Scale(7); }
int UiMetrics::LargeGap() { return Scale(12); }
int UiMetrics::IconSize() { return Scale(24); }
int UiMetrics::ToolbarHeight() { return IconSize() + NormalGap(); }

UINT UiMetrics::DpiForWindow(HWND window) { return GetDpi(window); }
int UiMetrics::ScaleForDpi(int px, UINT dpi) { return ::MulDiv(px, static_cast<int>(dpi ? dpi : 96), 96); }
HFONT UiMetrics::CreateDialogFontForDpi(UINT dpi)
{
	NONCLIENTMETRICS metrics = {}; metrics.cbSize = sizeof(metrics);
	typedef BOOL (WINAPI* SystemParametersInfoForDpiProc)(UINT, UINT, PVOID, UINT, UINT);
	HMODULE user32 = ::GetModuleHandle(L"user32.dll");
	SystemParametersInfoForDpiProc forDpi = user32 ? reinterpret_cast<SystemParametersInfoForDpiProc>(::GetProcAddress(user32, "SystemParametersInfoForDpi")) : NULL;
	LOGFONTW font = {};
	const BOOL read = forDpi != NULL ? forDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi) : ::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
	if(read) font = metrics.lfMessageFont;
	else { HFONT fallback = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)); if(fallback != NULL) ::GetObjectW(fallback, sizeof(font), &font); }
	return ::CreateFontIndirectW(&font);
}
