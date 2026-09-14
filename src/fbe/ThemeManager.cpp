#include "stdafx.h"
#include "ThemeManager.h"

namespace
{
InterfaceTheme g_selected = INTERFACE_THEME_AUTOMATIC;
bool g_systemDark = false;
HBRUSH g_windowBrush = NULL;
HBRUSH g_controlBrush = NULL;
HBRUSH g_brushes[THEME_COLOR_COUNT] = {};

bool IsHighContrastEnabled()
{
	HIGHCONTRAST highContrast = {}; highContrast.cbSize = sizeof(highContrast);
	return ::SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, 0) &&
		(highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

bool ReadAppsUseLightTheme()
{
	DWORD value = 1, valueSize = sizeof(value);
	const LONG result = ::RegGetValueW(HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &value, &valueSize);
	return result == ERROR_SUCCESS && value == 0;
}

void RebuildBrushes()
{
	if(g_windowBrush) ::DeleteObject(g_windowBrush);
	if(g_controlBrush) ::DeleteObject(g_controlBrush);
	for(int i = 0; i < THEME_COLOR_COUNT; ++i) if(g_brushes[i]) { ::DeleteObject(g_brushes[i]); g_brushes[i] = NULL; }
	g_windowBrush = ::CreateSolidBrush(ThemeManager::WindowColor());
	g_controlBrush = ::CreateSolidBrush(ThemeManager::ControlColor());
	for(int i = 0; i < THEME_COLOR_COUNT; ++i)
		g_brushes[i] = ::CreateSolidBrush(ThemeManager::Color(static_cast<ThemeColorRole>(i)));
}

typedef HRESULT (WINAPI* DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
typedef HRESULT (WINAPI* SetPreferredAppModeFn)(int);

void ApplyPreferredAppMode(bool dark)
{
	// Ordinal 135 exists only on modern Windows 10 builds.  Resolving it at
	// runtime leaves Windows 7 on the normal, supported light-menu path.
	HMODULE uxtheme = ::LoadLibraryW(L"uxtheme.dll");
	if(!uxtheme) return;
	SetPreferredAppModeFn setMode = reinterpret_cast<SetPreferredAppModeFn>(::GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
	if(setMode) setMode(dark ? 1 /* AllowDark */ : 0 /* Default */);
	::FreeLibrary(uxtheme);
}

void ApplyModernTitleBar(HWND window, bool dark)
{
	HMODULE dwmapi = ::GetModuleHandleW(L"dwmapi.dll");
	DwmSetWindowAttributeFn setAttribute = dwmapi ? reinterpret_cast<DwmSetWindowAttributeFn>(::GetProcAddress(dwmapi, "DwmSetWindowAttribute")) : NULL;
	if(setAttribute)
	{
		const BOOL enabled = dark ? TRUE : FALSE;
		// 20 is the documented Windows 10 20H1 attribute; 19 is used by 1809.
		if(FAILED(setAttribute(window, 20, &enabled, sizeof(enabled))))
			setAttribute(window, 19, &enabled, sizeof(enabled));
	}
	HMODULE uxtheme = ::LoadLibraryW(L"uxtheme.dll");
	if(uxtheme)
	{
		typedef BOOL (WINAPI* AllowDarkModeForWindowFn)(HWND, BOOL);
		AllowDarkModeForWindowFn allow = reinterpret_cast<AllowDarkModeForWindowFn>(::GetProcAddress(uxtheme, MAKEINTRESOURCEA(133)));
		if(allow) allow(window, dark ? TRUE : FALSE);
		::FreeLibrary(uxtheme);
	}
}

BOOL CALLBACK ApplyChild(HWND window, LPARAM)
{
	ThemeManager::ApplyToWindow(window);
	return TRUE;
}

BOOL CALLBACK ApplyThreadWindow(HWND window, LPARAM)
{
	ThemeManager::ApplyToWindow(window);
	return TRUE;
}
}

namespace ThemeManager
{
void SetSelectedTheme(InterfaceTheme theme)
{
	if(theme < INTERFACE_THEME_AUTOMATIC || theme > INTERFACE_THEME_DARK)
		theme = INTERFACE_THEME_AUTOMATIC;
	const bool wasDark = IsDark();
	g_selected = theme;
	g_systemDark = ReadAppsUseLightTheme();
	ApplyPreferredAppMode(IsDark());
	if(wasDark != IsDark() || !g_windowBrush) RebuildBrushes();
}

InterfaceTheme GetSelectedTheme() { return g_selected; }
bool IsDark() { return g_selected == INTERFACE_THEME_DARK || (g_selected == INTERFACE_THEME_AUTOMATIC && g_systemDark); }
COLORREF Color(ThemeColorRole role)
{
	if(IsHighContrastEnabled())
	{
		switch(role)
		{
		case THEME_COLOR_WINDOW: return ::GetSysColor(COLOR_WINDOW);
		case THEME_COLOR_CONTROL: return ::GetSysColor(COLOR_BTNFACE);
		case THEME_COLOR_TEXT: case THEME_COLOR_SECONDARY_TEXT: return ::GetSysColor(COLOR_WINDOWTEXT);
		case THEME_COLOR_DISABLED_TEXT: return ::GetSysColor(COLOR_GRAYTEXT);
		case THEME_COLOR_SELECTION_BACKGROUND: return ::GetSysColor(COLOR_HIGHLIGHT);
		case THEME_COLOR_SELECTION_TEXT: return ::GetSysColor(COLOR_HIGHLIGHTTEXT);
		default: return ::GetSysColor(COLOR_WINDOWTEXT);
		}
	}
	if(!IsDark())
	{
		switch(role)
		{
		case THEME_COLOR_WINDOW: return ::GetSysColor(COLOR_WINDOW);
		case THEME_COLOR_CONTROL: return ::GetSysColor(COLOR_BTNFACE);
		case THEME_COLOR_TEXT: return ::GetSysColor(COLOR_WINDOWTEXT);
		case THEME_COLOR_BORDER: return ::GetSysColor(COLOR_3DSHADOW);
		case THEME_COLOR_SEPARATOR: return ::GetSysColor(COLOR_3DLIGHT);
		case THEME_COLOR_SECONDARY_TEXT: return ::GetSysColor(COLOR_GRAYTEXT);
		case THEME_COLOR_DISABLED_TEXT: return ::GetSysColor(COLOR_GRAYTEXT);
		case THEME_COLOR_SELECTION_BACKGROUND: return ::GetSysColor(COLOR_HIGHLIGHT);
		case THEME_COLOR_SELECTION_TEXT: return ::GetSysColor(COLOR_HIGHLIGHTTEXT);
		case THEME_COLOR_HOVER: return RGB(229, 241, 251);
		case THEME_COLOR_PRESSED: return RGB(204, 228, 247);
		case THEME_COLOR_FOCUS: case THEME_COLOR_ACCENT: return RGB(0, 120, 215);
		case THEME_COLOR_ERROR: return RGB(196, 43, 28);
		case THEME_COLOR_WARNING: return RGB(156, 99, 0);
		case THEME_COLOR_SUCCESS: return RGB(16, 124, 16);
		default: return ::GetSysColor(COLOR_WINDOW);
		}
	}
	switch(role)
	{
	case THEME_COLOR_WINDOW: return RGB(32, 32, 32);
	case THEME_COLOR_CONTROL: return RGB(45, 45, 45);
	case THEME_COLOR_TEXT: return RGB(230, 230, 230);
	case THEME_COLOR_BORDER: return RGB(92, 92, 92);
	case THEME_COLOR_SEPARATOR: return RGB(62, 62, 62);
	case THEME_COLOR_SECONDARY_TEXT: return RGB(184, 184, 184);
	case THEME_COLOR_DISABLED_TEXT: return RGB(136, 136, 136);
	case THEME_COLOR_SELECTION_BACKGROUND: return RGB(38, 79, 120);
	case THEME_COLOR_SELECTION_TEXT: return RGB(255, 255, 255);
	case THEME_COLOR_HOVER: return RGB(58, 58, 58);
	case THEME_COLOR_PRESSED: return RGB(72, 72, 72);
	case THEME_COLOR_FOCUS: case THEME_COLOR_ACCENT: return RGB(76, 194, 255);
	case THEME_COLOR_ERROR: return RGB(255, 99, 71);
	case THEME_COLOR_WARNING: return RGB(255, 184, 77);
	case THEME_COLOR_SUCCESS: return RGB(98, 202, 125);
	default: return RGB(32, 32, 32);
	}
}
COLORREF WindowColor() { return Color(THEME_COLOR_WINDOW); }
COLORREF TextColor() { return Color(THEME_COLOR_TEXT); }
COLORREF ControlColor() { return Color(THEME_COLOR_CONTROL); }
COLORREF BorderColor() { return Color(THEME_COLOR_BORDER); }
COLORREF SeparatorColor() { return Color(THEME_COLOR_SEPARATOR); }
COLORREF SecondaryTextColor() { return Color(THEME_COLOR_SECONDARY_TEXT); }
COLORREF DisabledTextColor() { return Color(THEME_COLOR_DISABLED_TEXT); }
COLORREF SelectionBackgroundColor() { return Color(THEME_COLOR_SELECTION_BACKGROUND); }
COLORREF SelectionTextColor() { return Color(THEME_COLOR_SELECTION_TEXT); }
COLORREF HoverColor() { return Color(THEME_COLOR_HOVER); }
COLORREF PressedColor() { return Color(THEME_COLOR_PRESSED); }
COLORREF FocusColor() { return Color(THEME_COLOR_FOCUS); }
COLORREF AccentColor() { return Color(THEME_COLOR_ACCENT); }
COLORREF ErrorColor() { return Color(THEME_COLOR_ERROR); }
COLORREF WarningColor() { return Color(THEME_COLOR_WARNING); }
COLORREF SuccessColor() { return Color(THEME_COLOR_SUCCESS); }
HBRUSH Brush(ThemeColorRole role) { if(!g_brushes[role]) RebuildBrushes(); return g_brushes[role]; }
HBRUSH WindowBrush() { if(!g_windowBrush) RebuildBrushes(); return g_windowBrush; }
HBRUSH ControlBrush() { if(!g_controlBrush) RebuildBrushes(); return g_controlBrush; }

void ApplyToWindow(HWND window)
{
	if(!::IsWindow(window)) return;
	const bool dark = IsDark();
	::SetWindowTheme(window, dark ? L"DarkMode_Explorer" : L"Explorer", NULL);
	ApplyModernTitleBar(window, dark);
	::SendMessage(window, WM_THEMECHANGED, 0, 0);
	::SendMessage(window, WM_FBE_THEMECHANGED, 0, 0);
	::RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
	::EnumChildWindows(window, ApplyChild, 0);
}

void ApplyToAllThreadWindows(DWORD threadId)
{
	RebuildBrushes();
	::EnumThreadWindows(threadId, ApplyThreadWindow, 0);
}

bool RefreshSystemTheme()
{
	const bool oldDark = IsDark();
	g_systemDark = ReadAppsUseLightTheme();
	if(g_selected != INTERFACE_THEME_AUTOMATIC || oldDark == IsDark()) return false;
	ApplyPreferredAppMode(IsDark());
	ApplyToAllThreadWindows(::GetCurrentThreadId());
	return true;
}
}
