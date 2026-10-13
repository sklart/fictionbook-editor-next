#include "stdafx.h"
#include "ScriptCardPreviewPopup.h"
#include "../ThemeManager.h"
#include "../UiMetrics.h"
#include <windowsx.h>

namespace {
constexpr wchar_t kPreviewClass[] = L"FbeScriptCardPreview";
}

ScriptCardPreviewPopup::~ScriptCardPreviewPopup()
{
	if(m_window) ::DestroyWindow(m_window);
	if(m_font) ::DeleteObject(m_font);
	if(m_titleFont) ::DeleteObject(m_titleFont);
	if(m_linkFont) ::DeleteObject(m_linkFont);
}

void ScriptCardPreviewPopup::UpdateFonts(UINT dpi)
{
	if(m_font) ::DeleteObject(m_font);
	if(m_titleFont) ::DeleteObject(m_titleFont);
	if(m_linkFont) ::DeleteObject(m_linkFont);
	m_font = UiMetrics::CreateDialogFontForDpi(dpi);
	LOGFONTW face = {};
	if(!m_font || !::GetObjectW(m_font, sizeof(face), &face)) return;
	face.lfWeight = FW_SEMIBOLD;
	face.lfHeight -= UiMetrics::ScaleForDpi(2, dpi);
	m_titleFont = ::CreateFontIndirectW(&face);
	face.lfWeight = FW_NORMAL;
	face.lfUnderline = TRUE;
	face.lfHeight += UiMetrics::ScaleForDpi(2, dpi);
	m_linkFont = ::CreateFontIndirectW(&face);
}

bool ScriptCardPreviewPopup::Show(HWND owner, const RECT& anchor, const CString& uid,
	const CString& title, const CString& summary, const CString& more,
	const std::function<void(const CString&)>& open,
	const std::function<void(bool)>& hover)
{
	if(!::IsWindow(owner) || uid.IsEmpty()) return false;
	if(!m_window)
	{
		WNDCLASSW cls = {};
		cls.lpfnWndProc = WindowProc;
		cls.hInstance = _Module.GetModuleInstance();
		cls.lpszClassName = kPreviewClass;
		cls.hCursor = ::LoadCursorW(NULL, IDC_ARROW);
		::RegisterClassW(&cls);
		m_window = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
			kPreviewClass, L"", WS_POPUP, 0, 0, 0, 0, ::GetAncestor(owner, GA_ROOT), NULL,
			_Module.GetModuleInstance(), this);
		if(!m_window) return false;
	}
	m_uid = uid; m_title = title; m_summary = summary; m_more = more;
	m_open = open; m_hover = hover; m_mouseInside = false;
	const UINT dpi = UiMetrics::DpiForWindow(owner);
	UpdateFonts(dpi);
	const int pad = UiMetrics::ScaleForDpi(12, dpi);
	const int gap = UiMetrics::ScaleForDpi(8, dpi);
	const int width = UiMetrics::ScaleForDpi(340, dpi);
	const int contentWidth = width - 2 * pad;
	HDC dc = ::GetDC(m_window);
	if(!dc) return false;
	HFONT old = static_cast<HFONT>(::SelectObject(dc, m_titleFont ? m_titleFont : m_font));
	auto clipLines = [&](CString& value, int maxLines) {
		TEXTMETRICW metrics = {};
		::GetTextMetricsW(dc, &metrics);
		const int limit = maxLines * (metrics.tmHeight + metrics.tmExternalLeading);
		auto measuredHeight = [&](const CString& candidate) {
			RECT rect = { 0, 0, contentWidth, 0 };
			::DrawTextW(dc, candidate, -1, &rect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
			return static_cast<int>(rect.bottom);
		};
		if(measuredHeight(value) > limit)
		{
			int low = 0, high = value.GetLength();
			while(low < high)
			{
				const int middle = (low + high + 1) / 2;
				if(measuredHeight(value.Left(middle) + L"…") <= limit) low = middle;
				else high = middle - 1;
			}
			value = value.Left(low) + L"…";
		}
		return (std::max)(static_cast<int>(metrics.tmHeight), measuredHeight(value));
	};
	m_titleHeight = clipLines(m_title, 2);
	::SelectObject(dc, m_font);
	m_summaryHeight = clipLines(m_summary, 5);
	if(old) ::SelectObject(dc, old);
	::ReleaseDC(m_window, dc);
	const int linkHeight = UiMetrics::ScaleForDpi(23, dpi);
	const int height = 2 * pad + m_titleHeight + gap + m_summaryHeight + gap + linkHeight;
	m_linkRect = { pad, height - pad - linkHeight, width - pad, height - pad };
	MONITORINFO monitor = { sizeof(monitor) };
	POINT point = { anchor.left, anchor.bottom };
	if(!::GetMonitorInfoW(::MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), &monitor))
		return false;
	int x = anchor.left;
	int y = anchor.bottom + UiMetrics::ScaleForDpi(4, dpi);
	if(x + width > monitor.rcWork.right) x = anchor.right - width;
	if(y + height > monitor.rcWork.bottom) y = anchor.top - height - UiMetrics::ScaleForDpi(4, dpi);
	x = (std::max)(static_cast<int>(monitor.rcWork.left), (std::min)(x, static_cast<int>(monitor.rcWork.right) - width));
	y = (std::max)(static_cast<int>(monitor.rcWork.top), (std::min)(y, static_cast<int>(monitor.rcWork.bottom) - height));
	::SetWindowPos(m_window, HWND_TOPMOST, x, y, width, height,
		SWP_NOACTIVATE | SWP_SHOWWINDOW);
	::InvalidateRect(m_window, NULL, TRUE);
	return true;
}

void ScriptCardPreviewPopup::Hide()
{
	if(m_window) ::ShowWindow(m_window, SW_HIDE);
	m_mouseInside = false;
}

void ScriptCardPreviewPopup::RefreshTheme()
{
	if(m_window) ::InvalidateRect(m_window, NULL, TRUE);
}

bool ScriptCardPreviewPopup::IsVisible() const
{
	return m_window && ::IsWindowVisible(m_window) != FALSE;
}

bool ScriptCardPreviewPopup::Contains(POINT screenPoint) const
{
	RECT rect = {};
	return IsVisible() && ::GetWindowRect(m_window, &rect) && ::PtInRect(&rect, screenPoint) != FALSE;
}

void ScriptCardPreviewPopup::Paint()
{
	PAINTSTRUCT paint = {};
	HDC dc = ::BeginPaint(m_window, &paint);
	if(!dc) return;
	RECT client = {}; ::GetClientRect(m_window, &client);
	const bool contrast = ThemeManager::IsHighContrast();
	const COLORREF background = contrast ? ::GetSysColor(COLOR_WINDOW) : ThemeManager::WindowColor();
	const COLORREF foreground = contrast ? ::GetSysColor(COLOR_WINDOWTEXT) : ThemeManager::TextColor();
	const COLORREF accent = contrast ? ::GetSysColor(COLOR_HOTLIGHT) : ThemeManager::AccentColor();
	HBRUSH fill = ::CreateSolidBrush(background);
	if(fill) { ::FillRect(dc, &client, fill); ::DeleteObject(fill); }
	::FrameRect(dc, &client, static_cast<HBRUSH>(::GetStockObject(GRAY_BRUSH)));
	const UINT dpi = UiMetrics::DpiForWindow(m_window);
	const int pad = UiMetrics::ScaleForDpi(12, dpi), gap = UiMetrics::ScaleForDpi(8, dpi);
	::SetBkMode(dc, TRANSPARENT);
	::SetTextColor(dc, foreground);
	HFONT old = static_cast<HFONT>(::SelectObject(dc, m_titleFont ? m_titleFont : m_font));
	RECT title = { pad, pad, client.right - pad, pad + m_titleHeight };
	::DrawTextW(dc, m_title, -1, &title, DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
	RECT summary = { pad, title.bottom + gap, client.right - pad, title.bottom + gap + m_summaryHeight };
	::SelectObject(dc, m_font);
	::DrawTextW(dc, m_summary, -1, &summary, DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
	::SetTextColor(dc, accent);
	::SelectObject(dc, m_linkFont ? m_linkFont : m_font);
	RECT link = m_linkRect;
	::DrawTextW(dc, m_more, -1, &link, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
	if(old) ::SelectObject(dc, old);
	::EndPaint(m_window, &paint);
}

LRESULT CALLBACK ScriptCardPreviewPopup::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	ScriptCardPreviewPopup* self = reinterpret_cast<ScriptCardPreviewPopup*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
	if(message == WM_NCCREATE)
	{
		self = static_cast<ScriptCardPreviewPopup*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
		::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		return TRUE;
	}
	if(!self) return ::DefWindowProcW(window, message, wParam, lParam);
	switch(message)
	{
	case WM_PAINT: self->Paint(); return 0;
	case WM_ERASEBKGND: return 1;
	case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
	case WM_MOUSEMOVE:
		if(!self->m_mouseInside)
		{
			self->m_mouseInside = true;
			TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, window, 0 };
			::TrackMouseEvent(&track);
			if(self->m_hover) self->m_hover(true);
		}
		return 0;
	case WM_MOUSELEAVE:
		self->m_mouseInside = false;
		if(self->m_hover) self->m_hover(false);
		return 0;
	case WM_SETCURSOR:
		{
			POINT point = {}; ::GetCursorPos(&point); ::ScreenToClient(window, &point);
			if(::PtInRect(&self->m_linkRect, point)) { ::SetCursor(::LoadCursorW(NULL, IDC_HAND)); return TRUE; }
			break;
		}
	case WM_LBUTTONUP:
		{
			POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			if(::PtInRect(&self->m_linkRect, point))
			{
				const CString uid(self->m_uid);
				self->Hide();
				if(self->m_open) self->m_open(uid);
			}
			return 0;
		}
	case WM_DESTROY: self->m_window = NULL; return 0;
	}
	return ::DefWindowProcW(window, message, wParam, lParam);
}
