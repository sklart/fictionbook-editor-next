#include "stdafx.h"
#include "SettingsAdvancedPage.h"
#include "Settings.h"
#include "utils.h"
#include "RuntimeLocalization.h"

extern CSettings _Settings;

LRESULT CSettingsAdvancedPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_SETTINGS_ADVANCED);
	::SetDlgItemText(m_hWnd, IDC_SETTINGS_OTHER_SCRIPTS, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_other.scripts_folder", L"Scripts folder"));
	::SetDlgItemText(m_hWnd, IDC_DEFAULT_SCRIPTS_FOLDER, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_other.default_scripts_folder", L"Default folder"));
	::SetDlgItemText(m_hWnd, IDC_SELECT_SCRIPTS_FOLDER_BUTTON, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_other.browse", L"..."));
	::SetDlgItemText(m_hWnd, IDC_FAST_MODE, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.fast_mode", L"Fast mode"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_INTERFACE_GROUP, FbeLoadRuntimeStringByKey(L"fbe.settings.advanced.group", L"Advanced"));
	m_defaultScriptsFolder = GetDlgItem(IDC_DEFAULT_SCRIPTS_FOLDER);
	m_scriptsFolder = GetDlgItem(IDC_SCRIPTS_FOLDER_PATH);
	m_selectScriptsFolder = GetDlgItem(IDC_SELECT_SCRIPTS_FOLDER_BUTTON);
	m_fastMode = GetDlgItem(IDC_FAST_MODE);
	_Settings.m_initial_scripts_folder = _Settings.GetScriptsFolder();
	m_defaultScriptsFolder.SetCheck(_Settings.IsDefaultScriptsFolder());
	m_scriptsFolder.SetWindowText(_Settings.m_initial_scripts_folder);
	m_scriptsFolder.SetReadOnly(_Settings.IsDefaultScriptsFolder());
	m_selectScriptsFolder.EnableWindow(!_Settings.IsDefaultScriptsFolder());
	m_scriptsSwitched = _Settings.IsDefaultScriptsFolder();
	m_fastMode.SetCheck(_Settings.FastMode());
	return 1;
}
LRESULT CSettingsAdvancedPage::OnClickedOK(WORD, WORD, HWND, BOOL&)
{
	CString folder; m_scriptsFolder.GetWindowText(folder);
	_Settings.SetScriptsFolder(folder.IsEmpty() ? _Settings.GetDefaultScriptsFolder() : folder, true);
	if(_Settings.m_initial_scripts_folder != _Settings.GetScriptsFolder()) _Settings.SetNeedRestart();
	_Settings.SetFastMode(m_fastMode.GetCheck() == BST_CHECKED);
	return 0;
}
LRESULT CSettingsAdvancedPage::OnClickedCancel(WORD, WORD, HWND, BOOL&) { return 0; }
LRESULT CSettingsAdvancedPage::OnDefaultScriptsFolder(WORD, WORD, HWND, BOOL&)
{
	if(!m_scriptsSwitched) { m_scriptsFolder.SetWindowText(_Settings.GetDefaultScriptsFolder()); _Settings.SetScriptsFolder(_Settings.GetDefaultScriptsFolder(), true); m_scriptsFolder.SetReadOnly(true); m_selectScriptsFolder.EnableWindow(false); }
	else { m_scriptsFolder.SetReadOnly(false); m_selectScriptsFolder.EnableWindow(true); }
	m_scriptsSwitched = !m_scriptsSwitched; return 0;
}
LRESULT CSettingsAdvancedPage::OnSelectScriptsFolder(WORD, WORD, HWND, BOOL&)
{
	CFolderDialog dialog(NULL, FbeLoadCString(IDS_CHOOSE_SCRIPTS_FLD), BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS);
	if(dialog.DoModal(*this) == IDOK) { CString folder(dialog.m_szFolderPath); if(folder.ReverseFind(L'\\') != folder.GetLength() - 1) folder.Append(L"\\"); m_scriptsFolder.SetWindowText(folder); }
	return 0;
}
