#include "stdafx.h"
#include "SettingsSpellingPage.h"
#include "Settings.h"
#include "RuntimeLocalization.h"
#include "..\\common\\ModernFileDialog.h"
#include "StartupTrace.h"

extern CSettings _Settings;

namespace { void SetText(HWND window, int id, LPCWSTR key, LPCWSTR fallback) { ::SetDlgItemText(window, id, FbeLoadRuntimeStringByKey(key, fallback)); } }
namespace { void DictionaryError(HWND owner, LPCWSTR key, LPCWSTR fallback) { MessageBeep(MB_ICONERROR); ::MessageBox(owner, FbeLoadRuntimeStringByKey(key, fallback), FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONERROR); } }

LRESULT CSettingsSpellingPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_SETTINGS_SPELLING);
	m_enabled = GetDlgItem(IDC_USESPELLCHECKER); m_highlight = GetDlgItem(IDC_BACKGROUNDSPELLCHECK); m_dictionary = GetDlgItem(IDC_CUSTOM_DICT);
	m_tooltips.Initialize(m_hWnd);
	m_tooltips.Add(m_enabled, L"fbe.settings.tooltip.spelling.enabled", L"Enables spell checking.");
	m_tooltips.Add(m_highlight, L"fbe.settings.tooltip.spelling.background", L"Marks possible spelling errors while editing.");
	m_tooltips.Add(m_dictionary, L"fbe.settings.tooltip.spelling.custom_dictionary", L"File of additional words that should be treated as correct.");
	m_tooltips.Add(GetDlgItem(IDC_DICTPATH), L"fbe.settings.tooltip.spelling.browse", L"Choose a custom dictionary file.");
	m_tooltips.Add(GetDlgItem(IDS_SPELL_CUSTOM_DICT), L"fbe.settings.tooltip.spelling.custom_dictionary", L"File of additional words that should be treated as correct.");
	m_tooltips.AddDisabledControlArea(m_highlight, L"fbe.settings.tooltip.spelling.background", L"Marks possible spelling errors while editing.");
	m_tooltips.AddDisabledControlArea(m_dictionary, L"fbe.settings.tooltip.spelling.custom_dictionary", L"File of additional words that should be treated as correct.");
	m_tooltips.AddDisabledControlArea(GetDlgItem(IDC_DICTPATH), L"fbe.settings.tooltip.spelling.browse", L"Choose a custom dictionary file.");
	SetText(m_hWnd, IDC_OPTIONS_SPELLCHECK_GROUP, L"fbe.dialog.idd_options.spell_checking", L"Spelling");
	SetText(m_hWnd, IDC_USESPELLCHECKER, L"fbe.dialog.idd_options.use_spellchecker", L"Use spellchecker");
	SetText(m_hWnd, IDC_BACKGROUNDSPELLCHECK, L"fbe.dialog.idd_options.background_spell_check", L"Highlight misspelled words");
	SetText(m_hWnd, IDS_SPELL_CUSTOM_DICT, L"fbe.dialog.idd_options.custom_dict", L"Custom dictionary:");
	SetText(m_hWnd, IDC_DICTPATH, L"fbe.dialog.idd_options.dict_browse", L"Browse...");
	m_enabled.SetCheck(_Settings.GetUseSpellChecker() ? BST_CHECKED : BST_UNCHECKED);
	m_highlight.SetCheck(_Settings.GetHighlightMisspells() ? BST_CHECKED : BST_UNCHECKED);
	m_dictionary.SetWindowText(_Settings.GetCustomDict()); UpdateDictionaryTooltip(); UpdateDependencies(); return 1;
}
void CSettingsSpellingPage::UpdateDependencies() { const BOOL enabled = m_enabled.GetCheck() == BST_CHECKED; m_highlight.EnableWindow(enabled); m_dictionary.EnableWindow(enabled); GetDlgItem(IDC_DICTPATH).EnableWindow(enabled); }
LRESULT CSettingsSpellingPage::OnSpellcheckerChanged(WORD, WORD, HWND, BOOL&) { UpdateDependencies(); return 0; }
LRESULT CSettingsSpellingPage::OnClickedOK(WORD, WORD, HWND, BOOL&) { if(Validate()) Commit(); return 0; }
LRESULT CSettingsSpellingPage::OnClickedCancel(WORD, WORD, HWND, BOOL&) { return CancelChanges() ? 0 : 1; }
bool CSettingsSpellingPage::Validate()
{
	if(m_enabled.GetCheck() != BST_CHECKED)
		return true;
	CString storedPath; m_dictionary.GetWindowText(storedPath); storedPath.Trim();
	if(storedPath.IsEmpty())
		return true;
	const CString path = U::ResolveUserDataFile(storedPath);
	const DWORD attributes = ::GetFileAttributes(path);
	if(attributes == INVALID_FILE_ATTRIBUTES)
	{
		CString directory(path); ::PathRemoveFileSpec(directory.GetBuffer()); directory.ReleaseBuffer();
		const DWORD directoryAttributes = ::GetFileAttributes(directory);
		if(directoryAttributes != INVALID_FILE_ATTRIBUTES && (directoryAttributes & FILE_ATTRIBUTE_DIRECTORY)) return true;
		DictionaryError(m_hWnd, L"fbe.settings.spelling.parent_missing", L"The custom dictionary parent directory does not exist."); m_dictionary.SetFocus(); return false;
	}
	if(attributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		DictionaryError(m_hWnd, L"fbe.settings.spelling.path_is_directory", L"The selected custom dictionary path is a directory."); m_dictionary.SetFocus(); return false;
	}
	if(attributes & FILE_ATTRIBUTE_READONLY) { DictionaryError(m_hWnd, L"fbe.settings.spelling.read_only", L"The custom dictionary is read-only."); m_dictionary.SetFocus(); return false; }
	HANDLE file = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) { DictionaryError(m_hWnd, L"fbe.settings.spelling.read_failed", L"The custom dictionary cannot be read."); m_dictionary.SetFocus(); return false; }
	::CloseHandle(file);
	file = ::CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) { DictionaryError(m_hWnd, L"fbe.settings.spelling.write_failed", L"The custom dictionary cannot be written."); m_dictionary.SetFocus(); return false; }
	::CloseHandle(file);
	return true;
}
void CSettingsSpellingPage::Commit() { _Settings.SetUseSpellChecker(m_enabled.GetCheck() == BST_CHECKED); _Settings.SetHighlightMisspells(m_highlight.GetCheck() == BST_CHECKED); CString path; m_dictionary.GetWindowText(path); path.Trim(); _Settings.SetCustomDict(path); }
bool CSettingsSpellingPage::CancelChanges() { return true; }
LRESULT CSettingsSpellingPage::OnBrowseDictionary(WORD, WORD, HWND, BOOL&)
{
	CString current; m_dictionary.GetWindowText(current); current.Trim();
	const std::wstring dicCaption = std::wstring(FbeLoadRuntimeStringByKey(L"fbe.settings.spelling.filter.dictionary", L"Dictionaries (*.dic)"));
	const std::wstring allCaption = std::wstring(FbeLoadRuntimeStringByKey(L"fbe.settings.spelling.filter.all_files", L"All files (*.*)"));
	const COMDLG_FILTERSPEC filters[] = { { dicCaption.c_str(), L"*.dic" }, { allCaption.c_str(), L"*.*" } };
	ModernFileDialog::Request request;
	request.title = FbeLoadRuntimeStringByKey(L"fbe.settings.spelling.browse_title", L"Choose custom dictionary").GetString();
	request.defaultExtension = L"dic"; request.fileMustExist = true; request.pathMustExist = true;
	request.filters = filters; request.filterCount = _countof(filters); request.filterIndex = 1;
	request.initialFileName = current.GetString();
	const ModernFileDialog::Result result = ModernFileDialog::Show(m_hWnd, request);
	if(result.outcome == ModernFileDialog::Outcome::Accepted && !result.paths.empty()) { m_dictionary.SetWindowText(result.paths.front().c_str()); UpdateDictionaryTooltip(); }
	else if(result.outcome == ModernFileDialog::Outcome::Failed) StartupTrace::HResult(L"file-dialog", L"FD106", result.error, L"Browse custom dictionary");
	return 0;
}
LRESULT CSettingsSpellingPage::OnDictionaryChanged(WORD, WORD, HWND, BOOL&)
{
	UpdateDictionaryTooltip();
	return 0;
}
void CSettingsSpellingPage::UpdateDictionaryTooltip()
{
	CString stored; m_dictionary.GetWindowText(stored); stored.Trim();
	const CString resolved = U::ResolveUserDataFile(stored);
	CString text;
	if(!stored.IsEmpty() && stored.CompareNoCase(resolved) != 0)
		text.Format(FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.spelling.custom_dictionary_dynamic", L"Stored: %s\nActual path: %s"), stored.GetString(), resolved.GetString());
	else
		text.Format(FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.spelling.custom_dictionary_absolute", L"Path: %s"), resolved.GetString());
	m_tooltips.UpdateText(m_dictionary, text);
}
