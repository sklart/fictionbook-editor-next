#include "stdafx.h"
#include "SettingsSpellingPage.h"
#include "Settings.h"
#include "RuntimeLocalization.h"

extern CSettings _Settings;

namespace { void SetText(HWND window, int id, LPCWSTR key, LPCWSTR fallback) { ::SetDlgItemText(window, id, FbeLoadRuntimeStringByKey(key, fallback)); } }

LRESULT CSettingsSpellingPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_SETTINGS_SPELLING);
	m_enabled = GetDlgItem(IDC_USESPELLCHECKER); m_highlight = GetDlgItem(IDC_BACKGROUNDSPELLCHECK); m_dictionary = GetDlgItem(IDC_CUSTOM_DICT);
	SetText(m_hWnd, IDC_OPTIONS_SPELLCHECK_GROUP, L"fbe.dialog.idd_options.spell_checking", L"Spelling");
	SetText(m_hWnd, IDC_USESPELLCHECKER, L"fbe.dialog.idd_options.use_spellchecker", L"Use spellchecker");
	SetText(m_hWnd, IDC_BACKGROUNDSPELLCHECK, L"fbe.dialog.idd_options.background_spell_check", L"Highlight misspelled words");
	SetText(m_hWnd, IDS_SPELL_CUSTOM_DICT, L"fbe.dialog.idd_options.custom_dict", L"Custom dictionary:");
	SetText(m_hWnd, IDC_DICTPATH, L"fbe.dialog.idd_options.dict_browse", L"Browse...");
	m_enabled.SetCheck(_Settings.GetUseSpellChecker() ? BST_CHECKED : BST_UNCHECKED);
	m_highlight.SetCheck(_Settings.GetHighlightMisspells() ? BST_CHECKED : BST_UNCHECKED);
	m_dictionary.SetWindowText(_Settings.GetCustomDict()); UpdateDependencies(); return 1;
}
void CSettingsSpellingPage::UpdateDependencies() { const BOOL enabled = m_enabled.GetCheck() == BST_CHECKED; m_highlight.EnableWindow(enabled); m_dictionary.EnableWindow(enabled); GetDlgItem(IDC_DICTPATH).EnableWindow(enabled); }
LRESULT CSettingsSpellingPage::OnSpellcheckerChanged(WORD, WORD, HWND, BOOL&) { UpdateDependencies(); return 0; }
LRESULT CSettingsSpellingPage::OnClickedOK(WORD, WORD, HWND, BOOL&) { _Settings.SetUseSpellChecker(m_enabled.GetCheck() == BST_CHECKED); _Settings.SetHighlightMisspells(m_highlight.GetCheck() == BST_CHECKED); CString path; m_dictionary.GetWindowText(path); _Settings.SetCustomDict(path); return 0; }
LRESULT CSettingsSpellingPage::OnClickedCancel(WORD, WORD, HWND, BOOL&) { return 0; }
LRESULT CSettingsSpellingPage::OnBrowseDictionary(WORD, WORD, HWND, BOOL&)
{
	wchar_t path[_MAX_PATH] = {}; m_dictionary.GetWindowText(path, _countof(path));
	OPENFILENAME dialog = {}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = m_hWnd; dialog.hInstance = _Module.m_hInst; dialog.lpstrDefExt = L"dic"; dialog.lpstrFilter = L"Dictionaries (*.dic)\0*.dic\0All files (*.*)\0*.*\0\0"; dialog.lpstrFile = path; dialog.nMaxFile = _countof(path); dialog.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY;
	if(GetOpenFileName(&dialog)) m_dictionary.SetWindowText(path); return 0;
}
