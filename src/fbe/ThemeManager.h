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

namespace ThemeManager
{
	void SetSelectedTheme(InterfaceTheme theme);
	InterfaceTheme GetSelectedTheme();
	bool IsDark();
	COLORREF WindowColor();
	COLORREF TextColor();
	COLORREF ControlColor();
	HBRUSH WindowBrush();
	HBRUSH ControlBrush();

	// Applies the appropriate system theme, DWM title-bar attribute and colours
	// to a window and all its children.  Safe to call on Windows 7.
	void ApplyToWindow(HWND window);
	void ApplyToAllThreadWindows(DWORD threadId);
	// Returns true only when Automatic observed a Windows theme transition.
	bool RefreshSystemTheme();
}
