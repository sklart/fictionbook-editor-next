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
	const CString windowTitleText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.window_title",
		L"");
	const CString showFullPathText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.show_full_path_in_window_title",
		L"Show full file path in the window title");
	if (!savingText.IsEmpty())
		::SetDlgItemText(m_hWnd, IDC_FBE_NEXT_SAVING_GROUP, savingText);
	if (!windowTitleText.IsEmpty())
		::SetDlgItemText(m_hWnd, IDC_FBE_NEXT_WINDOW_TITLE_GROUP, windowTitleText);
	::SetDlgItemText(m_hWnd, IDC_CREATE_BACKUP_FILE, backupText);
	::SetDlgItemText(m_hWnd, IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE, showFullPathText);
	::SendMessage(GetDlgItem(IDC_CREATE_BACKUP_FILE), BM_SETCHECK,
		_Settings.GetCreateBackupFile() ? BST_CHECKED : BST_UNCHECKED, 0);
	::SendMessage(GetDlgItem(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE), BM_SETCHECK,
		_Settings.GetShowFullPathInWindowTitle() ? BST_CHECKED : BST_UNCHECKED, 0);
	return 1;
}

LRESULT CSettingsNextDlg::OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	_Settings.SetCreateBackupFile(IsDlgButtonChecked(IDC_CREATE_BACKUP_FILE) == BST_CHECKED);
	_Settings.SetShowFullPathInWindowTitle(IsDlgButtonChecked(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE) == BST_CHECKED);
	EndDialog(wID);
	return 0;
}

LRESULT CSettingsNextDlg::OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	EndDialog(wID);
	return 0;
}
