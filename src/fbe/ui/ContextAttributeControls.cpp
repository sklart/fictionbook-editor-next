#include "stdafx.h"
#include "ContextAttributeControls.h"

void CCustomStatic::DoPaint(CDCHandle dc)
{
	RECT rc; GetClientRect(&rc);
	::FillRect(dc, &rc, ::GetSysColorBrush(COLOR_BTNFACE));
	HFONT oldFont = (HFONT)SelectObject(dc, m_font);
	const int length = GetWindowTextLength();
	std::vector<wchar_t> text(length + 1);
	GetWindowText(&text[0], length + 1);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(GetSysColor(m_enabled ? COLOR_BTNTEXT : COLOR_GRAYTEXT));
	dc.DrawText(&text[0], -1, &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	SelectObject(dc, oldFont);
}

LRESULT CCustomStatic::OnPaint(UINT, WPARAM wParam, LPARAM, BOOL&)
{
	if(wParam != NULL) DoPaint((HDC)wParam); else { CPaintDC dc(m_hWnd); DoPaint(dc.m_hDC); }
	return 0;
}

LRESULT CCustomStatic::OnSetFont(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
{
	m_font = reinterpret_cast<HFONT>(wParam); Invalidate(); bHandled = FALSE; return 0;
}

void CCustomStatic::SetFont(HFONT font)
{
	ATLASSERT(IsWindow()); ::SendMessage(m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void CCustomStatic::SetEnabled(bool enabled) { m_enabled = enabled; Invalidate(); }
