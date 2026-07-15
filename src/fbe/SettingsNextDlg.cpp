// SettingsNextDlg.cpp : реализация вкладки параметров FBE Next.

#include "stdafx.h"
#include "SettingsNextDlg.h"
#include "Settings.h"
#include "RuntimeLocalization.h"

extern CSettings _Settings;

LRESULT CSettingsNextDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CSettingsNextDlg>::OnInitDialog(uMsg, wParam, lParam, bHandled);

	const CString backupText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.create_backup_file",
		L"Create a backup copy (.bak) when saving an existing file");
	const CString savingText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.saving",
		L"");
	if (!savingText.IsEmpty())
		::SetDlgItemText(m_hWnd, IDC_FBE_NEXT_SAVING_GROUP, savingText);
	::SetDlgItemText(m_hWnd, IDC_CREATE_BACKUP_FILE, backupText);
	::SendMessage(GetDlgItem(IDC_CREATE_BACKUP_FILE), BM_SETCHECK,
		_Settings.GetCreateBackupFile() ? BST_CHECKED : BST_UNCHECKED, 0);
	return 1;
}

LRESULT CSettingsNextDlg::OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	_Settings.SetCreateBackupFile(IsDlgButtonChecked(IDC_CREATE_BACKUP_FILE) == BST_CHECKED);
	EndDialog(wID);
	return 0;
}

LRESULT CSettingsNextDlg::OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	EndDialog(wID);
	return 0;
}
