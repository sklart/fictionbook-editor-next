// SettingsNextDlg.h : вкладка параметров, относящихся к FBE Next.

#pragma once

#include <atlhost.h>
#include "resource.h"

class CSettingsNextDlg : public CAxDialogImpl<CSettingsNextDlg>
{
public:
	enum { IDD = IDD_SETTING_NEXT };

BEGIN_MSG_MAP(CSettingsNextDlg)
	MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
	COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
	COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
	CHAIN_MSG_MAP(CAxDialogImpl<CSettingsNextDlg>)
END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
};
