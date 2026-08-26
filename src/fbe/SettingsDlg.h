// SettingsDlg.h : Declaration of the CSettingsDlg

#pragma once

#include "resource.h"
#include "OptDlg.h"
#include "SettingsOtherDlg.h"
#include "SettingsNextDlg.h"
#include "SettingsHotkeysDlg.h"
#include "SettingsWordsDlg.h"

// CSettingsDlg

class CSettingsDlg : public CAxDialogImpl<CSettingsDlg>
{
	CListBox m_navigation;
	COptDlg* m_optionsPage;
	CSettingsOtherDlg* m_otherPage;
	CSettingsNextDlg* m_sourcePage;
	CSettingsHotkeysDlg* m_hotkeysPage;
	CSettingsWordsDlg* m_wordsPage;
	CWindow* m_pages[8];
	int m_currentPage;

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
	void SelectPage(int page);
	void LayoutControls(int width, int height);
	CRect GetPageRect();
};
