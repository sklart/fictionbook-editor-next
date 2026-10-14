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
	m_automaticSize(false), m_sizeMoveDpi(0), m_sizeMoveDpiChanged(false), m_sizingDuringMove(false),
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
	ModifyStyle(0, WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
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
	UpdateSourcePreviewContentHeight();

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
	const int navigationHeight = 8 * m_navigation.GetItemHeight(0) + m_navigationBottomGap;
	const int minimumPageHeight = max(navigationHeight, 6 * m_buttonHeight);
	m_minimumWindowSize = CSize(nonClientWidth + m_pageLeft + minimumPageWidth + m_pageRightMargin,
		nonClientHeight + m_pageTop + minimumPageHeight + m_pageBottomMargin);
	int preferredPageWidth = minimumPageWidth;
	int preferredPageHeight = max(minimumPageHeight, m_navigationRect.Height());
	for(const PageLayout& page : m_pageLayouts)
	{
		preferredPageWidth = max(preferredPageWidth, page.contentSize.cx);
		preferredPageHeight = max(preferredPageHeight, page.contentSize.cy);
	}
	m_preferredWindowSize = CSize(nonClientWidth + m_pageLeft + preferredPageWidth + m_pageRightMargin,
		nonClientHeight + m_pageTop + preferredPageHeight + m_pageBottomMargin);
	RestoreGeometry();
	GetClientRect(client);
	LayoutControls(client.Width(), client.Height());
	m_navigation.SetCurSel(0);
	SelectPage(SettingsPageId::General);
	if (IsSettingsDialogRuntimeTest())
	{
		::SetPropW(::GetParent(m_hWnd), L"FBE_CARET_INITIAL_SYSTEM",
			reinterpret_cast<HANDLE>(_Settings.GetCaretPreferences().mode == TextCaretMode::System ? 1 : 2));
		// Test-only automation still commits through the normal global OK path.
		// Use independent pages so ApplyConfChanges covers more than one category.
		m_generalPage->CheckDlgButton(IDC_CREATE_BACKUP_FILE, IsBackupSettingsSaveRuntimeTest() ? BST_UNCHECKED : BST_CHECKED);
		m_editorPage->SendDlgItemMessage(IDC_NBSP_CHAR, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"\x25AB"));
		m_sourcePage->SendDlgItemMessage(IDC_EDITOR_CARET_MODE, CB_SETCURSEL, 2, 0);
		m_sourcePage->SendDlgItemMessage(IDC_EDITOR_CARET_WIDTH, CB_SETCURSEL, 4, 0);
		m_sourcePage->SendDlgItemMessage(IDC_EDITOR_CARET_COLOR, CB_SETCURSEL, 3, 0);
		m_sourcePage->SendDlgItemMessage(IDC_EDITOR_CARET_BLINK, CB_SETCURSEL, 4, 0);
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

LRESULT CSettingsDlg::OnEnterSizeMove(UINT, WPARAM, LPARAM, BOOL&)
{
	GetWindowRect(m_sizeMoveStart);
	m_sizeMoveDpi = UiMetrics::DpiForWindow(m_hWnd);
	m_sizeMoveDpiChanged = false;
	m_sizingDuringMove = false;
	return 0;
}

LRESULT CSettingsDlg::OnSizing(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
	m_sizingDuringMove = true;
	bHandled = FALSE;
	return 0;
}

LRESULT CSettingsDlg::OnExitSizeMove(UINT, WPARAM, LPARAM, BOOL&)
{
	if(m_sizeMoveDpi)
	{
		CRect current;
		GetWindowRect(current);
		const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
		const int widthAtStartDpi = ::MulDiv(current.Width(), m_sizeMoveDpi, dpi);
		const int heightAtStartDpi = ::MulDiv(current.Height(), m_sizeMoveDpi, dpi);
		const int tolerance = UiMetrics::ScaleForDpi(2, m_sizeMoveDpi);
		const bool sizeChanged = abs(widthAtStartDpi - m_sizeMoveStart.Width()) > tolerance ||
			abs(heightAtStartDpi - m_sizeMoveStart.Height()) > tolerance;
		if(sizeChanged && (!m_sizeMoveDpiChanged || m_sizingDuringMove))
			m_automaticSize = false;
		m_sizeMoveDpi = 0;
	}
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

LRESULT CSettingsDlg::OnSystemAppearanceChanged(UINT message, WPARAM wParam, LPARAM lParam, BOOL&)
{
	ThemeManager::RefreshSystemTheme();
	if(m_editorPage && ::IsWindow(m_editorPage->m_hWnd))
		::SendMessage(m_editorPage->m_hWnd, message, wParam, lParam);
	return 0;
}

LRESULT CSettingsDlg::OnDpiChanged(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
	const UINT dpi = LOWORD(wParam);
	if(m_sizeMoveDpi && dpi && dpi != m_sizeMoveDpi) m_sizeMoveDpiChanged = true;
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
			page.templateSize = CSize(scale(page.templateSize.cx), scale(page.templateSize.cy));
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
			const int minimumWidth = min(::MulDiv(m_minimumWindowSize.cx, dpi, m_initialDpi), workWidth);
			const int minimumHeight = min(::MulDiv(m_minimumWindowSize.cy, dpi, m_initialDpi), workHeight);
			const int margin = UiMetrics::ScaleForDpi(28, dpi);
			const int preferredWidth = min(::MulDiv(m_preferredWindowSize.cx, dpi, m_initialDpi),
				max(minimumWidth, workWidth - 2 * margin));
			const int preferredHeight = min(::MulDiv(m_preferredWindowSize.cy, dpi, m_initialDpi),
				max(minimumHeight, workHeight - 2 * margin));
			const int width = m_automaticSize ? preferredWidth :
				min(max(suggested->right - suggested->left, minimumWidth), workWidth);
			const int height = m_automaticSize ? preferredHeight :
				min(max(suggested->bottom - suggested->top, minimumHeight), workHeight);
			const int wantedX = suggested->left + (m_automaticSize ?
				(suggested->right - suggested->left - width) / 2 : 0);
			const int wantedY = suggested->top + (m_automaticSize ?
				(suggested->bottom - suggested->top - height) / 2 : 0);
			const int x = min(max(wantedX, monitor.rcWork.left), monitor.rcWork.right - width);
			const int y = min(max(wantedY, monitor.rcWork.top), monitor.rcWork.bottom - height);
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
	{
		if(::IsChild(m_pages[PageIndex(m_currentPage)]->m_hWnd, ::GetFocus()))
			m_navigation.SetFocus();
		m_pages[PageIndex(m_currentPage)]->ShowWindow(SW_HIDE);
	}
	m_currentPage = page;
	if(page == SettingsPageId::Source && m_editorPage && m_sourcePage)
		m_sourcePage->SetPreviewFontSize(m_editorPage->CurrentPreviewFontSize());
	CRect rect = GetPageRect();
	m_pages[index]->MoveWindow(rect);
	LayoutPage(index);
	m_pages[index]->ShowWindow(SW_SHOW);
}

void CSettingsDlg::LayoutControls(int width, int height)
{
	if(!m_navigation.IsWindow())
		return;
	const int navigationHeight = max(0, GetPageRect().bottom - m_navigationRect.top);
	const int cancelLeft = width - m_pageRightMargin - m_buttonWidth;
	const int buttonTop = height - m_buttonBottomMargin - m_buttonHeight;
	HDWP positions = ::BeginDeferWindowPos(3);
	if(positions) positions = ::DeferWindowPos(positions, m_navigation, NULL, m_navigationRect.left,
		m_navigationRect.top, m_navigationRect.Width(), navigationHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	if(positions) positions = ::DeferWindowPos(positions, GetDlgItem(IDOK), NULL,
		cancelLeft - m_buttonGap - m_buttonWidth, buttonTop, m_buttonWidth, m_buttonHeight,
		SWP_NOZORDER | SWP_NOACTIVATE);
	if(positions) positions = ::DeferWindowPos(positions, GetDlgItem(IDCANCEL), NULL,
		cancelLeft, buttonTop, m_buttonWidth, m_buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	if(positions) ::EndDeferWindowPos(positions);
	else
	{
		m_navigation.MoveWindow(m_navigationRect.left, m_navigationRect.top, m_navigationRect.Width(), navigationHeight);
		GetDlgItem(IDOK).MoveWindow(cancelLeft - m_buttonGap - m_buttonWidth, buttonTop, m_buttonWidth, m_buttonHeight);
		GetDlgItem(IDCANCEL).MoveWindow(cancelLeft, buttonTop, m_buttonWidth, m_buttonHeight);
	}
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
		layout.templateSize = CSize(client.right, client.bottom);
		int right = 0, bottom = 0;
		for(HWND child = ::GetWindow(page, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT))
		{
			RECT rect = {};
			::GetWindowRect(child, &rect);
			::MapWindowPoints(NULL, page, reinterpret_cast<POINT*>(&rect), 2);
			layout.controls.push_back({ child, CRect(rect), ::GetDlgCtrlID(child) });
			// A combo's window rectangle includes its closed dropdown list. Only
			// the visible selection field needs to fit in the page viewport.
			RECT visible = rect;
			wchar_t className[16] = {};
			if(::GetClassNameW(child, className, _countof(className)) &&
				_wcsicmp(className, L"ComboBox") == 0)
			{
				COMBOBOXINFO combo = { sizeof(combo) };
				if(::GetComboBoxInfo(child, &combo))
				{
					POINT fieldBottom = { 0, max(combo.rcItem.bottom, combo.rcButton.bottom) };
					::ScreenToClient(page, &fieldBottom);
					visible.bottom = min(visible.bottom, fieldBottom.y +
						UiMetrics::ScaleForDpi(2, UiMetrics::DpiForWindow(page)));
				}
			}
			right = max(right, visible.right);
			bottom = max(bottom, visible.bottom);
		}
		const int padding = UiMetrics::ScaleForDpi(8, UiMetrics::DpiForWindow(page));
		layout.contentSize = CSize(right + padding, bottom + padding);
		layout.naturalContentHeight = layout.contentSize.cy;
		::SetWindowLongPtrW(page, GWL_STYLE, ::GetWindowLongPtrW(page, GWL_STYLE) | WS_VSCROLL | WS_HSCROLL);
		::SetWindowSubclass(page, PageSubclassProc, static_cast<UINT_PTR>(index), reinterpret_cast<DWORD_PTR>(this));
	}
}

void CSettingsDlg::UpdateSourcePreviewContentHeight()
{
	PageLayout& layout = m_pageLayouts[PageIndex(SettingsPageId::Source)];
	const int lineHeight = m_sourcePage->PreviewLineHeight();
	layout.contentSize.cy = max(layout.templateSize.cy, layout.naturalContentHeight);
	if(lineHeight <= 0) return;
	for(const PageControlLayout& control : layout.controls)
		if(control.id == IDC_OPTIONS_SOURCE_PREVIEW)
		{
			// Measure rendered Scintilla rows, including the child window border.
			const int minimum = 4 * lineHeight + 2 * ::GetSystemMetrics(SM_CYEDGE);
			layout.contentSize.cy += max(0, minimum - control.rect.Height());
			break;
		}
}

void CSettingsDlg::LayoutPage(int index)
{
	const HWND page = m_pages[index] ? m_pages[index]->m_hWnd : NULL;
	if(!page || !::IsWindow(page)) return;
	PageLayout& layout = m_pageLayouts[index];
	if(index == PageIndex(SettingsPageId::Source)) UpdateSourcePreviewContentHeight();
	if(layout.contentSize.cx <= 0 || layout.contentSize.cy <= 0) return;
	RECT client = {};
	// Re-evaluate both axes until the viewport accounts for the other bar.
	// Start without bars so an obsolete bar cannot keep its partner visible.
	::ShowScrollBar(page, SB_VERT, FALSE);
	::ShowScrollBar(page, SB_HORZ, FALSE);
	for(int pass = 0; pass < 3; ++pass)
	{
		::GetClientRect(page, &client);
		const bool vertical = client.bottom < layout.contentSize.cy;
		const bool horizontal = client.right < layout.contentSize.cx;
		const bool hadVertical = (::GetWindowLongPtrW(page, GWL_STYLE) & WS_VSCROLL) != 0;
		const bool hadHorizontal = (::GetWindowLongPtrW(page, GWL_STYLE) & WS_HSCROLL) != 0;
		if(vertical == hadVertical && horizontal == hadHorizontal) break;
		::ShowScrollBar(page, SB_VERT, vertical);
		::ShowScrollBar(page, SB_HORZ, horizontal);
	}
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
	const int extraWidth = max(0, contentWidth - max(layout.templateSize.cx, layout.contentSize.cx));
	const int extraHeight = max(0, contentHeight - max(layout.templateSize.cy,
		index == PageIndex(SettingsPageId::Source) ? layout.naturalContentHeight : layout.contentSize.cy));
	struct PageMove { HWND window; CRect rect; };
	std::vector<PageMove> moves;
	moves.reserve(layout.controls.size());
	for(const PageControlLayout& control : layout.controls)
	{
		if(!::IsWindow(control.window)) continue;
		CRect rect(control.rect);
		const bool nearRight = rect.right >= layout.templateSize.cx - UiMetrics::ScaleForDpi(20, m_layoutDpi);
		const bool stretchField =
			(index == PageIndex(SettingsPageId::General) && (control.id == IDC_LANG ||
				control.id == IDC_GENRE_CATALOG || control.id == IDC_INTERFACE_THEME ||
				control.id == IDC_DEFAULT_ENC || control.id == IDC_UPDATE_CHANNEL)) ||
			(index == PageIndex(SettingsPageId::Editor) && (control.id == IDC_EDITOR_BACKGROUND_IMAGE ||
				control.id == IDC_EDITOR_BACKGROUND_LAYOUT || control.id == IDC_EDITOR_BACKGROUND_PREVIEW ||
				control.id == IDC_EDITOR_PREVIEW_GROUP || control.id == IDC_EDITOR_CARET_MODE ||
				control.id == IDC_EDITOR_CARET_WIDTH || control.id == IDC_EDITOR_CARET_BLINK)) ||
			(index == PageIndex(SettingsPageId::Source) && (control.id == IDC_SRCFONT ||
				control.id == IDC_FBE_NEXT_SOURCE_CODE_GROUP || control.id == IDC_OPTIONS_SOURCE_GROUP ||
				control.id == IDC_EDITOR_CARET_GROUP || control.id == IDC_OPTIONS_SOURCE_PREVIEW_GROUP ||
				control.id == IDC_OPTIONS_SOURCE_PREVIEW)) ||
			(index == PageIndex(SettingsPageId::Spelling) && control.id == IDC_CUSTOM_DICT) ||
			(index == PageIndex(SettingsPageId::Words) && (control.id == IDC_EDIT_NEW ||
				control.id == IDC_LIST_WORDS)) ||
			(index == PageIndex(SettingsPageId::Advanced) && control.id == IDC_SCRIPTS_FOLDER_PATH);
		if((index == PageIndex(SettingsPageId::Words) && control.id == IDC_BUTTON_ADD) ||
			(index == PageIndex(SettingsPageId::Advanced) && control.id == IDC_SELECT_SCRIPTS_FOLDER_BUTTON))
			rect.OffsetRect(extraWidth, 0);
		else if(stretchField || (nearRight && rect.Width() > layout.templateSize.cx / 2 &&
			index != PageIndex(SettingsPageId::Source))) rect.right += extraWidth;
		else if(nearRight && rect.left > layout.templateSize.cx / 2 &&
			index != PageIndex(SettingsPageId::Source)) rect.OffsetRect(extraWidth, 0);
		if(index == PageIndex(SettingsPageId::Keyboard))
		{
			if(control.id == IDC_LIST_HOTKEYS_GROUPS || control.id == IDC_LIST_HOTKEYS) rect.bottom += extraHeight;
			else if(rect.top >= layout.templateSize.cy / 3) rect.OffsetRect(0, extraHeight);
		}
		else if(index == PageIndex(SettingsPageId::Words))
		{
			if(control.id == IDC_STATIC_WORDS_CHEVRON || control.id == IDC_LIST_WORDS) rect.bottom += extraHeight;
			else if(rect.top >= layout.templateSize.cy / 2) rect.OffsetRect(0, extraHeight);
		}
		else if(index == PageIndex(SettingsPageId::Source) &&
			(control.id == IDC_OPTIONS_SOURCE_PREVIEW || control.id == IDC_OPTIONS_SOURCE_PREVIEW_GROUP))
			rect.bottom += extraHeight;
		else if(index == PageIndex(SettingsPageId::Editor) &&
			(control.id == IDC_EDITOR_BACKGROUND_PREVIEW || control.id == IDC_EDITOR_PREVIEW_GROUP))
			rect.bottom += extraHeight;
		rect.OffsetRect(-layout.scrollX, -layout.scrollY);
		RECT current = {};
		if(::GetWindowRect(control.window, &current))
		{
			::MapWindowPoints(NULL, page, reinterpret_cast<POINT*>(&current), 2);
			if(CRect(current) == rect) continue;
		}
		moves.push_back({ control.window, rect });
	}
	if(moves.empty()) return;
	const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW;
	HDWP positions = ::BeginDeferWindowPos(static_cast<int>(moves.size()));
	for(const PageMove& move : moves)
	{
		if(!positions) break;
		positions = ::DeferWindowPos(positions, move.window, NULL, move.rect.left, move.rect.top,
			max(0, move.rect.Width()), max(0, move.rect.Height()),
			::GetFocus() == move.window ? flags & ~SWP_NOREDRAW : flags);
	}
	if(!positions || !::EndDeferWindowPos(positions))
		for(const PageMove& move : moves)
			::SetWindowPos(move.window, NULL, move.rect.left, move.rect.top,
				max(0, move.rect.Width()), max(0, move.rect.Height()),
				::GetFocus() == move.window ? flags & ~SWP_NOREDRAW : flags);
	if(index == PageIndex(SettingsPageId::Words) && m_wordsPage)
		m_wordsPage->ResizeColumns();
	if(index == PageIndex(SettingsPageId::Keyboard) && m_hotkeysPage)
		m_hotkeysPage->RefreshInfoScrollbars();
	::RedrawWindow(page, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
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
	CRect client;
	auto controlRect = [&](HWND control) {
		CRect rect;
		::GetWindowRect(control, &rect);
		::MapWindowPoints(NULL, m_hWnd, reinterpret_cast<POINT*>(&rect), 2);
		return rect;
	};
	auto inside = [&](const CRect& rect) {
		return rect.left >= client.left && rect.top >= client.top &&
			rect.right <= client.right && rect.bottom <= client.bottom;
	};
	auto wordsGeometry = [&]() {
		RECT listClient = {};
		m_wordsPage->m_list_words.GetClientRect(&listClient);
		const CRect list = controlRect(m_wordsPage->m_list_words);
		const CRect group = controlRect(m_wordsPage->GetDlgItem(IDC_STATIC_WORDS_CHEVRON));
		const int columns = m_wordsPage->m_list_words.GetColumnWidth(0) +
			m_wordsPage->m_list_words.GetColumnWidth(1);
		return list.left > group.left && list.right < group.right && list.bottom < group.bottom &&
			columns <= listClient.right - listClient.left &&
			!(::GetWindowLongPtrW(m_wordsPage->m_list_words, GWL_STYLE) & WS_HSCROLL);
	};
	bool passed = true;
	GetClientRect(client);
	const CRect ok = controlRect(GetDlgItem(IDOK));
	const CRect cancel = controlRect(GetDlgItem(IDCANCEL));
	const CRect navigation = controlRect(m_navigation);
	passed = passed && inside(ok) && inside(cancel) && inside(navigation) && ok.right <= cancel.left;
	MONITORINFO monitor = { sizeof(monitor) };
	const bool hasWorkArea = ::GetMonitorInfoW(::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &monitor) != FALSE;
	const int margin = UiMetrics::ScaleForDpi(28, m_layoutDpi);
	const bool preferredFits = hasWorkArea &&
		m_preferredWindowSize.cx + 2 * margin <= monitor.rcWork.right - monitor.rcWork.left &&
		m_preferredWindowSize.cy + 2 * margin <= monitor.rcWork.bottom - monitor.rcWork.top;
	RECT saved = {};
	UINT savedDpi = 0;
	if(hasWorkArea && !m_automaticSize && _Settings.GetSettingsDialogGeometry(saved, savedDpi) &&
		savedDpi == m_layoutDpi && saved.left >= monitor.rcWork.left &&
		saved.top >= monitor.rcWork.top && saved.right <= monitor.rcWork.right &&
		saved.bottom <= monitor.rcWork.bottom &&
		saved.right - saved.left >= m_minimumWindowSize.cx &&
		saved.bottom - saved.top >= m_minimumWindowSize.cy)
		passed = passed && original == CRect(saved);
	if(preferredFits && m_automaticSize)
		passed = passed && original.Width() >= m_preferredWindowSize.cx &&
			original.Height() >= m_preferredWindowSize.cy;
	// Switching among all eight pages must not resize the outer dialog. On a
	// monitor that can hold the preferred size, its initial pages need no bars.
	for(int index = 0; index < PageIndex(SettingsPageId::Count); ++index)
	{
		SelectPage(static_cast<SettingsPageId>(index));
		const HWND page = m_pages[index]->m_hWnd;
		const CRect pageRect = controlRect(page);
		passed = passed && inside(pageRect) && navigation.right <= pageRect.left && pageRect.bottom <= ok.top &&
			controlRect(m_navigation).bottom == pageRect.bottom;
		if(index == PageIndex(SettingsPageId::Words)) passed = passed && wordsGeometry();
		if(index == PageIndex(SettingsPageId::Keyboard))
		{
			const HWND label = ::GetDlgItem(page, IDC_SETTINGS_OTHER_CHANGE_TO);
			const HWND combo = ::GetDlgItem(page, IDC_KEYB_LAYOUT);
			const HWND toggle = ::GetDlgItem(page, IDC_CHANGE_KEYB);
			passed = passed && label && ::IsWindowEnabled(label) && combo && toggle &&
				(::IsWindowEnabled(combo) != FALSE) ==
				(::SendMessage(toggle, BM_GETCHECK, 0, 0) == BST_CHECKED);
		}
		CRect unchanged;
		GetWindowRect(unchanged);
		passed = passed && unchanged == original;
		if(preferredFits && m_automaticSize)
		{
			RECT viewport = {};
			::GetClientRect(page, &viewport);
			passed = passed && viewport.right >= m_pageLayouts[index].contentSize.cx &&
				viewport.bottom >= m_pageLayouts[index].contentSize.cy &&
				!(::GetWindowLongPtrW(page, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL));
		}
	}
	SelectPage(SettingsPageId::Editor);
	const CRect backgroundGroup = controlRect(m_editorPage->GetDlgItem(IDC_EDITOR_BACKGROUND_GROUP));
	const CRect editorPreview = controlRect(m_editorPage->GetDlgItem(IDC_EDITOR_BACKGROUND_PREVIEW));
	const CRect caretGroup = controlRect(m_sourcePage->GetDlgItem(IDC_EDITOR_CARET_GROUP));
	const CRect sourcePreview = controlRect(m_sourcePage->GetDlgItem(IDC_OPTIONS_SOURCE_PREVIEW));
	passed = passed && editorPreview.top >= backgroundGroup.bottom &&
		caretGroup.bottom <= sourcePreview.top;
	auto fontListValid = [](HWND combo) {
		const int count = static_cast<int>(::SendMessageW(combo, CB_GETCOUNT, 0, 0));
		if(count < 2 || !(::GetWindowLongPtrW(combo, GWL_STYLE) & CBS_SORT)) return false;
		std::vector<CString> names;
		for(int index = 0; index < count; ++index)
		{
			const int length = static_cast<int>(::SendMessageW(combo, CB_GETLBTEXTLEN, index, 0));
			if(length <= 0 || length == CB_ERR) return false;
			std::vector<wchar_t> value(static_cast<size_t>(length) + 1);
			::SendMessageW(combo, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(value.data()));
			const CString current(value.data());
			if(current[0] == L'@' || std::find_if(names.begin(), names.end(), [&](const CString& existing) {
				return existing.CompareNoCase(current) == 0;
			}) != names.end()) return false;
			names.push_back(current);
		}
		return true;
	};
	const HWND editorFonts = m_editorPage->GetDlgItem(IDC_FONT);
	const HWND sourceFonts = m_sourcePage->GetDlgItem(IDC_SRCFONT);
	passed = passed && fontListValid(editorFonts) && fontListValid(sourceFonts);
	const HWND previewHost = m_sourcePage->GetDlgItem(IDC_OPTIONS_SOURCE_PREVIEW);
	const HWND previewEditor = ::FindWindowExW(previewHost, NULL, L"Scintilla", NULL);
	if(previewEditor && sourceFonts)
	{
		char originalFont[128] = {};
		char selectedFont[128] = {};
		::SendMessageW(previewEditor, SCI_STYLEGETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>(originalFont));
		const int originalIndex = static_cast<int>(::SendMessageW(sourceFonts, CB_GETCURSEL, 0, 0));
		const int alternateIndex = originalIndex == 0 ? 1 : 0;
		::SendMessageW(sourceFonts, CB_SETCURSEL, alternateIndex, 0);
		::SendMessageW(m_sourcePage->m_hWnd, WM_COMMAND, MAKEWPARAM(IDC_SRCFONT, CBN_SELCHANGE),
			reinterpret_cast<LPARAM>(sourceFonts));
		::SendMessageW(previewEditor, SCI_STYLEGETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>(selectedFont));
		passed = passed && strcmp(originalFont, selectedFont) != 0;
		::SendMessageW(sourceFonts, CB_SETCURSEL, originalIndex, 0);
		::SendMessageW(m_sourcePage->m_hWnd, WM_COMMAND, MAKEWPARAM(IDC_SRCFONT, CBN_SELCHANGE),
			reinterpret_cast<LPARAM>(sourceFonts));
	}
	else passed = false;
	if(preferredFits && m_automaticSize)
	{
		const int deficit = UiMetrics::ScaleForDpi(20, m_layoutDpi);
		::SetWindowPos(m_hWnd, NULL, original.left, original.top,
			original.Width() - deficit, original.Height() - deficit, SWP_NOZORDER | SWP_NOACTIVATE);
		SelectPage(SettingsPageId::Source);
		RECT viewport = {};
		::GetClientRect(m_sourcePage->m_hWnd, &viewport);
		passed = passed && (viewport.right < m_pageLayouts[PageIndex(SettingsPageId::Source)].contentSize.cx ||
			viewport.bottom < m_pageLayouts[PageIndex(SettingsPageId::Source)].contentSize.cy);
		::SetWindowPos(m_hWnd, NULL, original.left, original.top, original.Width(), original.Height(),
			SWP_NOZORDER | SWP_NOACTIVATE);
	}
	// Distinguish moving the window from a deliberate resize. Restore the
	// runtime probe's temporary state before the normal close/save path.
	const bool originalAutomaticSize = m_automaticSize;
	::SendMessageW(m_hWnd, WM_ENTERSIZEMOVE, 0, 0);
	::SetWindowPos(m_hWnd, NULL, original.left + UiMetrics::ScaleForDpi(8, m_layoutDpi),
		original.top, original.Width(), original.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
	m_sizeMoveDpiChanged = true; // A move across monitors must retain automatic sizing.
	::SendMessageW(m_hWnd, WM_EXITSIZEMOVE, 0, 0);
	passed = passed && m_automaticSize == originalAutomaticSize;
	::SetWindowPos(m_hWnd, NULL, original.left, original.top, original.Width(), original.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE);
	::SendMessageW(m_hWnd, WM_ENTERSIZEMOVE, 0, 0);
	BOOL sizingHandled = FALSE;
	OnSizing(WM_SIZING, WMSZ_RIGHT, 0, sizingHandled);
	::SetWindowPos(m_hWnd, NULL, original.left, original.top,
		original.Width() + UiMetrics::ScaleForDpi(8, m_layoutDpi), original.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE);
	::SetWindowPos(m_hWnd, NULL, original.left, original.top, original.Width(), original.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE);
	::SendMessageW(m_hWnd, WM_EXITSIZEMOVE, 0, 0);
	passed = passed && m_automaticSize == originalAutomaticSize;
	::SendMessageW(m_hWnd, WM_ENTERSIZEMOVE, 0, 0);
	const int resizeStep = UiMetrics::ScaleForDpi(20, m_layoutDpi);
	const int resizedWidth = hasWorkArea && original.Width() + resizeStep <=
		monitor.rcWork.right - monitor.rcWork.left ? original.Width() + resizeStep :
		max(m_minimumWindowSize.cx, original.Width() - resizeStep);
	::SetWindowPos(m_hWnd, NULL, original.left, original.top,
		resizedWidth, original.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE);
	::SendMessageW(m_hWnd, WM_EXITSIZEMOVE, 0, 0);
	if(resizedWidth != original.Width()) passed = passed && !m_automaticSize;
	::SetWindowPos(m_hWnd, NULL, original.left, original.top, original.Width(), original.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE);
	m_automaticSize = originalAutomaticSize;
	const int width = m_minimumWindowSize.cx + UiMetrics::ScaleForDpi(8, m_layoutDpi);
	const int height = m_minimumWindowSize.cy + UiMetrics::ScaleForDpi(8, m_layoutDpi);
	::SetWindowPos(m_hWnd, NULL, original.left, original.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	GetClientRect(client);
	passed = passed && inside(controlRect(GetDlgItem(IDOK))) && inside(controlRect(GetDlgItem(IDCANCEL))) &&
		inside(controlRect(m_navigation));
	int verticallyScrollablePages = 0;
	int horizontallyScrollablePages = 0;
	for(int index = 0; index < PageIndex(SettingsPageId::Count); ++index)
	{
		SelectPage(static_cast<SettingsPageId>(index));
		const HWND page = m_pages[index]->m_hWnd;
		const CRect pageRect = controlRect(page);
		passed = passed && inside(pageRect) && navigation.right <= pageRect.left &&
			pageRect.bottom <= ok.top && controlRect(m_navigation).bottom == pageRect.bottom;
		if(index == PageIndex(SettingsPageId::Words)) passed = passed && wordsGeometry();
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
	// Repeat narrow and preferred layouts: group frames and their controls on
	// the Keyboard page must return to the same non-overlapping geometry.
	for(int cycle = 0; cycle < 3; ++cycle)
	{
		const bool narrow = (cycle % 2) == 0;
		::SetWindowPos(m_hWnd, NULL, original.left, original.top,
			narrow ? width : original.Width(), narrow ? height : original.Height(),
			SWP_NOZORDER | SWP_NOACTIVATE);
		SelectPage(SettingsPageId::Keyboard);
		const HWND page = m_hotkeysPage->m_hWnd;
		auto pageRect = [&](int id) {
			CRect rect;
			const HWND control = ::GetDlgItem(page, id);
			if(control) { ::GetWindowRect(control, &rect); ::MapWindowPoints(NULL, page, reinterpret_cast<POINT*>(&rect), 2); }
			return rect;
		};
		const CRect actions = pageRect(IDC_STATIC_HOTKEY_ACTIONS);
		const CRect collision = pageRect(IDC_STATIC_HOTKEY_COLLISION);
		const CRect description = pageRect(IDC_STATIC_HOTKEY_DESCRIPTION);
		const CRect assign = pageRect(IDC_BUTTON_HOTKEY_ASSIGN);
		const CRect collisionText = pageRect(IDC_EDIT_HOTKEY_COLLISION);
		const CRect descriptionText = pageRect(IDC_EDIT_HOTKEY_DESCRIPTION);
		passed = passed && actions.Width() > 0 && collision.Width() > 0 && description.Width() > 0 &&
			collision.right < actions.left && collision.bottom <= description.top &&
			assign.left >= actions.left && assign.right <= actions.right && assign.bottom <= actions.bottom &&
			collisionText.left >= collision.left && collisionText.right <= collision.right &&
			descriptionText.left >= description.left && descriptionText.right <= description.right;
		::SetFocus(m_hotkeysPage->GetDlgItem(IDC_EDIT_HOTKEY_DESCRIPTION));
		passed = passed && ::GetFocus() == m_hotkeysPage->m_hotkeys;
		::SetFocus(m_hotkeysPage->GetDlgItem(IDC_EDIT_HOTKEY_COLLISION));
		passed = passed && ::GetFocus() == m_hotkeysPage->m_hotkeys;
		::SetFocus(m_hotkeysPage->m_editHotkey);
		passed = passed && ::GetFocus() == m_hotkeysPage->m_editHotkey;
		GetClientRect(client);
		passed = passed && inside(controlRect(GetDlgItem(IDOK))) && inside(controlRect(GetDlgItem(IDCANCEL))) &&
			controlRect(GetDlgItem(IDOK)).right <= controlRect(GetDlgItem(IDCANCEL)).left;
	}
	SelectPage(SettingsPageId::Keyboard);
	const HWND descriptionEdit = m_hotkeysPage->GetDlgItem(IDC_EDIT_HOTKEY_DESCRIPTION);
	const int savedLength = ::GetWindowTextLengthW(descriptionEdit);
	std::vector<wchar_t> savedDescription(static_cast<size_t>(savedLength) + 1);
	::GetWindowTextW(descriptionEdit, savedDescription.data(), savedLength + 1);
	::SetWindowTextW(descriptionEdit, L"Short description");
	m_hotkeysPage->RefreshInfoScrollbars();
	passed = passed && !(::GetWindowLongPtrW(descriptionEdit, GWL_STYLE) & WS_VSCROLL);
	::SetWindowTextW(descriptionEdit,
		L"A long description of the selected command wraps across several lines and remains readable "
		L"when the settings window is narrow. Additional details keep the text longer than the edit field.");
	m_hotkeysPage->RefreshInfoScrollbars();
	passed = passed && ::SendMessageW(descriptionEdit, EM_GETLINECOUNT, 0, 0) > 1 &&
		(::GetWindowLongPtrW(descriptionEdit, GWL_STYLE) & WS_VSCROLL) != 0;
	::SetWindowTextW(descriptionEdit, savedDescription.data());
	m_hotkeysPage->RefreshInfoScrollbars();
	// Exercise the three main pages across repeated compact, original and
	// expanded sizes. Measure preview rows using Scintilla's current font metrics.
	const int expandedHeight = hasWorkArea ?
		min(original.Height() + UiMetrics::ScaleForDpi(160, m_layoutDpi),
			monitor.rcWork.bottom - original.top) : original.Height();
	int sourcePreviewBaselineHeight = 0;
	for(const PageControlLayout& control : m_pageLayouts[PageIndex(SettingsPageId::Source)].controls)
		if(control.id == IDC_OPTIONS_SOURCE_PREVIEW) sourcePreviewBaselineHeight = control.rect.Height();
	for(int cycle = 0; cycle < 6; ++cycle)
	{
		const int targetHeight = cycle % 3 == 0 ? height :
			cycle % 3 == 1 ? original.Height() : expandedHeight;
		const int targetWidth = cycle % 3 == 0 ? width : original.Width();
		::SetWindowPos(m_hWnd, NULL, original.left, original.top, targetWidth, targetHeight,
			SWP_NOZORDER | SWP_NOACTIVATE);
		GetClientRect(client);
		for(SettingsPageId pageId : { SettingsPageId::General, SettingsPageId::Editor, SettingsPageId::Source })
		{
			SelectPage(pageId);
			passed = passed && inside(controlRect(GetDlgItem(IDOK))) &&
				inside(controlRect(GetDlgItem(IDCANCEL)));
			if(pageId != SettingsPageId::Source) continue;
			for(const int fontSize : { 10, 12, 16 })
			{
				m_sourcePage->SetPreviewFontSize(fontSize);
				LayoutPage(PageIndex(SettingsPageId::Source));
				const int lineHeight = m_sourcePage->PreviewLineHeight();
				RECT previewClient = {};
				::GetClientRect(previewHost, &previewClient);
				const CRect group = controlRect(m_sourcePage->GetDlgItem(IDC_OPTIONS_SOURCE_PREVIEW_GROUP));
				const CRect preview = controlRect(previewHost);
				const bool minimumRows = lineHeight > 0 && previewClient.bottom >= 4 * lineHeight;
				const bool insideGroup = group.left < preview.left && group.top < preview.top &&
					preview.right < group.right && preview.bottom < group.bottom;
				passed = passed && minimumRows && insideGroup;
				RECT pageClient = {};
				::GetClientRect(m_sourcePage->m_hWnd, &pageClient);
				if(pageClient.bottom >= max(m_pageLayouts[PageIndex(SettingsPageId::Source)].templateSize.cy,
					m_pageLayouts[PageIndex(SettingsPageId::Source)].naturalContentHeight) +
					max(0, 6 * lineHeight + 2 * ::GetSystemMetrics(SM_CYEDGE) - sourcePreviewBaselineHeight))
					passed = passed && previewClient.bottom >= 6 * lineHeight;
			}
		}
	}
	m_sourcePage->SetPreviewFontSize(m_editorPage->CurrentPreviewFontSize());
	::SetWindowPos(m_hWnd, NULL, original.left, original.top, original.Width(), original.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE);
	CRect restored;
	GetWindowRect(restored);
	passed = passed && restored == original;
	SelectPage(SettingsPageId::Words);
	m_wordsPage->m_editActive = true;
	m_wordsPage->m_editidx = -1;
	m_wordsPage->m_edit.ShowWindow(SW_SHOWNA);
	::SetFocus(m_wordsPage->m_edit);
	SelectPage(SettingsPageId::Editor);
	passed = passed && !m_wordsPage->m_editActive && !::IsWindowVisible(m_wordsPage->m_edit) &&
		::GetFocus() == m_navigation;
	const UINT originalLayoutDpi = m_layoutDpi;
	const UINT probeDpi = originalLayoutDpi == 144 ? 192 : 144;
	::SendMessageW(m_hWnd, WM_DPICHANGED, MAKELONG(probeDpi, probeDpi), 0);
	SelectPage(SettingsPageId::Words);
	passed = passed && controlRect(m_navigation).bottom == controlRect(m_wordsPage->m_hWnd).bottom &&
		wordsGeometry();
	::SendMessageW(m_hWnd, WM_DPICHANGED, MAKELONG(originalLayoutDpi, originalLayoutDpi), 0);
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
	const bool hasSavedGeometry = _Settings.GetSettingsDialogGeometry(saved, savedDpi);
	m_automaticSize = !hasSavedGeometry || !_Settings.GetSettingsDialogManualSize();
	if(hasSavedGeometry)
	{
		wanted = saved;
		wanted.right = wanted.left + ::MulDiv(saved.right - saved.left, dpi, savedDpi);
		wanted.bottom = wanted.top + ::MulDiv(saved.bottom - saved.top, dpi, savedDpi);
	}
	MONITORINFO monitor = { sizeof(monitor) };
	if(!::GetMonitorInfoW(::MonitorFromRect(&wanted, MONITOR_DEFAULTTONEAREST), &monitor)) return;
	const int workWidth = monitor.rcWork.right - monitor.rcWork.left;
	const int workHeight = monitor.rcWork.bottom - monitor.rcWork.top;
	const int minimumWidth = min(::MulDiv(m_minimumWindowSize.cx, dpi, m_initialDpi), workWidth);
	const int minimumHeight = min(::MulDiv(m_minimumWindowSize.cy, dpi, m_initialDpi), workHeight);
	const int margin = UiMetrics::ScaleForDpi(28, dpi);
	const int preferredWidth = min(m_preferredWindowSize.cx, max(minimumWidth, workWidth - 2 * margin));
	const int preferredHeight = min(m_preferredWindowSize.cy, max(minimumHeight, workHeight - 2 * margin));
	const int width = m_automaticSize ? preferredWidth : min(max(wanted.Width(), minimumWidth), workWidth);
	const int height = m_automaticSize ? preferredHeight : min(max(wanted.Height(), minimumHeight), workHeight);
	if(m_automaticSize)
	{
		wanted.left -= (width - wanted.Width()) / 2;
		wanted.top -= (height - wanted.Height()) / 2;
	}
	const int x = min(max(wanted.left, monitor.rcWork.left), monitor.rcWork.right - width);
	const int y = min(max(wanted.top, monitor.rcWork.top), monitor.rcWork.bottom - height);
	::SetWindowPos(m_hWnd, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void CSettingsDlg::SaveGeometry()
{
	if(!m_initialDpi || ::IsIconic(m_hWnd)) return;
	RECT rect = {};
	if(::GetWindowRect(m_hWnd, &rect))
		_Settings.SetSettingsDialogGeometry(rect, UiMetrics::DpiForWindow(m_hWnd), !m_automaticSize, true);
}
