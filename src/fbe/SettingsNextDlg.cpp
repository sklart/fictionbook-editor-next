// SettingsNextDlg.cpp : реализация вкладки параметров FBE Next.

#include "stdafx.h"
#include "SettingsNextDlg.h"
#include "Settings.h"
#include "RuntimeLocalization.h"

extern CSettings _Settings;

struct SourceColorPaletteChoice
{
	DWORD palette;
	LPCWSTR key;
	LPCWSTR fallback;
};

static const SourceColorPaletteChoice kSourceColorPalettes[] = {
	{ XML_SRC_COLOR_PALETTE_CLASSIC, L"fbe.dialog.idd_setting_next.source_palette.classic", L"Classic" },
	{ XML_SRC_COLOR_PALETTE_CONTRAST, L"fbe.dialog.idd_setting_next.source_palette.contrast", L"Contrast" },
	{ XML_SRC_COLOR_PALETTE_DARK, L"fbe.dialog.idd_setting_next.source_palette.dark", L"Dark" },
};

LRESULT CSettingsNextDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CSettingsNextDlg>::OnInitDialog(uMsg, wParam, lParam, bHandled);
	m_source_palette = GetDlgItem(IDC_OPTIONS_SOURCE_PALETTE);
	m_source_palette.SetDroppedWidth(190);

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
	const CString paletteText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.source_palette", L"Color scheme:");
	const CString sourceCodeText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.source_code", L"Source code");
	if (!savingText.IsEmpty())
		::SetDlgItemText(m_hWnd, IDC_FBE_NEXT_SAVING_GROUP, savingText);
	if (!windowTitleText.IsEmpty())
		::SetDlgItemText(m_hWnd, IDC_FBE_NEXT_WINDOW_TITLE_GROUP, windowTitleText);
	if (!sourceCodeText.IsEmpty())
		::SetDlgItemText(m_hWnd, IDC_FBE_NEXT_SOURCE_CODE_GROUP, sourceCodeText);
	::SetDlgItemText(m_hWnd, IDC_CREATE_BACKUP_FILE, backupText);
	::SetDlgItemText(m_hWnd, IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE, showFullPathText);
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_PALETTE_LABEL, paletteText);
	::SendMessage(GetDlgItem(IDC_CREATE_BACKUP_FILE), BM_SETCHECK,
		_Settings.GetCreateBackupFile() ? BST_CHECKED : BST_UNCHECKED, 0);
	::SendMessage(GetDlgItem(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE), BM_SETCHECK,
		_Settings.GetShowFullPathInWindowTitle() ? BST_CHECKED : BST_UNCHECKED, 0);

	const DWORD currentPalette = _Settings.GetXmlSrcColorPalette();
	int selectedPaletteIndex = 0;
	for(int i = 0; i < _countof(kSourceColorPalettes); ++i)
	{
		const int item = m_source_palette.AddString(FbeLoadRuntimeStringByKey(
			kSourceColorPalettes[i].key, kSourceColorPalettes[i].fallback));
		if(item >= 0)
		{
			m_source_palette.SetItemData(item, kSourceColorPalettes[i].palette);
			if(kSourceColorPalettes[i].palette == currentPalette)
				selectedPaletteIndex = item;
		}
	}
	m_source_palette.SetCurSel(selectedPaletteIndex);
	return 1;
}

LRESULT CSettingsNextDlg::OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	_Settings.SetCreateBackupFile(IsDlgButtonChecked(IDC_CREATE_BACKUP_FILE) == BST_CHECKED);
	_Settings.SetShowFullPathInWindowTitle(IsDlgButtonChecked(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE) == BST_CHECKED);
	const int selectedPaletteIndex = m_source_palette.GetCurSel();
	if(selectedPaletteIndex >= 0)
		_Settings.SetXmlSrcColorPalette(static_cast<DWORD>(m_source_palette.GetItemData(selectedPaletteIndex)));
	EndDialog(wID);
	return 0;
}

LRESULT CSettingsNextDlg::OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	EndDialog(wID);
	return 0;
}
