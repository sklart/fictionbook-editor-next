#pragma once

#include <atlhost.h>
#include "..\\..\\resource.h"
#include "..\\..\\ThemeManager.h"
#include "SettingsPageLifecycle.h"
#include "SettingsTooltips.h"

class CSettingsGeneralPage : public CAxDialogImpl<CSettingsGeneralPage>, public ISettingsPage
{
	CComboBox m_language;
	CComboBox m_interfaceTheme;
	CComboBox m_genreCatalog;
	CComboBox m_defaultEncoding;
	CButton m_keepEncoding;
	CButton m_restorePosition;
	CComboBox m_updateChannel;
	CSettingsTooltips m_tooltips;
	InterfaceTheme m_originalTheme;

public:
	enum { IDD = IDD_SETTINGS_GENERAL };

	BEGIN_MSG_MAP(CSettingsGeneralPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
		COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
		COMMAND_HANDLER(IDC_INTERFACE_THEME, CBN_SELCHANGE, OnInterfaceThemeChanged)
		CHAIN_MSG_MAP(CAxDialogImpl<CSettingsGeneralPage>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnClickedOK(WORD, WORD, HWND, BOOL&);
	LRESULT OnClickedCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnInterfaceThemeChanged(WORD, WORD, HWND, BOOL&);
	bool Validate(); void Commit(); bool CancelChanges();
};
