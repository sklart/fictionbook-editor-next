// SettingsDlg.cpp : Implementation of CSettingsDlg

#include "stdafx.h"
#include "..\\..\\Settings.h"
#include "SettingsDlg.h"
#include "..\\..\\RuntimeLocalization.h"
#include "..\\..\\res1.h"
#include "..\\..\\ThemeManager.h"
#include "..\\..\\UiMetrics.h"
#include "..\\..\\testing\\RuntimeTestScenarioMode.h"

extern CSettings _Settings;

namespace
{
	bool IsSettingsDialogRuntimeTest()
	{
		wchar_t mode[4] = {}, scenario[64] = {};
		if (::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", mode, _countof(mode)) != 1 || mode[0] != L'1')
			return false;
		const DWORD scenarioLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SCENARIO", scenario, _countof(scenario));
		return (scenarioLength == wcslen(L"settings-dialog-runtime") && wcscmp(scenario, L"settings-dialog-runtime") == 0) ||
			(scenarioLength == wcslen(L"backup-settings-save-runtime") && wcscmp(scenario, L"backup-settings-save-runtime") == 0);
	}

	bool IsBackupSettingsSaveRuntimeTest() { return RuntimeTests::IsScenario(L"backup-settings-save-runtime"); }
}

// CSettingsDlg

CSettingsDlg::CSettingsDlg() :
	m_advancedPage(NULL), m_generalPage(NULL), m_editorPage(NULL), m_spellingPage(NULL), m_imagesPage(NULL), m_sourcePage(NULL),
	m_hotkeysPage(NULL), m_wordsPage(NULL), m_currentPage(SettingsPageId::Count),
	m_initialDpi(0), m_layoutDpi(0),
	m_pageLeft(0), m_pageTop(0), m_pageRightMargin(0), m_pageBottomMargin(0),
	m_buttonWidth(0), m_buttonHeight(0), m_buttonGap(0), m_buttonBottomMargin(0),
	m_navigationBottomGap(0)
{
	ZeroMemory(m_pages, sizeof(m_pages));
	ZeroMemory(m_pageLifecycle, sizeof(m_pageLifecycle));
}

CSettingsDlg::~CSettingsDlg()
{
}

LRESULT CSettingsDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CSettingsDlg>::OnInitDialog(uMsg, wParam, lParam, bHandled);
	ThemeManager::ApplyToWindow(m_hWnd);
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_TOOLS_SETTINGS);
	m_navigation = GetDlgItem(IDC_SETTINGS_NAV);
	const struct { LPCWSTR key; LPCWSTR fallback; } navigationItems[] = {
		{ L"fbe.settings.nav.general", L"General" },
		{ L"fbe.settings.nav.editor", L"Editor" },
		{ L"fbe.settings.nav.source", L"Source code" },
		{ L"fbe.settings.nav.images", L"Images" },
		{ L"fbe.settings.nav.spelling", L"Spelling" },
		{ L"fbe.settings.nav.keyboard", L"Keyboard" },
		{ L"fbe.settings.nav.words", L"Words" },
		{ L"fbe.settings.nav.advanced", L"Advanced" }
	};
	for(int i = 0; i < _countof(navigationItems); ++i)
		m_navigation.AddString(FbeLoadRuntimeStringByKey(navigationItems[i].key, navigationItems[i].fallback));

	m_generalPage = new CSettingsGeneralPage;
	m_generalPage->Create(m_hWnd);
	m_advancedPage = new CSettingsAdvancedPage;
	m_advancedPage->Create(m_hWnd);
	m_editorPage = new CSettingsEditorPage;
	m_editorPage->Create(m_hWnd);
	m_spellingPage = new CSettingsSpellingPage;
	m_spellingPage->Create(m_hWnd);
	m_imagesPage = new CSettingsImagesPage;
	m_imagesPage->Create(m_hWnd);
	m_sourcePage = new CSettingsSourcePage;
	m_sourcePage->Create(m_hWnd);
	m_hotkeysPage = new CSettingsHotkeysDlg;
	m_hotkeysPage->Create(m_hWnd);
	m_wordsPage = new CSettingsWordsDlg;
	m_wordsPage->Create(m_hWnd);
	// The pages are created after the dialog itself.  Apply the selected
	// interface palette before any page becomes visible, preventing the light
	// system background from flashing before a later theme refresh.
	const HWND pagesToTheme[] = {
		m_generalPage->m_hWnd, m_advancedPage->m_hWnd, m_editorPage->m_hWnd,
		m_spellingPage->m_hWnd, m_imagesPage->m_hWnd, m_sourcePage->m_hWnd,
		m_hotkeysPage->m_hWnd, m_wordsPage->m_hWnd
	};
	for(HWND page : pagesToTheme)
		ThemeManager::ApplyToWindow(page);

	m_pages[PageIndex(SettingsPageId::General)] = m_generalPage;
	m_pages[PageIndex(SettingsPageId::Editor)] = m_editorPage;
	m_pages[PageIndex(SettingsPageId::Source)] = m_sourcePage;
	m_pages[PageIndex(SettingsPageId::Images)] = m_imagesPage;
	m_pages[PageIndex(SettingsPageId::Spelling)] = m_spellingPage;
	m_pages[PageIndex(SettingsPageId::Keyboard)] = m_hotkeysPage;
	m_pages[PageIndex(SettingsPageId::Words)] = m_wordsPage;
	m_pages[PageIndex(SettingsPageId::Advanced)] = m_advancedPage;
	m_pageLifecycle[PageIndex(SettingsPageId::General)] = m_generalPage;
	m_pageLifecycle[PageIndex(SettingsPageId::Editor)] = m_editorPage;
	m_pageLifecycle[PageIndex(SettingsPageId::Source)] = m_sourcePage;
	m_pageLifecycle[PageIndex(SettingsPageId::Images)] = m_imagesPage;
	m_pageLifecycle[PageIndex(SettingsPageId::Spelling)] = m_spellingPage;
	m_pageLifecycle[PageIndex(SettingsPageId::Keyboard)] = m_hotkeysPage;
	m_pageLifecycle[PageIndex(SettingsPageId::Words)] = m_wordsPage;
	m_pageLifecycle[PageIndex(SettingsPageId::Advanced)] = m_advancedPage;
	CapturePageLayouts();

	CRect client;
	GetClientRect(client);
	m_navigation.GetWindowRect(m_navigationRect);
	ScreenToClient(m_navigationRect);
	CRect okRect;
	CRect cancelRect;
	GetDlgItem(IDOK).GetWindowRect(okRect);
	GetDlgItem(IDCANCEL).GetWindowRect(cancelRect);
	ScreenToClient(okRect);
	ScreenToClient(cancelRect);
	CRect window;
	GetWindowRect(window);
	m_minimumWindowSize = window.Size();
	m_initialDpi = m_layoutDpi = UiMetrics::DpiForWindow(m_hWnd);
	m_pageLeft = m_navigationRect.right + (m_navigationRect.left > 0 ? m_navigationRect.left : 1);
	m_pageTop = m_navigationRect.top;
	m_pageRightMargin = client.right - cancelRect.right;
	m_pageBottomMargin = client.bottom - okRect.top;
	m_buttonWidth = okRect.Width();
	m_buttonHeight = okRect.Height();
	m_buttonGap = cancelRect.left - okRect.right;
	m_buttonBottomMargin = client.bottom - okRect.bottom;
	m_navigationBottomGap = okRect.top - m_navigationRect.bottom;
	const int nonClientWidth = window.Width() - client.Width();
	const int nonClientHeight = window.Height() - client.Height();
	const int buttonRowWidth = 2 * m_buttonWidth + m_buttonGap;
	const int minimumPageWidth = max(buttonRowWidth, 2 * m_navigationRect.Width());
	const int minimumPageHeight = max(5 * m_navigation.GetItemHeight(0), 6 * m_buttonHeight);
	m_minimumWindowSize = CSize(nonClientWidth + m_pageLeft + minimumPageWidth + m_pageRightMargin,
		nonClientHeight + m_pageTop + minimumPageHeight + m_pageBottomMargin);
	RestoreGeometry();
	GetClientRect(client);
	LayoutControls(client.Width(), client.Height());
	m_navigation.SetCurSel(0);
	SelectPage(SettingsPageId::General);
	if (IsSettingsDialogRuntimeTest())
	{
		// Test-only automation still commits through the normal global OK path.
		// Use independent pages so ApplyConfChanges covers more than one category.
		m_generalPage->CheckDlgButton(IDC_CREATE_BACKUP_FILE, IsBackupSettingsSaveRuntimeTest() ? BST_UNCHECKED : BST_CHECKED);
		m_editorPage->SendDlgItemMessage(IDC_NBSP_CHAR, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"\x25AB"));
		m_sourcePage->CheckDlgButton(IDC_WRAP, BST_CHECKED);
		m_spellingPage->CheckDlgButton(IDC_USESPELLCHECKER, BST_UNCHECKED);
		m_spellingPage->SetDlgItemText(IDC_CUSTOM_DICT, L"");
		m_advancedPage->SendMessage(WM_COMMAND, MAKEWPARAM(IDC_DEFAULT_SCRIPTS_FOLDER, BN_CLICKED), 0);
		SetTimer(0x53455454, 1);
	}
	return 1;
}

LRESULT CSettingsDlg::OnTimer(UINT, WPARAM timerId, LPARAM, BOOL&)
{
	if (timerId != 0x53455454 || !IsSettingsDialogRuntimeTest()) return 0;
	KillTimer(timerId);
	if(RuntimeTests::IsScenario(L"settings-dialog-runtime") && !ProbeLayout())
	{
		EndDialog(IDCANCEL);
		return 0;
	}
	BOOL handled = FALSE;
	return OnClickedOK(BN_CLICKED, IDOK, GetDlgItem(IDOK), handled);
}

LRESULT CSettingsDlg::OnClickedOK(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND hWndCtl, BOOL& /* unused: bHandled */)
{
	const HWND globalOk = GetDlgItem(IDOK);
	if(hWndCtl != globalOk && m_currentPage == SettingsPageId::Words &&
		m_wordsPage && m_wordsPage->HandleDefaultAction())
		return 0;

	for(int i = 0; i < _countof(m_pageLifecycle); ++i)
		if(m_pageLifecycle[i] && !m_pageLifecycle[i]->Validate()) { SelectPage(static_cast<SettingsPageId>(i)); return 0; }
	for(int i = 0; i < _countof(m_pageLifecycle); ++i)
		if(m_pageLifecycle[i]) m_pageLifecycle[i]->Commit();
	EndDialog(IDOK);
	return 0;
}

LRESULT CSettingsDlg::OnClickedCancel(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	for(int i = _countof(m_pageLifecycle) - 1; i >= 0; --i)
		if(m_pageLifecycle[i] && !m_pageLifecycle[i]->CancelChanges()) { SelectPage(static_cast<SettingsPageId>(i)); return 0; }
	EndDialog(IDCANCEL);

	return 0;
}

LRESULT CSettingsDlg::OnNavigationChanged(WORD, WORD, HWND, BOOL&)
{
	const int selection = m_navigation.GetCurSel();
	if(selection >= 0 && selection < PageIndex(SettingsPageId::Count))
		SelectPage(static_cast<SettingsPageId>(selection));
	return 0;
}

LRESULT CSettingsDlg::OnSize(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	LayoutControls(LOWORD(lParam), HIWORD(lParam));
	return 0;
}

LRESULT CSettingsDlg::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	MINMAXINFO* minMax = reinterpret_cast<MINMAXINFO*>(lParam);
	if(!m_initialDpi) return 0;
	MONITORINFO monitor = { sizeof(monitor) };
	if(!::GetMonitorInfoW(::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &monitor)) return 0;
	const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
	minMax->ptMinTrackSize.x = min(::MulDiv(m_minimumWindowSize.cx, dpi, m_initialDpi), monitor.rcWork.right - monitor.rcWork.left);
	minMax->ptMinTrackSize.y = min(::MulDiv(m_minimumWindowSize.cy, dpi, m_initialDpi), monitor.rcWork.bottom - monitor.rcWork.top);
	return 0;
}

LRESULT CSettingsDlg::OnDpiChanged(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
	const UINT dpi = LOWORD(wParam);
	if(m_layoutDpi && dpi && dpi != m_layoutDpi)
	{
		const auto scale = [this, dpi](int value) { return ::MulDiv(value, dpi, m_layoutDpi); };
		m_navigationRect = CRect(scale(m_navigationRect.left), scale(m_navigationRect.top),
			scale(m_navigationRect.right), scale(m_navigationRect.bottom));
		m_pageLeft = scale(m_pageLeft); m_pageTop = scale(m_pageTop);
		m_pageRightMargin = scale(m_pageRightMargin); m_pageBottomMargin = scale(m_pageBottomMargin);
		m_buttonWidth = scale(m_buttonWidth); m_buttonHeight = scale(m_buttonHeight);
		m_buttonGap = scale(m_buttonGap); m_buttonBottomMargin = scale(m_buttonBottomMargin);
		m_navigationBottomGap = scale(m_navigationBottomGap);
		for(PageLayout& page : m_pageLayouts)
		{
			page.contentSize = CSize(scale(page.contentSize.cx), scale(page.contentSize.cy));
			page.scrollX = scale(page.scrollX);
			page.scrollY = scale(page.scrollY);
			for(PageControlLayout& control : page.controls)
				control.rect = CRect(scale(control.rect.left), scale(control.rect.top),
					scale(control.rect.right), scale(control.rect.bottom));
		}
		m_layoutDpi = dpi;
	}
	const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
	if(suggested)
	{
		MONITORINFO monitor = { sizeof(monitor) };
		if(::GetMonitorInfoW(::MonitorFromRect(suggested, MONITOR_DEFAULTTONEAREST), &monitor))
		{
			const int workWidth = monitor.rcWork.right - monitor.rcWork.left;
			const int workHeight = monitor.rcWork.bottom - monitor.rcWork.top;
			const int width = min(max(suggested->right - suggested->left,
				min(::MulDiv(m_minimumWindowSize.cx, dpi, m_initialDpi), workWidth)), workWidth);
			const int height = min(max(suggested->bottom - suggested->top,
				min(::MulDiv(m_minimumWindowSize.cy, dpi, m_initialDpi), workHeight)), workHeight);
			const int x = min(max(suggested->left, monitor.rcWork.left), monitor.rcWork.right - width);
			const int y = min(max(suggested->top, monitor.rcWork.top), monitor.rcWork.bottom - height);
			::SetWindowPos(m_hWnd, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}
	CRect client; GetClientRect(client); LayoutControls(client.Width(), client.Height());
	return 0;
}

LRESULT CSettingsDlg::OnDestroy(UINT /* unused: uMsg */, WPARAM /* unused: wParam */, LPARAM /* unused: lParam */, BOOL& /* unused: bHandled */)
{	
	SaveGeometry();
	for(int index = 0; index < PageIndex(SettingsPageId::Count); ++index)
		if(m_pages[index] && ::IsWindow(m_pages[index]->m_hWnd))
			::RemoveWindowSubclass(m_pages[index]->m_hWnd, PageSubclassProc, static_cast<UINT_PTR>(index));
	if(m_advancedPage) { m_advancedPage->DestroyWindow(); delete m_advancedPage; m_advancedPage = NULL; }
	if(m_generalPage) { m_generalPage->DestroyWindow(); delete m_generalPage; m_generalPage = NULL; }
	if(m_editorPage) { m_editorPage->DestroyWindow(); delete m_editorPage; m_editorPage = NULL; }
	if(m_spellingPage) { m_spellingPage->DestroyWindow(); delete m_spellingPage; m_spellingPage = NULL; }
	if(m_imagesPage) { m_imagesPage->DestroyWindow(); delete m_imagesPage; m_imagesPage = NULL; }
	if(m_sourcePage) { m_sourcePage->DestroyWindow(); delete m_sourcePage; m_sourcePage = NULL; }
	if(m_hotkeysPage) { m_hotkeysPage->DestroyWindow(); delete m_hotkeysPage; m_hotkeysPage = NULL; }
	if(m_wordsPage) { m_wordsPage->DestroyWindow(); delete m_wordsPage; m_wordsPage = NULL; }
	return 0;
}

CRect CSettingsDlg::GetPageRect()
{
	CRect client;
	GetClientRect(client);
	return CRect(m_pageLeft, m_pageTop, client.right - m_pageRightMargin, client.bottom - m_pageBottomMargin);
}

int CSettingsDlg::PageIndex(SettingsPageId page)
{
	return static_cast<int>(page);
}

void CSettingsDlg::SelectPage(SettingsPageId page)
{
	const int index = PageIndex(page);
	if(index < 0 || index >= _countof(m_pages) || !m_pages[index])
		return;
	if(m_currentPage != SettingsPageId::Count && m_pages[PageIndex(m_currentPage)] != m_pages[index])
		m_pages[PageIndex(m_currentPage)]->ShowWindow(SW_HIDE);
	m_currentPage = page;
	CRect rect = GetPageRect();
	m_pages[index]->MoveWindow(rect);
	LayoutPage(index);
	m_pages[index]->ShowWindow(SW_SHOW);
}

void CSettingsDlg::LayoutControls(int width, int height)
{
	if(!m_navigation.IsWindow())
		return;
	m_navigation.MoveWindow(m_navigationRect.left, m_navigationRect.top, m_navigationRect.Width(),
		max(0, height - m_navigationRect.top - m_buttonBottomMargin - m_buttonHeight - m_navigationBottomGap));
	const int cancelLeft = width - m_pageRightMargin - m_buttonWidth;
	const int buttonTop = height - m_buttonBottomMargin - m_buttonHeight;
	GetDlgItem(IDOK).MoveWindow(cancelLeft - m_buttonGap - m_buttonWidth, buttonTop, m_buttonWidth, m_buttonHeight);
	GetDlgItem(IDCANCEL).MoveWindow(cancelLeft, buttonTop, m_buttonWidth, m_buttonHeight);
	if(m_currentPage != SettingsPageId::Count && m_pages[PageIndex(m_currentPage)])
	{
		const int index = PageIndex(m_currentPage);
		m_pages[index]->MoveWindow(GetPageRect());
		LayoutPage(index);
	}
}

void CSettingsDlg::CapturePageLayouts()
{
	for(int index = 0; index < PageIndex(SettingsPageId::Count); ++index)
	{
		const HWND page = m_pages[index] ? m_pages[index]->m_hWnd : NULL;
		if(!page) continue;
		RECT client = {};
		::GetClientRect(page, &client);
		PageLayout& layout = m_pageLayouts[index];
		layout.contentSize = CSize(client.right, client.bottom);
		for(HWND child = ::GetWindow(page, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT))
		{
			RECT rect = {};
			::GetWindowRect(child, &rect);
			::MapWindowPoints(NULL, page, reinterpret_cast<POINT*>(&rect), 2);
			layout.controls.push_back({ child, CRect(rect), ::GetDlgCtrlID(child) });
		}
		::SetWindowLongPtrW(page, GWL_STYLE, ::GetWindowLongPtrW(page, GWL_STYLE) | WS_VSCROLL | WS_HSCROLL);
		::SetWindowSubclass(page, PageSubclassProc, static_cast<UINT_PTR>(index), reinterpret_cast<DWORD_PTR>(this));
	}
}

void CSettingsDlg::LayoutPage(int index)
{
	const HWND page = m_pages[index] ? m_pages[index]->m_hWnd : NULL;
	if(!page || !::IsWindow(page)) return;
	PageLayout& layout = m_pageLayouts[index];
	if(layout.contentSize.cx <= 0 || layout.contentSize.cy <= 0) return;
	RECT client = {};
	::GetClientRect(page, &client);
	::ShowScrollBar(page, SB_VERT, client.bottom < layout.contentSize.cy);
	::ShowScrollBar(page, SB_HORZ, client.right < layout.contentSize.cx);
	::GetClientRect(page, &client);
	::ShowScrollBar(page, SB_VERT, client.bottom < layout.contentSize.cy);
	::ShowScrollBar(page, SB_HORZ, client.right < layout.contentSize.cx);
	::GetClientRect(page, &client);
	const int availableWidth = max(0, client.right - client.left);
	const int availableHeight = max(0, client.bottom - client.top);
	const int contentWidth = max(availableWidth, layout.contentSize.cx);
	const int contentHeight = max(availableHeight, layout.contentSize.cy);
	layout.scrollX = min(max(0, layout.scrollX), contentWidth - availableWidth);
	layout.scrollY = min(max(0, layout.scrollY), contentHeight - availableHeight);
	SCROLLINFO info = { sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS };
	info.nMin = 0; info.nMax = contentWidth - 1; info.nPage = availableWidth; info.nPos = layout.scrollX;
	::SetScrollInfo(page, SB_HORZ, &info, TRUE);
	info.nMin = 0; info.nMax = contentHeight - 1; info.nPage = availableHeight; info.nPos = layout.scrollY;
	::SetScrollInfo(page, SB_VERT, &info, TRUE);
	const int extraWidth = contentWidth - layout.contentSize.cx;
	const int extraHeight = contentHeight - layout.contentSize.cy;
	for(const PageControlLayout& control : layout.controls)
	{
		if(!::IsWindow(control.window)) continue;
		CRect rect(control.rect);
		const bool nearRight = rect.right >= layout.contentSize.cx - UiMetrics::ScaleForDpi(20, m_layoutDpi);
		const bool stretchField = control.id == IDC_LANG || control.id == IDC_GENRE_CATALOG ||
			control.id == IDC_INTERFACE_THEME || control.id == IDC_UPDATE_CHANNEL ||
			control.id == IDC_EDITOR_BACKGROUND_IMAGE || control.id == IDC_EDITOR_BACKGROUND_LAYOUT ||
			control.id == IDC_SRCFONT || control.id == IDC_OPTIONS_SOURCE_PALETTE ||
			control.id == IDC_CUSTOM_DICT || control.id == IDC_SCRIPTS_FOLDER_PATH ||
			control.id == IDC_EDIT_NEW;
		if(stretchField || (nearRight && rect.Width() > layout.contentSize.cx / 2)) rect.right += extraWidth;
		else if(nearRight && rect.left > layout.contentSize.cx / 2) rect.OffsetRect(extraWidth, 0);
		if(index == PageIndex(SettingsPageId::Keyboard))
		{
			if(control.id == IDC_LIST_HOTKEYS_GROUPS || control.id == IDC_LIST_HOTKEYS) rect.bottom += extraHeight;
			else if(rect.top >= layout.contentSize.cy / 3) rect.OffsetRect(0, extraHeight);
		}
		else if(index == PageIndex(SettingsPageId::Words))
		{
			if(control.id == IDC_STATIC_WORDS_CHEVRON || control.id == IDC_LIST_WORDS) rect.bottom += extraHeight;
			else if(rect.top >= layout.contentSize.cy / 2) rect.OffsetRect(0, extraHeight);
		}
		else if((index == PageIndex(SettingsPageId::Editor) &&
			(control.id == IDC_EDITOR_BACKGROUND_PREVIEW || control.id == IDC_EDITOR_BACKGROUND_GROUP)) ||
			(index == PageIndex(SettingsPageId::Source) &&
			(control.id == IDC_OPTIONS_SOURCE_PREVIEW || control.id == IDC_OPTIONS_SOURCE_GROUP)))
			rect.bottom += extraHeight;
		rect.OffsetRect(-layout.scrollX, -layout.scrollY);
		::SetWindowPos(control.window, NULL, rect.left, rect.top, max(0, rect.Width()), max(0, rect.Height()),
			SWP_NOZORDER | SWP_NOACTIVATE);
	}
}

void CSettingsDlg::ScrollPage(int index, WPARAM command, bool horizontal)
{
	PageLayout& layout = m_pageLayouts[index];
	RECT client = {};
	::GetClientRect(m_pages[index]->m_hWnd, &client);
	const int maximum = max(0, horizontal ? layout.contentSize.cx - (client.right - client.left) :
		layout.contentSize.cy - (client.bottom - client.top));
	int& position = horizontal ? layout.scrollX : layout.scrollY;
	const int page = horizontal ? client.right - client.left : client.bottom - client.top;
	const int line = UiMetrics::ScaleForDpi(24, m_layoutDpi);
	switch(LOWORD(command))
	{
	case SB_LINEUP: position -= line; break;
	case SB_LINEDOWN: position += line; break;
	case SB_PAGEUP: position -= page; break;
	case SB_PAGEDOWN: position += page; break;
	case SB_THUMBTRACK: case SB_THUMBPOSITION:
		{ SCROLLINFO info = { sizeof(info), SIF_TRACKPOS }; ::GetScrollInfo(m_pages[index]->m_hWnd,
			horizontal ? SB_HORZ : SB_VERT, &info); position = info.nTrackPos; }
		break;
	case SB_TOP: position = 0; break;
	case SB_BOTTOM: position = maximum; break;
	default: return;
	}
	position = min(max(0, position), maximum);
	LayoutPage(index);
}

void CSettingsDlg::RevealFocusedPageControl(int index)
{
	const HWND page = m_pages[index]->m_hWnd;
	const HWND focus = ::GetFocus();
	if(!focus || !::IsChild(page, focus)) return;
	RECT rect = {}, client = {};
	::GetWindowRect(focus, &rect);
	::MapWindowPoints(NULL, page, reinterpret_cast<POINT*>(&rect), 2);
	::GetClientRect(page, &client);
	PageLayout& layout = m_pageLayouts[index];
	const int oldX = layout.scrollX, oldY = layout.scrollY;
	if(rect.left < client.left) layout.scrollX += rect.left - client.left;
	else if(rect.right > client.right) layout.scrollX += rect.right - client.right;
	if(rect.top < client.top) layout.scrollY += rect.top - client.top;
	else if(rect.bottom > client.bottom) layout.scrollY += rect.bottom - client.bottom;
	if(layout.scrollX != oldX || layout.scrollY != oldY) LayoutPage(index);
}

LRESULT CALLBACK CSettingsDlg::PageSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR subclassId, DWORD_PTR reference)
{
	CSettingsDlg* dialog = reinterpret_cast<CSettingsDlg*>(reference);
	const int index = static_cast<int>(subclassId);
	if(message == WM_VSCROLL && !lParam) { dialog->ScrollPage(index, wParam, false); return 0; }
	if(message == WM_HSCROLL && !lParam) { dialog->ScrollPage(index, wParam, true); return 0; }
	if(message == WM_MOUSEWHEEL)
	{
		const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		dialog->m_pageLayouts[index].scrollY -= ::MulDiv(delta, UiMetrics::ScaleForDpi(3 * 24, dialog->m_layoutDpi), WHEEL_DELTA);
		dialog->LayoutPage(index);
		return 0;
	}
	const LRESULT result = ::DefSubclassProc(window, message, wParam, lParam);
	if(message == WM_COMMAND || message == WM_NOTIFY) dialog->RevealFocusedPageControl(index);
	return result;
}

bool CSettingsDlg::ProbeLayout()
{
	CRect original;
	GetWindowRect(original);
	const int width = m_minimumWindowSize.cx + UiMetrics::ScaleForDpi(8, m_layoutDpi);
	const int height = m_minimumWindowSize.cy + UiMetrics::ScaleForDpi(8, m_layoutDpi);
	::SetWindowPos(m_hWnd, NULL, original.left, original.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	CRect client;
	GetClientRect(client);
	auto controlRect = [&](HWND control) {
		CRect rect;
		::GetWindowRect(control, &rect);
		::MapWindowPoints(NULL, m_hWnd, reinterpret_cast<POINT*>(&rect), 2);
		return rect;
	};
	const CRect ok = controlRect(GetDlgItem(IDOK));
	const CRect cancel = controlRect(GetDlgItem(IDCANCEL));
	const CRect navigation = controlRect(m_navigation);
	auto inside = [&](const CRect& rect) {
		return rect.left >= client.left && rect.top >= client.top &&
			rect.right <= client.right && rect.bottom <= client.bottom;
	};
	bool passed = inside(ok) && inside(cancel) && inside(navigation) && ok.right <= cancel.left;
	int verticallyScrollablePages = 0;
	int horizontallyScrollablePages = 0;
	for(int index = 0; index < PageIndex(SettingsPageId::Count); ++index)
	{
		SelectPage(static_cast<SettingsPageId>(index));
		const HWND page = m_pages[index]->m_hWnd;
		const CRect pageRect = controlRect(page);
		passed = passed && inside(pageRect) && navigation.right <= pageRect.left &&
			pageRect.bottom <= ok.top;
		RECT viewport = {};
		::GetClientRect(page, &viewport);
		const bool needsVerticalScroll = m_pageLayouts[index].contentSize.cy > viewport.bottom;
		if(needsVerticalScroll)
		{
			++verticallyScrollablePages;
			SCROLLINFO info = { sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS };
			::GetScrollInfo(page, SB_VERT, &info);
			passed = passed && info.nMax + 1 > static_cast<int>(info.nPage);
			::SendMessageW(page, WM_VSCROLL, MAKEWPARAM(SB_BOTTOM, 0), 0);
			::GetScrollInfo(page, SB_VERT, &info);
			passed = passed && info.nPos > 0;
			::SendMessageW(page, WM_VSCROLL, MAKEWPARAM(SB_TOP, 0), 0);
		}
		if(m_pageLayouts[index].contentSize.cx > viewport.right)
		{
			++horizontallyScrollablePages;
			SCROLLINFO info = { sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS };
			::GetScrollInfo(page, SB_HORZ, &info);
			passed = passed && info.nMax + 1 > static_cast<int>(info.nPage);
			::SendMessageW(page, WM_HSCROLL, MAKEWPARAM(SB_RIGHT, 0), 0);
			::GetScrollInfo(page, SB_HORZ, &info);
			passed = passed && info.nPos > 0;
			::SendMessageW(page, WM_HSCROLL, MAKEWPARAM(SB_LEFT, 0), 0);
		}
	}
	passed = passed && verticallyScrollablePages > 0 && horizontallyScrollablePages > 0;
	::SetWindowPos(m_hWnd, NULL, original.left, original.top, original.Width(), original.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE);
	SelectPage(SettingsPageId::General);
	::SetPropW(::GetParent(m_hWnd), L"FBE_SETTINGS_LAYOUT_PASSED", reinterpret_cast<HANDLE>(passed ? 1 : 2));
	return passed;
}

void CSettingsDlg::RestoreGeometry()
{
	CRect wanted;
	GetWindowRect(wanted);
	RECT saved = {};
	UINT savedDpi = 0;
	const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
	if(_Settings.GetSettingsDialogGeometry(saved, savedDpi))
	{
		wanted = saved;
		wanted.right = wanted.left + ::MulDiv(saved.right - saved.left, dpi, savedDpi);
		wanted.bottom = wanted.top + ::MulDiv(saved.bottom - saved.top, dpi, savedDpi);
	}
	MONITORINFO monitor = { sizeof(monitor) };
	if(!::GetMonitorInfoW(::MonitorFromRect(&wanted, MONITOR_DEFAULTTONEAREST), &monitor)) return;
	const int workWidth = monitor.rcWork.right - monitor.rcWork.left;
	const int workHeight = monitor.rcWork.bottom - monitor.rcWork.top;
	const int width = min(max(wanted.Width(), min(::MulDiv(m_minimumWindowSize.cx, dpi, m_initialDpi), workWidth)), workWidth);
	const int height = min(max(wanted.Height(), min(::MulDiv(m_minimumWindowSize.cy, dpi, m_initialDpi), workHeight)), workHeight);
	const int x = min(max(wanted.left, monitor.rcWork.left), monitor.rcWork.right - width);
	const int y = min(max(wanted.top, monitor.rcWork.top), monitor.rcWork.bottom - height);
	::SetWindowPos(m_hWnd, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void CSettingsDlg::SaveGeometry()
{
	if(!m_initialDpi || ::IsIconic(m_hWnd)) return;
	RECT rect = {};
	if(::GetWindowRect(m_hWnd, &rect))
		_Settings.SetSettingsDialogGeometry(rect, UiMetrics::DpiForWindow(m_hWnd), true);
}
