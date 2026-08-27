#include "stdafx.h"
#include "SettingsGeneralPage.h"
#include "Settings.h"
#include "RuntimeLocalization.h"
#include "res1.h"

extern CSettings _Settings;

namespace
{
struct InterfaceLanguageChoice
{
	DWORD languageId;
	UINT stringId;
	LPCWSTR localeName;
};

const InterfaceLanguageChoice kInterfaceLanguages[] = {
	{ FBE_INTERFACE_LANGUAGE_AUTO, IDS_LANG_SYSTEM_DEFAULT, NULL },
	{ FBE_INTERFACE_LANGUAGE_ENGLISH, IDS_LANG_ENGLISH, L"en-US" },
	{ FBE_INTERFACE_LANGUAGE_RUSSIAN, IDS_LANG_RUSSIAN, L"ru-RU" },
	{ FBE_INTERFACE_LANGUAGE_UKRAINIAN, IDS_LANG_UKRAINIAN, L"uk-UA" },
	{ FBE_INTERFACE_LANGUAGE_GERMAN, IDS_LANG_GERMAN, L"de-DE" },
	{ FBE_INTERFACE_LANGUAGE_FRENCH, IDS_LANG_FRENCH, L"fr-FR" },
	{ FBE_INTERFACE_LANGUAGE_SPANISH, IDS_LANG_SPANISH, L"es-ES" },
	{ FBE_INTERFACE_LANGUAGE_ITALIAN, IDS_LANG_ITALIAN, L"it-IT" },
	{ FBE_INTERFACE_LANGUAGE_POLISH, IDS_LANG_POLISH, L"pl-PL" },
	{ FBE_INTERFACE_LANGUAGE_PORTUGUESE, IDS_LANG_PORTUGUESE, L"pt-PT" },
	{ FBE_INTERFACE_LANGUAGE_DUTCH, IDS_LANG_DUTCH, L"nl-NL" },
	{ FBE_INTERFACE_LANGUAGE_CZECH, IDS_LANG_CZECH, L"cs-CZ" },
	{ FBE_INTERFACE_LANGUAGE_BULGARIAN, IDS_LANG_BULGARIAN, L"bg-BG" },
};

void SetText(HWND window, int controlId, LPCWSTR key, LPCWSTR fallback)
{
	::SetDlgItemText(window, controlId, FbeLoadRuntimeStringByKey(key, fallback));
}
}

LRESULT CSettingsGeneralPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_SETTINGS_GENERAL);
	m_language = GetDlgItem(IDC_LANG);
	m_genreCatalog = GetDlgItem(IDC_GENRE_CATALOG);
	m_defaultEncoding = GetDlgItem(IDC_DEFAULT_ENC);
	m_keepEncoding = GetDlgItem(IDC_KEEP);
	m_restorePosition = GetDlgItem(IDC_RESTORE_POS);
	m_updateChannel = GetDlgItem(IDC_UPDATE_CHANNEL);

	SetText(m_hWnd, IDC_OPTIONS_INTERFACE_GROUP, L"fbe.dialog.idd_options.interface", L"Interface");
	SetText(m_hWnd, IDC_OPTIONS_LANGUAGE_LABEL, L"fbe.dialog.idd_options.language", L"Language:");
	SetText(m_hWnd, IDC_OPTIONS_GENRE_CATALOG_LABEL, L"fbe.dialog.idd_options.genre_catalog", L"Genre catalog:");
	SetText(m_hWnd, IDC_SETTINGS_OTHER_OPEN_GROUP, L"fbe.dialog.idd_setting_other.open_file", L"Open file");
	SetText(m_hWnd, IDC_RESTORE_POS, L"fbe.dialog.idd_setting_other.restore_position", L"Restore position");
	SetText(m_hWnd, IDC_SETTINGS_OTHER_SAVE_GROUP, L"fbe.dialog.idd_setting_other.save_file", L"Saving");
	SetText(m_hWnd, IDC_SETTINGS_OTHER_ENCODING, L"fbe.dialog.idd_setting_other.default_encoding", L"Default encoding:");
	SetText(m_hWnd, IDC_KEEP, L"fbe.dialog.idd_setting_other.keep_manual", L"Keep original encoding");
	SetText(m_hWnd, IDC_FBE_NEXT_SAVING_GROUP, L"fbe.dialog.idd_setting_next.saving", L"Backup");
	SetText(m_hWnd, IDC_CREATE_BACKUP_FILE, L"fbe.dialog.idd_setting_next.create_backup_file", L"Create a backup copy (.bak) when saving an existing file");
	SetText(m_hWnd, IDC_FBE_NEXT_WINDOW_TITLE_GROUP, L"fbe.dialog.idd_setting_next.window_title", L"Window title");
	SetText(m_hWnd, IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE, L"fbe.dialog.idd_setting_next.show_full_path_in_window_title", L"Show full file path in the window title");
	SetText(m_hWnd, IDC_FBE_NEXT_UPDATES_GROUP, L"fbe.dialog.idd_setting_next.updates", L"Updates");
	SetText(m_hWnd, IDC_UPDATE_CHANNEL_LABEL, L"fbe.dialog.idd_setting_next.update_channel", L"Update channel:");

	m_language.SetDroppedWidth(320);
	const DWORD currentLanguage = _Settings.GetInterfaceLanguageID();
	int selectedLanguage = 0;
	wchar_t text[MAX_LOAD_STRING + 1];
	for(int i = 0; i < _countof(kInterfaceLanguages); ++i)
	{
		if(kInterfaceLanguages[i].localeName && !FbeIsRuntimeLocaleInstalled(kInterfaceLanguages[i].localeName))
			continue;
		if(FbeLoadString(_Module.GetResourceInstance(), kInterfaceLanguages[i].stringId, text, MAX_LOAD_STRING))
		{
			const int item = m_language.AddString(text);
			m_language.SetItemData(item, kInterfaceLanguages[i].languageId);
			if(kInterfaceLanguages[i].languageId == currentLanguage) selectedLanguage = item;
		}
	}
	m_language.SetCurSel(selectedLanguage);
	m_genreCatalog.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.genre_catalog.standard", L"Standard"));
	m_genreCatalog.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.genre_catalog.librusec", L"Librusec"));
	m_genreCatalog.SetCurSel(_Settings.GetGenreCatalog() == GenreCatalog::Librusec ? 1 : 0);

	if(FbeLoadString(_Module.GetResourceInstance(), IDS_ENCODINGS, text, _countof(text)))
	{
		for(TCHAR* encoding = text; *encoding; )
		{
			size_t length = _tcscspn(encoding, _T(","));
			if(encoding[length]) encoding[length++] = _T('\0');
			if(*encoding) m_defaultEncoding.AddString(encoding);
			encoding += length;
		}
	}
	m_defaultEncoding.SelectString(0, _Settings.GetDefaultEncoding());
	m_keepEncoding.SetCheck(_Settings.KeepEncoding() ? BST_CHECKED : BST_UNCHECKED);
	m_restorePosition.SetCheck(_Settings.RestoreFilePosition() ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CREATE_BACKUP_FILE, _Settings.GetCreateBackupFile() ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE, _Settings.GetShowFullPathInWindowTitle() ? BST_CHECKED : BST_UNCHECKED);
	m_updateChannel.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.update_channel.stable", L"Stable versions (recommended)"));
	m_updateChannel.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.update_channel.prerelease", L"Prerelease versions (Beta / RC)"));
	m_updateChannel.SetCurSel(_Settings.GetUpdateChannel() == UpdateChannel::Prerelease ? 1 : 0);
	return 1;
}

LRESULT CSettingsGeneralPage::OnClickedOK(WORD, WORD, HWND, BOOL&)
{
	CString encoding;
	m_defaultEncoding.GetLBText(m_defaultEncoding.GetCurSel(), encoding);
	_Settings.SetDefaultEncoding(encoding);
	_Settings.SetKeepEncoding(m_keepEncoding.GetCheck() == BST_CHECKED);
	_Settings.SetRestoreFilePosition(m_restorePosition.GetCheck() == BST_CHECKED);
	_Settings.SetCreateBackupFile(IsDlgButtonChecked(IDC_CREATE_BACKUP_FILE) == BST_CHECKED);
	_Settings.SetShowFullPathInWindowTitle(IsDlgButtonChecked(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE) == BST_CHECKED);
	_Settings.SetUpdateChannel(m_updateChannel.GetCurSel() == 1 ? UpdateChannel::Prerelease : UpdateChannel::Stable);
	_Settings.SetGenreCatalog(m_genreCatalog.GetCurSel() == 1 ? GenreCatalog::Librusec : GenreCatalog::Standard);
	const int language = m_language.GetCurSel();
	if(language >= 0)
	{
		const DWORD selectedLanguage = static_cast<DWORD>(m_language.GetItemData(language));
		if(selectedLanguage != _Settings.GetInterfaceLanguageID())
		{
			_Settings.SetInterfaceLanguage(selectedLanguage);
			FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName());
			FbeResetRuntimeLocalization();
		}
	}
	return 0;
}

LRESULT CSettingsGeneralPage::OnClickedCancel(WORD, WORD, HWND, BOOL&)
{
	return 0;
}
