#pragma once

#include <atlhost.h>
#include <ColorButton.h>
#include "resource.h"
#include "SettingsPageLifecycle.h"

class CSettingsEditorPage : public CAxDialogImpl<CSettingsEditorPage>, public ISettingsPage
{
	CColorButton m_foreground;
	CColorButton m_background;
	CComboBox m_fonts;
	CComboBox m_fontSize;
	CComboBox m_nbspCharacter;

public:
	enum { IDD = IDD_SETTINGS_EDITOR };
	BEGIN_MSG_MAP(CSettingsEditorPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
		COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
		REFLECT_NOTIFICATIONS()
		CHAIN_MSG_MAP(CAxDialogImpl<CSettingsEditorPage>)
	END_MSG_MAP()
	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnClickedOK(WORD, WORD, HWND, BOOL&);
	LRESULT OnClickedCancel(WORD, WORD, HWND, BOOL&);
	bool Validate(); void Commit(); bool CancelChanges();
};
