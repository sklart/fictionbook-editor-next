#pragma once

#include <atlhost.h>
#include "resource.h"
#include "SettingsPageLifecycle.h"
#include "SettingsTooltips.h"

class CSettingsAdvancedPage : public CAxDialogImpl<CSettingsAdvancedPage>, public ISettingsPage
{
	CButton m_defaultScriptsFolder, m_fastMode;
	CEdit m_scriptsFolder;
	CButton m_selectScriptsFolder;
	bool m_scriptsSwitched;
	CString m_initialScriptsFolder;
	CSettingsTooltips m_tooltips;
public:
	enum { IDD = IDD_SETTINGS_ADVANCED };
BEGIN_MSG_MAP(CSettingsAdvancedPage)
	MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
	COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
	COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
	COMMAND_HANDLER(IDC_DEFAULT_SCRIPTS_FOLDER, BN_CLICKED, OnDefaultScriptsFolder)
	COMMAND_HANDLER(IDC_SELECT_SCRIPTS_FOLDER_BUTTON, BN_CLICKED, OnSelectScriptsFolder)
	CHAIN_MSG_MAP(CAxDialogImpl<CSettingsAdvancedPage>)
END_MSG_MAP()
	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnClickedOK(WORD, WORD, HWND, BOOL&);
	LRESULT OnClickedCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnDefaultScriptsFolder(WORD, WORD, HWND, BOOL&);
	LRESULT OnSelectScriptsFolder(WORD, WORD, HWND, BOOL&);
	bool Validate(); void Commit(); bool CancelChanges();
};
