#pragma once

#include <atlhost.h>
#include <ColorButton.h>
#include "resource.h"
#include "SettingsPageLifecycle.h"
#include "SettingsTooltips.h"
#include "EditorBackgrounds.h"

class CSettingsEditorPage : public CAxDialogImpl<CSettingsEditorPage>, public ISettingsPage
{
	CColorButton m_foreground;
	CColorButton m_background;
	CComboBox m_fonts;
	CComboBox m_fontSize;
	CComboBox m_nbspCharacter;
	CComboBox m_backgroundImage;
	CComboBox m_backgroundLayout;
	CStatic m_backgroundPreview;
	std::vector<EditorBackgroundDescriptor> m_builtInBackgrounds;
	CString m_customBackgroundPath;
	CSettingsTooltips m_tooltips;

public:
	enum { IDD = IDD_SETTINGS_EDITOR };
	BEGIN_MSG_MAP(CSettingsEditorPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
		COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
		COMMAND_HANDLER(IDC_EDITOR_BACKGROUND_BROWSE, BN_CLICKED, OnBrowseBackground)
		COMMAND_HANDLER(IDC_EDITOR_BACKGROUND_IMAGE, CBN_SELCHANGE, OnBackgroundSelectionChanged)
		REFLECT_NOTIFICATIONS()
		CHAIN_MSG_MAP(CAxDialogImpl<CSettingsEditorPage>)
	END_MSG_MAP()
	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnClickedOK(WORD, WORD, HWND, BOOL&);
	LRESULT OnClickedCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnBrowseBackground(WORD, WORD, HWND, BOOL&);
	LRESULT OnBackgroundSelectionChanged(WORD, WORD, HWND, BOOL&);
	void UpdateBackgroundPreview();
	bool Validate(); void Commit(); bool CancelChanges();
};
