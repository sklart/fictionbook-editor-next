#pragma once
#include <atlhost.h>
#include "resource.h"

class CSettingsSpellingPage : public CAxDialogImpl<CSettingsSpellingPage>
{
	CButton m_enabled;
	CButton m_highlight;
	CEdit m_dictionary;
public:
	enum { IDD = IDD_SETTINGS_SPELLING };
	BEGIN_MSG_MAP(CSettingsSpellingPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
		COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
		COMMAND_HANDLER(IDC_USESPELLCHECKER, BN_CLICKED, OnSpellcheckerChanged)
		COMMAND_HANDLER(IDC_DICTPATH, BN_CLICKED, OnBrowseDictionary)
		CHAIN_MSG_MAP(CAxDialogImpl<CSettingsSpellingPage>)
	END_MSG_MAP()
	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnClickedOK(WORD, WORD, HWND, BOOL&);
	LRESULT OnClickedCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnSpellcheckerChanged(WORD, WORD, HWND, BOOL&);
	LRESULT OnBrowseDictionary(WORD, WORD, HWND, BOOL&);
private:
	void UpdateDependencies();
};
