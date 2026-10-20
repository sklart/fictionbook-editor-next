#include "stdafx.h"
#include "SettingsGeneralPage.h"
#include "..\\..\\Settings.h"
#include "..\\..\\ThemeManager.h"
#include "..\\..\\UiMetrics.h"
#include "..\\..\\RuntimeLocalization.h"
#include "..\\..\\res1.h"
#include <initializer_list>

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
	m_originalTheme = _Settings.GetInterfaceTheme();
	m_interfaceTheme = GetDlgItem(IDC_INTERFACE_THEME);
	m_scriptIconSize = GetDlgItem(IDC_SCRIPT_ICON_SIZE);
	m_genreCatalog = GetDlgItem(IDC_GENRE_CATALOG);
	m_defaultEncoding = GetDlgItem(IDC_DEFAULT_ENC);
	m_keepEncoding = GetDlgItem(IDC_KEEP);
	m_restorePosition = GetDlgItem(IDC_RESTORE_POS);
	m_updateChannel = GetDlgItem(IDC_UPDATE_CHANNEL);
	m_tooltips.Initialize(m_hWnd);
	m_tooltips.Add(m_language, L"fbe.settings.tooltip.general.language", L"Interface language. System default uses the Windows language.");
	m_tooltips.Add(m_interfaceTheme, L"fbe.settings.tooltip.general.interface_theme", L"Automatic follows the Windows app theme.");
	m_tooltips.Add(m_genreCatalog, L"fbe.settings.tooltip.general.genre_catalog", L"Selects genres available when editing book metadata.");
	m_tooltips.Add(m_restorePosition, L"fbe.settings.tooltip.general.restore_position", L"Restores the saved reading or editing position when reopening a document.");
	m_tooltips.Add(m_defaultEncoding, L"fbe.settings.tooltip.general.default_encoding", L"Encoding used when saving if the original encoding is not kept.");
	m_tooltips.Add(m_keepEncoding, L"fbe.settings.tooltip.general.keep_encoding", L"Keeps the encoding in which the document was opened whenever possible.");
	m_tooltips.Add(m_updateChannel, L"fbe.settings.tooltip.general.update_channel", L"Controls which application versions are offered by the built-in updater.");
	m_tooltips.Add(GetDlgItem(IDC_CREATE_BACKUP_FILE), L"fbe.settings.tooltip.general.backup", L"Creates a .bak copy before saving an existing file.");
	m_tooltips.Add(GetDlgItem(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE), L"fbe.settings.tooltip.general.full_path", L"Shows the complete file path in the application window title.");

	SetText(m_hWnd, IDC_OPTIONS_INTERFACE_GROUP, L"fbe.dialog.idd_options.interface", L"Interface");
	SetText(m_hWnd, IDC_OPTIONS_LANGUAGE_LABEL, L"fbe.dialog.idd_options.language", L"Language:");
	SetText(m_hWnd, IDC_INTERFACE_THEME_LABEL, L"fbe.dialog.idd_options.interface_theme", L"Interface theme:");
	SetText(m_hWnd, IDC_SCRIPT_ICON_SIZE_LABEL, L"fbe.settings.script_icon_size", L"Script icon size:");
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
	FitLocalizedLayout();

	m_language.SetDroppedWidth(320);
	const DWORD currentLanguage = _Settings.GetInterfaceLanguageID();
	int selectedLanguage = 0;
	bool currentLanguagePresent = false;
	wchar_t text[MAX_LOAD_STRING + 1];
	for(int i = 0; i < _countof(kInterfaceLanguages); ++i)
	{
		if(kInterfaceLanguages[i].localeName && !FbeIsRuntimeLocaleInstalled(kInterfaceLanguages[i].localeName))
			continue;
		if(FbeLoadString(_Module.GetResourceInstance(), kInterfaceLanguages[i].stringId, text, MAX_LOAD_STRING))
		{
			const int item = m_language.AddString(text);
			m_language.SetItemData(item, kInterfaceLanguages[i].languageId);
			if(kInterfaceLanguages[i].languageId == currentLanguage) { selectedLanguage = item; currentLanguagePresent = true; }
		}
	}
	if(!currentLanguagePresent)
	{
		for(int i = 0; i < _countof(kInterfaceLanguages); ++i)
			if(kInterfaceLanguages[i].languageId == currentLanguage && FbeLoadString(_Module.GetResourceInstance(), kInterfaceLanguages[i].stringId, text, MAX_LOAD_STRING))
			{
				selectedLanguage = m_language.AddString(text);
				m_language.SetItemData(selectedLanguage, currentLanguage);
				break;
			}
	}
	m_language.SetCurSel(selectedLanguage);
	m_interfaceTheme.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.automatic", L"Automatic \x2014 follow Windows"));
	m_interfaceTheme.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.light", L"Light"));
	m_interfaceTheme.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.dark", L"Dark"));
	m_interfaceTheme.SetCurSel(static_cast<int>(_Settings.GetInterfaceTheme()));
	for(int size : {16, 20, 24, 32}) {
		CString iconText; iconText.Format(L"%d \x00d7 %d", size, size);
		const int index = m_scriptIconSize.AddString(iconText); m_scriptIconSize.SetItemData(index, size);
		if(size == _Settings.GetScriptIconSize()) m_scriptIconSize.SetCurSel(index);
	}
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
	int encodingIndex = m_defaultEncoding.SelectString(0, _Settings.GetDefaultEncoding());
	if(encodingIndex == CB_ERR) encodingIndex = m_defaultEncoding.AddString(_Settings.GetDefaultEncoding());
	m_defaultEncoding.SetCurSel(encodingIndex);
	m_keepEncoding.SetCheck(_Settings.KeepEncoding() ? BST_CHECKED : BST_UNCHECKED);
	m_restorePosition.SetCheck(_Settings.RestoreFilePosition() ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CREATE_BACKUP_FILE, _Settings.GetCreateBackupFile() ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE, _Settings.GetShowFullPathInWindowTitle() ? BST_CHECKED : BST_UNCHECKED);
	m_updateChannel.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.update_channel.stable", L"Stable versions (recommended)"));
	m_updateChannel.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.update_channel.prerelease", L"Prerelease versions (Beta / RC)"));
	m_updateChannel.SetCurSel(_Settings.GetUpdateChannel() == UpdateChannel::Prerelease ? 1 : 0);
	return 1;
}

void CSettingsGeneralPage::FitLocalizedLayout()
{
	const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
	const int margin = UiMetrics::ScaleForDpi(8, dpi);
	const int gap = UiMetrics::ScaleForDpi(8, dpi);
	const int padding = UiMetrics::ScaleForDpi(10, dpi);
	const int rowGap = UiMetrics::ScaleForDpi(2, dpi);
	const int minimumTextHeight = UiMetrics::ScaleForDpi(26, dpi);
	auto bounds = [this](int id) {
		CRect result;
		::GetWindowRect(GetDlgItem(id), &result);
		::MapWindowPoints(NULL, m_hWnd, reinterpret_cast<POINT*>(&result), 2);
		return result;
	};
	auto place = [this](int id, const CRect& rect) {
		::SetWindowPos(GetDlgItem(id), NULL, rect.left, rect.top, rect.Width(), rect.Height(),
			SWP_NOZORDER | SWP_NOACTIVATE);
	};
	auto measure = [this](int id, int width, bool wrap) {
		HWND control = GetDlgItem(id);
		CString caption;
		const int length = ::GetWindowTextLengthW(control);
		::GetWindowTextW(control, caption.GetBuffer(length + 1), length + 1);
		caption.ReleaseBuffer();
		HDC dc = ::GetDC(control);
		if(!dc) return CSize(0, 0);
		HFONT font = reinterpret_cast<HFONT>(::SendMessage(control, WM_GETFONT, 0, 0));
		HGDIOBJ previous = font ? ::SelectObject(dc, font) : NULL;
		RECT text = { 0, 0, max(1, width), 0 };
		::DrawTextW(dc, caption, caption.GetLength(), &text,
			DT_CALCRECT | (wrap ? DT_WORDBREAK : DT_SINGLELINE));
		if(previous) ::SelectObject(dc, previous);
		::ReleaseDC(control, dc);
		return CSize(text.right - text.left, text.bottom - text.top);
	};
	int offset = 0;
	auto shift = [&](int groupId, std::initializer_list<int> children) {
		CRect group = bounds(groupId);
		group.OffsetRect(0, offset);
		place(groupId, group);
		for(int id : children) { CRect rect = bounds(id); rect.OffsetRect(0, offset); place(id, rect); }
	};
	auto grow = [&](int groupId, std::initializer_list<int> children) {
		CRect group = bounds(groupId);
		int bottom = group.bottom;
		for(int id : children) bottom = max(bottom, bounds(id).bottom + margin);
		const int growth = bottom - group.bottom;
		if(growth > 0) { group.bottom = bottom; place(groupId, group); offset += growth; }
	};
	auto fitLabelCombo = [&](int labelId, int comboId, int comboLeft, int comboRight) {
		CRect label = bounds(labelId), combo = bounds(comboId);
		label.right = comboLeft - gap;
		const CSize text = measure(labelId, label.Width(), true);
		label.bottom = label.top + max(minimumTextHeight, max(label.Height(), text.cy + padding));
		combo.left = comboLeft;
		combo.right = max(comboLeft, comboRight);
		place(labelId, label); place(comboId, combo);
	};
	auto separateRows = [&](int previousLabelId, int previousComboId, int nextLabelId, int nextComboId) {
		const int previousBottom = max(bounds(previousLabelId).bottom, bounds(previousComboId).bottom);
		CRect label = bounds(nextLabelId), combo = bounds(nextComboId);
		const int move = max(0, previousBottom + rowGap - min(label.top, combo.top));
		if(move > 0) { label.OffsetRect(0, move); combo.OffsetRect(0, move); place(nextLabelId, label); place(nextComboId, combo); }
	};
	auto fitCheckbox = [&](int groupId, int controlId, bool multiline) {
		CRect group = bounds(groupId), check = bounds(controlId);
		check.right = group.right - margin;
		const CSize text = measure(controlId, check.Width() - UiMetrics::ScaleForDpi(24, dpi), multiline);
		const int singleLineHeight = measure(controlId, 10000, false).cy;
		const int minimumHeight = (multiline ? 2 : 1) * singleLineHeight + padding;
		check.bottom = check.top + max(check.Height(), max(text.cy + padding, minimumHeight));
		place(controlId, check);
	};
	shift(IDC_OPTIONS_INTERFACE_GROUP, { IDC_OPTIONS_LANGUAGE_LABEL, IDC_LANG,
		IDC_OPTIONS_GENRE_CATALOG_LABEL, IDC_GENRE_CATALOG, IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME,
		IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE });
	const CRect interfaceGroup = bounds(IDC_OPTIONS_INTERFACE_GROUP);
	int comboLeft = bounds(IDC_LANG).left;
	for(int id : { IDC_OPTIONS_LANGUAGE_LABEL, IDC_OPTIONS_GENRE_CATALOG_LABEL,
		IDC_INTERFACE_THEME_LABEL, IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SETTINGS_OTHER_ENCODING, IDC_UPDATE_CHANNEL_LABEL })
		comboLeft = max(comboLeft, bounds(id).left + measure(id, 10000, false).cx + gap);
	comboLeft = min(comboLeft, interfaceGroup.right - margin - UiMetrics::ScaleForDpi(130, dpi));
	const int interfaceComboRight = interfaceGroup.right - margin;
	fitLabelCombo(IDC_OPTIONS_LANGUAGE_LABEL, IDC_LANG, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_OPTIONS_GENRE_CATALOG_LABEL, IDC_GENRE_CATALOG, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE, comboLeft, interfaceComboRight);
	separateRows(IDC_OPTIONS_LANGUAGE_LABEL, IDC_LANG, IDC_OPTIONS_GENRE_CATALOG_LABEL, IDC_GENRE_CATALOG);
	separateRows(IDC_OPTIONS_GENRE_CATALOG_LABEL, IDC_GENRE_CATALOG, IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME);
	separateRows(IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME, IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE);
	grow(IDC_OPTIONS_INTERFACE_GROUP, { IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE });
	shift(IDC_SETTINGS_OTHER_OPEN_GROUP, { IDC_RESTORE_POS });
	fitCheckbox(IDC_SETTINGS_OTHER_OPEN_GROUP, IDC_RESTORE_POS, false);
	grow(IDC_SETTINGS_OTHER_OPEN_GROUP, { IDC_RESTORE_POS });
	shift(IDC_SETTINGS_OTHER_SAVE_GROUP, { IDC_SETTINGS_OTHER_ENCODING, IDC_DEFAULT_ENC, IDC_KEEP });
	const CRect savingGroup = bounds(IDC_SETTINGS_OTHER_SAVE_GROUP);
	fitLabelCombo(IDC_SETTINGS_OTHER_ENCODING, IDC_DEFAULT_ENC, comboLeft, savingGroup.right - margin);
	CRect keep = bounds(IDC_KEEP);
	const int keepMove = max(0, max(bounds(IDC_SETTINGS_OTHER_ENCODING).bottom,
		bounds(IDC_DEFAULT_ENC).bottom) + rowGap - keep.top);
	if(keepMove > 0) { keep.OffsetRect(0, keepMove); place(IDC_KEEP, keep); }
	fitCheckbox(IDC_SETTINGS_OTHER_SAVE_GROUP, IDC_KEEP, false);
	grow(IDC_SETTINGS_OTHER_SAVE_GROUP, { IDC_KEEP });
	shift(IDC_FBE_NEXT_SAVING_GROUP, { IDC_CREATE_BACKUP_FILE });
	fitCheckbox(IDC_FBE_NEXT_SAVING_GROUP, IDC_CREATE_BACKUP_FILE, true);
	grow(IDC_FBE_NEXT_SAVING_GROUP, { IDC_CREATE_BACKUP_FILE });
	shift(IDC_FBE_NEXT_WINDOW_TITLE_GROUP, { IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE });
	fitCheckbox(IDC_FBE_NEXT_WINDOW_TITLE_GROUP, IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE, true);
	grow(IDC_FBE_NEXT_WINDOW_TITLE_GROUP, { IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE });
	shift(IDC_FBE_NEXT_UPDATES_GROUP, { IDC_UPDATE_CHANNEL_LABEL, IDC_UPDATE_CHANNEL });
	const CRect updatesGroup = bounds(IDC_FBE_NEXT_UPDATES_GROUP);
	fitLabelCombo(IDC_UPDATE_CHANNEL_LABEL, IDC_UPDATE_CHANNEL, comboLeft, updatesGroup.right - margin);
	grow(IDC_FBE_NEXT_UPDATES_GROUP, { IDC_UPDATE_CHANNEL_LABEL, IDC_UPDATE_CHANNEL });
	RECT client = {}, window = {};
	::GetClientRect(m_hWnd, &client); ::GetWindowRect(m_hWnd, &window);
	const int requiredHeight = bounds(IDC_FBE_NEXT_UPDATES_GROUP).bottom + margin;
	if(requiredHeight > client.bottom)
		::SetWindowPos(m_hWnd, NULL, 0, 0, window.right - window.left,
			requiredHeight + (window.bottom - window.top - client.bottom),
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CSettingsGeneralPage::OnClickedOK(WORD, WORD, HWND, BOOL&)
{
	return ForwardSettingsDialogCommand(m_hWnd, IDOK);
}

LRESULT CSettingsGeneralPage::OnClickedCancel(WORD, WORD, HWND, BOOL&)
{
	return ForwardSettingsDialogCommand(m_hWnd, IDCANCEL);
}

LRESULT CSettingsGeneralPage::OnInterfaceThemeChanged(WORD, WORD, HWND, BOOL&)
{
	const int selection = m_interfaceTheme.GetCurSel();
	if(selection >= INTERFACE_THEME_AUTOMATIC && selection <= INTERFACE_THEME_DARK)
	{
		ThemeManager::SetSelectedTheme(static_cast<InterfaceTheme>(selection));
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
	}
	return 0;
}

bool CSettingsGeneralPage::Validate() { return true; }
void CSettingsGeneralPage::Commit()
{
	const int iconSize = m_scriptIconSize.GetCurSel();
	if(iconSize != CB_ERR) _Settings.SetScriptIconSize(static_cast<int>(m_scriptIconSize.GetItemData(iconSize)));
	const int interfaceTheme = m_interfaceTheme.GetCurSel();
	if(interfaceTheme >= INTERFACE_THEME_AUTOMATIC && interfaceTheme <= INTERFACE_THEME_DARK)
		_Settings.SetInterfaceTheme(static_cast<InterfaceTheme>(interfaceTheme));
	const int encodingIndex = m_defaultEncoding.GetCurSel();
	if(encodingIndex != CB_ERR) { CString encoding; m_defaultEncoding.GetLBText(encodingIndex, encoding); _Settings.SetDefaultEncoding(encoding); }
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
}
bool CSettingsGeneralPage::CancelChanges()
{
	ThemeManager::SetSelectedTheme(m_originalTheme);
	ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
	return true;
}
