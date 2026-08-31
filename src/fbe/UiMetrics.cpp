#include "stdafx.h"
#include "UiMetrics.h"

HFONT UiMetrics::s_dialogFont = NULL;
UINT UiMetrics::s_dpi = 96;

UINT UiMetrics::GetDpi(HWND window)
{
	typedef UINT (WINAPI* GetDpiForWindowProc)(HWND);
	HMODULE user32 = ::GetModuleHandle(L"user32.dll");
	GetDpiForWindowProc getDpiForWindow = user32
		? reinterpret_cast<GetDpiForWindowProc>(::GetProcAddress(user32, "GetDpiForWindow")) : NULL;
	if(getDpiForWindow && window != NULL)
		return getDpiForWindow(window);

	HDC dc = ::GetDC(window);
	const UINT dpi = dc ? static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSX)) : 96;
	if(dc) ::ReleaseDC(window, dc);
	return dpi ? dpi : 96;
}

void UiMetrics::EnsureFont()
{
	if(s_dialogFont != NULL)
		return;

	NONCLIENTMETRICS metrics = {};
	metrics.cbSize = sizeof(metrics);
	LOGFONTW font = {};
	if(::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
		font = metrics.lfMessageFont;
	else
	{
		HFONT defaultFont = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
		if(defaultFont != NULL)
			::GetObjectW(defaultFont, sizeof(font), &font);
	}
	s_dialogFont = ::CreateFontIndirectW(&font);
}

void UiMetrics::UpdateForWindow(HWND window)
{
	const UINT dpi = GetDpi(window);
	if(s_dialogFont != NULL)
	{
		::DeleteObject(s_dialogFont);
		s_dialogFont = NULL;
	}
	s_dpi = dpi;
	EnsureFont();
}

void UiMetrics::Shutdown()
{
	if(s_dialogFont != NULL)
	{
		::DeleteObject(s_dialogFont);
		s_dialogFont = NULL;
	}
}

HFONT UiMetrics::DialogFont()
{
	EnsureFont();
	return s_dialogFont != NULL ? s_dialogFont : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
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
