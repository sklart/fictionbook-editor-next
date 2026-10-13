#pragma once

#include <windows.h>
#include <atlstr.h>
#include <functional>

// One reusable, non-activating preview for the Scripts tree. The tree owns it.
class ScriptCardPreviewPopup
{
public:
	~ScriptCardPreviewPopup();
	bool Show(HWND owner, const RECT& anchor, const CString& uid, const CString& title,
		const CString& summary, const CString& more,
		const std::function<void(const CString&)>& open,
		const std::function<void(bool)>& hover);
	void Hide();
	void RefreshTheme();
	bool IsVisible() const;
	bool Contains(POINT screenPoint) const;
	HWND Window() const { return m_window; }
	const CString& Uid() const { return m_uid; }
	const CString& Summary() const { return m_summary; }

private:
	static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	void Paint();
	void UpdateFonts(UINT dpi);
	HWND m_window = NULL;
	HFONT m_font = NULL;
	HFONT m_titleFont = NULL;
	HFONT m_linkFont = NULL;
	RECT m_linkRect = {};
	int m_titleHeight = 0;
	int m_summaryHeight = 0;
	CString m_uid, m_title, m_summary, m_more;
	std::function<void(const CString&)> m_open;
	std::function<void(bool)> m_hover;
	bool m_mouseInside = false;
};
