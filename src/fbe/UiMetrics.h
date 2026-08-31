#pragma once

// Shared Windows UI metrics for the native FBE chrome.  The metrics are
// deliberately based on the message font, not on a named font family.
class UiMetrics
{
public:
	static void UpdateForWindow(HWND window);
	static void Shutdown();

	static HFONT DialogFont();
	static int Scale(int px);
	static int SmallGap();
	static int NormalGap();
	static int LargeGap();
	static int IconSize();
	static int ToolbarHeight();

private:
	static void EnsureFont();
	static UINT GetDpi(HWND window);

	static HFONT s_dialogFont;
	static UINT s_dpi;
};
