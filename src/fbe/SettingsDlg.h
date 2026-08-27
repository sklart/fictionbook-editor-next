// SettingsDlg.h : Declaration of the CSettingsDlg

#pragma once

#include "resource.h"
#include "SettingsAdvancedPage.h"
#include "SettingsOtherDlg.h"
#include "SettingsHotkeysDlg.h"
#include "SettingsWordsDlg.h"
#include "SettingsGeneralPage.h"
#include "SettingsEditorPage.h"
#include "SettingsSpellingPage.h"
#include "SettingsSourcePage.h"

enum class SettingsPageId
{
	General,
	Editor,
	Source,
	Images,
	Spelling,
	Keyboard,
	Words,
	Advanced,
	Count
};

// CSettingsDlg

class CSettingsDlg : public CAxDialogImpl<CSettingsDlg>
{
	CListBox m_navigation;
	CSettingsAdvancedPage* m_advancedPage;
	CSettingsGeneralPage* m_generalPage;
	CSettingsEditorPage* m_editorPage;
	CSettingsSpellingPage* m_spellingPage;
	CSettingsOtherDlg* m_otherPage;
	CSettingsSourcePage* m_sourcePage;
	CSettingsHotkeysDlg* m_hotkeysPage;
	CSettingsWordsDlg* m_wordsPage;
	CWindow* m_pages[static_cast<int>(SettingsPageId::Count)];
	SettingsPageId m_currentPage;
	CRect m_navigationRect;
	CSize m_minimumWindowSize;
	int m_pageLeft;
	int m_pageTop;
	int m_pageRightMargin;
	int m_pageBottomMargin;
	int m_buttonWidth;
	int m_buttonHeight;
	int m_buttonGap;
	int m_buttonBottomMargin;
	int m_navigationBottomGap;

public:
	CSettingsDlg();
	~CSettingsDlg();

	enum { IDD = IDD_TOOLS_SETTINGS };

BEGIN_MSG_MAP(CSettingsDlg)
	MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
	MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
	COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
	COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
	COMMAND_HANDLER(IDC_SETTINGS_NAV, LBN_SELCHANGE, OnNavigationChanged)
	MESSAGE_HANDLER(WM_SIZE, OnSize)
	MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
	CHAIN_MSG_MAP(CAxDialogImpl<CSettingsDlg>)
END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnNavigationChanged(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnGetMinMaxInfo(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

private:
	void SelectPage(SettingsPageId page);
	void LayoutControls(int width, int height);
	CRect GetPageRect();
	static int PageIndex(SettingsPageId page);
};
