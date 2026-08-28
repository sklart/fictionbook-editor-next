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
	m_tooltips.Initialize(m_hWnd);
	m_tooltips.Add(m_scriptsFolder, L"fbe.settings.tooltip.advanced.scripts_folder", L"Folder from which FictionBook Editor Next loads user scripts.");
	m_tooltips.Add(m_defaultScriptsFolder, L"fbe.settings.tooltip.advanced.default_scripts_folder", L"Uses the standard Scripts folder for this installation or portable copy.");
	m_tooltips.Add(m_selectScriptsFolder, L"fbe.settings.tooltip.advanced.browse", L"Choose a folder containing user scripts.");
	m_tooltips.Add(m_fastMode, L"fbe.settings.tooltip.advanced.fast_mode", L"Uses the application's reduced-feature fast mode.");
	m_initialScriptsFolder = _Settings.GetResolvedScriptsFolder();
	m_defaultScriptsFolder.SetCheck(_Settings.IsDefaultScriptsFolder());
	m_scriptsFolder.SetWindowText(_Settings.GetScriptsFolderStored());
	m_scriptsFolder.SetReadOnly(_Settings.IsDefaultScriptsFolder());
	m_selectScriptsFolder.EnableWindow(!_Settings.IsDefaultScriptsFolder());
	m_scriptsSwitched = _Settings.IsDefaultScriptsFolder();
	m_fastMode.SetCheck(_Settings.FastMode());
	return 1;
}
LRESULT CSettingsAdvancedPage::OnClickedOK(WORD, WORD, HWND, BOOL&)
{
	if(Validate()) Commit();
	return 0;
}
bool CSettingsAdvancedPage::Validate()
{
	if(m_scriptsSwitched)
		return true;
	CString folder; m_scriptsFolder.GetWindowText(folder); folder = ResolveScriptsFolderPath(folder);
	const DWORD attributes = folder.IsEmpty() ? INVALID_FILE_ATTRIBUTES : ::GetFileAttributes(folder);
	if(attributes == INVALID_FILE_ATTRIBUTES)
	{
		MessageBeep(MB_ICONERROR);
		const DWORD error = ::GetLastError();
		::MessageBox(m_hWnd, FbeLoadRuntimeStringByKey(error == ERROR_ACCESS_DENIED ? L"fbe.settings.advanced.folder_access_failed" : L"fbe.settings.advanced.folder_missing", error == ERROR_ACCESS_DENIED ? L"The scripts folder cannot be accessed." : L"The scripts folder does not exist."), FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONERROR);
		m_scriptsFolder.SetFocus();
		return false;
	}
	if(!(attributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		MessageBeep(MB_ICONERROR);
		::MessageBox(m_hWnd, FbeLoadRuntimeStringByKey(L"fbe.settings.advanced.folder_not_directory", L"The selected scripts path is not a folder."), FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONERROR);
		m_scriptsFolder.SetFocus();
		return false;
	}
	return true;
}
void CSettingsAdvancedPage::Commit()
{
	CString folder; m_scriptsFolder.GetWindowText(folder); folder = NormalizeScriptsFolderStoredPath(folder);
	_Settings.SetScriptsFolder(folder.IsEmpty() ? _Settings.GetDefaultScriptsFolder() : folder, true);
	if(m_initialScriptsFolder.CompareNoCase(_Settings.GetResolvedScriptsFolder()) != 0) _Settings.SetNeedRestart();
	_Settings.SetFastMode(m_fastMode.GetCheck() == BST_CHECKED);
}
LRESULT CSettingsAdvancedPage::OnClickedCancel(WORD, WORD, HWND, BOOL&) { return 0; }
bool CSettingsAdvancedPage::CancelChanges() { return true; }
LRESULT CSettingsAdvancedPage::OnDefaultScriptsFolder(WORD, WORD, HWND, BOOL&)
{
	if(!m_scriptsSwitched) { m_scriptsFolder.SetWindowText(_Settings.GetDefaultScriptsFolder()); m_scriptsFolder.SetReadOnly(true); m_selectScriptsFolder.EnableWindow(false); }
	else { m_scriptsFolder.SetReadOnly(false); m_selectScriptsFolder.EnableWindow(true); }
	m_scriptsSwitched = !m_scriptsSwitched; return 0;
}
LRESULT CSettingsAdvancedPage::OnSelectScriptsFolder(WORD, WORD, HWND, BOOL&)
{
	CFolderDialog dialog(NULL, FbeLoadCString(IDS_CHOOSE_SCRIPTS_FLD), BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS);
	if(dialog.DoModal(*this) == IDOK) { CString folder(dialog.m_szFolderPath); m_scriptsFolder.SetWindowText(folder); }
	return 0;
}
