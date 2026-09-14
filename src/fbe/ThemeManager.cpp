#include "stdafx.h"
#include "ThemeManager.h"

namespace
{
InterfaceTheme g_selected = INTERFACE_THEME_AUTOMATIC;
bool g_systemDark = false;
bool g_highContrast = false;
HHOOK g_themeCbtHook = NULL;
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
typedef void (WINAPI* FlushMenuThemesFn)();

const UINT_PTR kThemeControlSubclassId = 0x46424554; // "FBET"

bool IsClass(HWND window, LPCWSTR className)
{
	wchar_t actual[64] = {};
	return ::GetClassNameW(window, actual, _countof(actual)) && ::lstrcmpiW(actual, className) == 0;
}

bool IsGroupBox(HWND window)
{
	return IsClass(window, L"Button") &&
		((::GetWindowLongPtrW(window, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX);
}

bool UsesClassicSurfacePalette(HWND window)
{
	// Explorer visual styles ignore the colours set through the common-control
	// messages for rebar/toolbar/status surfaces. Disable them only for these
	// surfaces in dark mode; ordinary controls keep native modern rendering.
	return IsClass(window, TOOLBARCLASSNAMEW) || IsClass(window, REBARCLASSNAMEW) ||
		IsClass(window, STATUSCLASSNAMEW);
}

void PaintDarkGroupBox(HWND window)
{
	PAINTSTRUCT paint = {};
	HDC dc = ::BeginPaint(window, &paint);
	RECT client = {}; ::GetClientRect(window, &client);
	::FillRect(dc, &client, ThemeManager::WindowBrush());

	wchar_t caption[256] = {};
	::GetWindowTextW(window, caption, _countof(caption));
	HFONT font = reinterpret_cast<HFONT>(::SendMessage(window, WM_GETFONT, 0, 0));
	HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
	SIZE textSize = {};
	::GetTextExtentPoint32W(dc, caption, ::lstrlenW(caption), &textSize);

	RECT border = client;
	border.top += (textSize.cy + 1) / 2;
	::FrameRect(dc, &border, ThemeManager::Brush(THEME_COLOR_BORDER));
	RECT captionRect = { 8, 0, (std::min)(client.right - 2, 12 + textSize.cx), textSize.cy };
	::FillRect(dc, &captionRect, ThemeManager::WindowBrush());
	::SetBkMode(dc, TRANSPARENT);
	::SetTextColor(dc, ::IsWindowEnabled(window) ? ThemeManager::TextColor() : ThemeManager::DisabledTextColor());
	::DrawTextW(dc, caption, -1, &captionRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
	if(oldFont) ::SelectObject(dc, oldFont);
	::EndPaint(window, &paint);
}

LRESULT CALLBACK ThemeControlSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
	if(message == WM_NCDESTROY)
	{
		::RemoveWindowSubclass(window, ThemeControlSubclassProc, kThemeControlSubclassId);
		return ::DefSubclassProc(window, message, wParam, lParam);
	}
	if(IsHighContrastEnabled()) return ::DefSubclassProc(window, message, wParam, lParam);
	if(ThemeManager::IsDark() && IsGroupBox(window))
	{
		if(message == WM_ERASEBKGND) return 1;
		if(message == WM_PAINT) { PaintDarkGroupBox(window); return 0; }
	}
	if(message == WM_CTLCOLORDLG)
	{
		HDC dc = reinterpret_cast<HDC>(wParam);
		::SetBkColor(dc, ThemeManager::WindowColor());
		::SetTextColor(dc, ThemeManager::TextColor());
		return reinterpret_cast<LRESULT>(ThemeManager::WindowBrush());
	}
	if(message == WM_CTLCOLORSTATIC)
	{
		// Labels are normally painted directly on a dialog/panel.  Using the
		// control brush here leaves visible rectangles behind captions in dark
		// dialogs, so keep them on the owning window background.
		HDC dc = reinterpret_cast<HDC>(wParam);
		HWND control = reinterpret_cast<HWND>(lParam);
		const bool enabled = !control || ::IsWindowEnabled(control) != FALSE;
		::SetTextColor(dc, enabled ? ThemeManager::TextColor() : ThemeManager::DisabledTextColor());
		::SetBkColor(dc, ThemeManager::WindowColor());
		return reinterpret_cast<LRESULT>(ThemeManager::WindowBrush());
	}
	if(message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX || message == WM_CTLCOLORBTN)
	{
		HDC dc = reinterpret_cast<HDC>(wParam);
		HWND control = reinterpret_cast<HWND>(lParam);
		const bool enabled = !control || ::IsWindowEnabled(control) != FALSE;
		::SetTextColor(dc, enabled ? ThemeManager::TextColor() : ThemeManager::DisabledTextColor());
		::SetBkColor(dc, ThemeManager::ControlColor());
		return reinterpret_cast<LRESULT>(ThemeManager::ControlBrush());
	}
	return ::DefSubclassProc(window, message, wParam, lParam);
}

void ApplyNativeControlPalette(HWND window)
{
	if(IsHighContrastEnabled()) return;
	if(IsClass(window, WC_TREEVIEWW))
	{
		::SendMessage(window, TVM_SETBKCOLOR, 0, ThemeManager::WindowColor());
		::SendMessage(window, TVM_SETTEXTCOLOR, 0, ThemeManager::TextColor());
		::SendMessage(window, TVM_SETLINECOLOR, 0, ThemeManager::SeparatorColor());
	}
	else if(IsClass(window, WC_LISTVIEWW))
	{
		::SendMessage(window, LVM_SETBKCOLOR, 0, ThemeManager::WindowColor());
		::SendMessage(window, LVM_SETTEXTBKCOLOR, 0, ThemeManager::WindowColor());
		::SendMessage(window, LVM_SETTEXTCOLOR, 0, ThemeManager::TextColor());
	}
	else if(IsClass(window, WC_TABCONTROLW))
	{
		::SendMessage(window, CCM_SETBKCOLOR, 0, ThemeManager::ControlColor());
	}
	else if(IsClass(window, TOOLBARCLASSNAMEW))
		::SendMessage(window, CCM_SETBKCOLOR, 0, ThemeManager::ControlColor());
	else if(IsClass(window, L"Edit"))
		::SendMessage(window, EM_SETBKGNDCOLOR, 0, ThemeManager::ControlColor());
	else if(IsClass(window, STATUSCLASSNAMEW))
		::SendMessage(window, SB_SETBKCOLOR, 0, ThemeManager::ControlColor());
	else if(IsClass(window, REBARCLASSNAMEW))
		::SendMessage(window, RB_SETBKCOLOR, 0, ThemeManager::ControlColor());
}

void ApplyPreferredAppMode(bool dark)
{
	// Ordinal 135 exists only on modern Windows 10 builds.  Resolving it at
	// runtime leaves Windows 7 on the normal, supported light-menu path.
	HMODULE uxtheme = ::LoadLibraryW(L"uxtheme.dll");
	if(!uxtheme) return;
	SetPreferredAppModeFn setMode = reinterpret_cast<SetPreferredAppModeFn>(::GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
	// AllowDark leaves popup menus light when FBE is explicitly Dark but Windows
	// itself is light. ForceDark makes FBE's selected theme govern its menus.
	if(setMode) setMode(dark ? 2 /* ForceDark */ : 0 /* Default */);
	// Rebuild popup-menu rendering after changing the preferred app mode.  This
	// export is available only on supported Windows 10/11 builds, so resolving
	// it dynamically keeps the Windows 7 path untouched.
	FlushMenuThemesFn flushMenus = reinterpret_cast<FlushMenuThemesFn>(::GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));
	if(flushMenus) flushMenus();
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

LRESULT CALLBACK ThemeCbtHookProc(int code, WPARAM wParam, LPARAM lParam)
{
	if(code == HCBT_ACTIVATE && wParam != 0)
		ThemeManager::ApplyToWindow(reinterpret_cast<HWND>(wParam));
	return ::CallNextHookEx(g_themeCbtHook, code, wParam, lParam);
}

void EnsureThemeCbtHook()
{
	if(g_themeCbtHook == NULL)
		g_themeCbtHook = ::SetWindowsHookExW(WH_CBT, ThemeCbtHookProc, NULL, ::GetCurrentThreadId());
}
}

namespace ThemeManager
{
void SetSelectedTheme(InterfaceTheme theme)
{
	if(theme < INTERFACE_THEME_AUTOMATIC || theme > INTERFACE_THEME_DARK)
		theme = INTERFACE_THEME_AUTOMATIC;
	const bool wasDark = IsDark();
	const bool wasHighContrast = g_highContrast;
	g_selected = theme;
	g_systemDark = ReadAppsUseLightTheme();
	g_highContrast = IsHighContrastEnabled();
	EnsureThemeCbtHook();
	ApplyPreferredAppMode(IsDark() && !g_highContrast);
	if(wasDark != IsDark() || wasHighContrast != g_highContrast || !g_windowBrush) RebuildBrushes();
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
	const bool dark = IsDark() && !IsHighContrastEnabled();
	::SetWindowSubclass(window, ThemeControlSubclassProc, kThemeControlSubclassId, 0);
	// A single-space app/sub-app pair is the documented opt-out marker for
	// visual styles. An empty string merely selects the default theme again.
	if(dark && UsesClassicSurfacePalette(window))
		::SetWindowTheme(window, L" ", L" ");
	else
		::SetWindowTheme(window, dark ? L"DarkMode_Explorer" : L"Explorer", NULL);
	ApplyModernTitleBar(window, dark);
	// Common controls reset custom colours while processing WM_THEMECHANGED.
	// Set their palette only after that notification has completed.
	::SendMessage(window, WM_THEMECHANGED, 0, 0);
	ApplyNativeControlPalette(window);
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
	const bool oldHighContrast = g_highContrast;
	g_systemDark = ReadAppsUseLightTheme();
	g_highContrast = IsHighContrastEnabled();
	const bool highContrastChanged = oldHighContrast != g_highContrast;
	if((g_selected != INTERFACE_THEME_AUTOMATIC || oldDark == IsDark()) && !highContrastChanged) return false;
	ApplyPreferredAppMode(IsDark() && !g_highContrast);
	ApplyToAllThreadWindows(::GetCurrentThreadId());
	return true;
}
}
