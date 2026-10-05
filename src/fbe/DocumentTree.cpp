#include "stdafx.h"
#include "resource.h"
#include "res1.h"
#include "utils.h"

#include "DocumentTree.h"
#include "ElementDescMnr.h"
#include "toolbars\\ToolbarFactory.h"
#include "UiMetrics.h"

#include "Settings.h"
extern CSettings _Settings;

extern CElementDescMnr _EDMnr;

namespace
{
const UINT_PTR kDocumentTreeViewBarThemeSubclassId = 0xFBE4;
const UINT_PTR kDocumentTreeViewBarWindowThemeSubclassId = 0xFBE5;
static_assert(ID_DOCUMENT_TREE_MODE_STRUCTURE > ID_VIEW_SCRIPT_TOOLBAR_DYNAMIC_LAST, "Navigation mode commands must not overlap dynamic toolbar commands");
static_assert(ID_DOCUMENT_TREE_MODE_SCRIPTS <= 0xffffu, "Navigation mode command must fit WM_COMMAND");

HBITMAP CreateDocumentTreeMenuCheckmarkBitmap(UINT dpi, bool checked)
{
	const int extent = (std::max)(1, UiMetrics::ScaleForDpi(16, dpi));
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = extent;
	info.bmiHeader.biHeight = -extent;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	void* bits = NULL;
	HBITMAP bitmap = ::CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
	if(bitmap == NULL || bits == NULL) { if(bitmap != NULL) ::DeleteObject(bitmap); return NULL; }
	HDC dc = ::CreateCompatibleDC(NULL);
	if(dc == NULL) { ::DeleteObject(bitmap); return NULL; }
	HGDIOBJ oldBitmap = ::SelectObject(dc, bitmap);
	HBRUSH background = ::CreateSolidBrush(ThemeManager::ControlColor());
	RECT canvas = { 0, 0, extent, extent };
	::FillRect(dc, &canvas, background);
	::DeleteObject(background);

	const int inset = (std::max)(1, UiMetrics::ScaleForDpi(3, dpi));
	RECT box = { inset, inset, extent - inset, extent - inset };
	HPEN border = ::CreatePen(PS_SOLID, (std::max)(1, UiMetrics::ScaleForDpi(1, dpi)), RGB(205, 205, 205));
	HGDIOBJ oldPen = ::SelectObject(dc, border);
	HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(HOLLOW_BRUSH));
	::Rectangle(dc, box.left, box.top, box.right, box.bottom);
	::SelectObject(dc, oldBrush);
	::SelectObject(dc, oldPen);
	::DeleteObject(border);
	if(checked)
	{
		HPEN check = ::CreatePen(PS_SOLID, (std::max)(1, UiMetrics::ScaleForDpi(2, dpi)), RGB(255, 255, 255));
		oldPen = ::SelectObject(dc, check);
		const int left = box.left + (std::max)(1, UiMetrics::ScaleForDpi(2, dpi));
		const int middle = box.top + (box.bottom - box.top) * 3 / 5;
		const int right = box.right - (std::max)(1, UiMetrics::ScaleForDpi(2, dpi));
		::MoveToEx(dc, left, middle, NULL);
		::LineTo(dc, box.left + (box.right - box.left) * 9 / 20, box.bottom - (std::max)(1, UiMetrics::ScaleForDpi(3, dpi)));
		::LineTo(dc, right, box.top + (std::max)(1, UiMetrics::ScaleForDpi(3, dpi)));
		::SelectObject(dc, oldPen);
		::DeleteObject(check);
	}
	::SelectObject(dc, oldBitmap);
	::DeleteDC(dc);
	return bitmap;
}

bool AddDocumentTreeModeImage(CImageList& images, UINT resourceId)
{
	HIMAGELIST source = ::ImageList_LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(resourceId), 16, 1,
		RGB(255, 0, 255), IMAGE_BITMAP, LR_CREATEDIBSECTION);
	if(source == NULL) return false;
	HICON icon = ::ImageList_GetImageCount(source) > 0 ? ::ImageList_GetIcon(source, 0, ILD_NORMAL) : NULL;
	const bool copied = icon != NULL && ::ImageList_AddIcon(images, icon) >= 0;
	if(icon != NULL) ::DestroyIcon(icon);
	::ImageList_Destroy(source);
	return copied;
}

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
	// Match the main command bar: let it prepare its native popup state before
	// ThemeManager restores FBE-owned menu bitmaps and opens the menu.  Direct
	// Direct native tracking skips that preparation and flashes a light popup first.
	::SendMessage(commandBar, WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(popup), 0);
	const UINT command = ThemeManager::TrackPopupMenu(popup,
		TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON, point.x, point.y, owner);
	if(command != 0)
		::SendMessage(owner, WM_COMMAND, MAKEWPARAM(command, 0), 0);
	return true;
}

LRESULT CALLBACK DocumentTreeViewBarWindowThemeProc(HWND window, UINT message, WPARAM wParam,
	LPARAM lParam, UINT_PTR, DWORD_PTR reference)
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
		CTreeWithToolBar* const tree = reinterpret_cast<CTreeWithToolBar*>(reference);
		if(tree != NULL) tree->FinalizeViewBarTheme();
		::RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
	}
	return result;
}

LRESULT CALLBACK DocumentTreeViewBarThemeProc(HWND window, UINT message, WPARAM wParam,
	LPARAM lParam, UINT_PTR, DWORD_PTR reference)
{
	if(message != WM_NOTIFY || ThemeManager::IsHighContrast())
		return ::DefSubclassProc(window, message, wParam, lParam);
	LPNMHDR header = reinterpret_cast<LPNMHDR>(lParam);
	HWND viewBar = reinterpret_cast<HWND>(reference);
	if(header == NULL || header->hwndFrom != viewBar || header->code != NM_CUSTOMDRAW)
		return ::DefSubclassProc(window, message, wParam, lParam);

	NMTBCUSTOMDRAW* draw = reinterpret_cast<NMTBCUSTOMDRAW*>(header);
	const bool dark = ThemeManager::IsDark() && !ThemeManager::IsHighContrast();
	if(draw->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		RECT client = {}; ::GetClientRect(viewBar, &client);
		::FillRect(draw->nmcd.hdc, &client, dark ? ThemeManager::ControlBrush() : ::GetSysColorBrush(COLOR_BTNFACE));
		return CDRF_NOTIFYITEMDRAW;
	}
	if(draw->nmcd.dwDrawStage != CDDS_ITEMPREPAINT)
		return CDRF_DODEFAULT;

	const bool disabled = (draw->nmcd.uItemState & (CDIS_DISABLED | CDIS_GRAYED)) != 0;
	const int buttonIndex = static_cast<int>(::SendMessage(viewBar, TB_COMMANDTOINDEX, draw->nmcd.dwItemSpec, 0));
	const bool active = buttonIndex == 0;
	const bool pressed = (draw->nmcd.uItemState & CDIS_SELECTED) != 0;
	const bool hot = (draw->nmcd.uItemState & CDIS_HOT) != 0;
	if(dark)
	{
		const ThemeColorRole surface = active ? (hot ? THEME_COLOR_HOVER : THEME_COLOR_PRESSED) : (pressed ? THEME_COLOR_PRESSED : hot ? THEME_COLOR_HOVER : THEME_COLOR_CONTROL);
		::FillRect(draw->nmcd.hdc, &draw->nmcd.rc, ThemeManager::Brush(surface));
		if(active || hot || pressed) ::FrameRect(draw->nmcd.hdc, &draw->nmcd.rc, ThemeManager::Brush(THEME_COLOR_BORDER));
	}
	else
	{
		::FillRect(draw->nmcd.hdc, &draw->nmcd.rc, ::GetSysColorBrush(active ? (hot ? COLOR_HIGHLIGHT : COLOR_3DLIGHT) : (pressed ? COLOR_3DLIGHT : hot ? COLOR_3DFACE : COLOR_BTNFACE)));
		if(active || hot || pressed) ::FrameRect(draw->nmcd.hdc, &draw->nmcd.rc, ::GetSysColorBrush(active && hot ? COLOR_HIGHLIGHTTEXT : COLOR_3DSHADOW));
	}

	wchar_t text[256] = {};
	TBBUTTONINFOW button = {}; button.cbSize = sizeof(button); button.dwMask = TBIF_TEXT;
	button.pszText = text; button.cchText = _countof(text);
	::SendMessage(viewBar, TB_GETBUTTONINFOW, static_cast<WPARAM>(draw->nmcd.dwItemSpec), reinterpret_cast<LPARAM>(&button));
	RECT textRect = draw->nmcd.rc; ::InflateRect(&textRect, -8, 0);
	HFONT font = reinterpret_cast<HFONT>(::SendMessage(viewBar, WM_GETFONT, 0, 0));
	HGDIOBJ oldFont = font ? ::SelectObject(draw->nmcd.hdc, font) : NULL;
	::SetBkMode(draw->nmcd.hdc, TRANSPARENT);
	::SetTextColor(draw->nmcd.hdc, disabled ? (dark ? ThemeManager::DisabledTextColor() : ::GetSysColor(COLOR_GRAYTEXT)) : (dark ? ThemeManager::TextColor() : ::GetSysColor(active && hot ? COLOR_HIGHLIGHTTEXT : COLOR_BTNTEXT)));
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
	m_tree.ApplyModeAppearance();
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
	SetRect(&rect, 0, 0, 500, UiMetrics::ScaleForDpi(28, UiMetrics::DpiForWindow(m_hWnd)));
	this->ModifyStyle(0, WS_POPUP, 0);
	m_view_bar.Create(*this, rect, NULL, ATL_SIMPLE_TOOLBAR_PANE_STYLE);
	m_view_bar.SetStyle(ATL_SIMPLE_TOOLBAR_PANE_STYLE);
	FillViewBar();
	ApplyViewBarMetrics();
	UpdateViewBarMode(false);
	::SetWindowSubclass(m_view_bar, DocumentTreeViewBarWindowThemeProc,
		kDocumentTreeViewBarWindowThemeSubclassId, reinterpret_cast<DWORD_PTR>(this));
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
	ClearStructureMenuCheckmarks();
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
	EnsureViewBarElementTextWidth();
	RECT clientRect = {0, 0, 0, 0};
	RECT rebarRect = {0, 0, 0, 0};
	RECT treeRect = {0, 0, 0, 0};
	RECT viewBarRect = {0, 0, 0, 0};
	
	this->GetClientRect(&clientRect);
	::GetWindowRect(m_toolbar, &rebarRect);
	::GetWindowRect(m_view_bar, &viewBarRect);
	const bool dark = ThemeManager::IsDark() && !ThemeManager::IsHighContrast();
	const bool rebarVisible = m_rebar.IsWindowVisible() != FALSE;
	int rebarHight = rebarVisible ? rebarRect.bottom - rebarRect.top + (dark ? 0 : GetSystemMetrics(SM_CYEDGE) * 2) : 0;
	rebarRect.left = treeRect.left = clientRect.left;
	rebarRect.right = treeRect.right = clientRect.right;

	const bool viewBarVisible = m_view_bar.IsWindowVisible() != FALSE;
	int viewBarHight = viewBarVisible ? viewBarRect.bottom - viewBarRect.top : 0;

	// The view bar is a pane-wide header, not a fixed-width toolbar.  Its
	// right edge must follow every splitter and DPI-driven resize.
	viewBarRect.left = clientRect.left;
	viewBarRect.right = clientRect.right;

	if(m_toolbarOrientation == CTreeWithToolBar::bottom)
	{
		rebarRect.top = clientRect.bottom - rebarHight;
		
		rebarRect.bottom = rebarRect.top + rebarHight;
		treeRect.top = clientRect.top + viewBarHight;
		treeRect.bottom = rebarRect.top;

		viewBarRect.top = clientRect.top + UiMetrics::ScaleForDpi(2, UiMetrics::DpiForWindow(m_hWnd));
		viewBarRect.bottom = viewBarRect.top + viewBarHight;
		treeRect.top = viewBarRect.bottom;
	}

	// ?????? ????? ????? ???????????? ??????. ??? ???? ???????? ???????? ??? ????.
	/*if(m_toolbarOrientation == CTreeWithToolBar::top)
	{
		rebarRect.top = clientRect.top;
		rebarRect.bottom = rebarRect.top + rebarHight;

		treeRect.top = rebarRect.bottom ;
		treeRect.bottom = clientRect.bottom;
	}*/

	// Both headers follow every pane resize, including a narrower splitter.
	if(rebarVisible) ::MoveWindow(m_rebar, rebarRect.left, rebarRect.top, rebarRect.right - rebarRect.left, rebarRect.bottom - rebarRect.top, true);
	if(viewBarVisible) ::MoveWindow(m_view_bar, viewBarRect.left, viewBarRect.top, viewBarRect.right - viewBarRect.left, viewBarRect.bottom - viewBarRect.top, true);
	m_maxTbwidth = rebarRect.right - rebarRect.left;
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
	RefreshStructureMenuCheckmarks();
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
		ApplyViewBarMetrics();
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
	HMENU bar = ::CreateMenu();


	wchar_t elsMenuItem[MAX_LOAD_STRING + 1];
	wchar_t scriptsMenuItem[MAX_LOAD_STRING + 1];

	FbeLoadString(_Module.GetResourceInstance(), IDS_DOCTREE_MENU_ELEMENTS, elsMenuItem, MAX_LOAD_STRING);
	FbeLoadString(_Module.GetResourceInstance(), IDS_DOCTREE_MENU_SCRIPTS, scriptsMenuItem, MAX_LOAD_STRING);

	::AppendMenu(bar, MF_POPUP|MF_STRING, (UINT)(HMENU)m_st_menu, elsMenuItem);
	::AppendMenu(bar, MF_POPUP|MF_STRING, (UINT)(HMENU)m_script_menu, scriptsMenuItem);

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
	RefreshStructureMenuCheckmarks();
}

void CTreeWithToolBar::ClearStructureMenuCheckmarks()
{
	if(!m_st_menu.IsNull())
	{
		const int count = m_st_menu.GetMenuItemCount();
		for(int index = 0; index < count; ++index)
		{
			MENUITEMINFOW info = {};
			info.cbSize = sizeof(info);
			info.fMask = MIIM_CHECKMARKS;
			info.hbmpChecked = NULL;
			info.hbmpUnchecked = NULL;
			::SetMenuItemInfoW(m_st_menu, index, TRUE, &info);
		}
	}
	if(m_structureMenuCheckedBitmap != NULL) ::DeleteObject(m_structureMenuCheckedBitmap);
	if(m_structureMenuUncheckedBitmap != NULL) ::DeleteObject(m_structureMenuUncheckedBitmap);
	m_structureMenuCheckedBitmap = NULL;
	m_structureMenuUncheckedBitmap = NULL;
	m_structureMenuCheckmarkDpi = 0;
}

void CTreeWithToolBar::RefreshStructureMenuCheckmarks()
{
	const bool dark = ThemeManager::IsDark() && !ThemeManager::IsHighContrast();
	if(!dark)
	{
		ClearStructureMenuCheckmarks();
		return;
	}
	if(m_st_menu.IsNull()) return;
	const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
	if(m_structureMenuCheckedBitmap != NULL && m_structureMenuUncheckedBitmap != NULL && m_structureMenuCheckmarkDpi == dpi) return;
	ClearStructureMenuCheckmarks();
	m_structureMenuCheckedBitmap = CreateDocumentTreeMenuCheckmarkBitmap(dpi, true);
	m_structureMenuUncheckedBitmap = CreateDocumentTreeMenuCheckmarkBitmap(dpi, false);
	if(m_structureMenuCheckedBitmap == NULL || m_structureMenuUncheckedBitmap == NULL)
	{
		ClearStructureMenuCheckmarks();
		return;
	}
	const int count = m_st_menu.GetMenuItemCount();
	for(int index = 0; index < count; ++index)
	{
		MENUITEMINFOW info = {};
		info.cbSize = sizeof(info);
		info.fMask = MIIM_CHECKMARKS;
		info.hbmpChecked = m_structureMenuCheckedBitmap;
		info.hbmpUnchecked = m_structureMenuUncheckedBitmap;
		::SetMenuItemInfoW(m_st_menu, index, TRUE, &info);
	}
	m_structureMenuCheckmarkDpi = dpi;
}

void CTreeWithToolBar::RefreshLocalizedMenuCaptions()
{
	CMenuHandle bar = m_view_bar.GetMenu();
	if(!bar.IsNull())
	{
		wchar_t elsMenuItem[MAX_LOAD_STRING + 1];
		wchar_t scriptsMenuItem[MAX_LOAD_STRING + 1];
		wchar_t cleanupMenuItem[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(), IDS_DOCTREE_MENU_ELEMENTS, elsMenuItem, MAX_LOAD_STRING);
		FbeLoadString(_Module.GetResourceInstance(), IDS_DOCTREE_MENU_SCRIPTS, scriptsMenuItem, MAX_LOAD_STRING);
		FbeLoadString(_Module.GetResourceInstance(), IDS_DOC_TREE_CLEANUP, cleanupMenuItem, MAX_LOAD_STRING);
		bar.ModifyMenu(0, MF_BYPOSITION | MF_POPUP | MF_STRING, (HMENU)m_st_menu, elsMenuItem);
		bar.ModifyMenu(1, MF_BYPOSITION | MF_POPUP | MF_STRING, (HMENU)m_script_menu, scriptsMenuItem);
		m_script_menu.ModifyMenu(IDC_TREE_CLEAR_ALL, MF_BYCOMMAND | MF_STRING, IDC_TREE_CLEAR_ALL, cleanupMenuItem);
		// CCommandBarCtrl does not copy a changed menu caption back into an
		// already-created toolbar button. Keep the visible selector in sync
		// before measuring it for the current runtime language.
		RefreshViewBarElementText(elsMenuItem);
		m_view_bar.Invalidate();
	}
	EnsureViewBarElementTextWidth();
}

void CTreeWithToolBar::ApplyViewBarMetrics()
{
	if(!m_view_bar.IsWindow()) return;
	::SendMessage(m_view_bar, WM_SETFONT, reinterpret_cast<WPARAM>(UiMetrics::MenuFont()), TRUE);
	m_view_bar.AutoSize();
	EnsureViewBarElementTextWidth();
}

void CTreeWithToolBar::FinalizeViewBarTheme()
{
	if(!m_view_bar.IsWindow()) return;
	wchar_t elements[MAX_LOAD_STRING + 1] = {};
	FbeLoadString(_Module.GetResourceInstance(), IDS_DOCTREE_MENU_ELEMENTS, elements, MAX_LOAD_STRING);
	// SetWindowTheme/WM_SETTINGCHANGE may rebuild CCommandBarCtrl's buttons.
	// Restore its localized caption before measuring the font selected by that
	// freshly rebuilt native control.
	RefreshViewBarElementText(elements);
	EnsureViewBarElementTextWidth();
	::RedrawWindow(m_view_bar, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
}

void CTreeWithToolBar::RefreshViewBarElementText(LPCWSTR text)
{
	if(!m_view_bar.IsWindow() || text == NULL) return;
	TBBUTTONINFOW writeButton = {};
	writeButton.cbSize = sizeof(writeButton);
	writeButton.dwMask = TBIF_TEXT | TBIF_BYINDEX;
	writeButton.pszText = const_cast<LPWSTR>(text);
	writeButton.cchText = static_cast<int>(wcslen(text));
	::SendMessage(m_view_bar, TB_SETBUTTONINFOW, 0, reinterpret_cast<LPARAM>(&writeButton));
}

void CTreeWithToolBar::EnsureViewBarElementTextWidth()
{
	if(!m_view_bar.IsWindow()) return;
	wchar_t text[MAX_LOAD_STRING + 1] = {};
	TBBUTTONINFOW readButton = {};
	readButton.cbSize = sizeof(readButton);
	readButton.dwMask = TBIF_TEXT | TBIF_SIZE | TBIF_BYINDEX;
	readButton.pszText = text;
	readButton.cchText = _countof(text);
	const LRESULT result = ::SendMessage(m_view_bar, TB_GETBUTTONINFOW, 0, reinterpret_cast<LPARAM>(&readButton));
	if(result == -1 || text[0] == L'\0') return;
	HDC dc = ::GetDC(m_view_bar); if(dc == NULL) return;
	HFONT font = reinterpret_cast<HFONT>(::SendMessage(m_view_bar, WM_GETFONT, 0, 0));
	HGDIOBJ oldFont = font != NULL ? ::SelectObject(dc, font) : NULL;
	SIZE extent = {}; ::GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &extent);
	if(oldFont != NULL) ::SelectObject(dc, oldFont);
	::ReleaseDC(m_view_bar, dc);
	const int desiredWidth = extent.cx + UiMetrics::ScaleForDpi(16, UiMetrics::DpiForWindow(m_view_bar));
	if(readButton.cx == desiredWidth) return;
	// Keep the write mask deliberately separate from the read mask. Passing
	// TBIF_TEXT with a null pszText clears the CommandBar caption on some
	// common-controls versions, leaving the selector blank or as "V..".
	TBBUTTONINFOW writeButton = {};
	writeButton.cbSize = sizeof(writeButton);
	writeButton.dwMask = TBIF_SIZE | TBIF_BYINDEX;
	writeButton.cx = static_cast<WORD>((std::min)(desiredWidth, 0xffff));
	::SendMessage(m_view_bar, TB_SETBUTTONINFOW, 0, reinterpret_cast<LPARAM>(&writeButton));
}

bool CTreeWithToolBar::GetViewBarElementProbe(CString& text, int& buttonWidth, int& measuredTextWidth, int& padding) const
{
	text.Empty(); buttonWidth = 0; measuredTextWidth = 0; padding = 0;
	if(!m_view_bar.IsWindow()) return false;
	wchar_t buffer[MAX_LOAD_STRING + 1] = {};
	TBBUTTONINFOW button = {};
	button.cbSize = sizeof(button);
	button.dwMask = TBIF_TEXT | TBIF_SIZE | TBIF_BYINDEX;
	button.pszText = buffer;
	button.cchText = _countof(buffer);
	if(::SendMessage(m_view_bar, TB_GETBUTTONINFOW, 0, reinterpret_cast<LPARAM>(&button)) == -1 || buffer[0] == L'\0') return false;
	HDC dc = ::GetDC(m_view_bar); if(dc == NULL) return false;
	HFONT font = reinterpret_cast<HFONT>(::SendMessage(m_view_bar, WM_GETFONT, 0, 0));
	HGDIOBJ oldFont = font != NULL ? ::SelectObject(dc, font) : NULL;
	SIZE extent = {}; const BOOL measured = ::GetTextExtentPoint32W(dc, buffer, static_cast<int>(wcslen(buffer)), &extent);
	if(oldFont != NULL) ::SelectObject(dc, oldFont);
	::ReleaseDC(m_view_bar, dc);
	if(!measured) return false;
	text = buffer;
	buttonWidth = button.cx;
	measuredTextWidth = extent.cx;
	padding = UiMetrics::ScaleForDpi(16, UiMetrics::DpiForWindow(m_view_bar));
	return true;
}

bool CTreeWithToolBar::GetStructureMenuCheckmarkProbe(bool expectCustomBitmaps) const
{
	if(m_st_menu.IsNull() || m_st_menu.GetMenuItemCount() == 0) return false;
	for(int index = 0; index < m_st_menu.GetMenuItemCount(); ++index)
	{
		MENUITEMINFOW info = {};
		info.cbSize = sizeof(info);
		info.fMask = MIIM_CHECKMARKS;
		if(!::GetMenuItemInfoW(m_st_menu, index, TRUE, &info)) return false;
		const bool custom = info.hbmpChecked == m_structureMenuCheckedBitmap &&
			info.hbmpUnchecked == m_structureMenuUncheckedBitmap && info.hbmpChecked != NULL && info.hbmpUnchecked != NULL;
		if(custom != expectCustomBitmaps) return false;
	}
	return expectCustomBitmaps ? m_structureMenuCheckmarkDpi == UiMetrics::DpiForWindow(m_hWnd) :
		m_structureMenuCheckedBitmap == NULL && m_structureMenuUncheckedBitmap == NULL;
}

bool CTreeWithToolBar::PrepareViewBarPopupThemeProbe()
{
	if(!m_view_bar.IsWindow()) return false;
	RefreshStructureMenuCheckmarks();
	const HMENU menu = reinterpret_cast<HMENU>(::SendMessage(m_view_bar, CBRM_GETMENU, 0, 0));
	const HMENU popup = menu != NULL ? ::GetSubMenu(menu, 0) : NULL;
	if(popup == NULL) return false;
	// This is the non-modal preparation portion of ShowNativeDocumentTreeViewBarPopup.
	// It lets the runtime test verify the active theme path without synthesizing
	// input into a modal native menu loop.
	::SendMessage(m_view_bar, WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(popup), 0);
	return true;
}

void CTreeWithToolBar::UpdateViewBarMode(bool scripts)
{
	if(!m_view_bar.IsWindow()) return;
	// The Elements selector belongs to Structure mode.  The old descriptor
	// scripts popup is no longer part of either navigation mode.
	m_view_bar.ShowWindow(scripts ? SW_HIDE : SW_SHOW);
	m_rebar.ShowWindow(scripts ? SW_HIDE : SW_SHOW);
	m_view_bar.HideButton(0, scripts ? TRUE : FALSE);
	m_view_bar.HideButton(1, TRUE);
	m_view_bar.Invalidate();
	SendMessage(WM_SIZE);
}

void CTreeWithToolBar::RefreshModeControls()
{
	UpdateViewBarMode(m_tree.IsScriptMode());
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
	SetScriptMode(false);
	return 0;
}

LRESULT CTreeWithToolBar::OnShowScripts(WORD, WORD, HWND, BOOL&)
{
	SetScriptMode(true);
	return 0;
}

void CTreeWithToolBar::SetScriptMode(bool scripts)
{
	_Settings.SetDocumentTreeScripts(scripts, true);
	m_tree.SetScriptMode(scripts);
	UpdateViewBarMode(scripts);
	if(m_modeChanged) m_modeChanged();
}

void CTreeWithToolBar::SetScriptCatalog(const std::vector<ScriptDescriptor>& items, const std::vector<ScriptTreeVisual>& visuals, const std::vector<ScriptTreeToolbarTarget>& toolbars,
	const std::function<void(const CString&, const CString&)>& addToToolbar,
	const std::function<void(const CString&)>& openLocation, const std::function<void(UINT)>& runScript)
{
	m_tree.SetScriptCatalog(items, visuals, toolbars, addToToolbar, openLocation, runScript);
}

void CTreeWithToolBar::SetScriptToolbarTargets(const std::vector<ScriptTreeToolbarTarget>& toolbars) { m_tree.SetScriptToolbarTargets(toolbars); }


//==================================================================================================================


LRESULT CDocumentTree::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);

	m_tree.Create(*this, rcDefault);
	m_tree.m_tree.SetMainwindow(GetParent());
	m_tree.SetModeChangedHandler([this]() { RefreshLocalizedTitle(); });
	m_tree.m_tree.SetScriptMode(_Settings.DocumentTreeScripts());
	m_tree.RefreshModeControls();
	CreateModeButton();
	/*m_element_browser.Create(*this, rcDefault);
	m_element_browser.m_tree.SetMainwindow(GetParent());*/
	this->SetClient(m_tree);
	LayoutModeButton();
	RefreshLocalizedTitle();
	ThemeManager::ApplyToWindow(m_hWnd);
    bHandled=FALSE;
    return lRet;
}


void CDocumentTree::RefreshLocalizedTitle()
{
	m_title = FbeLoadRuntimeStringByKey(m_tree.m_tree.IsScriptMode() ? L"fbe.document_tree.mode.scripts" : L"fbe.document_tree.mode.structure",
		m_tree.m_tree.IsScriptMode() ? L"Scripts" : L"Document structure");
	this->SetTitle(m_title);
	this->SetWindowText(m_title);
	m_tree.RefreshLocalizedMenuCaptions();
	RefreshModeButton();
}

LRESULT CDocumentTree::OnSize(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
{
	// Move the close button first.  It is the anchor for the mode button and
	// changes position when a startup dialog restores the frame size.
	CPaneContainer::UpdateLayout(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	LayoutModeButton();
	bHandled = TRUE;
	return 0;
}

void CDocumentTree::CreateModeButton()
{
	bool imagesReady = m_mode_images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 1) &&
		AddDocumentTreeModeImage(m_mode_images, IDR_SCRIPTS) && AddDocumentTreeModeImage(m_mode_images, IDB_STRUCTURE);
	if(!imagesReady)
	{
		m_mode_images.Destroy();
		imagesReady = m_mode_images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 1);
		HICON fallback = ::LoadIcon(NULL, IDI_APPLICATION);
		if(imagesReady && fallback != NULL) { ::ImageList_AddIcon(m_mode_images, fallback); ::ImageList_AddIcon(m_mode_images, fallback); }
	}
	const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT |
		CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN | CCS_NOMOVEY;
	if(!imagesReady || m_mode_button.Create(m_hWnd, rcDefault, NULL, style) == NULL) return;
	m_mode_button.SetButtonStructSize();
	m_mode_button.SetImageList(m_mode_images);
	const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
	const int iconSize = UiMetrics::ScaleForDpi(16, dpi);
	const int buttonSize = UiMetrics::ScaleForDpi(22, dpi);
	m_mode_button.SetBitmapSize(iconSize, iconSize);
	m_mode_button.SetButtonSize(buttonSize, buttonSize);
	TBBUTTON button = {}; button.iBitmap = 0; button.idCommand = ID_DOCUMENT_TREE_MODE_SCRIPTS;
	button.fsState = TBSTATE_ENABLED; button.fsStyle = BTNS_BUTTON;
	m_mode_button.AddButtons(1, &button);
	LayoutModeButton();
}

void CDocumentTree::LayoutModeButton()
{
	if(!m_mode_button.IsWindow()) return;
	RECT client = {}; GetClientRect(&client);
	const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
	const int extent = (std::max)(1, (std::min)(UiMetrics::ScaleForDpi(18, dpi), m_cxyHeader));
	const int iconSize = (std::min)(UiMetrics::ScaleForDpi(14, dpi), (std::max)(1, extent - 4));
	m_mode_button.SetBitmapSize(iconSize, iconSize);
	m_mode_button.SetButtonSize(extent, extent);
	RECT close = {};
	if(m_tb.IsWindow()) { ::GetWindowRect(m_tb, &close); ::MapWindowPoints(NULL, m_hWnd, reinterpret_cast<POINT*>(&close), 2); }
	const int left = (std::max)(0, static_cast<int>((close.left > 0 ? close.left : client.right - m_cxToolBar) - extent));
	const int top = (std::max)(0, (m_cxyHeader - extent) / 2);
	m_mode_button.SetWindowPos(NULL, left, top, extent, extent, SWP_NOZORDER | SWP_NOACTIVATE);
}

void CDocumentTree::RefreshModeButton()
{
	if(!m_mode_button.IsWindow()) return;
	const bool scripts = m_tree.m_tree.IsScriptMode();
	TBBUTTONINFOW button = {}; button.cbSize = sizeof(button); button.dwMask = TBIF_COMMAND | TBIF_IMAGE | TBIF_BYINDEX;
	button.idCommand = scripts ? ID_DOCUMENT_TREE_MODE_STRUCTURE : ID_DOCUMENT_TREE_MODE_SCRIPTS;
	button.iImage = scripts ? 1 : 0;
	::SendMessage(m_mode_button, TB_SETBUTTONINFOW, 0, reinterpret_cast<LPARAM>(&button));
	::InvalidateRect(m_mode_button, NULL, TRUE);
	LayoutModeButton();
}

bool CDocumentTree::GetModeButtonProbe(RECT& title, RECT& button, RECT& close, int& image, UINT& command) const
{
	::SetRectEmpty(&title); ::SetRectEmpty(&button); ::SetRectEmpty(&close); image = -1; command = 0;
	if(!m_mode_button.IsWindow() || !::IsWindowVisible(m_mode_button)) return false;
	GetClientRect(&title); title.bottom = m_cxyHeader;
	::GetWindowRect(m_mode_button, &button); ::MapWindowPoints(NULL, m_hWnd, reinterpret_cast<POINT*>(&button), 2);
	if(m_tb.IsWindow()) { ::GetWindowRect(m_tb, &close); ::MapWindowPoints(NULL, m_hWnd, reinterpret_cast<POINT*>(&close), 2); }
	TBBUTTON nativeButton = {};
	if(!::SendMessage(m_mode_button, TB_GETBUTTON, 0, reinterpret_cast<LPARAM>(&nativeButton))) return false;
	image = nativeButton.iBitmap; command = nativeButton.idCommand;
	return image >= 0 && button.right - button.left == button.bottom - button.top && button.right - button.left <= title.bottom - title.top && button.left >= title.left && button.top >= title.top && button.right <= title.right && button.bottom <= title.bottom && (!m_tb.IsWindow() || button.right <= close.left);
}

LRESULT CDocumentTree::OnToggleMode(WORD, WORD, HWND, BOOL&)
{
	m_tree.ToggleScriptMode();
	return 0;
}

LRESULT CDocumentTree::OnModeToolTip(int idCtrl, LPNMHDR header, BOOL& bHandled)
{
	if(header == NULL || (idCtrl != ID_DOCUMENT_TREE_MODE_SCRIPTS && idCtrl != ID_DOCUMENT_TREE_MODE_STRUCTURE)) { bHandled = FALSE; return 0; }
	LPNMTTDISPINFOW tooltip = reinterpret_cast<LPNMTTDISPINFOW>(header);
	const bool scripts = m_tree.m_tree.IsScriptMode();
	const CString text = FbeLoadRuntimeStringByKey(scripts ? L"fbe.document_tree.mode.show_structure" : L"fbe.document_tree.mode.show_scripts",
		scripts ? L"Show document structure" : L"Show scripts");
	SecureHelper::strncpyW_x(tooltip->szText, _countof(tooltip->szText), text.GetString(), _TRUNCATE);
	bHandled = TRUE;
	return 0;
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
		RECT text = title; text.left += 6; text.right -= m_cxToolBar + UiMetrics::ScaleForDpi(22, UiMetrics::DpiForWindow(m_hWnd));
		HFONT font = reinterpret_cast<HFONT>(::SendMessage(m_hWnd, WM_GETFONT, 0, 0));
		HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
		::SetBkMode(dc, TRANSPARENT); ::SetTextColor(dc, ThemeManager::TextColor());
		::DrawTextW(dc, m_title, -1, &text, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
		if(oldFont) ::SelectObject(dc, oldFont);
	}
}

void CDocumentTree::PaintLightTitle(HDC dc)
{
	RECT title = {}; GetClientRect(&title);
	title.bottom = m_cxyHeader;
	UINT border = BF_LEFT | BF_TOP | BF_ADJUST | BF_RIGHT;
	if((m_dwExtendedStyle & PANECNT_NOBORDER) == 0)
	{
		if((m_dwExtendedStyle & PANECNT_FLATBORDER) != 0) border |= BF_FLAT;
		::DrawEdge(dc, &title, EDGE_ETCHED, border);
	}
	if((m_dwExtendedStyle & PANECNT_DIVIDER) != 0)
		::DrawEdge(dc, &title, BDR_SUNKENOUTER, BF_FLAT | BF_ADJUST | BF_BOTTOM);
	RECT text = title; text.left += m_cxyTextOffset; text.right -= m_cxyTextOffset + m_cxToolBar + UiMetrics::ScaleForDpi(22, UiMetrics::DpiForWindow(m_hWnd));
	HFONT font = reinterpret_cast<HFONT>(::SendMessage(m_hWnd, WM_GETFONT, 0, 0));
	HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
	::SetTextColor(dc, ::GetSysColor(COLOR_WINDOWTEXT));
	::SetBkMode(dc, TRANSPARENT);
	::DrawTextW(dc, m_title, -1, &text, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
	if(oldFont) ::SelectObject(dc, oldFont);
}

LRESULT CDocumentTree::OnThemeEraseBackground(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
{
	if(!ThemeManager::IsDark() || ThemeManager::IsHighContrast()) { bHandled = FALSE; return 0; }
	PaintDarkTitle(reinterpret_cast<HDC>(wParam));
	return 1;
}

LRESULT CDocumentTree::OnThemePaint(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
	PAINTSTRUCT paint = {}; HDC dc = ::BeginPaint(m_hWnd, &paint);
	if(ThemeManager::IsDark() && !ThemeManager::IsHighContrast()) PaintDarkTitle(dc);
	else PaintLightTitle(dc);
	::EndPaint(m_hWnd, &paint);
	bHandled = TRUE;
	return 0;
}

LRESULT CDocumentTree::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&)
{
	if(m_mode_button.IsWindow())
	{
		if(ThemeManager::IsDark() && !ThemeManager::IsHighContrast())
		{
			::SendMessage(m_mode_button, CCM_SETBKCOLOR, 0, ThemeManager::ControlColor());
			COLORSCHEME colours = {}; colours.dwSize = sizeof(colours);
			colours.clrBtnHighlight = ThemeManager::HoverColor();
			colours.clrBtnShadow = ThemeManager::BorderColor();
			::SendMessage(m_mode_button, TB_SETCOLORSCHEME, 0, reinterpret_cast<LPARAM>(&colours));
		}
		else ::SendMessage(m_mode_button, CCM_SETBKCOLOR, 0, ::GetSysColor(COLOR_BTNFACE));
	}
	LayoutModeButton();
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

void CDocumentTree::SetScriptCatalog(const std::vector<ScriptDescriptor>& items, const std::vector<ScriptTreeVisual>& visuals, const std::vector<ScriptTreeToolbarTarget>& toolbars,
	const std::function<void(const CString&, const CString&)>& addToToolbar,
	const std::function<void(const CString&)>& openLocation, const std::function<void(UINT)>& runScript)
{
	m_tree.SetScriptCatalog(items, visuals, toolbars, addToToolbar, openLocation, runScript);
}

void CDocumentTree::SetScriptToolbarTargets(const std::vector<ScriptTreeToolbarTarget>& toolbars) { m_tree.SetScriptToolbarTargets(toolbars); }

CTreeItem CDocumentTree::GetSelectedItem()
{
	return m_tree.GetSelectedItem();
}
