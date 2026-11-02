// SettingsDlg.cpp : Implementation of CSettingsDlg

#include "stdafx.h"
#include "../../utils/utils.h"
#include "..\\..\\Settings.h"
#include "SettingsDlg.h"
#include "EditorFontFamilies.h"
#include "../SettingsEditSession.h"
#include "../SettingsStore.h"
#include "../SettingsPaths.h"
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
		return RuntimeTests::IsScenario(L"settings-residual-runtime") ||
			(scenarioLength == wcslen(L"settings-dialog-runtime") && wcscmp(scenario, L"settings-dialog-runtime") == 0) ||
			(scenarioLength == wcslen(L"backup-settings-save-runtime") && wcscmp(scenario, L"backup-settings-save-runtime") == 0);
	}

	bool IsBackupSettingsSaveRuntimeTest() { return RuntimeTests::IsScenario(L"backup-settings-save-runtime"); }
}

// CSettingsDlg

CSettingsDlg::CSettingsDlg(const CString& legacyDictionaryDirectory, bool nbspDisplayLocked) :
	m_legacyDictionaryDirectory(legacyDictionaryDirectory),
	m_nbspDisplayLocked(nbspDisplayLocked),
	m_advancedPage(NULL), m_generalPage(NULL), m_commonPage(NULL), m_editorPage(NULL), m_spellingPage(NULL), m_imagesPage(NULL), m_sourcePage(NULL),
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
	const bool measure = RuntimeTests::IsScenario(L"community-settings-search-performance");
	LARGE_INTEGER frequency = {}, last = {};
	CStringA measurements;
	if (measure) { ::QueryPerformanceFrequency(&frequency); ::QueryPerformanceCounter(&last); }
	auto stage = [&](const char* name) {
		if (!measure) return;
		LARGE_INTEGER now = {}; ::QueryPerformanceCounter(&now);
		measurements.AppendFormat("%s\t%.3f\r\n", name, (now.QuadPart - last.QuadPart) * 1000.0 / frequency.QuadPart);
		last = now;
	};
	CAxDialogImpl<CSettingsDlg>::OnInitDialog(uMsg, wParam, lParam, bHandled);
	ModifyStyle(0, WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	ThemeManager::ApplyToWindow(m_hWnd);
	stage("dialog_theme");
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_TOOLS_SETTINGS);
	stage("dialog_localization");
	m_navigation = GetDlgItem(IDC_SETTINGS_NAV);
	::SetWindowSubclass(m_navigation, NavigationKeysSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
	const struct { LPCWSTR key; LPCWSTR fallback; } navigationItems[] = {
		{ L"fbe.settings.nav.general", L"General" },
		{ L"fbe.settings.nav.appearance", L"Appearance" },
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

	EditorFontFamilies::ScopedCache fontCache;
	m_generalPage = new CSettingsGeneralPage;
	m_generalPage->Create(m_hWnd);
	m_generalPage->MakeAppearanceOnly();
	m_commonPage = new CSettingsCommonPage;
	m_commonPage->Create(m_hWnd);
	stage("general_page");
	m_advancedPage = new CSettingsAdvancedPage;
	m_advancedPage->Create(m_hWnd);
	stage("advanced_page");
	m_editorPage = new CSettingsEditorPage(m_nbspDisplayLocked);
	m_editorPage->Create(m_hWnd);
	stage("editor_page");
	m_spellingPage = new CSettingsSpellingPage(m_legacyDictionaryDirectory);
	m_spellingPage->Create(m_hWnd);
	stage("spelling_page");
	m_imagesPage = new CSettingsImagesPage;
	m_imagesPage->Create(m_hWnd);
	stage("images_page");
	m_sourcePage = new CSettingsSourcePage;
	m_sourcePage->Create(m_hWnd);
	stage("source_page");
	m_hotkeysPage = new CSettingsHotkeysDlg;
	m_hotkeysPage->Create(m_hWnd);
	stage("hotkeys_page");
	m_wordsPage = new CSettingsWordsDlg;
	m_wordsPage->Create(m_hWnd);
	stage("words_page");
	// The pages are created after the dialog itself.  Apply the selected
	// interface palette before any page becomes visible, preventing the light
	// system background from flashing before a later theme refresh.
	const HWND pagesToTheme[] = {
		m_generalPage->m_hWnd, m_commonPage->m_hWnd, m_advancedPage->m_hWnd, m_editorPage->m_hWnd,
		m_spellingPage->m_hWnd, m_imagesPage->m_hWnd, m_sourcePage->m_hWnd,
		m_hotkeysPage->m_hWnd, m_wordsPage->m_hWnd
	};
	for(HWND page : pagesToTheme) {
		// The parent modal dialog must traverse controls inside each child page.
		::SetWindowLongPtrW(page, GWL_EXSTYLE, ::GetWindowLongPtrW(page, GWL_EXSTYLE) | WS_EX_CONTROLPARENT);
		ThemeManager::ApplyToWindow(page);
	}
	stage("page_themes");

	m_pages[PageIndex(SettingsPageId::General)] = m_commonPage;
	m_pages[PageIndex(SettingsPageId::Appearance)] = m_generalPage;
	m_pages[PageIndex(SettingsPageId::Editor)] = m_editorPage;
	m_pages[PageIndex(SettingsPageId::Source)] = m_sourcePage;
	m_pages[PageIndex(SettingsPageId::Images)] = m_imagesPage;
	m_pages[PageIndex(SettingsPageId::Spelling)] = m_spellingPage;
	m_pages[PageIndex(SettingsPageId::Keyboard)] = m_hotkeysPage;
	m_pages[PageIndex(SettingsPageId::Words)] = m_wordsPage;
	m_pages[PageIndex(SettingsPageId::Advanced)] = m_advancedPage;
	m_pageLifecycle[PageIndex(SettingsPageId::General)] = m_commonPage;
	m_pageLifecycle[PageIndex(SettingsPageId::Appearance)] = m_generalPage;
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
	const int navigationHeight = static_cast<int>(SettingsPageId::Count) * m_navigation.GetItemHeight(0) + m_navigationBottomGap;
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
	// Keep the common appearance pages compact by default. Longer pages can
	// scroll, and a user-resized dialog continues to use its saved geometry.
	const int compactHeight = max(m_pageLayouts[PageIndex(SettingsPageId::General)].contentSize.cy,
		m_pageLayouts[PageIndex(SettingsPageId::Appearance)].contentSize.cy);
	preferredPageHeight = max(minimumPageHeight, min(preferredPageHeight, compactHeight));
	m_preferredWindowSize = CSize(nonClientWidth + m_pageLeft + preferredPageWidth + m_pageRightMargin,
		nonClientHeight + m_pageTop + preferredPageHeight + m_pageBottomMargin);
	RestoreGeometry();
	GetClientRect(client);
	LayoutControls(client.Width(), client.Height());
	m_navigation.SetCurSel(0);
	SelectPage(SettingsPageId::General);
	if(RuntimeTests::IsScenario(L"ui-defects-runtime")) SetTimer(0x55494446, 1);
	if(RuntimeTests::IsScenario(L"main-toolbar-size-dialog-runtime")) SetTimer(0x5442535a, 1);
	if (IsSettingsDialogRuntimeTest())
	{
		::SetPropW(::GetParent(m_hWnd), L"FBE_CARET_INITIAL_SYSTEM",
			reinterpret_cast<HANDLE>(_Settings.GetCaretPreferences().mode == TextCaretMode::System ? 1 : 2));
		// Test-only automation still commits through the normal global OK path.
		// Use independent pages so ApplyConfChanges covers more than one category.
		m_commonPage->CheckDlgButton(IDC_CREATE_BACKUP_FILE, IsBackupSettingsSaveRuntimeTest() ? BST_UNCHECKED : BST_CHECKED);
		m_editorPage->SendDlgItemMessage(IDC_NBSP_CHAR, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"\x25AB"));
		m_sourcePage->SendDlgItemMessage(IDC_EDITOR_CARET_MODE, CB_SETCURSEL, 2, 0);
		m_sourcePage->SendDlgItemMessage(IDC_EDITOR_CARET_WIDTH, CB_SETCURSEL, 4, 0);
		m_sourcePage->SendDlgItemMessage(IDC_EDITOR_CARET_COLOR, CB_SETCURSEL, 3, 0);
		m_sourcePage->SendDlgItemMessage(IDC_EDITOR_CARET_BLINK, CB_SETCURSEL, 4, 0);
		m_sourcePage->CheckDlgButton(IDC_WRAP, BST_CHECKED);
		m_spellingPage->CheckDlgButton(IDC_CHECK_RUSSIAN_YO_USAGE, BST_CHECKED);
		m_spellingPage->CheckDlgButton(IDC_USESPELLCHECKER, BST_UNCHECKED);
		m_spellingPage->SetDlgItemText(IDC_CUSTOM_DICT, L"");
		m_advancedPage->SendMessage(WM_COMMAND, MAKEWPARAM(IDC_DEFAULT_SCRIPTS_FOLDER, BN_CLICKED), 0);
		SetTimer(0x53455454, 1);
	}
	// Creating child pages can move focus to a control on a hidden page.
	// Establish the modal dialog's initial focus once, after all pages/layouts
	// are ready; FALSE prevents the dialog manager from assigning it again.
	m_navigation.SetFocus();
	if(RuntimeTests::IsScenario(L"popular-palette-write"))
		::SetPropW(m_hWnd, L"FBE_THEME_TEST_SETTINGS_READY", reinterpret_cast<HANDLE>(1));
	stage("layout_and_focus");
	if (measure)
	{
		wchar_t path[32768] = {};
		const DWORD length = ::GetEnvironmentVariableW(L"FBE_SETTINGS_PERF_STAGE_FILE", path, _countof(path));
		if (length && length < _countof(path))
		{
			const HANDLE file = ::CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (file != INVALID_HANDLE_VALUE) { DWORD written = 0; ::WriteFile(file, static_cast<LPCSTR>(measurements), measurements.GetLength(), &written, NULL); ::CloseHandle(file); }
		}
	}
	return 0;
}

LRESULT CSettingsDlg::OnTimer(UINT, WPARAM timerId, LPARAM, BOOL&)
{
	if(timerId == 0x5442535a && RuntimeTests::IsScenario(L"main-toolbar-size-dialog-runtime")) {
		KillTimer(timerId);
		wchar_t action[16] = {};
		::GetEnvironmentVariableW(L"FBE_NEXT_TEST_TOOLBAR_DIALOG_ACTION", action, _countof(action));
		m_generalPage->SendDlgItemMessage(IDC_MAIN_TOOLBAR_ICON_SIZE, CB_SETCURSEL, wcscmp(action, L"failure") == 0 ? 3 : 0, 0);
		if(wcscmp(action, L"scriptfail") == 0)
			m_generalPage->SendDlgItemMessage(IDC_SCRIPT_ICON_SIZE, CB_SETCURSEL, 3, 0);
		BOOL handled = FALSE;
		return wcscmp(action, L"cancel") == 0 ? OnClickedCancel(0, IDCANCEL, NULL, handled) : OnClickedOK(BN_CLICKED, IDOK, GetDlgItem(IDOK), handled);
	}
	if(timerId == 0x55494446 && RuntimeTests::IsScenario(L"ui-defects-runtime")) {
		KillTimer(timerId);
		wchar_t menuTheme[16]={};
		if(::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MENU_THEME",menuTheme,_countof(menuTheme))) {
			m_generalPage->SendDlgItemMessage(IDC_INTERFACE_THEME,CB_SETCURSEL,_wtoi(menuTheme),0);
			m_generalPage->SendMessage(WM_COMMAND,MAKEWPARAM(IDC_INTERFACE_THEME,CBN_SELCHANGE),reinterpret_cast<LPARAM>(m_generalPage->GetDlgItem(IDC_INTERFACE_THEME).m_hWnd));
			BOOL handled=FALSE;return OnClickedOK(0,IDOK,NULL,handled);
		}
		if(m_uiProbeStage==0) {
			::GetKeyboardState(m_uiProbeKeyboard);
			BYTE keys[256]={};keys[VK_CONTROL]=0x80;::SetKeyboardState(keys);
			SelectPage(SettingsPageId::Keyboard);m_navigation.SetFocus();
		} else {
			const HWND expected=m_uiProbeStage==3?m_hotkeysPage->m_editHotkey.m_hWnd:m_hotkeysPage->m_search.m_hWnd;
			GUITHREADINFO info={};info.cbSize=sizeof(info);RECT client={};::GetClientRect(expected,&client);
			const bool ok=::GetGUIThreadInfo(::GetCurrentThreadId(),&info)&&info.hwndFocus==expected&&
				info.hwndCaret==expected&&info.rcCaret.left>=0&&info.rcCaret.top>=0&&
				info.rcCaret.right>info.rcCaret.left&&info.rcCaret.bottom>info.rcCaret.top&&
				info.rcCaret.right<=client.right&&info.rcCaret.bottom<=client.bottom;
			m_uiModalKeysPassed=ok&&m_uiModalKeysPassed;
			if(m_uiProbeStage==3) {
				::SetKeyboardState(m_uiProbeKeyboard);SelectPage(SettingsPageId::General);m_navigation.SetFocus();
				const bool probes=ProbeUiDefects();
				::SetPropW(GetParent(), L"FBE_UI_DEFECTS_SETTINGS", reinterpret_cast<HANDLE>(probes&&m_uiModalKeysPassed?1:2));
				BOOL handled=FALSE;return OnClickedCancel(0,IDCANCEL,NULL,handled);
			}
			if(m_uiProbeStage==1)m_hotkeysPage->m_hkGroups.SetFocus();
			else m_hotkeysPage->m_editHotkey.SetFocus();
		}
		// Return to DoModal: the queued key must pass through the real modal
		// dialog manager, rather than calling a message filter from the test.
		++m_uiProbeStage;
		::PostMessageW(::GetFocus(),WM_KEYDOWN,'F',1);
		SetTimer(timerId,100);
		return 0;
	}
	if (timerId != 0x53455454 || !IsSettingsDialogRuntimeTest()) return 0;
	KillTimer(timerId);
	if(RuntimeTests::IsScenario(L"settings-residual-runtime"))
	{
		const int result = ProbeResidual();
		::SetPropW(::GetParent(m_hWnd), L"FBE_SETTINGS_RESIDUAL_RESULT", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(result)));
		BOOL handled = FALSE;
		return OnClickedCancel(0, IDCANCEL, NULL, handled);
	}
	if(RuntimeTests::IsScenario(L"settings-dialog-runtime")) {
		wchar_t paintOnly[2] = {};
		if(::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SETTINGS_PAINT_ONLY", paintOnly, _countof(paintOnly)) == 1 && paintOnly[0] == L'1') {
			ProbeLayout(); EndDialog(IDCANCEL); return 0;
		}
	}
	bool behaviorPassed = !RuntimeTests::IsScenario(L"settings-dialog-runtime") || ProbeFollowup();
	if(RuntimeTests::IsScenario(L"settings-dialog-runtime"))
	{
		const auto originalWords = m_wordsPage->m_words;
		const bool dirty = m_wordsPage->m_wordsDirty;
		m_wordsPage->m_words = { WordsItem(L"before-word", 37) };
		m_wordsPage->m_words[0].m_percent = 19;
		m_wordsPage->m_editidx = 0; m_wordsPage->m_editActive = true;
		m_wordsPage->m_edit.SetWindowText(L"after-word");
		behaviorPassed = m_wordsPage->FinishInlineEdit() && m_wordsPage->m_words.size() == 1 &&
			m_wordsPage->m_words[0].m_word == L"after-word" && m_wordsPage->m_words[0].m_count == 37 &&
			m_wordsPage->m_words[0].m_percent == 19;
		const int originalThemeIndex = m_sourcePage->m_source_palette.GetCurSel();
		const int otherThemeIndex = originalThemeIndex == 1 ? 2 : 1;
		DWORD originalColors[XML_SRC_COLOR_GROUP_COUNT] = {};
		for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i) originalColors[i] = m_sourcePage->m_source_colors[i].GetColor();
		m_sourcePage->m_source_colors[XML_SRC_COLOR_COMMENT].SetColor(RGB(12, 34, 56));
		m_sourcePage->m_source_palette.SetCurSel(otherThemeIndex);
		BOOL handled = FALSE; m_sourcePage->OnSourcePaletteChanged(0, 0, NULL, handled);
		m_sourcePage->m_source_palette.SetCurSel(originalThemeIndex);
		m_sourcePage->OnSourcePaletteChanged(0, 0, NULL, handled);
		behaviorPassed = behaviorPassed && m_sourcePage->m_source_colors[XML_SRC_COLOR_COMMENT].GetColor() == RGB(12, 34, 56);
		for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
		{
			m_sourcePage->m_source_colors[i].SetColor(originalColors[i]);
			m_sourcePage->m_source_color_custom[i] = originalColors[i] != CLR_DEFAULT;
		}
		m_sourcePage->m_themeDrafts.clear();
		m_wordsPage->m_words = originalWords; m_wordsPage->m_wordsDirty = dirty;
		m_wordsPage->m_list_words.SetItemCount(static_cast<int>(originalWords.size()));
	}
	if(RuntimeTests::IsScenario(L"settings-dialog-runtime") && (!behaviorPassed || !ProbeLayout()))
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
	CString error;
	if(!TryCommit(error))
	{
		CString message = FbeLoadRuntimeStringByKey(L"fbe.settings.save_failed", L"Could not save settings files. Check file access and free disk space.");
		if(!error.IsEmpty()) message += L"\n\n" + error;
		MessageBox(message, FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONERROR);
		return 0;
	}
	EndDialog(IDOK);
	return 0;
}

bool CSettingsDlg::TryCommit(CString& error)
{
	error.Empty();
	SettingsFileTransaction files;
	for(int i = 0; i < _countof(m_pageLifecycle); ++i)
		if(m_pageLifecycle[i] && !m_pageLifecycle[i]->PrepareCommit(files, error)) { SelectPage(static_cast<SettingsPageId>(i)); return false; }
	SettingsEditSession edit(_Settings);
	for(int i = 0; i < _countof(m_pageLifecycle); ++i)
		if(m_pageLifecycle[i]) m_pageLifecycle[i]->Commit();
	SaveGeometry();
	if(edit.HasChanges() && !FbeSettings::Store::PrepareSave(_Settings, files, error)) return false;
	if(!files.Commit())
	{
		error = U::Win32ErrMsg(files.Error());
		if(files.RollbackFailed()) error += L"\n" + FbeLoadRuntimeStringByKey(L"fbe.settings.rollback_failed", L"Some files could not be restored. Their recovery copies have been retained beside the original files.");
		return false;
	}
	for(int i = 0; i < _countof(m_pageLifecycle); ++i)
		if(m_pageLifecycle[i]) m_pageLifecycle[i]->FinishCommit();
	edit.Accept();
	return true;
}

LRESULT CSettingsDlg::OnClose(UINT, WPARAM, LPARAM, BOOL&)
{
	BOOL handled = FALSE;
	return OnClickedCancel(0, IDCANCEL, NULL, handled);
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
	for(int index = 0; index < PageIndex(SettingsPageId::Count); ++index)
		if(m_pages[index] && ::IsWindow(m_pages[index]->m_hWnd))
			::RemoveWindowSubclass(m_pages[index]->m_hWnd, PageSubclassProc, static_cast<UINT_PTR>(index));
	if(m_advancedPage) { m_advancedPage->DestroyWindow(); delete m_advancedPage; m_advancedPage = NULL; }
	if(m_generalPage) { m_generalPage->DestroyWindow(); delete m_generalPage; m_generalPage = NULL; }
	if(m_commonPage) { m_commonPage->DestroyWindow(); delete m_commonPage; m_commonPage = NULL; }
	if(m_editorPage) { m_editorPage->DestroyWindow(); delete m_editorPage; m_editorPage = NULL; }
	if(m_spellingPage) { m_spellingPage->DestroyWindow(); delete m_spellingPage; m_spellingPage = NULL; }
	if(m_imagesPage) { m_imagesPage->DestroyWindow(); delete m_imagesPage; m_imagesPage = NULL; }
	if(m_sourcePage) { m_sourcePage->DestroyWindow(); delete m_sourcePage; m_sourcePage = NULL; }
	if(m_hotkeysPage) { m_hotkeysPage->DestroyWindow(); delete m_hotkeysPage; m_hotkeysPage = NULL; }
	if(m_wordsPage) { m_wordsPage->DestroyWindow(); delete m_wordsPage; m_wordsPage = NULL; }
	return 0;
}

LRESULT CSettingsDlg::OnSourcePreviewLayout(UINT, WPARAM, LPARAM, BOOL&)
{
	if(m_sourcePage && m_currentPage == SettingsPageId::Source &&
		!m_pageLayouts[PageIndex(SettingsPageId::Source)].controls.empty())
		LayoutPage(PageIndex(SettingsPageId::Source));
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
			// The palette editor controls remain owned by its single model but are
			// hidden here and exposed through the separate management dialog.
			if(index == PageIndex(SettingsPageId::Appearance) &&
				(::GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) == 0) continue;
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
	const int displayLines = m_sourcePage->PreviewDisplayLineCount();
	layout.contentSize.cy = max(layout.templateSize.cy, layout.naturalContentHeight);
	if(lineHeight <= 0 || displayLines <= 0) return;
	for(const PageControlLayout& control : layout.controls)
		if(control.id == IDC_OPTIONS_SOURCE_PREVIEW)
		{
			// Reserve four visible display rows. Wider windows can use their spare
			// height for all wrapped rows without growing the startup dialog.
			const int chrome = m_sourcePage->PreviewVerticalChromeHeight();
			const int fullHeight = displayLines * lineHeight + chrome;
			const int minimum = min(fullHeight, 4 * lineHeight + chrome);
			layout.contentSize.cy += max(0, minimum - control.rect.Height());
			break;
		}
}

void CSettingsDlg::LayoutPage(int index)
{
	if(index != PageIndex(SettingsPageId::Source))
	{
		if(LayoutPagePass(index))
			::RedrawWindow(m_pages[index]->m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
		return;
	}
	bool moved = false;
	for(int pass = 0; pass < 3; ++pass)
	{
		UpdateSourcePreviewContentHeight();
		const int rowsBefore = m_sourcePage->PreviewDisplayLineCount();
		const int chromeBefore = m_sourcePage->PreviewVerticalChromeHeight();
		moved = LayoutPagePass(index) || moved;
		if(rowsBefore == m_sourcePage->PreviewDisplayLineCount() &&
			chromeBefore == m_sourcePage->PreviewVerticalChromeHeight()) break;
	}
	if(moved)
		::RedrawWindow(m_pages[index]->m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

bool CSettingsDlg::LayoutPagePass(int index)
{
	const HWND page = m_pages[index] ? m_pages[index]->m_hWnd : NULL;
	if(!page || !::IsWindow(page)) return false;
	PageLayout& layout = m_pageLayouts[index];
	if(layout.contentSize.cx <= 0 || layout.contentSize.cy <= 0) return false;
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
				control.id == IDC_GENRE_CATALOG || control.id == IDC_DEFAULT_ENC || control.id == IDC_UPDATE_CHANNEL)) ||
			(index == PageIndex(SettingsPageId::Appearance) && (control.id == IDC_INTERFACE_THEME ||
				control.id == IDC_INTERFACE_PALETTE || control.id == IDC_MAIN_TOOLBAR_ICON_SIZE ||
				control.id == IDC_SCRIPT_ICON_SIZE || control.id == IDC_DESCRIPTION_LAYOUT)) ||
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
			(index == PageIndex(SettingsPageId::Spelling) && control.id == IDC_DICTPATH) ||
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
	if(moves.empty()) { PublishLayoutAuditMetrics(index); return false; }
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
	PublishLayoutAuditMetrics(index);
	if(index == PageIndex(SettingsPageId::Words) && m_wordsPage)
		m_wordsPage->ResizeColumns();
	if(index == PageIndex(SettingsPageId::Keyboard) && m_hotkeysPage)
		m_hotkeysPage->RefreshInfoScrollbars();
	return true;
}

void CSettingsDlg::PublishLayoutAuditMetrics(int index)
{
	wchar_t enabled[4] = {};
	if(::GetEnvironmentVariableW(L"FBE_NEXT_SETTINGS_LAYOUT_AUDIT", enabled, _countof(enabled)) != 1 || enabled[0] != L'1') return;
	for(const auto& control : m_pageLayouts[index].controls)
	{
		wchar_t kind[32] = {}; ::GetClassNameW(control.window, kind, _countof(kind));
		const bool button = _wcsicmp(kind, L"Button") == 0, label = _wcsicmp(kind, L"Static") == 0;
		if(!button && !label) continue;
		const DWORD style = static_cast<DWORD>(::GetWindowLongPtrW(control.window, GWL_STYLE));
		if((button && (style & BS_TYPEMASK) == BS_GROUPBOX) || (label && ((style & SS_TYPEMASK) == SS_ICON || (style & SS_TYPEMASK) == SS_BITMAP))) continue;
		const CString text = U::GetWindowText(control.window);
		if(text.IsEmpty()) continue;
		RECT bounds = {}; ::GetClientRect(control.window, &bounds);
		const UINT dpi = UiMetrics::DpiForWindow(control.window);
		const int inset = max(2, static_cast<int>(dpi / 48));
		int width = max(1, static_cast<int>(bounds.right) - 2 * inset);
		if(button && ((style & BS_TYPEMASK) == BS_CHECKBOX || (style & BS_TYPEMASK) == BS_AUTOCHECKBOX)) width = max(1, width - static_cast<int>(dpi / 5));
		HDC dc = ::GetDC(control.window); if(!dc) continue;
		HFONT font = reinterpret_cast<HFONT>(::SendMessageW(control.window, WM_GETFONT, 0, 0));
		HGDIOBJ previous = font ? ::SelectObject(dc, font) : NULL;
		RECT measured = { 0, 0, width, 0 };
		UINT flags = DT_CALCRECT | ((label || (style & BS_MULTILINE)) ? DT_WORDBREAK : DT_SINGLELINE);
		if(label && (style & SS_NOPREFIX)) flags |= DT_NOPREFIX;
		::DrawTextW(dc, text, text.GetLength(), &measured, flags);
		if(previous) ::SelectObject(dc, previous);
		::ReleaseDC(control.window, dc);
		::SetPropW(control.window, L"FBE_AUDIT_TEXT_WIDTH", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(measured.right + 1)));
		::SetPropW(control.window, L"FBE_AUDIT_TEXT_HEIGHT", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(measured.bottom + 1)));
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

LRESULT CALLBACK CSettingsDlg::NavigationKeysSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR subclassId, DWORD_PTR reference)
{
	auto* dialog=reinterpret_cast<CSettingsDlg*>(reference);
	if(message==WM_NCDESTROY) ::RemoveWindowSubclass(window,NavigationKeysSubclassProc,subclassId);
	// DoModal runs the dialog manager, not the outer CMessageLoop filters.
	// Keep this local to navigation; the hotkey recorder retains its chords.
	if(dialog->m_currentPage==SettingsPageId::Keyboard&&wParam=='F'&&(::GetKeyState(VK_CONTROL)&0x8000)) {
		if(message==WM_GETDLGCODE) return ::DefSubclassProc(window,message,wParam,lParam)|DLGC_WANTMESSAGE;
		if(message==WM_KEYDOWN) {
			MSG key={};key.hwnd=window;key.message=message;key.wParam=wParam;key.lParam=lParam;
			if(dialog->m_hotkeysPage->PreTranslateMessage(&key))return 0;
		}
	}
	return ::DefSubclassProc(window,message,wParam,lParam);
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
	bool passed = m_navigation.GetCount() == static_cast<int>(SettingsPageId::Count) &&
		::IsWindow(m_commonPage->GetDlgItem(IDC_LANG)) &&
		::IsWindow(m_generalPage->GetDlgItem(IDC_INTERFACE_PALETTE)) &&
		!::IsWindow(m_commonPage->GetDlgItem(IDC_INTERFACE_PALETTE)) &&
		!::IsWindow(m_generalPage->GetDlgItem(IDC_LANG));
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
	int paintAreas = 0, paintFailures = 0, repaintPages = 0, repaintFailures = 0, hoverPages = 0, hoverFailures = 0;
	auto probePageRepaint = [&](HWND page) {
		RECT dialogBounds = {}, pageBounds = {}, visible = {};
		::GetWindowRect(m_hWnd, &dialogBounds); ::GetWindowRect(page, &pageBounds);
		if(!::IntersectRect(&visible, &dialogBounds, &pageBounds)) { ++repaintFailures; return; }
		const int width = dialogBounds.right - dialogBounds.left, height = dialogBounds.bottom - dialogBounds.top;
		CImage before, after;
		if(!before.Create(width, height, 32) || !after.Create(width, height, 32)) { ++repaintFailures; return; }
		::UpdateWindow(m_hWnd); ::UpdateWindow(page);
		HDC first = before.GetDC(); const bool capturedBefore = first && ::PrintWindow(m_hWnd, first, 0);
		if(first) before.ReleaseDC();
		::RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
		HDC second = after.GetDC(); const bool capturedAfter = second && ::PrintWindow(m_hWnd, second, 0);
		if(second) after.ReleaseDC();
		bool same = capturedBefore && capturedAfter;
		for(int y = visible.top - dialogBounds.top; y < visible.bottom - dialogBounds.top && same; ++y) {
			const BYTE* a = static_cast<const BYTE*>(before.GetPixelAddress(0, y));
			const BYTE* b = static_cast<const BYTE*>(after.GetPixelAddress(0, y));
			for(int x = visible.left - dialogBounds.left; x < visible.right - dialogBounds.left && same; ++x)
				for(int channel = 0; channel < 3; ++channel) if(a[4*x+channel] != b[4*x+channel]) { same = false; break; }
		}
		++repaintPages; if(!same) ++repaintFailures;
	};
	auto probePageHover = [&](HWND page) {
		RECT dialogBounds = {}, pageBounds = {}, visible = {}, targetBounds = {};
		::GetWindowRect(m_hWnd, &dialogBounds); ::GetWindowRect(page, &pageBounds);
		if(!::IntersectRect(&visible, &dialogBounds, &pageBounds)) { ++hoverFailures; return; }
		HWND target = NULL;
		for(HWND child = ::GetWindow(page, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT)) {
			wchar_t kind[32] = {}; ::GetClassNameW(child, kind, _countof(kind));
			const LONG_PTR style = ::GetWindowLongPtrW(child, GWL_STYLE);
			if(!(style & WS_VISIBLE) || !::IsWindowEnabled(child) ||
				(_wcsicmp(kind, L"Button") && _wcsicmp(kind, L"ComboBox")) ||
				(!_wcsicmp(kind, L"Button") && (style & BS_TYPEMASK) == BS_GROUPBOX)) continue;
			RECT bounds = {}, intersection = {}; ::GetWindowRect(child, &bounds);
			if(::IntersectRect(&intersection, &bounds, &visible) && ::EqualRect(&intersection, &bounds) &&
				bounds.right - bounds.left > 8 && bounds.bottom - bounds.top > 8) { target = child; targetBounds = bounds; break; }
		}
		POINT savedCursor = {};
		if(!target || !::GetCursorPos(&savedCursor)) { ++hoverFailures; return; }
		const int width = dialogBounds.right - dialogBounds.left, height = dialogBounds.bottom - dialogBounds.top;
		CImage before, after;
		if(!before.Create(width, height, 32) || !after.Create(width, height, 32)) { ++hoverFailures; return; }
		::SetCursorPos(dialogBounds.left + 5, dialogBounds.top + 5);
		::SendMessageW(target, WM_MOUSELEAVE, 0, 0);
		::RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
		HDC first = before.GetDC(); const bool capturedBefore = first && ::PrintWindow(m_hWnd, first, 0); if(first) before.ReleaseDC();
		const POINT hover = {(targetBounds.left + targetBounds.right) / 2, (targetBounds.top + targetBounds.bottom) / 2};
		const bool moved = ::SetCursorPos(hover.x, hover.y) != FALSE;
		::SendMessageW(target, WM_MOUSEMOVE, 0, MAKELPARAM(hover.x - targetBounds.left, hover.y - targetBounds.top));
		::RedrawWindow(target, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
		HDC second = after.GetDC(); const bool capturedAfter = second && ::PrintWindow(m_hWnd, second, 0); if(second) after.ReleaseDC();
		::SetCursorPos(savedCursor.x, savedCursor.y); ::SendMessageW(target, WM_MOUSELEAVE, 0, 0);
		bool same = capturedBefore && capturedAfter && moved;
		for(int y = visible.top; y < visible.bottom && same; ++y) {
			const BYTE* a = static_cast<const BYTE*>(before.GetPixelAddress(0, y - dialogBounds.top));
			const BYTE* b = static_cast<const BYTE*>(after.GetPixelAddress(0, y - dialogBounds.top));
			for(int x = visible.left; x < visible.right && same; ++x) {
				if(x >= targetBounds.left && x < targetBounds.right && y >= targetBounds.top && y < targetBounds.bottom) continue;
				const int offset = 4 * (x - dialogBounds.left);
				for(int channel = 0; channel < 3; ++channel) if(a[offset+channel] != b[offset+channel]) { same = false; break; }
			}
		}
		++hoverPages; if(!same) ++hoverFailures;
	};
	auto probePageFrames = [&](HWND page) {
		::RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
		RECT dialogBounds = {}, pageBounds = {}; ::GetWindowRect(m_hWnd, &dialogBounds); ::GetWindowRect(page, &pageBounds);
		CImage dialog; if(!dialog.Create(dialogBounds.right - dialogBounds.left, dialogBounds.bottom - dialogBounds.top, 32)) { ++paintFailures; return; }
		HDC source = dialog.GetDC(); if(!source || !::PrintWindow(m_hWnd, source, 0)) { if(source) dialog.ReleaseDC(); ++paintFailures; return; }
		for(HWND frame = ::GetWindow(page, GW_CHILD); frame; frame = ::GetWindow(frame, GW_HWNDNEXT)) {
			wchar_t kind[32] = {}; ::GetClassNameW(frame, kind, _countof(kind));
			if(_wcsicmp(kind, L"Button") || !(::GetWindowLongPtrW(frame, GWL_STYLE) & WS_VISIBLE) ||
				(::GetWindowLongPtrW(frame, GWL_STYLE) & BS_TYPEMASK) != BS_GROUPBOX) continue;
			RECT frameBounds = {}; ::GetWindowRect(frame, &frameBounds);
			for(HWND field = ::GetWindow(page, GW_CHILD); field; field = ::GetWindow(field, GW_HWNDNEXT)) {
				if(field == frame || !(::GetWindowLongPtrW(field, GWL_STYLE) & WS_VISIBLE)) continue;
				wchar_t fieldKind[32] = {}; ::GetClassNameW(field, fieldKind, _countof(fieldKind));
				if(!_wcsicmp(fieldKind, L"Button") && (::GetWindowLongPtrW(field, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX) continue;
				RECT bounds = {}, overlap = {}, visible = {}; ::GetWindowRect(field, &bounds);
				if(!::IntersectRect(&overlap, &frameBounds, &bounds) || !::IntersectRect(&visible, &overlap, &pageBounds) ||
					!::IntersectRect(&visible, &visible, &dialogBounds) || visible.right <= visible.left || visible.bottom <= visible.top) continue;
				const int width = visible.right - visible.left, height = visible.bottom - visible.top;
				CImage image; if(!image.Create(width, height, 32)) { ++paintFailures; continue; }
				HDC target = image.GetDC(); if(!target) { ++paintFailures; continue; }
				if(!::BitBlt(target, 0, 0, width, height, source, visible.left - dialogBounds.left, visible.top - dialogBounds.top, SRCCOPY)) { image.ReleaseDC(); ++paintFailures; continue; }
				image.ReleaseDC(); std::vector<BYTE> before;
				for(int y=0; y<height; ++y) { const BYTE* row=static_cast<const BYTE*>(image.GetPixelAddress(0,y)); for(int x=0; x<width; ++x) before.insert(before.end(),row+4*x,row+4*x+3); }
				target = image.GetDC(); const int saved = ::SaveDC(target);
				::SetViewportOrgEx(target, frameBounds.left - visible.left, frameBounds.top - visible.top, NULL);
				::SendMessageW(frame, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(target), PRF_CLIENT);
				if(saved) ::RestoreDC(target, saved); image.ReleaseDC();
				bool same = true; size_t offset = 0;
				for(int y=0; y<height && same; ++y) { const BYTE* row=static_cast<const BYTE*>(image.GetPixelAddress(0,y)); for(int x=0; x<width && same; ++x) for(int c=0; c<3; ++c) if(before[offset++] != row[4*x+c]) { same=false; break; } }
				++paintAreas; if(!same) ++paintFailures;
			}
		}
		dialog.ReleaseDC();
	};
	// Switching among all pages must not resize the outer dialog. On a
	// monitor that can hold the preferred size, its initial pages need no bars.
	for(int index = 0; index < PageIndex(SettingsPageId::Count); ++index)
	{
		SelectPage(static_cast<SettingsPageId>(index));
		const HWND page = m_pages[index]->m_hWnd;
		const CRect pageRect = controlRect(page);
		passed = passed && inside(pageRect) && navigation.right <= pageRect.left && pageRect.bottom <= ok.top &&
			controlRect(m_navigation).bottom == pageRect.bottom;
		wchar_t paintProbe[2] = {};
		const bool paintOnly = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SETTINGS_PAINT_ONLY", paintProbe, _countof(paintProbe)) == 1 && paintProbe[0] == L'1';
		if(ThemeManager::UsesPaletteSurface() && paintOnly) { probePageRepaint(page); probePageFrames(page); probePageHover(page); }
		if(index == PageIndex(SettingsPageId::Images) && ThemeManager::UsesPaletteSurface() && paintOnly) {
			const int previousNavigation = m_navigation.GetCurSel();
			m_navigation.SetCurSel(index);
			::RedrawWindow(m_navigation, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
			// Compare the real checkbox pixels before and after repainting its
			// sibling group frame. Repainting the frame must not erase its label.
			HWND field = ::GetDlgItem(page, IDC_OPTIONS_CLEARIMGS);
			HWND frame = ::GetDlgItem(page, IDC_SETTINGS_OTHER_DIALOGS);
			wchar_t caption[256] = {}; ::GetWindowTextW(field, caption, _countof(caption));
			const CString expectedCaption = FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_other.clear_images", L"Insert empty images");
			RECT fieldClient = {}; ::GetClientRect(field, &fieldClient);
			HDC fieldDc = ::GetDC(field); SIZE textSize = {};
			HFONT fieldFont = reinterpret_cast<HFONT>(::SendMessageW(field, WM_GETFONT, 0, 0));
			HGDIOBJ oldFieldFont = fieldDc && fieldFont ? ::SelectObject(fieldDc, fieldFont) : NULL;
			const bool measured = fieldDc && ::GetTextExtentPoint32W(fieldDc, caption, ::lstrlenW(caption), &textSize);
			if(oldFieldFont) ::SelectObject(fieldDc, oldFieldFont); if(fieldDc) ::ReleaseDC(field, fieldDc);
			const bool captionValid = expectedCaption == caption && measured &&
				textSize.cx + UiMetrics::ScaleForDpi(20, m_layoutDpi) <= fieldClient.right && textSize.cy <= fieldClient.bottom &&
				(::GetWindowLongPtrW(field, GWL_STYLE) & BS_TYPEMASK) == BS_AUTOCHECKBOX;
			::SetPropW(GetParent(), L"FBE_SETTINGS_CHECKBOX_CAPTION_PASSED", reinterpret_cast<HANDLE>(captionValid ? 1 : 2));
			auto togglePixel = [&](HWND control) {
				RECT bounds={}; ::GetClientRect(control,&bounds); CImage image;
				if(bounds.right<10 || bounds.bottom<10 || !image.Create(bounds.right,bounds.bottom,32)) return static_cast<COLORREF>(CLR_INVALID);
				HDC dc=image.GetDC(); COLORREF pixel=CLR_INVALID;
				if(dc) { ::SendMessageW(control,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
					const int size=(std::min)(static_cast<int>(bounds.bottom),UiMetrics::ScaleForDpi(13,UiMetrics::DpiForWindow(control)));
					pixel=::GetPixel(dc,UiMetrics::ScaleForDpi(2,UiMetrics::DpiForWindow(control)),(bounds.bottom-size)/2+UiMetrics::ScaleForDpi(2,UiMetrics::DpiForWindow(control))); image.ReleaseDC(); }
				return pixel;
			};
			auto toggleMarkVisible = [&](HWND control) {
				RECT bounds={}; ::GetClientRect(control,&bounds); CImage image; bool found=false;
				if(bounds.right>0 && bounds.bottom>0 && image.Create(bounds.right,bounds.bottom,32)) {
					HDC dc=image.GetDC(); if(dc) {
						::SendMessageW(control,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
						const int limit=(std::min)(static_cast<int>(bounds.right),UiMetrics::ScaleForDpi(13,UiMetrics::DpiForWindow(control)));
						for(int y=0;y<bounds.bottom&&!found;++y) for(int x=0;x<limit;++x) if(::GetPixel(dc,x,y)==ThemeManager::TextColor()) { found=true; break; }
						image.ReleaseDC();
					}
				}
				return found;
			};
			const BOOL initialEnabled=::IsWindowEnabled(field); const LRESULT initialCheck=::SendMessageW(field,BM_GETCHECK,0,0);
			::EnableWindow(field,TRUE); ::SendMessageW(field,BM_SETCHECK,BST_UNCHECKED,0); ::SendMessageW(field,WM_MOUSELEAVE,0,0);
			unsigned toggleStates=togglePixel(field)==ThemeManager::ControlColor()?1U:0U;
			::SendMessageW(field,WM_MOUSEMOVE,0,MAKELPARAM(1,1)); if(togglePixel(field)==ThemeManager::HoverColor()) toggleStates|=2U;
			::SendMessageW(field,BM_SETSTATE,TRUE,0); if(togglePixel(field)==ThemeManager::PressedColor()) toggleStates|=4U;
			::SendMessageW(field,BM_SETSTATE,FALSE,0); ::SendMessageW(field,WM_MOUSELEAVE,0,0); ::EnableWindow(field,FALSE);
			if(togglePixel(field)==ThemeManager::ControlColor()) toggleStates|=8U;
			::EnableWindow(field,TRUE); ::SendMessageW(field,BM_SETCHECK,BST_CHECKED,0);
			if(::SendMessageW(field,BM_GETCHECK,0,0)==BST_CHECKED) toggleStates|=16U;
			if(toggleMarkVisible(field)) toggleStates|=128U;
			::SendMessageW(field,BM_SETCHECK,initialCheck,0); ::EnableWindow(field,initialEnabled);
			HWND radio=::CreateWindowExW(0,L"Button",L"Probe",WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON,-200,-200,120,24,page,NULL,_Module.GetModuleInstance(),NULL);
			if(radio) {
				ThemeManager::ApplyToWindow(radio); if(togglePixel(radio)==ThemeManager::ControlColor()) toggleStates|=32U;
				::SendMessageW(radio,BM_CLICK,0,0); if(::SendMessageW(radio,BM_GETCHECK,0,0)==BST_CHECKED) toggleStates|=64U;
				if(toggleMarkVisible(radio)) toggleStates|=256U;
				::DestroyWindow(radio);
			}
			::SetPropW(GetParent(),L"FBE_SETTINGS_TOGGLE_PAINT",reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(toggleStates)));
			passed = passed && toggleStates==511U;
			HWND spinBuddy=::CreateWindowExW(0,L"Edit",L"50",WS_CHILD|ES_NUMBER,-200,-200,60,24,page,NULL,_Module.GetModuleInstance(),NULL);
			HWND spin=::CreateWindowExW(0,UPDOWN_CLASSW,L"",WS_CHILD|WS_VISIBLE|UDS_ARROWKEYS|UDS_SETBUDDYINT,0,0,20,30,page,NULL,_Module.GetModuleInstance(),NULL);
			unsigned spinStates=0;
			if(spin && spinBuddy) {
				ThemeManager::ApplyToWindow(spin); ::SendMessageW(spin,UDM_SETBUDDY,reinterpret_cast<WPARAM>(spinBuddy),0); ::SendMessageW(spin,UDM_SETRANGE32,0,100); ::SendMessageW(spin,UDM_SETPOS32,0,50);
				auto spinPixel=[&](int y) { CImage image; if(!image.Create(20,30,32)) return static_cast<COLORREF>(CLR_INVALID); HDC dc=image.GetDC(); COLORREF color=CLR_INVALID;
					if(dc) { ::SendMessageW(spin,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT); color=::GetPixel(dc,1,y); image.ReleaseDC(); } return color; };
				if(spinPixel(1)==ThemeManager::ControlColor() && spinPixel(16)==ThemeManager::ControlColor()) spinStates|=1U;
				::SendMessageW(spin,WM_MOUSEMOVE,0,MAKELPARAM(2,2));
				if(spinPixel(1)==ThemeManager::HoverColor() && spinPixel(16)==ThemeManager::ControlColor()) spinStates|=2U;
				POINT originalCursor={}, clickPoint={2,2}; const bool savedCursor=::GetCursorPos(&originalCursor)!=FALSE;
				const bool movedCursor=savedCursor && ::ClientToScreen(spin,&clickPoint) && ::SetCursorPos(clickPoint.x,clickPoint.y);
				::SendMessageW(spin,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(2,2));
				if(spinPixel(1)==ThemeManager::PressedColor()) spinStates|=4U;
				::SendMessageW(spin,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(2,17));
				if(spinPixel(1)==ThemeManager::ControlColor() && spinPixel(16)==ThemeManager::HoverColor()) spinStates|=32U;
				::SendMessageW(spin,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(2,2));
				if(spinPixel(1)==ThemeManager::PressedColor()) spinStates|=64U;
				::SendMessageW(spin,WM_LBUTTONUP,0,MAKELPARAM(2,2));
				if(movedCursor) ::SetCursorPos(originalCursor.x,originalCursor.y);
				BOOL error=FALSE; if(::SendMessageW(spin,UDM_GETPOS32,0,reinterpret_cast<LPARAM>(&error))!=50 && !error) spinStates|=8U;
				::EnableWindow(spin,FALSE); if(spinPixel(1)==ThemeManager::ControlColor()) spinStates|=16U;
			}
			if(spin) ::DestroyWindow(spin); if(spinBuddy) ::DestroyWindow(spinBuddy);
			::SetPropW(GetParent(),L"FBE_SETTINGS_SPIN_PAINT",reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(spinStates)));
			passed = passed && spinStates==127U;
			::RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
			RECT rect = {}; ::GetWindowRect(field, &rect);
			::RedrawWindow(field, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
			RECT dialogRect = {}; ::GetWindowRect(m_hWnd, &dialogRect);
			const int width = rect.right - rect.left, height = rect.bottom - rect.top;
			CImage dialog;
			const bool dialogCreated = dialog.Create(dialogRect.right - dialogRect.left, dialogRect.bottom - dialogRect.top, 32) != FALSE;
			HDC dialogDc = dialogCreated ? dialog.GetDC() : NULL;
			const bool captured = dialogDc && ::PrintWindow(m_hWnd, dialogDc, 0) != FALSE;
			auto saveDialogScreenshot = [&](const wchar_t* filename) {
				wchar_t artifactDir[32768] = {};
				const DWORD length = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_APPEARANCE_ARTIFACT_DIR", artifactDir, _countof(artifactDir));
				if(!length || length >= _countof(artifactDir)) return;
				CImage snapshot;
				if(!snapshot.Create(dialogRect.right - dialogRect.left, dialogRect.bottom - dialogRect.top, 32)) return;
				HDC dc = snapshot.GetDC(); const bool saved = dc && ::PrintWindow(m_hWnd, dc, 0);
				if(dc) snapshot.ReleaseDC();
				if(saved) snapshot.Save(CString(artifactDir) + filename);
			};
			auto capture = [&](const wchar_t* filename, bool repaintFrame) {
				std::vector<BYTE> pixels; CImage image;
				if(captured && width > 0 && height > 0 && image.Create(width, height, 32)) {
					HDC target = image.GetDC();
					const bool copied = target && ::BitBlt(target, 0, 0, width, height, dialogDc, rect.left - dialogRect.left, rect.top - dialogRect.top, SRCCOPY);
					if(copied && repaintFrame) {
						RECT frameRect = {}; ::GetWindowRect(frame, &frameRect);
						const int saved = ::SaveDC(target);
						::SetViewportOrgEx(target, frameRect.left - rect.left, frameRect.top - rect.top, NULL);
						::SendMessageW(frame, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(target), PRF_CLIENT);
						if(saved) ::RestoreDC(target, saved);
					}
					if(target) image.ReleaseDC();
					if(copied) {
						wchar_t artifactDir[32768] = {};
						const DWORD length = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_APPEARANCE_ARTIFACT_DIR", artifactDir, _countof(artifactDir));
						if(length && length < _countof(artifactDir)) image.Save(CString(artifactDir) + filename);
						for(int y = 0; y < height; ++y) {
							const BYTE* row = static_cast<const BYTE*>(image.GetPixelAddress(0, y));
							for(int x = 0; x < width; ++x) pixels.insert(pixels.end(), row + 4 * x, row + 4 * x + 3);
						}
					}
				}
				return pixels;
			};
			const auto before = capture(L"\\checkbox-before.bmp", false);
			saveDialogScreenshot(L"\\images-page-before.bmp");
			const auto after = capture(L"\\checkbox-after.bmp", true);
			const int textStart = UiMetrics::ScaleForDpi(20, m_layoutDpi);
			POINT savedCursor = {}; const bool cursorSaved = ::GetCursorPos(&savedCursor) != FALSE;
			const bool cursorMoved = cursorSaved && ::SetCursorPos((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
			if(cursorMoved) {
				::SendMessageW(field, WM_MOUSEMOVE, 0, MAKELPARAM(width / 2, height / 2));
				::RedrawWindow(field, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
				if(dialogDc) ::PrintWindow(m_hWnd, dialogDc, 0);
			}
			const auto hover = cursorMoved ? capture(L"\\checkbox-hover.bmp", false) : std::vector<BYTE>();
			if(cursorMoved) saveDialogScreenshot(L"\\images-page-hover.bmp");
			if(cursorMoved) { ::SetCursorPos(savedCursor.x, savedCursor.y); ::SendMessageW(field, WM_MOUSELEAVE, 0, 0); }
			m_navigation.SetCurSel(previousNavigation);
			bool hoverTextStable = !before.empty() && hover.size() == before.size();
			if(hoverTextStable) for(int y = 0; y < height; ++y) for(int x = textStart; x < width; ++x) {
				const size_t offset = static_cast<size_t>((y * width + x) * 3);
				if(before[offset] != hover[offset] || before[offset + 1] != hover[offset + 1] || before[offset + 2] != hover[offset + 2]) hoverTextStable = false;
			}
			::SetPropW(GetParent(), L"FBE_SETTINGS_HOVER_TEXT_PASSED", reinterpret_cast<HANDLE>(hoverTextStable ? 1 : 2));
			const BOOL wasEnabled = ::IsWindowEnabled(field);
			const LRESULT originalCheck = ::SendMessageW(field, BM_GETCHECK, 0, 0);
			::EnableWindow(field, FALSE); ::SendMessageW(field, BM_SETCHECK, BST_UNCHECKED, 0);
			::RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
			if(dialogDc) ::PrintWindow(m_hWnd, dialogDc, 0);
			const auto disabled = capture(L"\\checkbox-disabled.bmp", false);
			::SendMessageW(field, BM_SETCHECK, BST_CHECKED, 0);
			::RedrawWindow(field, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
			if(dialogDc) ::PrintWindow(m_hWnd, dialogDc, 0);
			const auto disabledChecked = capture(L"\\checkbox-disabled-checked.bmp", false);
			::SendMessageW(field, BM_SETCHECK, originalCheck, 0); ::EnableWindow(field, wasEnabled);
			if(dialogDc) dialog.ReleaseDC();
			const COLORREF disabledColor = ThemeManager::DisabledTextColor();
			int textPixels = 0, highlightPixels = 0;
			for(int y = 0; y < height; ++y) for(int x = textStart; x < width; ++x) {
				const size_t offset = static_cast<size_t>((y * width + x) * 3);
				if(offset + 2 >= disabled.size()) continue;
				const BYTE b = disabled[offset], g = disabled[offset + 1], r = disabled[offset + 2];
				if(r == GetRValue(disabledColor) && g == GetGValue(disabledColor) && b == GetBValue(disabledColor)) ++textPixels;
				// White cannot reveal a white embossed shadow when it is also the
				// actual surface colour. The contrasting custom-palette fixtures
				// exercise that shadow check independently.
				if(ThemeManager::WindowColor() != RGB(255,255,255) && r == 255 && g == 255 && b == 255) ++highlightPixels;
			}
			bool boxChanged = false, textUnchanged = disabled.size() == disabledChecked.size();
			if(textUnchanged) for(int y = 0; y < height; ++y) for(int x = 0; x < width; ++x) {
				const size_t offset = static_cast<size_t>((y * width + x) * 3);
				const bool different = disabled[offset] != disabledChecked[offset] || disabled[offset + 1] != disabledChecked[offset + 1] || disabled[offset + 2] != disabledChecked[offset + 2];
				if(x < textStart) boxChanged = boxChanged || different; else if(different) textUnchanged = false;
			}
			const bool disabledPassed = textPixels > 0 && highlightPixels == 0 && boxChanged && textUnchanged;
			::SetPropW(GetParent(), L"FBE_SETTINGS_DISABLED_TEXT_PASSED", reinterpret_cast<HANDLE>(disabledPassed ? 1 : 2));
			const bool paintPassed = !before.empty() && before == after && disabledPassed && captionValid && hoverTextStable;
			::SetPropW(GetParent(), L"FBE_SETTINGS_GROUP_PAINT_PASSED", reinterpret_cast<HANDLE>(paintPassed ? 1 : 2));
			passed = passed && paintPassed;
		}
		if(index == PageIndex(SettingsPageId::Editor) && paintOnly && ThemeManager::UsesPaletteSurface()) {
			HWND combo = m_editorPage->GetDlgItem(IDC_FONT_SIZE);
			wchar_t initialText[128] = {}; ::GetWindowTextW(combo, initialText, _countof(initialText));
			const LRESULT initial = ::SendMessageW(combo, CB_GETCURSEL, 0, 0);
			unsigned states = 0;
			auto arrowPixel = [&]() {
				COMBOBOXINFO info = {}; info.cbSize = sizeof(info); RECT bounds = {}; ::GetClientRect(combo, &bounds); CImage image;
				if(!::GetComboBoxInfo(combo, &info) || info.rcButton.right - info.rcButton.left < 5 || info.rcButton.bottom - info.rcButton.top < 5 || !image.Create(bounds.right, bounds.bottom, 32)) return static_cast<COLORREF>(CLR_INVALID);
				HDC dc = image.GetDC(); COLORREF pixel = CLR_INVALID;
				if(dc) { ::SendMessageW(combo, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT); pixel = ::GetPixel(dc, info.rcButton.left + 2, info.rcButton.top + 2); image.ReleaseDC(); }
				return pixel;
			};
			::SendMessageW(combo, WM_MOUSELEAVE, 0, 0);
			if(arrowPixel() == ThemeManager::ControlColor()) states |= 16U;
			::SendMessageW(combo, WM_MOUSEMOVE, 0, MAKELPARAM(1, 1));
			if(arrowPixel() == ThemeManager::HoverColor()) states |= 32U;
			::SendMessageW(combo, WM_MOUSELEAVE, 0, 0);
			if(::SendMessageW(combo, CB_GETCOUNT, 0, 0) > 1) {
				::SendMessageW(combo, CB_SETCURSEL, 0, 0);
				::SendMessageW(combo, CB_SHOWDROPDOWN, TRUE, 0);
				if(arrowPixel() == ThemeManager::PressedColor()) states |= 64U;
				COMBOBOXINFO info = {}; info.cbSize = sizeof(info);
				if(::GetComboBoxInfo(combo, &info) && ::IsWindowVisible(info.hwndList)) {
					RECT bounds = {}; ::GetClientRect(info.hwndList, &bounds); CImage image;
					if(bounds.right > 0 && bounds.bottom > 0 && image.Create(bounds.right, bounds.bottom, 32)) {
						HDC dc = image.GetDC();
						if(dc) {
							::SendMessageW(info.hwndList, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
							const LRESULT selected = ::SendMessageW(info.hwndList, LB_GETCURSEL, 0, 0);
							for(LRESULT item = 0; item < 2; ++item) {
								RECT row = {}; if(::SendMessageW(info.hwndList, LB_GETITEMRECT, item, reinterpret_cast<LPARAM>(&row)) == LB_ERR || row.top < 0 || row.bottom > bounds.bottom || row.right < 8) continue;
								const COLORREF pixel = ::GetPixel(dc, row.right - 4, (row.top + row.bottom) / 2);
								if(item == selected && pixel == ThemeManager::SelectionBackgroundColor()) states |= 1U;
								if(item != selected && pixel == ThemeManager::ControlColor()) states |= 2U;
							}
							if(selected == 0) {
								::SendMessageW(info.hwndList, WM_KEYDOWN, VK_DOWN, 0);
								if(::SendMessageW(info.hwndList, LB_GETCURSEL, 0, 0) == 1) states |= 4U;
							}
							image.ReleaseDC();
						}
					}
				}
				::SendMessageW(combo, CB_SHOWDROPDOWN, FALSE, 0);
			}
			::SetWindowTextW(combo, L"13.5");
			wchar_t editedText[128] = {}; ::GetWindowTextW(combo, editedText, _countof(editedText));
			if(wcscmp(editedText, L"13.5") == 0) states |= 8U;
			::SendMessageW(combo, CB_SETCURSEL, initial, 0); ::SetWindowTextW(combo, initialText);
			::SetPropW(GetParent(), L"FBE_SETTINGS_EDITABLE_COMBO_PAINT", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(states)));
			passed = passed && states == 127U;
		}
		if(index == PageIndex(SettingsPageId::Appearance) && paintOnly && ThemeManager::UsesPaletteSurface()) {
			HWND label = m_generalPage->GetDlgItem(IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL);
			const bool labelWasEnabled = ::IsWindowEnabled(label)!=FALSE; ::EnableWindow(label,FALSE);
			RECT labelBounds={}; ::GetClientRect(label,&labelBounds); CImage labelImage; bool disabledLabel=false;
			if(labelBounds.right>0 && labelBounds.bottom>0 && labelImage.Create(labelBounds.right,labelBounds.bottom,32)) {
				HDC dc=labelImage.GetDC();
				if(dc) {
					::SendMessageW(label,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
					for(int y=0; y<labelBounds.bottom && !disabledLabel; ++y)
						for(int x=0; x<labelBounds.right; ++x) if(::GetPixel(dc,x,y)==ThemeManager::DisabledTextColor()) { disabledLabel=true; break; }
					labelImage.ReleaseDC();
				}
			}
			::EnableWindow(label,labelWasEnabled);
			::SetPropW(GetParent(),L"FBE_SETTINGS_DISABLED_LABEL_PAINT",reinterpret_cast<HANDLE>(disabledLabel?1:2));
			passed = passed && disabledLabel;
			HWND button = m_generalPage->GetDlgItem(IDC_INTERFACE_PALETTE_COPY);
			RECT buttonRect = {}; ::GetClientRect(button, &buttonRect);
			auto buttonPixel = [&](int x, int y) {
				CImage image; if(!image.Create(buttonRect.right, buttonRect.bottom, 32)) return static_cast<COLORREF>(CLR_INVALID);
				HDC dc = image.GetDC(); COLORREF pixel = CLR_INVALID;
				if(dc) { ::SendMessageW(button, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT); pixel = ::GetPixel(dc, x, y); image.ReleaseDC(); }
				return pixel;
			};
			const int sample = UiMetrics::ScaleForDpi(3, m_layoutDpi);
			const HWND oldFocus = ::GetFocus();
			::SendMessageW(button, WM_MOUSELEAVE, 0, 0); ::SetFocus(m_navigation);
			unsigned buttonStates = buttonPixel(sample, sample) == ThemeManager::ControlColor() ? 1U : 0U;
			::SendMessageW(button, WM_MOUSEMOVE, 0, MAKELPARAM(sample, sample));
			if(buttonPixel(sample, sample) == ThemeManager::HoverColor()) buttonStates |= 2U;
			::SendMessageW(button, BM_SETSTATE, TRUE, 0);
			if(buttonPixel(sample, sample) == ThemeManager::PressedColor()) buttonStates |= 4U;
			::SendMessageW(button, BM_SETSTATE, FALSE, 0); ::SendMessageW(button, WM_MOUSELEAVE, 0, 0);
			::EnableWindow(button, FALSE);
			if(buttonPixel(sample, sample) == ThemeManager::ControlColor()) buttonStates |= 8U;
			::EnableWindow(button, TRUE); ::SetFocus(button);
			if(buttonPixel(0, 0) == ThemeManager::FocusColor()) buttonStates |= 16U;
			if(oldFocus && ::IsWindow(oldFocus)) ::SetFocus(oldFocus);
			::SetPropW(GetParent(), L"FBE_SETTINGS_BUTTON_PAINT", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(buttonStates)));
			passed = passed && buttonStates == 31U;
			HWND combo = m_generalPage->GetDlgItem(IDC_MAIN_TOOLBAR_ICON_SIZE);
			RECT comboRect = {}; ::GetClientRect(combo, &comboRect);
			auto comboPixel = [&](int x, int y) {
				CImage image; if(!image.Create(comboRect.right, comboRect.bottom, 32)) return static_cast<COLORREF>(CLR_INVALID);
				HDC dc = image.GetDC(); COLORREF pixel = CLR_INVALID;
				if(dc) { ::SendMessageW(combo, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT); pixel = ::GetPixel(dc, x, y); image.ReleaseDC(); }
				return pixel;
			};
			::SendMessageW(combo, WM_MOUSELEAVE, 0, 0); ::SetFocus(m_navigation);
			unsigned comboStates = comboPixel(sample, sample) == ThemeManager::ControlColor() ? 1U : 0U;
			::SendMessageW(combo, WM_MOUSEMOVE, 0, MAKELPARAM(sample, sample));
			if(comboPixel(sample, sample) == ThemeManager::HoverColor()) comboStates |= 2U;
			::SendMessageW(combo, WM_MOUSELEAVE, 0, 0); ::EnableWindow(combo, FALSE);
			if(comboPixel(sample, sample) == ThemeManager::ControlColor()) comboStates |= 4U;
			::EnableWindow(combo, TRUE); ::SetFocus(combo);
			if(comboPixel(0, 0) == ThemeManager::FocusColor()) comboStates |= 8U;
			const LRESULT initial = ::SendMessageW(combo, CB_GETCURSEL, 0, 0);
			const LRESULT count = ::SendMessageW(combo, CB_GETCOUNT, 0, 0);
			if(count > 1 && initial != CB_ERR) {
				const LRESULT next = (initial + 1) % count;
				::SendMessageW(combo, CB_SETCURSEL, next, 0);
				if(::SendMessageW(combo, CB_GETCURSEL, 0, 0) == next) comboStates |= 16U;
				::SendMessageW(combo, CB_SETCURSEL, initial, 0);
			}
			::SendMessageW(combo, CB_SHOWDROPDOWN, TRUE, 0);
			COMBOBOXINFO comboInfo = {}; comboInfo.cbSize = sizeof(comboInfo);
			if(::GetComboBoxInfo(combo, &comboInfo) && ::IsWindowVisible(comboInfo.hwndList)) {
				if(comboPixel(sample, sample) == ThemeManager::PressedColor()) comboStates |= 32U;
				RECT listRect = {}; ::GetClientRect(comboInfo.hwndList, &listRect); CImage listImage;
				if(listRect.right > 0 && listRect.bottom > 0 && listImage.Create(listRect.right, listRect.bottom, 32)) {
					HDC listDc = listImage.GetDC();
					if(listDc) {
						::SendMessageW(comboInfo.hwndList, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(listDc), PRF_CLIENT);
						const LRESULT selected = ::SendMessageW(comboInfo.hwndList, LB_GETCURSEL, 0, 0);
						const LRESULT listCount = ::SendMessageW(comboInfo.hwndList, LB_GETCOUNT, 0, 0);
						for(LRESULT item = 0; item < listCount; ++item) {
							RECT row = {}; if(::SendMessageW(comboInfo.hwndList, LB_GETITEMRECT, item, reinterpret_cast<LPARAM>(&row)) == LB_ERR || row.top < 0 || row.bottom > listRect.bottom || row.right < 8) continue;
							const COLORREF pixel = ::GetPixel(listDc, row.right - 4, (row.top + row.bottom) / 2);
							if(item == selected && pixel == ThemeManager::SelectionBackgroundColor()) comboStates |= 64U;
							if(item != selected && pixel == ThemeManager::ControlColor()) comboStates |= 128U;
						}
						if(selected != LB_ERR && selected + 1 < listCount) {
							::SendMessageW(comboInfo.hwndList, WM_KEYDOWN, VK_DOWN, 0);
							const LRESULT moved = ::SendMessageW(comboInfo.hwndList, LB_GETCURSEL, 0, 0);
							if(moved == selected + 1) {
								comboStates |= 256U;
								::SendMessageW(comboInfo.hwndList, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(listDc), PRF_CLIENT);
								RECT row = {}; if(::SendMessageW(comboInfo.hwndList, LB_GETITEMRECT, moved, reinterpret_cast<LPARAM>(&row)) != LB_ERR &&
									row.top >= 0 && row.bottom <= listRect.bottom && row.right >= 8 &&
									::GetPixel(listDc, row.right - 4, (row.top + row.bottom) / 2) == ThemeManager::SelectionBackgroundColor()) comboStates |= 512U;
							}
						}
						listImage.ReleaseDC();
					}
				}
			}
			::SendMessageW(combo, CB_SHOWDROPDOWN, FALSE, 0);
			if(initial != CB_ERR) ::SendMessageW(combo, CB_SETCURSEL, initial, 0);
			if(oldFocus && ::IsWindow(oldFocus)) ::SetFocus(oldFocus);
			::SetPropW(GetParent(), L"FBE_SETTINGS_COMBO_PAINT", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(comboStates)));
			passed = passed && comboStates == 1023U;
			unsigned groupedStates = 0;
			for(int role = 0; role < 2; ++role) {
				HWND grouped = m_generalPage->GetDlgItem(role == 0 ? IDC_INTERFACE_THEME : IDC_INTERFACE_PALETTE);
				::SendMessageW(grouped, WM_MOUSELEAVE, 0, 0); ::SetFocus(m_navigation);
				RECT bounds = {}; ::GetClientRect(grouped, &bounds); CImage image;
				if(bounds.right > sample && bounds.bottom > sample && image.Create(bounds.right, bounds.bottom, 32)) {
					HDC dc = image.GetDC();
					if(dc) {
						::SendMessageW(grouped, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
						if(::GetPixel(dc, sample, sample) == ThemeManager::ControlColor()) groupedStates |= 1U << role;
						image.ReleaseDC();
					}
				}
				const LRESULT selection = ::SendMessageW(grouped, CB_GETCURSEL, 0, 0);
				::SendMessageW(grouped, CB_SHOWDROPDOWN, TRUE, 0);
				COMBOBOXINFO info = {}; info.cbSize = sizeof(info);
				if(::GetComboBoxInfo(grouped, &info) && ::IsWindowVisible(info.hwndList) &&
					::SendMessageW(grouped, CB_GETCOUNT, 0, 0) > 1 && ::SendMessageW(grouped, CB_GETCURSEL, 0, 0) == selection)
					groupedStates |= 1U << (role + 2);
				::SendMessageW(grouped, CB_SHOWDROPDOWN, FALSE, 0);
			}
			if(oldFocus && ::IsWindow(oldFocus)) ::SetFocus(oldFocus);
			::SetPropW(GetParent(), L"FBE_SETTINGS_GROUPED_COMBO_PAINT", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(groupedStates)));
			passed = passed && groupedStates == 15U;
		}
		if(index == PageIndex(SettingsPageId::Appearance)) {
			const CRect colours = controlRect(m_generalPage->GetDlgItem(IDC_OPTIONS_INTERFACE_GROUP));
			const CRect tools = controlRect(m_generalPage->GetDlgItem(IDC_APPEARANCE_TOOLBARS_GROUP));
			const CRect description = controlRect(m_generalPage->GetDlgItem(IDC_APPEARANCE_DESCRIPTION_GROUP));
			const CRect iconPreview = controlRect(m_generalPage->GetDlgItem(IDC_APPEARANCE_ICON_PREVIEW));
			passed = passed && colours.bottom <= tools.top && tools.bottom <= description.top &&
				(::GetWindowLongPtrW(m_generalPage->GetDlgItem(IDC_APPEARANCE_THEME_PREVIEW), GWL_STYLE) & WS_VISIBLE) == 0 &&
				iconPreview.top >= tools.top && iconPreview.bottom <= tools.bottom &&
				(::GetWindowLongPtrW(m_generalPage->GetDlgItem(IDC_PALETTE_ROLE), GWL_STYLE) & WS_VISIBLE) == 0 &&
				(::GetWindowLongPtrW(m_generalPage->GetDlgItem(IDC_PALETTE_MANAGE), GWL_STYLE) & WS_VISIBLE) != 0;
			wchar_t artifactDirectory[32768] = {};
			const DWORD artifactLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_APPEARANCE_ARTIFACT_DIR", artifactDirectory, _countof(artifactDirectory));
			if(artifactLength && artifactLength < _countof(artifactDirectory)) {
				const int previousNavigation = m_navigation.GetCurSel();
				m_navigation.SetCurSel(index);
				::RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
				RECT window = {}; ::GetWindowRect(m_hWnd, &window); CImage screenshot;
				const bool created = screenshot.Create(window.right - window.left, window.bottom - window.top, 32) != FALSE;
				HDC target = created ? screenshot.GetDC() : NULL;
				const bool captured = target && ::PrintWindow(m_hWnd, target, 0) != FALSE;
				if(target) screenshot.ReleaseDC();
				const CString path = CString(artifactDirectory) + L"\\settings-appearance.bmp";
				passed = captured && SUCCEEDED(screenshot.Save(path)) && passed;
				m_navigation.SetCurSel(previousNavigation);
			}
			const int width = UiMetrics::ScaleForDpi(200, m_layoutDpi), height = UiMetrics::ScaleForDpi(100, m_layoutDpi);
			HDC source = ::GetDC(page), dc = source ? ::CreateCompatibleDC(source) : NULL;
			HBITMAP bitmap = source ? ::CreateCompatibleBitmap(source, width, height) : NULL;
			HGDIOBJ old = dc && bitmap ? ::SelectObject(dc, bitmap) : NULL;
			if(!dc || !bitmap || !old) passed = false;
			else {
				DRAWITEMSTRUCT draw = {}; draw.CtlType = ODT_STATIC; draw.hDC = dc; draw.rcItem = {0, 0, width, height};
				BOOL handled = FALSE;
				const HWND mainSize = m_generalPage->GetDlgItem(IDC_MAIN_TOOLBAR_ICON_SIZE);
				const HWND scriptSize = m_generalPage->GetDlgItem(IDC_SCRIPT_ICON_SIZE);
				const LRESULT originalMain = ::SendMessageW(mainSize, CB_GETCURSEL, 0, 0), originalScript = ::SendMessageW(scriptSize, CB_GETCURSEL, 0, 0);
				::SendMessageW(mainSize, CB_SETCURSEL, 0, 0); ::SendMessageW(scriptSize, CB_SETCURSEL, 0, 0);
				draw.CtlID = IDC_APPEARANCE_ICON_PREVIEW; m_generalPage->OnDrawThemeItem(WM_DRAWITEM, 0, reinterpret_cast<LPARAM>(&draw), handled);
				const int edge = (std::max)(1, UiMetrics::ScaleForDpi(1, m_layoutDpi));
				const int pad = UiMetrics::ScaleForDpi(5, m_layoutDpi);
				const int menuHeight = UiMetrics::ScaleForDpi(22, m_layoutDpi);
				const int iconSize = UiMetrics::ScaleForDpi(16, m_layoutDpi);
				const int middle = menuHeight + (height - menuHeight) / 2;
				const int buttonX = pad + 4 * (iconSize + pad) - edge;
				const int buttonY = menuHeight + (middle - menuHeight - iconSize) / 2 - edge;
				passed = ::GetPixel(dc, width - edge * 3, menuHeight - edge) == ThemeManager::BorderColor() &&
					::GetPixel(dc, buttonX, buttonY) == ThemeManager::BorderColor() && passed;
				const HWND paletteControl = m_generalPage->GetDlgItem(IDC_INTERFACE_PALETTE);
				const LRESULT originalPalette = ::SendMessageW(paletteControl, CB_GETCURSEL, 0, 0);
				const InterfacePalette appliedPalette = ThemeManager::GetNamedPalette();
				for(InterfacePalette palette : {INTERFACE_PALETTE_SOLARIZED_LIGHT, INTERFACE_PALETTE_ROSE_PINE_MOON}) {
					if(!ThemeManager::IsDarkModeSupported() && ThemeManager::NamedPaletteIsDark(palette)) continue;
					::SendMessageW(paletteControl, CB_SETCURSEL, CSettingsGeneralPage::RowForPalette(palette), 0);
					m_generalPage->OnInterfacePaletteChanged(CBN_SELCHANGE, IDC_INTERFACE_PALETTE, paletteControl, handled);
					m_generalPage->OnDrawThemeItem(WM_DRAWITEM, 0, reinterpret_cast<LPARAM>(&draw), handled);
					COLORREF preview[THEME_COLOR_COUNT] = {};
					ThemeManager::PreviewColors(_Settings.GetInterfaceTheme(), palette, NULL, preview);
					passed = ::GetPixel(dc, width - edge * 3, menuHeight - edge) == preview[THEME_COLOR_BORDER] &&
						::GetPixel(dc, width - edge * 3, middle - edge) == preview[THEME_COLOR_BORDER] &&
						preview[THEME_COLOR_BORDER] != preview[THEME_COLOR_CONTROL] &&
						ThemeManager::GetNamedPalette() == appliedPalette && passed;
					if(artifactLength && artifactLength < _countof(artifactDirectory)) {
						CImage sample;
						const bool created = sample.Create(width, height, 32) != FALSE;
						HDC target = created ? sample.GetDC() : NULL;
						const bool captured = target && ::BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY) != FALSE;
						if(target) sample.ReleaseDC();
						const CString path = CString(artifactDirectory) + (palette == INTERFACE_PALETTE_SOLARIZED_LIGHT ? L"\\preview-solarized.bmp" : L"\\preview-rose-pine.bmp");
						passed = captured && SUCCEEDED(sample.Save(path)) && passed;
					}
				}
				::SendMessageW(paletteControl, CB_SETCURSEL, originalPalette, 0);
				m_generalPage->OnInterfacePaletteChanged(CBN_SELCHANGE, IDC_INTERFACE_PALETTE, paletteControl, handled);
				m_generalPage->OnDrawThemeItem(WM_DRAWITEM, 0, reinterpret_cast<LPARAM>(&draw), handled);
				std::vector<COLORREF> smallPreview;
				for(int y = 0; y < height; ++y) for(int x = 0; x < width; ++x) smallPreview.push_back(::GetPixel(dc, x, y));
				::SendMessageW(mainSize, CB_SETCURSEL, 3, 0); m_generalPage->OnDrawThemeItem(WM_DRAWITEM, 0, reinterpret_cast<LPARAM>(&draw), handled);
				bool changed = false; size_t pixel = 0;
				for(int y = 0; y < height; ++y) for(int x = 0; x < width; ++x, ++pixel)
					if(smallPreview[pixel] != ::GetPixel(dc, x, y)) changed = true;
				passed = changed && passed;
				::SendMessageW(mainSize, CB_SETCURSEL, originalMain, 0); ::SendMessageW(scriptSize, CB_SETCURSEL, originalScript, 0);
			}
			if(old) ::SelectObject(dc, old); if(bitmap) ::DeleteObject(bitmap); if(dc) ::DeleteDC(dc); if(source) ::ReleaseDC(page, source);
		}
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
			const bool fits = viewport.right >= m_pageLayouts[index].contentSize.cx &&
				viewport.bottom >= m_pageLayouts[index].contentSize.cy;
			if(index == PageIndex(SettingsPageId::General) || index == PageIndex(SettingsPageId::Appearance))
				passed = fits && passed;
			if(fits) passed = !(::GetWindowLongPtrW(page, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL)) && passed;
		}
	}
	SelectPage(SettingsPageId::Spelling);
	const HWND spellingGroup = m_spellingPage->GetDlgItem(IDC_OPTIONS_SPELLCHECK_GROUP);
	const HWND yoOption = m_spellingPage->GetDlgItem(IDC_CHECK_RUSSIAN_YO_USAGE);
	const HWND dictionaryField = m_spellingPage->GetDlgItem(IDC_CUSTOM_DICT);
	const HWND dictionaryButton = m_spellingPage->GetDlgItem(IDC_DICTPATH);
	auto spellingGeometry = [&]() {
		const CRect group = controlRect(spellingGroup);
		const CRect option = controlRect(yoOption);
		const CRect label = controlRect(m_spellingPage->GetDlgItem(IDS_SPELL_CUSTOM_DICT));
		const CRect field = controlRect(dictionaryField);
		const CRect browse = controlRect(dictionaryButton);
		return group.left < option.left && option.right < group.right &&
			option.bottom <= label.top && label.bottom <= field.top &&
			field.left == option.left && field.top == browse.top &&
			field.bottom == browse.bottom && field.right < browse.left &&
			browse.right < group.right && browse.bottom < group.bottom;
	};
	passed = passed && spellingGeometry();
	const int originalDictionaryWidth = controlRect(dictionaryField).Width();
	const int originalButtonLeft = controlRect(dictionaryButton).left;
	const LRESULT spellingEnabled = m_spellingPage->SendDlgItemMessage(IDC_USESPELLCHECKER, BM_GETCHECK);
	const HWND spellingToggle = m_spellingPage->GetDlgItem(IDC_USESPELLCHECKER);
	m_spellingPage->CheckDlgButton(IDC_USESPELLCHECKER, BST_UNCHECKED);
	::SendMessageW(m_spellingPage->m_hWnd, WM_COMMAND, MAKEWPARAM(IDC_USESPELLCHECKER, BN_CLICKED),
		reinterpret_cast<LPARAM>(spellingToggle));
	passed = passed && !::IsWindowEnabled(yoOption);
	m_spellingPage->CheckDlgButton(IDC_USESPELLCHECKER, spellingEnabled);
	::SendMessageW(m_spellingPage->m_hWnd, WM_COMMAND, MAKEWPARAM(IDC_USESPELLCHECKER, BN_CLICKED),
		reinterpret_cast<LPARAM>(spellingToggle));
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
	const HWND wrapControl = m_sourcePage->GetDlgItem(IDC_WRAP);
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
	SelectPage(SettingsPageId::Spelling);
	passed = passed && spellingGeometry();
	if(resizedWidth > original.Width())
		passed = passed && controlRect(dictionaryField).Width() > originalDictionaryWidth &&
			controlRect(dictionaryButton).left > originalButtonLeft;
	::SendMessageW(m_hWnd, WM_EXITSIZEMOVE, 0, 0);
	if(resizedWidth != original.Width()) passed = passed && !m_automaticSize;
	::SetWindowPos(m_hWnd, NULL, original.left, original.top, original.Width(), original.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE);
	SelectPage(SettingsPageId::Spelling);
	passed = passed && spellingGeometry() &&
		controlRect(dictionaryField).Width() == originalDictionaryWidth &&
		controlRect(dictionaryButton).left == originalButtonLeft;
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
	const LRESULT originalWrap = ::SendMessageW(wrapControl, BM_GETCHECK, 0, 0);
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
				for(const bool wrap : { false, true })
				{
					::SendMessageW(wrapControl, BM_SETCHECK, wrap ? BST_CHECKED : BST_UNCHECKED, 0);
					::SendMessageW(m_sourcePage->m_hWnd, WM_COMMAND, MAKEWPARAM(IDC_WRAP, BN_CLICKED),
						reinterpret_cast<LPARAM>(wrapControl));
					LayoutPage(PageIndex(SettingsPageId::Source));
					const int lineHeight = m_sourcePage->PreviewLineHeight();
					const int displayLines = m_sourcePage->PreviewDisplayLineCount();
					const int logicalLines = static_cast<int>(::SendMessageW(previewEditor, SCI_GETLINECOUNT, 0, 0));
					const bool wrapApplied = (::SendMessageW(previewEditor, SCI_GETWRAPMODE, 0, 0) != SC_WRAP_NONE) == wrap;
					passed = passed && wrapApplied && logicalLines == 6;
					RECT previewClient = {};
					::GetClientRect(previewEditor, &previewClient);
					const CRect group = controlRect(m_sourcePage->GetDlgItem(IDC_OPTIONS_SOURCE_PREVIEW_GROUP));
					const CRect preview = controlRect(previewHost);
					const bool minimumRows = lineHeight > 0 && displayLines >= 6 &&
						previewClient.bottom >= 4 * lineHeight;
					const bool insideGroup = group.left < preview.left && group.top < preview.top &&
						preview.right < group.right && preview.bottom < group.bottom;
					passed = passed && minimumRows && insideGroup;
					RECT pageClient = {};
					::GetClientRect(m_sourcePage->m_hWnd, &pageClient);
					const int fullHeight = displayLines * lineHeight + m_sourcePage->PreviewVerticalChromeHeight();
					if(pageClient.bottom >= max(m_pageLayouts[PageIndex(SettingsPageId::Source)].templateSize.cy,
						m_pageLayouts[PageIndex(SettingsPageId::Source)].naturalContentHeight) +
						max(0, fullHeight - sourcePreviewBaselineHeight))
					{
						passed = passed && previewClient.bottom >= displayLines * lineHeight &&
							::SendMessageW(previewEditor, SCI_GETFIRSTVISIBLELINE, 0, 0) == 0;
					}
				}
			}
		}
	}
	::SendMessageW(wrapControl, BM_SETCHECK, originalWrap, 0);
	::SendMessageW(m_sourcePage->m_hWnd, WM_COMMAND, MAKEWPARAM(IDC_WRAP, BN_CLICKED),
		reinterpret_cast<LPARAM>(wrapControl));
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
	for(const UINT probeDpi : { 96U, 120U, 144U, 192U })
	{
		::SendMessageW(m_hWnd, WM_DPICHANGED, MAKELONG(probeDpi, probeDpi), 0);
		SelectPage(SettingsPageId::Words);
		passed = passed && controlRect(m_navigation).bottom == controlRect(m_wordsPage->m_hWnd).bottom && wordsGeometry();
		SelectPage(SettingsPageId::Appearance);
		const CRect colourGroup = controlRect(m_generalPage->GetDlgItem(IDC_OPTIONS_INTERFACE_GROUP));
		const CRect toolbarGroup = controlRect(m_generalPage->GetDlgItem(IDC_APPEARANCE_TOOLBARS_GROUP));
		const CRect descriptionGroup = controlRect(m_generalPage->GetDlgItem(IDC_APPEARANCE_DESCRIPTION_GROUP));
		const CRect iconSample = controlRect(m_generalPage->GetDlgItem(IDC_APPEARANCE_ICON_PREVIEW));
		struct AppearanceRow { int label, field; CRect group; };
		const AppearanceRow rows[] = {
			{ IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME, colourGroup },
			{ IDC_INTERFACE_PALETTE_LABEL, IDC_INTERFACE_PALETTE, colourGroup },
			{ IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE, toolbarGroup },
			{ IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE, toolbarGroup },
			{ IDC_DESCRIPTION_LAYOUT_LABEL, IDC_DESCRIPTION_LAYOUT, descriptionGroup }
		};
		int previousBottom = 0, paletteRowBottom = 0, scriptRowBottom = 0;
		for(const auto& row : rows) {
			const HWND field = m_generalPage->GetDlgItem(row.field);
			COMBOBOXINFO info = {}; info.cbSize = sizeof(info);
			const bool gotInfo = ::GetComboBoxInfo(field, &info) != FALSE;
			const CRect label = controlRect(m_generalPage->GetDlgItem(row.label));
			CRect box = controlRect(field);
			if(gotInfo) box.bottom = box.top + max(info.rcItem.bottom, info.rcButton.bottom);
			passed = gotInfo && label.left >= row.group.left && label.right <= box.left &&
				box.right <= row.group.right && label.top >= row.group.top && box.top >= row.group.top &&
				label.bottom <= row.group.bottom && box.bottom <= row.group.bottom &&
				label.top >= previousBottom && box.top >= previousBottom && passed;
			previousBottom = max(label.bottom, box.bottom);
			if(row.field == IDC_INTERFACE_PALETTE) paletteRowBottom = previousBottom;
			if(row.field == IDC_SCRIPT_ICON_SIZE) scriptRowBottom = previousBottom;
		}
		const CRect copy = controlRect(m_generalPage->GetDlgItem(IDC_INTERFACE_PALETTE_COPY));
		const CRect manage = controlRect(m_generalPage->GetDlgItem(IDC_PALETTE_MANAGE));
		auto insideGroup = [](const CRect& child, const CRect& group) {
			return child.Width() > 0 && child.Height() > 0 && child.left >= group.left &&
				child.right <= group.right && child.top >= group.top && child.bottom <= group.bottom;
		};
		passed = colourGroup.bottom <= toolbarGroup.top && toolbarGroup.bottom <= descriptionGroup.top &&
			insideGroup(copy, colourGroup) && insideGroup(manage, colourGroup) &&
			insideGroup(iconSample, toolbarGroup) &&
			copy.right <= manage.left && copy.top >= paletteRowBottom && manage.top >= paletteRowBottom &&
			iconSample.top >= scriptRowBottom && passed;
	}
	// At 100% DPI, exercise the actual preferred window size against a
	// conservative 1366x728 work area (768px screen with a 40px taskbar).
	CRect beforeScreenProbe; GetWindowRect(beforeScreenProbe);
	const bool beforeAutomatic = m_automaticSize; m_automaticSize = true;
	RECT suggested = beforeScreenProbe;
	::SendMessageW(m_hWnd, WM_DPICHANGED, MAKELONG(96, 96), reinterpret_cast<LPARAM>(&suggested));
	SelectPage(SettingsPageId::Appearance);
	CRect preferred; GetWindowRect(preferred); GetClientRect(client);
	RECT appearanceClient = {}; ::GetClientRect(m_generalPage->m_hWnd, &appearanceClient);
	const PageLayout& appearanceLayout = m_pageLayouts[PageIndex(SettingsPageId::Appearance)];
	passed = preferred.Width() <= 1366 && preferred.Height() <= 728 &&
		appearanceLayout.contentSize.cx <= appearanceClient.right &&
		appearanceLayout.contentSize.cy <= appearanceClient.bottom &&
		inside(controlRect(GetDlgItem(IDOK))) && inside(controlRect(GetDlgItem(IDCANCEL))) && passed;
	::SetPropW(::GetParent(m_hWnd), L"FBE_SETTINGS_SCREEN_WIDTH", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(preferred.Width())));
	::SetPropW(::GetParent(m_hWnd), L"FBE_SETTINGS_SCREEN_HEIGHT", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(preferred.Height())));
	m_automaticSize = beforeAutomatic;
	::SendMessageW(m_hWnd, WM_DPICHANGED, MAKELONG(originalLayoutDpi, originalLayoutDpi), 0);
	::SetWindowPos(m_hWnd, NULL, beforeScreenProbe.left, beforeScreenProbe.top,
		beforeScreenProbe.Width(), beforeScreenProbe.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
	SelectPage(SettingsPageId::General);
	::SetPropW(::GetParent(m_hWnd), L"FBE_SETTINGS_PAINT_AREAS", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(paintAreas)));
	::SetPropW(::GetParent(m_hWnd), L"FBE_SETTINGS_PAINT_FAILURES", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(paintFailures)));
	::SetPropW(::GetParent(m_hWnd), L"FBE_SETTINGS_REPAINT_PAGES", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(repaintPages)));
	::SetPropW(::GetParent(m_hWnd), L"FBE_SETTINGS_REPAINT_FAILURES", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(repaintFailures)));
	::SetPropW(::GetParent(m_hWnd), L"FBE_SETTINGS_HOVER_PAGES", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(hoverPages)));
	::SetPropW(::GetParent(m_hWnd), L"FBE_SETTINGS_HOVER_FAILURES", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(hoverFailures)));
	if(paintAreas) passed = passed && paintFailures == 0;
	if(repaintPages) passed = passed && repaintFailures == 0;
	if(hoverPages) passed = passed && hoverFailures == 0;
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
		_Settings.SetSettingsDialogGeometry(rect, UiMetrics::DpiForWindow(m_hWnd), !m_automaticSize, false);
}

#include "../../testing/SettingsFollowupProbe.inl"
