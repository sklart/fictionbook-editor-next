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

	// Keeps FBE-owned native popups on the effective app palette.  The native
	// renderer remains system-owned in Light and High Contrast.
	UINT TrackPopupMenu(HMENU menu, UINT flags, int x, int y, HWND owner);
	// Registers a bitmap whose lifetime is owned by the caller.  The bitmap is
	// attached recursively immediately before an FBE native popup is shown.
	void RegisterNativeMenuBitmap(UINT command, HBITMAP bitmap);
	void UnregisterNativeMenuBitmap(UINT command);
	// FBE-owned replacement for ordinary confirmation and error message boxes.
	// Light and High Contrast deliberately keep the native system dialog.
	int MessageBox(HWND owner, LPCWSTR message, LPCWSTR caption, UINT type);
	// Applies the theme as soon as a TaskDialog has a valid HWND, while keeping
	// a caller-provided callback and its data intact.
	HRESULT TaskDialogIndirect(const TASKDIALOGCONFIG& config, int* button, int* radioButton, BOOL* verification);

	// Applies the appropriate system theme, DWM title-bar attribute and colours
	// to a window and all its children.  Safe to call on Windows 7.
	void ApplyToWindow(HWND window);
	void ApplyToAllThreadWindows(DWORD threadId);
	// Returns true only when Automatic observed a Windows theme transition.
	bool RefreshSystemTheme();
}
