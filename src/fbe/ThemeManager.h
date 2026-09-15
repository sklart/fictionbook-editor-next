#pragma once

// Interface theme is deliberately independent from BODY editor colours.
enum InterfaceTheme
{
	INTERFACE_THEME_AUTOMATIC = 0,
	INTERFACE_THEME_LIGHT,
	INTERFACE_THEME_DARK
};

// Sent after the effective theme changes.  Windows which own custom drawing
// can use it to discard cached colours and repaint.
#define WM_FBE_THEMECHANGED (WM_APP + 0x146)

enum ThemeColorRole
{
	THEME_COLOR_WINDOW = 0,
	THEME_COLOR_CONTROL,
	THEME_COLOR_TEXT,
	THEME_COLOR_BORDER,
	THEME_COLOR_SEPARATOR,
	THEME_COLOR_SECONDARY_TEXT,
	THEME_COLOR_DISABLED_TEXT,
	THEME_COLOR_SELECTION_BACKGROUND,
	THEME_COLOR_SELECTION_TEXT,
	THEME_COLOR_HOVER,
	THEME_COLOR_PRESSED,
	THEME_COLOR_FOCUS,
	THEME_COLOR_ACCENT,
	THEME_COLOR_ERROR,
	THEME_COLOR_WARNING,
	THEME_COLOR_SUCCESS,
	THEME_COLOR_COUNT
};

namespace ThemeManager
{
	void SetSelectedTheme(InterfaceTheme theme);
	InterfaceTheme GetSelectedTheme();
	bool IsDark();
	bool IsHighContrast();
	COLORREF Color(ThemeColorRole role);
	COLORREF WindowColor();
	COLORREF TextColor();
	COLORREF ControlColor();
	COLORREF BorderColor();
	COLORREF SeparatorColor();
	COLORREF SecondaryTextColor();
	COLORREF DisabledTextColor();
	COLORREF SelectionBackgroundColor();
	COLORREF SelectionTextColor();
	COLORREF HoverColor();
	COLORREF PressedColor();
	COLORREF FocusColor();
	COLORREF AccentColor();
	COLORREF ErrorColor();
	COLORREF WarningColor();
	COLORREF SuccessColor();
	HBRUSH Brush(ThemeColorRole role);
	HBRUSH WindowBrush();
	HBRUSH ControlBrush();

	// Applies the appropriate system theme, DWM title-bar attribute and colours
	// to a window and all its children.  Safe to call on Windows 7.
	void ApplyToWindow(HWND window);
	void ApplyToAllThreadWindows(DWORD threadId);
	// Returns true only when Automatic observed a Windows theme transition.
	bool RefreshSystemTheme();
}
