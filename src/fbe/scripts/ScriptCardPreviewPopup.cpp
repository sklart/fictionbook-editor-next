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
	m_open = open; m_hover = hover; m_mouseInside = false; m_anchor = anchor;
	const UINT dpi = UiMetrics::DpiForWindow(owner);
	UpdateFonts(dpi);
	const int pad = UiMetrics::ScaleForDpi(12, dpi);
	const int gap = UiMetrics::ScaleForDpi(8, dpi);
	const int width = UiMetrics::ScaleForDpi(340, dpi);
	const int contentWidth = width - 2 * pad;
	HDC dc = ::GetDC(m_window);
	if(!dc) return false;
	HFONT old = static_cast<HFONT>(::SelectObject(dc, m_linkFont ? m_linkFont : m_font));
	RECT linkSize = { 0, 0, contentWidth / 2, 0 };
	::DrawTextW(dc, m_more, -1, &linkSize, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
	const int linkWidth = (std::min)(contentWidth / 2, static_cast<int>(linkSize.right) + gap);
	::SelectObject(dc, m_titleFont ? m_titleFont : m_font);
	auto clipLines = [&](CString& value, int maxLines, int lineWidth) {
		TEXTMETRICW metrics = {};
		::GetTextMetricsW(dc, &metrics);
		const int limit = maxLines * (metrics.tmHeight + metrics.tmExternalLeading);
		auto measuredHeight = [&](const CString& candidate) {
			RECT rect = { 0, 0, lineWidth, 0 };
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
	m_titleHeight = clipLines(m_title, 2, contentWidth - linkWidth - gap);
	::SelectObject(dc, m_font);
	m_summaryHeight = clipLines(m_summary, 5, contentWidth);
	if(old) ::SelectObject(dc, old);
	::ReleaseDC(m_window, dc);
	const int linkHeight = UiMetrics::ScaleForDpi(23, dpi);
	m_headerHeight = (std::max)(m_titleHeight, linkHeight);
	const int height = 2 * pad + m_headerHeight + gap + m_summaryHeight;
	m_linkRect = { width - pad - linkWidth, pad, width - pad, pad + linkHeight };
	MONITORINFO monitor = { sizeof(monitor) };
	POINT point = { anchor.left, anchor.bottom };
	if(!::GetMonitorInfoW(::MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), &monitor))
		return false;
	int x = anchor.right - width;
	int y = anchor.bottom + UiMetrics::ScaleForDpi(4, dpi);
	if(x < monitor.rcWork.left) x = anchor.left;
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

bool ScriptCardPreviewPopup::ContainsTransition(POINT screenPoint) const
{
	RECT popup = {};
	if(!IsVisible() || !::GetWindowRect(m_window, &popup)) return false;
	if(::PtInRect(&m_anchor, screenPoint)) return true;
	const int sourceY = popup.top >= m_anchor.bottom ? m_anchor.bottom : m_anchor.top;
	const int targetY = popup.top >= m_anchor.bottom ? popup.top : popup.bottom;
	if(sourceY == targetY || (screenPoint.y < (std::min)(sourceY, targetY)) ||
		screenPoint.y > (std::max)(sourceY, targetY)) return false;
	const double progress = static_cast<double>(screenPoint.y - sourceY) / (targetY - sourceY);
	const int margin = UiMetrics::ScaleForDpi(5, UiMetrics::DpiForWindow(m_window));
	const int left = static_cast<int>(m_anchor.left + (popup.left - m_anchor.left) * progress) - margin;
	const int right = static_cast<int>(m_anchor.right + (popup.right - m_anchor.right) * progress) + margin;
	return screenPoint.x >= left && screenPoint.x <= right;
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
	RECT title = { pad, pad, m_linkRect.left - gap, pad + m_titleHeight };
	::DrawTextW(dc, m_title, -1, &title, DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
	RECT summary = { pad, pad + m_headerHeight + gap, client.right - pad, pad + m_headerHeight + gap + m_summaryHeight };
	::SelectObject(dc, m_font);
	::DrawTextW(dc, m_summary, -1, &summary, DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
	::SetTextColor(dc, accent);
	::SelectObject(dc, m_linkFont ? m_linkFont : m_font);
	RECT link = m_linkRect;
	::DrawTextW(dc, m_more, -1, &link, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
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
		::SetCursor(::LoadCursorW(NULL, IDC_HAND));
		return TRUE;
	case WM_LBUTTONUP:
		{
			const CString uid(self->m_uid);
			self->Hide();
			if(self->m_open) self->m_open(uid);
			return 0;
		}
	case WM_DESTROY: self->m_window = NULL; return 0;
	}
	return ::DefWindowProcW(window, message, wParam, lParam);
}
