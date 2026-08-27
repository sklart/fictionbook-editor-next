// SettingsDlg.cpp : Implementation of CSettingsDlg

#include "stdafx.h"
#include "Settings.h"
#include "SettingsDlg.h"
#include "RuntimeLocalization.h"
#include "res1.h"

extern CSettings _Settings;

// CSettingsDlg

CSettingsDlg::CSettingsDlg() :
	m_generalPage(NULL), m_editorPage(NULL), m_spellingPage(NULL), m_otherPage(NULL), m_sourcePage(NULL),
	m_hotkeysPage(NULL), m_wordsPage(NULL), m_currentPage(SettingsPageId::Count),
	m_pageLeft(0), m_pageTop(0), m_pageRightMargin(0), m_pageBottomMargin(0),
	m_buttonWidth(0), m_buttonHeight(0), m_buttonGap(0), m_buttonBottomMargin(0),
	m_navigationBottomGap(0)
{
	ZeroMemory(m_pages, sizeof(m_pages));
}

CSettingsDlg::~CSettingsDlg()
{
}

typedef BOOL (__stdcall *PFNISTHEMEACTIVE)();
typedef HRESULT (__stdcall *PFNENABLETHEMEDIALOGTEXTURE)(HWND hwnd, DWORD dwFlags);

static HMODULE LoadSystemLibrary(LPCTSTR fileName)
{
	TCHAR systemDirectory[MAX_PATH];
	const UINT length = ::GetSystemDirectory(systemDirectory, _countof(systemDirectory));
	if (length == 0 || length >= _countof(systemDirectory))
		return NULL;

	CString libraryPath(systemDirectory);
	if (libraryPath.Right(1) != _T("\\"))
		libraryPath += _T("\\");
	libraryPath += fileName;

	return ::LoadLibrary(libraryPath);
}

LRESULT CSettingsDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CSettingsDlg>::OnInitDialog(uMsg, wParam, lParam, bHandled);
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
	m_otherPage = new CSettingsOtherDlg;
	m_otherPage->Create(m_hWnd);
	m_sourcePage = new CSettingsSourcePage;
	m_sourcePage->Create(m_hWnd);
	m_hotkeysPage = new CSettingsHotkeysDlg;
	m_hotkeysPage->Create(m_hWnd);
	m_wordsPage = new CSettingsWordsDlg;
	m_wordsPage->Create(m_hWnd);

	// This host migration deliberately reuses the stable page implementations.
	// The following mapping is an interim compatibility layer while controls are
	// moved into dedicated logical pages in subsequent migration stages.
	m_pages[PageIndex(SettingsPageId::General)] = m_generalPage;
	m_pages[PageIndex(SettingsPageId::Editor)] = m_editorPage;
	m_pages[PageIndex(SettingsPageId::Source)] = m_sourcePage;
	m_pages[PageIndex(SettingsPageId::Images)] = m_otherPage;
	m_pages[PageIndex(SettingsPageId::Spelling)] = m_spellingPage;
	m_pages[PageIndex(SettingsPageId::Keyboard)] = m_hotkeysPage;
	m_pages[PageIndex(SettingsPageId::Words)] = m_wordsPage;
	m_pages[PageIndex(SettingsPageId::Advanced)] = m_advancedPage;

	HMODULE hThemeDll = LoadSystemLibrary(_T("UxTheme.dll"));
	if (hThemeDll != NULL)
	{
		PFNENABLETHEMEDIALOGTEXTURE pEnableThemeDialogTexture = (PFNENABLETHEMEDIALOGTEXTURE)GetProcAddress(hThemeDll, "EnableThemeDialogTexture");
		if(pEnableThemeDialogTexture)
		{
			pEnableThemeDialogTexture(*m_advancedPage, ETDT_USETABTEXTURE);
			pEnableThemeDialogTexture(*m_otherPage, ETDT_USETABTEXTURE);
			pEnableThemeDialogTexture(*m_sourcePage, ETDT_USETABTEXTURE);
		}
		FreeLibrary(hThemeDll);
	}	

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
	m_minimumWindowSize = CSize(client.Width(), client.Height());
	m_pageLeft = m_navigationRect.right + (m_navigationRect.left > 0 ? m_navigationRect.left : 1);
	m_pageTop = m_navigationRect.top;
	m_pageRightMargin = client.right - cancelRect.right;
	m_pageBottomMargin = client.bottom - okRect.top;
	m_buttonWidth = okRect.Width();
	m_buttonHeight = okRect.Height();
	m_buttonGap = cancelRect.left - okRect.right;
	m_buttonBottomMargin = client.bottom - okRect.bottom;
	m_navigationBottomGap = okRect.top - m_navigationRect.bottom;
	LayoutControls(client.Width(), client.Height());
	m_navigation.SetCurSel(0);
	SelectPage(SettingsPageId::General);
	return 1;
}

LRESULT CSettingsDlg::OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if(GetFocus() == GetDlgItem(IDOK))
	{
		CWindow* uniquePages[] = { m_generalPage, m_editorPage, m_spellingPage, m_advancedPage, m_otherPage, m_sourcePage, m_hotkeysPage, m_wordsPage };
		for(int i = _countof(uniquePages) - 1; i >= 0; --i)
			uniquePages[i]->SendMessage(WM_COMMAND, MAKELONG(IDOK, 0), 0);
		EndDialog(wID);
	}
	else
	{
		CWindow* pWnd = m_currentPage != SettingsPageId::Count ? m_pages[PageIndex(m_currentPage)] : NULL;
		if(pWnd)
		{
			pWnd->SendMessage(WM_COMMAND, MAKELONG(IDOK, 0), 0);
			pWnd->ShowWindow(SW_SHOW);
		}
	}

	return 0;
}

LRESULT CSettingsDlg::OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	int lres = 1;

	if(m_currentPage != SettingsPageId::Words)
	{
		if(_Settings.m_initial_scripts_folder != _Settings.GetScriptsFolder())
		{
			_Settings.SetScriptsFolder(_Settings.m_initial_scripts_folder, true);
		}

		CWindow* uniquePages[] = { m_generalPage, m_editorPage, m_spellingPage, m_advancedPage, m_otherPage, m_sourcePage, m_hotkeysPage, m_wordsPage };
		for(int i = _countof(uniquePages) - 1; i >= 0; --i)
			uniquePages[i]->SendMessage(WM_COMMAND, MAKELONG(IDCANCEL, 0), 0);
	}
	else
	{
		CWindow* pWnd = m_pages[PageIndex(SettingsPageId::Words)];
		if(pWnd)
		{
			lres = pWnd->SendMessage(WM_COMMAND, MAKELONG(IDCANCEL, 0), 0);
		}
	}

	if(lres)
		EndDialog(wID);

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
	minMax->ptMinTrackSize.x = m_minimumWindowSize.cx;
	minMax->ptMinTrackSize.y = m_minimumWindowSize.cy;
	return 0;
}

LRESULT CSettingsDlg::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) 
{	
	if(m_advancedPage) { m_advancedPage->DestroyWindow(); delete m_advancedPage; m_advancedPage = NULL; }
	if(m_generalPage) { m_generalPage->DestroyWindow(); delete m_generalPage; m_generalPage = NULL; }
	if(m_editorPage) { m_editorPage->DestroyWindow(); delete m_editorPage; m_editorPage = NULL; }
	if(m_spellingPage) { m_spellingPage->DestroyWindow(); delete m_spellingPage; m_spellingPage = NULL; }
	if(m_otherPage) { m_otherPage->DestroyWindow(); delete m_otherPage; m_otherPage = NULL; }
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
	m_pages[index]->ShowWindow(SW_SHOW);
}

void CSettingsDlg::LayoutControls(int width, int height)
{
	if(!m_navigation.IsWindow())
		return;
	m_navigation.MoveWindow(m_navigationRect.left, m_navigationRect.top, m_navigationRect.Width(), height - m_navigationRect.top - m_buttonBottomMargin - m_buttonHeight - m_navigationBottomGap);
	const int cancelLeft = width - m_pageRightMargin - m_buttonWidth;
	const int buttonTop = height - m_buttonBottomMargin - m_buttonHeight;
	GetDlgItem(IDOK).MoveWindow(cancelLeft - m_buttonGap - m_buttonWidth, buttonTop, m_buttonWidth, m_buttonHeight);
	GetDlgItem(IDCANCEL).MoveWindow(cancelLeft, buttonTop, m_buttonWidth, m_buttonHeight);
	if(m_currentPage != SettingsPageId::Count && m_pages[PageIndex(m_currentPage)])
		m_pages[PageIndex(m_currentPage)]->MoveWindow(GetPageRect());
}
