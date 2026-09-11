#pragma once

#include "../extras/atlctrlsext.h"
#include "../res1.h"

typedef CWinTraits<WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT, WS_EX_CLIENTEDGE> CCustomEditWinTraits;

// Edit controls in the context bars keep the legacy parent notification used by
// CMainFrame. The control deliberately has no editor or document dependency.
class CCustomEdit : public CWindowImpl<CCustomEdit, CEdit, CCustomEditWinTraits>, public CEditCommands<CCustomEdit>
{
public:
	DECLARE_WND_SUPERCLASS(NULL, CEdit::GetWndClassName())
	BEGIN_MSG_MAP(CCustomEdit)
		MESSAGE_HANDLER(WM_CHAR, OnChar)
		CHAIN_MSG_MAP_ALT(CEditCommands<CCustomEdit>, 1)
	END_MSG_MAP()
	LRESULT OnChar(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
	{
		if(wParam == VK_RETURN) ::PostMessage(::GetParent(GetParent()), WM_COMMAND, MAKELONG(GetDlgCtrlID(), IDN_ED_RETURN), (LPARAM)m_hWnd);
		bHandled = FALSE;
		return 0;
	}
};

class CCustomStatic : public CWindowImpl<CCustomStatic, CStatic>
{
	HFONT m_font;
	bool m_enabled;
public:
	CCustomStatic() : m_font(0), m_enabled(false) {}
	void DoPaint(CDCHandle dc);
	LRESULT OnPaint(UINT, WPARAM wParam, LPARAM, BOOL&);
	LRESULT OnSetFont(UINT, WPARAM wParam, LPARAM, BOOL& bHandled);
	void SetFont(HFONT font);
	void SetEnabled(bool enabled = true);
	BEGIN_MSG_MAP(CCustomStatic)
		MESSAGE_HANDLER(WM_SETFONT, OnSetFont)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
	END_MSG_MAP()
};

class CTableToolbarsWindow : public CFrameWindowImpl<CTableToolbarsWindow>, public CUpdateUI<CTableToolbarsWindow>
{
public:
	BEGIN_UPDATE_UI_MAP(CTableToolbarsWindow)
	END_UPDATE_UI_MAP()
};
