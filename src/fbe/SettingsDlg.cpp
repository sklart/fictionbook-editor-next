// SettingsDlg.cpp : Implementation of CSettingsDlg

#include "stdafx.h"
#include "Settings.h"
#include "SettingsDlg.h"
#include "RuntimeLocalization.h"
#include "res1.h"

extern CSettings _Settings;

// CSettingsDlg

CSettingsDlg::CSettingsDlg() :
	m_optionsPage(NULL), m_otherPage(NULL), m_sourcePage(NULL),
	m_hotkeysPage(NULL), m_wordsPage(NULL), m_currentPage(-1)
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

	m_optionsPage = new COptDlg;
	m_optionsPage->ShowDialog(m_hWnd);
	m_otherPage = new CSettingsOtherDlg;
	m_otherPage->Create(m_hWnd);
	m_sourcePage = new CSettingsNextDlg;
	m_sourcePage->Create(m_hWnd);
	m_hotkeysPage = new CSettingsHotkeysDlg;
	m_hotkeysPage->Create(m_hWnd);
	m_wordsPage = new CSettingsWordsDlg;
	m_wordsPage->Create(m_hWnd);

	// This host migration deliberately reuses the stable page implementations.
	// The following mapping is an interim compatibility layer while controls are
	// moved into dedicated logical pages in subsequent migration stages.
	m_pages[0] = m_optionsPage;
	m_pages[1] = m_optionsPage;
	m_pages[2] = m_sourcePage;
	m_pages[3] = m_otherPage;
	m_pages[4] = m_optionsPage;
	m_pages[5] = m_hotkeysPage;
	m_pages[6] = m_wordsPage;
	m_pages[7] = m_otherPage;

	HMODULE hThemeDll = LoadSystemLibrary(_T("UxTheme.dll"));
	if (hThemeDll != NULL)
	{
		PFNENABLETHEMEDIALOGTEXTURE pEnableThemeDialogTexture = (PFNENABLETHEMEDIALOGTEXTURE)GetProcAddress(hThemeDll, "EnableThemeDialogTexture");
		if(pEnableThemeDialogTexture)
		{
			pEnableThemeDialogTexture(*m_optionsPage, ETDT_USETABTEXTURE);
			pEnableThemeDialogTexture(*m_otherPage, ETDT_USETABTEXTURE);
			pEnableThemeDialogTexture(*m_sourcePage, ETDT_USETABTEXTURE);
		}
		FreeLibrary(hThemeDll);
	}	

	CRect client;
	GetClientRect(client);
	LayoutControls(client.Width(), client.Height());
	m_navigation.SetCurSel(0);
	SelectPage(0);
	return 1;
}

LRESULT CSettingsDlg::OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if(GetFocus() == GetDlgItem(IDOK))
	{
		CWindow* uniquePages[] = { m_optionsPage, m_otherPage, m_sourcePage, m_hotkeysPage, m_wordsPage };
		for(int i = _countof(uniquePages) - 1; i >= 0; --i)
			uniquePages[i]->SendMessage(WM_COMMAND, MAKELONG(IDOK, 0), 0);
		EndDialog(wID);
	}
	else
	{
		CWindow* pWnd = m_currentPage >= 0 ? m_pages[m_currentPage] : NULL;
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

	if(m_currentPage != 6)
	{
		if(_Settings.m_initial_scripts_folder != _Settings.GetScriptsFolder())
		{
			_Settings.SetScriptsFolder(_Settings.m_initial_scripts_folder, true);
		}

		CWindow* uniquePages[] = { m_optionsPage, m_otherPage, m_sourcePage, m_hotkeysPage, m_wordsPage };
		for(int i = _countof(uniquePages) - 1; i >= 0; --i)
			uniquePages[i]->SendMessage(WM_COMMAND, MAKELONG(IDCANCEL, 0), 0);
	}
	else
	{
		CWindow* pWnd = m_pages[6];
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
	SelectPage(m_navigation.GetCurSel());
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
	minMax->ptMinTrackSize.x = 430;
	minMax->ptMinTrackSize.y = 320;
	return 0;
}

LRESULT CSettingsDlg::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) 
{	
	if(m_optionsPage) { m_optionsPage->DestroyWindow(); delete m_optionsPage; m_optionsPage = NULL; }
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
	return CRect(118, 10, client.right - 10, client.bottom - 42);
}

void CSettingsDlg::SelectPage(int page)
{
	if(page < 0 || page >= _countof(m_pages) || !m_pages[page])
		return;
	if(m_currentPage >= 0 && m_pages[m_currentPage] != m_pages[page])
		m_pages[m_currentPage]->ShowWindow(SW_HIDE);
	m_currentPage = page;
	CRect rect = GetPageRect();
	m_pages[page]->MoveWindow(rect);
	m_pages[page]->ShowWindow(SW_SHOW);
}

void CSettingsDlg::LayoutControls(int width, int height)
{
	if(!m_navigation.IsWindow())
		return;
	m_navigation.MoveWindow(10, 10, 100, height - 52);
	GetDlgItem(IDOK).MoveWindow(width - 130, height - 30, 55, 15);
	GetDlgItem(IDCANCEL).MoveWindow(width - 65, height - 30, 55, 15);
	if(m_currentPage >= 0 && m_pages[m_currentPage])
		m_pages[m_currentPage]->MoveWindow(GetPageRect());
}
