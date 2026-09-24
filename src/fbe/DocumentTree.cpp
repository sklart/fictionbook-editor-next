#include "stdafx.h"
#include "resource.h"
#include "res1.h"
#include "utils.h"

#include "DocumentTree.h"
#include "ElementDescMnr.h"

#include "Settings.h"
extern CSettings _Settings;

extern CElementDescMnr _EDMnr;

namespace
{
const UINT_PTR kDocumentTreeViewBarThemeSubclassId = 0xFBE4;
const UINT_PTR kDocumentTreeViewBarWindowThemeSubclassId = 0xFBE5;
const UINT kDocumentTreeShowStructureCommand = 57872;
const UINT kDocumentTreeShowScriptsCommand = 57873;
static_assert(kDocumentTreeShowStructureCommand > ID_VIEW_SCRIPT_TOOLBAR_DYNAMIC_LAST, "Navigation mode commands must not overlap dynamic toolbar commands");
static_assert(kDocumentTreeShowScriptsCommand <= 0xffffu, "Navigation mode command must fit WM_COMMAND");

bool ShowNativeDocumentTreeViewBarPopup(HWND commandBar, int item)
{
	const HMENU menu = reinterpret_cast<HMENU>(::SendMessage(commandBar, CBRM_GETMENU, 0, 0));
	if(menu == NULL || item < 0 || item >= ::GetMenuItemCount(menu)) return false;
	const HMENU popup = ::GetSubMenu(menu, item);
	if(popup == NULL) return false;

	RECT itemRect = {};
	if(!::SendMessage(commandBar, TB_GETITEMRECT, item, reinterpret_cast<LPARAM>(&itemRect))) return false;
	POINT point = { itemRect.left, itemRect.bottom };
	::ClientToScreen(commandBar, &point);
	const HWND owner = ::GetParent(commandBar);
	const UINT command = ::TrackPopupMenuEx(popup,
		TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD,
		point.x, point.y, owner, NULL);
	if(command != 0)
		::SendMessage(owner, WM_COMMAND, MAKEWPARAM(command, 0), 0);
	return true;
}

LRESULT CALLBACK DocumentTreeViewBarWindowThemeProc(HWND window, UINT message, WPARAM wParam,
	LPARAM lParam, UINT_PTR, DWORD_PTR)
{
	if(ThemeManager::IsDark() && !ThemeManager::IsHighContrast())
	{
		int item = -1;
		if(message == WM_LBUTTONDOWN)
		{
			POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			item = static_cast<int>(::SendMessage(window, TB_HITTEST, 0, reinterpret_cast<LPARAM>(&point)));
		}
		else if(message == WM_KEYDOWN && (wParam == VK_DOWN || wParam == VK_RETURN || wParam == VK_SPACE))
			item = static_cast<int>(::SendMessage(window, TB_GETHOTITEM, 0, 0));
		if(item >= 0 && ShowNativeDocumentTreeViewBarPopup(window, item)) return 0;
	}
	const LRESULT result = ::DefSubclassProc(window, message, wParam, lParam);
	if(message == WM_FBE_THEMECHANGED)
	{
		// ThemeManager disables visual styles on ordinary toolbars so their
		// surfaces can be rendered from the shared palette. This toolbar is also
		// a WTL command bar, whose attached popup menus need the native dark-menu
		// visual style instead of the legacy COLOR_MENU owner drawing.
		const bool dark = ThemeManager::IsDark() && !ThemeManager::IsHighContrast();
		::SetWindowTheme(window, dark ? L"DarkMode_Explorer" : L"Explorer", NULL);
		::SendMessage(window, WM_SETTINGCHANGE, 0, 0);
		::RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
	}
	return result;
}

LRESULT CALLBACK DocumentTreeViewBarThemeProc(HWND window, UINT message, WPARAM wParam,
	LPARAM lParam, UINT_PTR, DWORD_PTR reference)
{
	if(message != WM_NOTIFY || !ThemeManager::IsDark() || ThemeManager::IsHighContrast())
		return ::DefSubclassProc(window, message, wParam, lParam);
	LPNMHDR header = reinterpret_cast<LPNMHDR>(lParam);
	HWND viewBar = reinterpret_cast<HWND>(reference);
	if(header == NULL || header->hwndFrom != viewBar || header->code != NM_CUSTOMDRAW)
		return ::DefSubclassProc(window, message, wParam, lParam);

	NMTBCUSTOMDRAW* draw = reinterpret_cast<NMTBCUSTOMDRAW*>(header);
	if(draw->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		RECT client = {}; ::GetClientRect(viewBar, &client);
		::FillRect(draw->nmcd.hdc, &client, ThemeManager::ControlBrush());
		return CDRF_NOTIFYITEMDRAW;
	}
	if(draw->nmcd.dwDrawStage != CDDS_ITEMPREPAINT)
		return CDRF_DODEFAULT;

	const bool disabled = (draw->nmcd.uItemState & (CDIS_DISABLED | CDIS_GRAYED)) != 0;
	const bool pressed = (draw->nmcd.uItemState & CDIS_SELECTED) != 0;
	const bool hot = (draw->nmcd.uItemState & CDIS_HOT) != 0;
	const ThemeColorRole surface = pressed ? THEME_COLOR_PRESSED : hot ? THEME_COLOR_HOVER : THEME_COLOR_CONTROL;
	::FillRect(draw->nmcd.hdc, &draw->nmcd.rc, ThemeManager::Brush(surface));
	if(hot || pressed)
		::FrameRect(draw->nmcd.hdc, &draw->nmcd.rc, ThemeManager::Brush(THEME_COLOR_BORDER));

	wchar_t text[256] = {};
	TBBUTTONINFOW button = {}; button.cbSize = sizeof(button); button.dwMask = TBIF_TEXT;
	button.pszText = text; button.cchText = _countof(text);
	::SendMessage(viewBar, TB_GETBUTTONINFOW, static_cast<WPARAM>(draw->nmcd.dwItemSpec), reinterpret_cast<LPARAM>(&button));
	RECT textRect = draw->nmcd.rc; ::InflateRect(&textRect, -6, 0);
	HFONT font = reinterpret_cast<HFONT>(::SendMessage(viewBar, WM_GETFONT, 0, 0));
	HGDIOBJ oldFont = font ? ::SelectObject(draw->nmcd.hdc, font) : NULL;
	::SetBkMode(draw->nmcd.hdc, TRANSPARENT);
	::SetTextColor(draw->nmcd.hdc, disabled ? ThemeManager::DisabledTextColor() : ThemeManager::TextColor());
	::DrawTextW(draw->nmcd.hdc, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
	if(oldFont) ::SelectObject(draw->nmcd.hdc, oldFont);
	return CDRF_SKIPDEFAULT;
}
}

BOOL CTreeWithToolBar::ModifyStyle(DWORD dwRemove, DWORD dwAdd, UINT nFlags) throw()
{
	ATLASSERT(::IsWindow(m_hWnd));

	DWORD dwStyle = ::GetWindowLong(m_hWnd, GWL_STYLE);
	DWORD dwNewStyle = (dwStyle & ~dwRemove) | dwAdd;
	if(dwStyle == dwNewStyle)
		return FALSE;

	::SetWindowLong(m_hWnd, GWL_STYLE, dwNewStyle);
	if(nFlags != 0)
	{
		::SetWindowPos(m_hWnd, NULL, 0, 0, 0, 0,
			SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | nFlags);
	}

	return TRUE;
}

LRESULT CTreeWithToolBar::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	m_toolbarOrientation = CTreeWithToolBar::bottom;

	LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);

	m_tree.Create(*this, rcDefault);
	m_tree.SetBkColor(ThemeManager::WindowColor());
	m_tree.SetTextColor(ThemeManager::TextColor());
	m_tree.SetLineColor(ThemeManager::SeparatorColor());
	m_rebar = CFrameWindowImplBase<>::CreateSimpleReBarCtrl(*this, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CCS_NODIVIDER | CCS_NOPARENTALIGN | CS_HREDRAW);
	m_toolbar = CFrameWindowImplBase<>::CreateSimpleToolBarCtrl(*this, IDR_DOCUMENT_TREE, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);
	CFrameWindowImplBase<>::AddSimpleReBarBandCtrl(m_rebar, m_toolbar);
	if(m_rebar.IsWindow())
	{
		m_rebarBaseStyle = ::GetWindowLongPtr(m_rebar, GWL_STYLE);
		REBARBANDINFO band = {}; band.cbSize = sizeof(band); band.fMask = RBBIM_STYLE;
		if(m_rebar.GetBandInfo(0, &band)) m_rebarBandBaseStyle = band.fStyle;
		m_rebarThemeStateCaptured = true;
	}

	_EDMnr.InitStandartEDs();
	int edsCount = _EDMnr.GetStEDsCount();
	for(int i = 0; i < edsCount; ++i)
	{
		CElementDescriptor* eld = _EDMnr.GetStED(i);
		eld->SetViewInTree(_Settings.GetDocTreeItemState(eld->GetCaption(), eld->ViewInTree()));
	}
	
	_EDMnr.InitScriptEDs();
	RECT rect;
	SetRect(&rect, 0, 0, 500, 20);
	this->ModifyStyle(0, WS_POPUP, 0);
	m_view_bar.Create(*this, rect, NULL, ATL_SIMPLE_TOOLBAR_PANE_STYLE);
	m_view_bar.SetStyle(ATL_SIMPLE_TOOLBAR_PANE_STYLE);
	FillViewBar();
	::SetWindowSubclass(m_view_bar, DocumentTreeViewBarWindowThemeProc,
		kDocumentTreeViewBarWindowThemeSubclassId, 0);
	::SetWindowSubclass(m_hWnd, DocumentTreeViewBarThemeProc, kDocumentTreeViewBarThemeSubclassId,
		reinterpret_cast<DWORD_PTR>(static_cast<HWND>(m_view_bar)));
	this->ModifyStyle(WS_POPUP, 0, 0);

	m_maxTbwidth = 1000;
	::MoveWindow(m_rebar, 0, 0, m_maxTbwidth, 0, true);

//	m_toolbar.HideButton(ID_DT_DELETE);
	bHandled = FALSE;
	return lRet;
}


LRESULT CTreeWithToolBar::OnDestroy(UINT /* unused: uMsg */, WPARAM /* unused: wParam */, LPARAM /* unused: lParam */, BOOL& bHandled)
{
	if(m_view_bar.IsWindow())
		::RemoveWindowSubclass(m_view_bar, DocumentTreeViewBarWindowThemeProc, kDocumentTreeViewBarWindowThemeSubclassId);
	::RemoveWindowSubclass(m_hWnd, DocumentTreeViewBarThemeProc, kDocumentTreeViewBarThemeSubclassId);
	bHandled=FALSE;
	return 0;
}

LRESULT CTreeWithToolBar::OnClose(UINT /* unused: uMsg */, WPARAM /* unused: wParam */, LPARAM /* unused: lParam */, BOOL& /* unused: bHandled */)
{    
	return 0;
}

LRESULT CTreeWithToolBar::OnSize(UINT /* unused: uMsg */, WPARAM /* unused: wParam */, LPARAM /* unused: lParam */, BOOL& /* unused: bHandled */)
{
	RECT clientRect = {0, 0, 0, 0};
	RECT rebarRect = {0, 0, 0, 0};
	RECT treeRect = {0, 0, 0, 0};
	RECT viewBarRect = {0, 0, 0, 0};
	
	this->GetClientRect(&clientRect);
	::GetWindowRect(m_toolbar, &rebarRect);
	::GetWindowRect(m_view_bar, &viewBarRect);
	const bool dark = ThemeManager::IsDark() && !ThemeManager::IsHighContrast();
	int rebarHight = rebarRect.bottom - rebarRect.top + (dark ? 0 : GetSystemMetrics(SM_CYEDGE) * 2);
	rebarRect.left = treeRect.left = clientRect.left;
	rebarRect.right = treeRect.right = clientRect.right;

	int viewBarHight = viewBarRect.bottom - viewBarRect.top;
	int viewBarWidth = viewBarRect.right - viewBarRect.left;

	viewBarRect.left = clientRect.left;
	viewBarRect.right = clientRect.left + viewBarWidth;

	bool moved = false;

	if(m_toolbarOrientation == CTreeWithToolBar::bottom)
	{
		if(rebarRect.top != clientRect.bottom - rebarHight)
			moved = true;

		rebarRect.top = clientRect.bottom - rebarHight;
		
		rebarRect.bottom = rebarRect.top + rebarHight;
		treeRect.top = clientRect.top + viewBarHight;
		treeRect.bottom = rebarRect.top;

		viewBarRect.top = clientRect.top;
		viewBarRect.bottom = viewBarRect.top + viewBarHight;
	}

	// ?????? ????? ????? ???????????? ??????. ??? ???? ???????? ???????? ??? ????.
	/*if(m_toolbarOrientation == CTreeWithToolBar::top)
	{
		rebarRect.top = clientRect.top;
		rebarRect.bottom = rebarRect.top + rebarHight;

		treeRect.top = rebarRect.bottom ;
		treeRect.bottom = clientRect.bottom;
	}*/

	if((rebarRect.right - rebarRect.left) > m_maxTbwidth || moved)
	{
		::MoveWindow(m_rebar, rebarRect.left, rebarRect.top, rebarRect.right - rebarRect.left, rebarRect.bottom - rebarRect.top, true);
		::MoveWindow(m_view_bar, viewBarRect.left, viewBarRect.top, viewBarRect.right - viewBarRect.left, viewBarRect.bottom - viewBarRect.top, true);
		m_maxTbwidth = rebarRect.right - rebarRect.left;
	}
	
	::MoveWindow(m_tree, treeRect.left, treeRect.top, treeRect.right - treeRect.left, treeRect.bottom - treeRect.top, true);	

	return 0;
}

LRESULT CTreeWithToolBar::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&)
{
	if(!m_tree.IsWindow()) return 0;
	// The tree may be created after its parent receives the initial theme
	// notification. Apply the effective palette both at creation and on every
	// refresh, including the return from Dark to Light.
	m_tree.SetBkColor(ThemeManager::WindowColor());
	m_tree.SetTextColor(ThemeManager::TextColor());
	m_tree.SetLineColor(ThemeManager::SeparatorColor());
	const bool dark = ThemeManager::IsDark() && !ThemeManager::IsHighContrast();
	if(m_rebar.IsWindow())
	{
		if(!m_rebarThemeStateCaptured)
		{
			m_rebarBaseStyle = ::GetWindowLongPtr(m_rebar, GWL_STYLE);
			REBARBANDINFO baseBand = {}; baseBand.cbSize = sizeof(baseBand); baseBand.fMask = RBBIM_STYLE;
			if(m_rebar.GetBandInfo(0, &baseBand)) m_rebarBandBaseStyle = baseBand.fStyle;
			m_rebarThemeStateCaptured = true;
		}
		::SetWindowLongPtr(m_rebar, GWL_STYLE, dark ? m_rebarBaseStyle & ~static_cast<LONG_PTR>(RBS_BANDBORDERS) : m_rebarBaseStyle);
		::SendMessage(m_rebar, RB_SETBKCOLOR, 0, dark ? ThemeManager::ControlColor() : ::GetSysColor(COLOR_BTNFACE));
		for(int index = 0; index < static_cast<int>(m_rebar.GetBandCount()); ++index)
		{
			REBARBANDINFO band = {}; band.cbSize = sizeof(band); band.fMask = RBBIM_STYLE | RBBIM_COLORS;
			if(!m_rebar.GetBandInfo(index, &band)) continue;
			band.fStyle = dark ? band.fStyle & ~RBBS_CHILDEDGE : m_rebarBandBaseStyle;
			band.clrBack = dark ? ThemeManager::ControlColor() : ::GetSysColor(COLOR_BTNFACE);
			band.clrFore = dark ? ThemeManager::TextColor() : ::GetSysColor(COLOR_BTNTEXT);
			m_rebar.SetBandInfo(index, &band);
		}
		::SetWindowPos(m_rebar, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}
	if(m_toolbar.IsWindow()) ::SendMessage(m_toolbar, CCM_SETBKCOLOR, 0, ThemeManager::ControlColor());
	if(m_view_bar.IsWindow())
	{
		::SendMessage(m_view_bar, CCM_SETBKCOLOR, 0, ThemeManager::ControlColor());
		COLORSCHEME colours = {}; colours.dwSize = sizeof(colours);
		colours.clrBtnHighlight = ThemeManager::HoverColor();
		colours.clrBtnShadow = ThemeManager::BorderColor();
		::SendMessage(m_view_bar, TB_SETCOLORSCHEME, 0, reinterpret_cast<LPARAM>(&colours));
		::InvalidateRect(m_view_bar, NULL, TRUE);
	}
	::RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
	return 0;
}

LRESULT CTreeWithToolBar::OnThemeEraseBackground(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
{
	if(!ThemeManager::IsDark() || ThemeManager::IsHighContrast()) { bHandled = FALSE; return 0; }
	RECT client = {}; GetClientRect(&client);
	::FillRect(reinterpret_cast<HDC>(wParam), &client, ThemeManager::ControlBrush());
	return 1;
}

LRESULT CTreeWithToolBar::OnToolbarCustomDraw(int, LPNMHDR header, BOOL& bHandled)
{
	if(!ThemeManager::IsDark() || ThemeManager::IsHighContrast() || (header->hwndFrom != m_toolbar && header->hwndFrom != m_view_bar))
	{
		bHandled = FALSE;
		return 0;
	}
	NMTBCUSTOMDRAW* draw = reinterpret_cast<NMTBCUSTOMDRAW*>(header);
	if(draw->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		RECT client = {}; ::GetClientRect(header->hwndFrom, &client);
		::FillRect(draw->nmcd.hdc, &client, ThemeManager::ControlBrush());
		return CDRF_NOTIFYITEMDRAW;
	}
	if(draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
	{
		const bool disabled = (draw->nmcd.uItemState & (CDIS_DISABLED | CDIS_GRAYED)) != 0;
		draw->clrText = disabled ? ThemeManager::DisabledTextColor() : ThemeManager::TextColor();
		draw->clrTextHighlight = ThemeManager::SelectionTextColor();
		draw->clrBtnFace = ThemeManager::ControlColor();
		draw->clrBtnHighlight = ThemeManager::HoverColor();
		draw->clrHighlightHotTrack = ThemeManager::HoverColor();
	}
	return CDRF_DODEFAULT;
}

void CTreeWithToolBar::GetDocumentStructure(const MSHTML::IHTMLDocument2Ptr& v)
{
	if(m_tree.IsScriptMode()) return;
	m_tree.GetDocumentStructure(v);
}

void CTreeWithToolBar::UpdateDocumentStructure(const MSHTML::IHTMLDocument2Ptr& v,MSHTML::IHTMLDOMNodePtr node)
{
	if(m_tree.IsScriptMode()) return;
	m_tree.UpdateDocumentStructure(v, node);
}

void CTreeWithToolBar::HighlightItemAtPos(MSHTML::IHTMLElement *p)
{
	if(m_tree.IsScriptMode()) return;
	m_tree.HighlightItemAtPos(p);
}

CTreeItem CTreeWithToolBar::GetSelectedItem()
{
	return m_tree.GetSelectedItem();
}

LRESULT CTreeWithToolBar::ForwardWMCommand(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /* unused: bHandled */)
{
	DWORD wParam = MAKELONG(wID, wNotifyCode);
	DWORD lParam = (LPARAM)hWndCtl;
	return ::SendMessage(m_tree, WM_COMMAND, wParam, lParam);
}

void CTreeWithToolBar::FillViewBar()
{
	unsigned int st_count = _EDMnr.GetStEDsCount();
	unsigned int count = _EDMnr.GetEDsCount();

	m_st_menu = ::CreateMenu();	
	m_script_menu = ::CreateMenu();
	m_navigation_menu = ::CreateMenu();
	HMENU bar = ::CreateMenu();


	wchar_t elsMenuItem[MAX_LOAD_STRING + 1];
	wchar_t scriptsMenuItem[MAX_LOAD_STRING + 1];

	FbeLoadString(_Module.GetResourceInstance(), IDS_DOCTREE_MENU_ELEMENTS, elsMenuItem, MAX_LOAD_STRING);
	FbeLoadString(_Module.GetResourceInstance(), IDS_DOCTREE_MENU_SCRIPTS, scriptsMenuItem, MAX_LOAD_STRING);

	::AppendMenu(bar, MF_POPUP|MF_STRING, (UINT)(HMENU)m_st_menu, elsMenuItem);
	::AppendMenu(bar, MF_POPUP|MF_STRING, (UINT)(HMENU)m_script_menu, scriptsMenuItem);
	::AppendMenu(m_navigation_menu, MF_STRING | (_Settings.DocumentTreeScripts() ? MF_UNCHECKED : MF_CHECKED), kDocumentTreeShowStructureCommand,
		FbeLoadRuntimeStringByKey(L"fbe.document_tree.mode.structure", L"Document structure"));
	::AppendMenu(m_navigation_menu, MF_STRING | (_Settings.DocumentTreeScripts() ? MF_CHECKED : MF_UNCHECKED), kDocumentTreeShowScriptsCommand,
		FbeLoadRuntimeStringByKey(L"fbe.document_tree.mode.scripts", L"Scripts"));
	::AppendMenu(bar, MF_POPUP|MF_STRING, (UINT)(HMENU)m_navigation_menu,
		FbeLoadRuntimeStringByKey(L"fbe.document_tree.mode.caption", L"View"));

	int picType = 0;
	HANDLE picHandle = 0;

	for(unsigned int i = 0; i < st_count; ++i)
	{
		::AppendMenu(m_st_menu, MF_STRING, IDC_TREE_ST_BASE + i, _EDMnr.GetStED(i)->GetCaption());
		if(_EDMnr.GetStED(i)->ViewInTree())
			::CheckMenuItem(m_st_menu, IDC_TREE_ST_BASE + i, MF_CHECKED);
		/*if(_EDMnr.GetStED(i)->GetPic(picHandle, picType))
		{
			switch(picType)
			{
			case 0:
				m_view_bar.AddBitmap((HBITMAP)picHandle, IDC_TREE_ST_BASE + i);
				break;
			case 1:
				m_view_bar.AddIcon((HICON)picHandle, IDC_TREE_ST_BASE + i);
				break;
			}
		}*/
	}

	for(unsigned int i = 0; i < count; ++i)
	{
		::AppendMenu(m_script_menu, MF_STRING, IDC_TREE_BASE + i, _EDMnr.GetED(i)->GetCaption());
		CElementDescriptor* ED = _EDMnr.GetED(i);

		if(ED->GetPic(picHandle, picType))
		{
			int imageID = -1;

			switch(picType)
			{
			case 0:
				m_view_bar.AddBitmap((HBITMAP)picHandle, IDC_TREE_BASE + i);
				imageID = m_tree.AddImage(picHandle);
				break;
			case 1:
				m_view_bar.AddIcon((HICON)picHandle, IDC_TREE_BASE + i);
				imageID = m_tree.AddIcon(picHandle);
				break;				
			}
			if(imageID >= 0)
				ED->SetImageID(imageID);
		}
	}

	::AppendMenu(m_script_menu, MF_SEPARATOR, 0, 0);
	wchar_t cleanupMenuItem[MAX_LOAD_STRING + 1];
	FbeLoadString(_Module.GetResourceInstance(), IDS_DOC_TREE_CLEANUP, cleanupMenuItem, MAX_LOAD_STRING);
	::AppendMenu(m_script_menu, MF_STRING, IDC_TREE_CLEAR_ALL, cleanupMenuItem);

	m_view_bar.AttachMenu(bar);
}

void CTreeWithToolBar::RefreshLocalizedMenuCaptions()
{
	CMenuHandle bar = m_view_bar.GetMenu();
	if(bar.IsNull())
		return;

	wchar_t elsMenuItem[MAX_LOAD_STRING + 1];
	wchar_t scriptsMenuItem[MAX_LOAD_STRING + 1];
	wchar_t cleanupMenuItem[MAX_LOAD_STRING + 1];

	FbeLoadString(_Module.GetResourceInstance(), IDS_DOCTREE_MENU_ELEMENTS, elsMenuItem, MAX_LOAD_STRING);
	FbeLoadString(_Module.GetResourceInstance(), IDS_DOCTREE_MENU_SCRIPTS, scriptsMenuItem, MAX_LOAD_STRING);
	FbeLoadString(_Module.GetResourceInstance(), IDS_DOC_TREE_CLEANUP, cleanupMenuItem, MAX_LOAD_STRING);

	bar.ModifyMenu(0, MF_BYPOSITION | MF_POPUP | MF_STRING, (HMENU)m_st_menu, elsMenuItem);
	bar.ModifyMenu(1, MF_BYPOSITION | MF_POPUP | MF_STRING, (HMENU)m_script_menu, scriptsMenuItem);
	bar.ModifyMenu(2, MF_BYPOSITION | MF_POPUP | MF_STRING, (HMENU)m_navigation_menu,
		FbeLoadRuntimeStringByKey(L"fbe.document_tree.mode.caption", L"View"));
	m_script_menu.ModifyMenu(IDC_TREE_CLEAR_ALL, MF_BYCOMMAND | MF_STRING, IDC_TREE_CLEAR_ALL, cleanupMenuItem);
	m_navigation_menu.ModifyMenu(kDocumentTreeShowStructureCommand, MF_BYCOMMAND | MF_STRING, kDocumentTreeShowStructureCommand,
		FbeLoadRuntimeStringByKey(L"fbe.document_tree.mode.structure", L"Document structure"));
	m_navigation_menu.ModifyMenu(kDocumentTreeShowScriptsCommand, MF_BYCOMMAND | MF_STRING, kDocumentTreeShowScriptsCommand,
		FbeLoadRuntimeStringByKey(L"fbe.document_tree.mode.scripts", L"Scripts"));
	m_view_bar.Invalidate();
}
LRESULT CTreeWithToolBar::OnMenuCommand(WORD, WORD wID, HWND, BOOL&)
{
	bool ctrl_state = (GetKeyState(VK_CONTROL) & 0x8000) != 0x0;
	unsigned int index = wID - IDC_TREE_BASE;
	CElementDescriptor* ED = _EDMnr.GetED(index);

	if(ctrl_state)
	{
		ClearTree();
	}
	else
	{
		ED->CleanUp();
	}

	ED->ProcessScript();
	ED->SetViewInTree(true);
	m_tree.UpdateAll();
	return 0;
}

void CTreeWithToolBar::ClearTree()
{
	_EDMnr.CleanTree();	
	int st_menu_item_count = m_st_menu.GetMenuItemCount();
	for(int i = 0; i < st_menu_item_count; ++i)
	{
		::CheckMenuItem(m_st_menu, IDC_TREE_ST_BASE + i, MF_UNCHECKED);
	}
}


LRESULT CTreeWithToolBar::OnMenuStCommand(WORD, WORD wID, HWND, BOOL&)
{
	unsigned int index = wID - IDC_TREE_ST_BASE;
	CElementDescriptor* ED = _EDMnr.GetStED(index);
	bool view = ED->ViewInTree();
	view = !view;
	ED->SetViewInTree(view);
	if(view)
		::CheckMenuItem(m_st_menu, wID, MF_CHECKED);
	else
		::CheckMenuItem(m_st_menu, wID, MF_UNCHECKED);
	m_tree.UpdateAll();
	return 0;
}

LRESULT CTreeWithToolBar::OnMenuClear(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL &/* unused: bHandled */)
{
	_EDMnr.CleanUpAll();
	return 0;
}

LRESULT CTreeWithToolBar::OnShowDocumentStructure(WORD, WORD, HWND, BOOL&)
{
	_Settings.SetDocumentTreeScripts(false, true); m_tree.SetScriptMode(false);
	::CheckMenuItem(m_navigation_menu, kDocumentTreeShowStructureCommand, MF_BYCOMMAND | MF_CHECKED);
	::CheckMenuItem(m_navigation_menu, kDocumentTreeShowScriptsCommand, MF_BYCOMMAND | MF_UNCHECKED);
	return 0;
}

LRESULT CTreeWithToolBar::OnShowScripts(WORD, WORD, HWND, BOOL&)
{
	_Settings.SetDocumentTreeScripts(true, true); m_tree.SetScriptMode(true);
	::CheckMenuItem(m_navigation_menu, kDocumentTreeShowStructureCommand, MF_BYCOMMAND | MF_UNCHECKED);
	::CheckMenuItem(m_navigation_menu, kDocumentTreeShowScriptsCommand, MF_BYCOMMAND | MF_CHECKED);
	return 0;
}

void CTreeWithToolBar::SetScriptCatalog(const std::vector<ScriptDescriptor>& items, const std::vector<HICON>& icons, const std::vector<ScriptTreeToolbarTarget>& toolbars,
	const std::function<void(const CString&, const CString&)>& addToToolbar,
	const std::function<void(const CString&)>& openLocation)
{
	m_tree.SetScriptCatalog(items, icons, toolbars, addToToolbar, openLocation);
}

void CTreeWithToolBar::SetScriptToolbarTargets(const std::vector<ScriptTreeToolbarTarget>& toolbars) { m_tree.SetScriptToolbarTargets(toolbars); }


//==================================================================================================================


LRESULT CDocumentTree::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);

	m_tree.Create(*this, rcDefault);
	m_tree.m_tree.SetMainwindow(GetParent());
	m_tree.m_tree.SetScriptMode(_Settings.DocumentTreeScripts());
	/*m_element_browser.Create(*this, rcDefault);
	m_element_browser.m_tree.SetMainwindow(GetParent());*/
	this->SetClient(m_tree);
	RefreshLocalizedTitle();
	ThemeManager::ApplyToWindow(m_hWnd);
    bHandled=FALSE;
    return lRet;
}


void CDocumentTree::RefreshLocalizedTitle()
{
	wchar_t capt[MAX_LOAD_STRING + 1];
	FbeLoadString(_Module.GetResourceInstance(), IDS_DOCUMENT_TREE_CAPTION, capt, MAX_LOAD_STRING);
	m_title = capt;
	this->SetTitle(capt);
	this->SetWindowText(capt);
	m_tree.RefreshLocalizedMenuCaptions();
}

void CDocumentTree::PaintDarkTitle(HDC dc)
{
	RECT client = {}; GetClientRect(&client);
	RECT childRect = {};
	if(m_tree.IsWindow())
	{
		::GetWindowRect(m_tree, &childRect);
		::MapWindowPoints(NULL, m_hWnd, reinterpret_cast<POINT*>(&childRect), 2);
	}
	else childRect.top = client.bottom;
	RECT title = client; title.bottom = (std::max)(0L, childRect.top);
	::FillRect(dc, &title, ThemeManager::ControlBrush());
	if(title.bottom > title.top)
	{
		RECT separator = title; separator.top = separator.bottom - 1;
		::FillRect(dc, &separator, ThemeManager::Brush(THEME_COLOR_SEPARATOR));
		RECT text = title; text.left += 6; text.right -= 26;
		HFONT font = reinterpret_cast<HFONT>(::SendMessage(m_hWnd, WM_GETFONT, 0, 0));
		HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
		::SetBkMode(dc, TRANSPARENT); ::SetTextColor(dc, ThemeManager::TextColor());
		::DrawTextW(dc, m_title, -1, &text, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
		if(oldFont) ::SelectObject(dc, oldFont);
	}
}

LRESULT CDocumentTree::OnThemeEraseBackground(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
{
	if(!ThemeManager::IsDark() || ThemeManager::IsHighContrast()) { bHandled = FALSE; return 0; }
	PaintDarkTitle(reinterpret_cast<HDC>(wParam));
	return 1;
}

LRESULT CDocumentTree::OnThemePaint(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
	if(!ThemeManager::IsDark() || ThemeManager::IsHighContrast()) { bHandled = FALSE; return 0; }
	PAINTSTRUCT paint = {}; HDC dc = ::BeginPaint(m_hWnd, &paint);
	PaintDarkTitle(dc);
	::EndPaint(m_hWnd, &paint);
	return 0;
}

LRESULT CDocumentTree::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&)
{
	::RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
	return 0;
}

//WS_DLGFRAME  | WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CCS_NODIVIDER | CCS_NOPARENTALIGN | TBSTYLE_TOOLTIPS | TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE

LRESULT CDocumentTree::OnDestroy(UINT /* unused: uMsg */, WPARAM /* unused: wParam */, LPARAM /* unused: lParam */, BOOL& bHandled)
{
	bHandled=FALSE;
	return 0;
}

LRESULT CDocumentTree::OnClose(UINT /* unused: uMsg */, WPARAM /* unused: wParam */, LPARAM /* unused: lParam */, BOOL& /* unused: bHandled */)
{
	return 0;
}

void CDocumentTree::GetDocumentStructure(const MSHTML::IHTMLDocument2Ptr& v)
{
	m_tree.GetDocumentStructure(v);
}

void CDocumentTree::UpdateDocumentStructure(const MSHTML::IHTMLDocument2Ptr& v,MSHTML::IHTMLDOMNodePtr node)
{
	m_tree.UpdateDocumentStructure(v, node);
}

void CDocumentTree::HighlightItemAtPos(MSHTML::IHTMLElement *p)
{
	m_tree.HighlightItemAtPos(p);
}

void CDocumentTree::SetScriptCatalog(const std::vector<ScriptDescriptor>& items, const std::vector<HICON>& icons, const std::vector<ScriptTreeToolbarTarget>& toolbars,
	const std::function<void(const CString&, const CString&)>& addToToolbar,
	const std::function<void(const CString&)>& openLocation)
{
	m_tree.SetScriptCatalog(items, icons, toolbars, addToToolbar, openLocation);
}

void CDocumentTree::SetScriptToolbarTargets(const std::vector<ScriptTreeToolbarTarget>& toolbars) { m_tree.SetScriptToolbarTargets(toolbars); }

CTreeItem CDocumentTree::GetSelectedItem()
{
	return m_tree.GetSelectedItem();
}
