#include "stdafx.h"
#include "SettingsGeneralPage.h"
#include "..\\..\\Settings.h"
#include "..\\..\\ThemeManager.h"
#include "..\\..\\UiMetrics.h"
#include "..\\..\\RuntimeLocalization.h"
#include "..\\..\\res1.h"
#include "..\\..\\apputils.h"
#include "..\\..\\..\\common\\ModernFileDialog.h"
#include <initializer_list>
#include <algorithm>
#include <cmath>

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
const wchar_t* PaletteId(int index)
{
	return ThemeManager::NamedPaletteId(static_cast<InterfacePalette>(index));
}
}

LRESULT CSettingsGeneralPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_SETTINGS_GENERAL);
	m_language = GetDlgItem(IDC_LANG);
	m_originalTheme = _Settings.GetInterfaceTheme();
	m_interfaceTheme = GetDlgItem(IDC_INTERFACE_THEME);
	m_interfacePalette = GetDlgItem(IDC_INTERFACE_PALETTE);
	::SetWindowSubclass(m_interfaceTheme, ThemeComboSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
	::SetWindowSubclass(m_interfacePalette, ThemeComboSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
	UpdateThemeComboItemHeight(m_interfaceTheme); UpdateThemeComboItemHeight(m_interfacePalette);
	m_paletteRole = GetDlgItem(IDC_PALETTE_ROLE);
	m_originalPalette = ThemeManager::GetNamedPalette();
	m_originalCustomPaletteActive = ThemeManager::GetCustomPalette(m_originalCustomPalette);
	for(int index = 0; index < INTERFACE_PALETTE_COUNT; ++index)
		if(_Settings.GetInterfacePalette() == PaletteId(index)) m_acceptedPaletteIndex = index;
	m_scriptIconSize = GetDlgItem(IDC_SCRIPT_ICON_SIZE);
	m_mainToolbarIconSize = GetDlgItem(IDC_MAIN_TOOLBAR_ICON_SIZE);
	m_genreCatalog = GetDlgItem(IDC_GENRE_CATALOG);
	m_descriptionLayout = GetDlgItem(IDC_DESCRIPTION_LAYOUT);
	SetText(m_hWnd, IDC_DESCRIPTION_LAYOUT_LABEL, L"fbe.description.layout", L"Description layout:");
	m_descriptionLayout.AddString(FbeLoadRuntimeStringByKey(L"fbe.description.modern", L"Modern"));
	m_descriptionLayout.AddString(FbeLoadRuntimeStringByKey(L"fbe.description.classic", L"Classic"));
	m_descriptionLayout.SetCurSel(_Settings.GetDescriptionLayout() == L"classic" ? 1 : 0);
	m_defaultEncoding = GetDlgItem(IDC_DEFAULT_ENC);
	m_keepEncoding = GetDlgItem(IDC_KEEP);
	m_restorePosition = GetDlgItem(IDC_RESTORE_POS);
	m_updateChannel = GetDlgItem(IDC_UPDATE_CHANNEL);
	m_tooltips.Initialize(m_hWnd);
	m_tooltips.Add(m_language, L"fbe.settings.tooltip.general.language", L"Interface language. System default uses the Windows language.");
	for(HWND control : {m_interfaceTheme.m_hWnd, GetDlgItem(IDC_INTERFACE_THEME_LABEL).m_hWnd})
		m_tooltips.Add(control, L"fbe.settings.tooltip.general.interface_theme", L"Automatic follows the Windows app theme.");
	for(HWND control : {m_interfacePalette.m_hWnd, GetDlgItem(IDC_INTERFACE_PALETTE_LABEL).m_hWnd})
		m_tooltips.Add(control, L"fbe.settings.tooltip.general.interface_palette", L"Choose interface colours independently of the Windows mode. High Contrast uses system colours.");
	m_tooltips.Add(GetDlgItem(IDC_INTERFACE_PALETTE_COPY), L"fbe.settings.tooltip.general.palette_copy", L"Create a copy of the selected palette. It is saved when you confirm settings.");
	m_tooltips.Add(GetDlgItem(IDC_PALETTE_IMPORT), L"fbe.settings.tooltip.general.palette_import", L"Import a JSON palette. It is saved when you confirm settings.");
	m_tooltips.Add(GetDlgItem(IDC_PALETTE_EXPORT), L"fbe.settings.tooltip.general.palette_export", L"Export the selected custom palette to a JSON file.");
	m_tooltips.Add(GetDlgItem(IDC_PALETTE_RENAME), L"fbe.settings.tooltip.general.palette_rename", L"Rename the selected custom palette. The change is saved when you confirm settings.");
	m_tooltips.Add(GetDlgItem(IDC_PALETTE_DELETE), L"fbe.settings.tooltip.general.palette_delete", L"Delete the selected custom palette when you confirm settings. Cancel keeps it.");
	m_tooltips.AddDisabledControlArea(GetDlgItem(IDC_PALETTE_RENAME), L"fbe.settings.tooltip.general.palette_rename", L"Rename the selected custom palette. The change is saved when you confirm settings.");
	m_tooltips.AddDisabledControlArea(GetDlgItem(IDC_PALETTE_DELETE), L"fbe.settings.tooltip.general.palette_delete", L"Delete the selected custom palette when you confirm settings. Cancel keeps it.");
	for(HWND control : {m_paletteRole.m_hWnd, GetDlgItem(IDC_PALETTE_ROLE_LABEL).m_hWnd, GetDlgItem(IDC_PALETTE_CHOOSE_COLOR).m_hWnd, GetDlgItem(IDC_PALETTE_RESET_COLOR).m_hWnd})
		m_tooltips.Add(control, L"fbe.settings.tooltip.general.palette_color", L"Edit a colour in a custom palette. Use the base colour to restore inheritance; Cancel discards changes.");
	m_tooltips.Add(GetDlgItem(IDC_PALETTE_CONTRAST), L"fbe.settings.tooltip.general.palette_contrast", L"Checks text against window, control, selection, hover and pressed backgrounds. Disabled text uses a 3:1 threshold.");
	for(HWND control : {m_descriptionLayout.m_hWnd, GetDlgItem(IDC_DESCRIPTION_LAYOUT_LABEL).m_hWnd})
		m_tooltips.Add(control, L"fbe.settings.tooltip.general.description_layout", L"Choose the Description layout. Modern uses compact, collapsible sections; Classic keeps the familiar FictionBook Editor field order. This does not change FB2 data.");
	for(HWND control : {m_scriptIconSize.m_hWnd, GetDlgItem(IDC_SCRIPT_ICON_SIZE_LABEL).m_hWnd})
		m_tooltips.Add(control, L"fbe.settings.tooltip.general.script_icon_size", L"Sets the icon size in the script tree and script toolbars. Available sizes: 16, 20, 24 and 32 pixels (scaled for DPI).");
	for(HWND control : {m_mainToolbarIconSize.m_hWnd, GetDlgItem(IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL).m_hWnd})
		m_tooltips.Add(control, L"fbe.settings.tooltip.general.main_toolbar_icon_size", L"Sets the main toolbar icon size independently of script icons. Windows DPI scaling is applied.");
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
	SetText(m_hWnd, IDC_INTERFACE_PALETTE_LABEL, L"fbe.settings.interface_palette", L"Interface palette:");
	SetText(m_hWnd, IDC_INTERFACE_PALETTE_COPY, L"fbe.settings.palette.copy", L"Create copy...");
	SetText(m_hWnd, IDC_PALETTE_IMPORT, L"fbe.settings.palette.import", L"Import...");
	SetText(m_hWnd, IDC_PALETTE_EXPORT, L"fbe.settings.palette.export", L"Export...");
	SetText(m_hWnd, IDC_PALETTE_RENAME, L"fbe.settings.palette.rename", L"Rename...");
	SetText(m_hWnd, IDC_PALETTE_DELETE, L"fbe.settings.palette.delete", L"Delete");
	SetText(m_hWnd, IDC_PALETTE_ROLE_LABEL, L"fbe.settings.palette.role", L"Colour role:");
	SetText(m_hWnd, IDC_PALETTE_CHOOSE_COLOR, L"fbe.settings.palette.choose_color", L"Choose colour...");
	SetText(m_hWnd, IDC_PALETTE_RESET_COLOR, L"fbe.settings.palette.reset_color", L"Use base colour");
	SetText(m_hWnd, IDC_SCRIPT_ICON_SIZE_LABEL, L"fbe.settings.script_icon_size", L"Script icon size:");
	SetText(m_hWnd, IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, L"fbe.settings.main_toolbar_icon_size", L"Main toolbar icon size:");
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
	ConfigureInterfaceTheme(m_darkThemeSupported);
	m_interfacePalette.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.classic", L"Classic (Windows mode)"));
	m_interfacePalette.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.graphite", L"Graphite"));
	m_interfacePalette.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.navy", L"Dark blue"));
	m_interfacePalette.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.sepia", L"Sepia"));
	for(int index = INTERFACE_PALETTE_CATPPUCCIN_LATTE; index < INTERFACE_PALETTE_COUNT; ++index)
		m_interfacePalette.AddString(ThemeManager::NamedPaletteName(static_cast<InterfacePalette>(index)));
	for(const ThemeManager::CustomPalette& palette : ThemeManager::ListCustomPalettesInProfile()) {
		const CString label = CString(palette.name.c_str()) + L" (" + palette.id.c_str() + L")";
		const int index = m_interfacePalette.AddString(label);
		if(index < 0) break;
		m_customPalettes.push_back(palette);
		if(_Settings.GetInterfacePalette() == CString(L"custom:") + palette.id.c_str()) m_acceptedPaletteIndex = index;
	}
	// Keep an unavailable stored preference untouched while showing the usable fallback.
	if(!PaletteItemAvailable(m_acceptedPaletteIndex)) m_acceptedPaletteIndex = INTERFACE_PALETTE_CLASSIC;
	m_interfacePalette.SetCurSel(m_acceptedPaletteIndex);
	CString roles = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.roles", L"Window|Control|Text|Border|Separator|Secondary text|Disabled text|Selection background|Selection text|Hover|Pressed|Focus|Accent|Error|Warning|Success");
	int rolePosition = 0;
	for(int role = 0; role < THEME_COLOR_COUNT; ++role) m_paletteRole.AddString(roles.Tokenize(L"|", rolePosition));
	m_paletteRole.SetCurSel(0);
	UpdatePaletteColorControls();
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
	for(int size : {16, 20, 24, 32}) {
		CString iconText; iconText.Format(L"%d \x00d7 %d", size, size);
		const int index = m_scriptIconSize.AddString(iconText); m_scriptIconSize.SetItemData(index, size);
		if(size == _Settings.GetScriptIconSize()) m_scriptIconSize.SetCurSel(index);
		const int mainIndex = m_mainToolbarIconSize.AddString(iconText); m_mainToolbarIconSize.SetItemData(mainIndex, size);
		if(size == _Settings.GetMainToolbarIconSize()) m_mainToolbarIconSize.SetCurSel(mainIndex);
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
		IDC_INTERFACE_PALETTE_LABEL, IDC_INTERFACE_PALETTE, IDC_INTERFACE_PALETTE_COPY, IDC_PALETTE_IMPORT, IDC_PALETTE_EXPORT, IDC_PALETTE_RENAME, IDC_PALETTE_DELETE, IDC_PALETTE_ROLE_LABEL, IDC_PALETTE_ROLE, IDC_PALETTE_CHOOSE_COLOR, IDC_PALETTE_RESET_COLOR, IDC_PALETTE_CONTRAST, IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE, IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE, IDC_DESCRIPTION_LAYOUT_LABEL, IDC_DESCRIPTION_LAYOUT });
	const CRect interfaceGroup = bounds(IDC_OPTIONS_INTERFACE_GROUP);
	int comboLeft = bounds(IDC_LANG).left;
	for(int id : { IDC_OPTIONS_LANGUAGE_LABEL, IDC_OPTIONS_GENRE_CATALOG_LABEL,
		IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_PALETTE_LABEL, IDC_PALETTE_ROLE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE_LABEL, IDC_DESCRIPTION_LAYOUT_LABEL, IDC_SETTINGS_OTHER_ENCODING, IDC_UPDATE_CHANNEL_LABEL })
		comboLeft = max(comboLeft, bounds(id).left + measure(id, 10000, false).cx + gap);
	comboLeft = min(comboLeft, interfaceGroup.right - margin - UiMetrics::ScaleForDpi(130, dpi));
	const int interfaceComboRight = interfaceGroup.right - margin;
	fitLabelCombo(IDC_OPTIONS_LANGUAGE_LABEL, IDC_LANG, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_OPTIONS_GENRE_CATALOG_LABEL, IDC_GENRE_CATALOG, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_INTERFACE_PALETTE_LABEL, IDC_INTERFACE_PALETTE, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_PALETTE_ROLE_LABEL, IDC_PALETTE_ROLE, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_DESCRIPTION_LAYOUT_LABEL, IDC_DESCRIPTION_LAYOUT, comboLeft, interfaceComboRight);
	separateRows(IDC_OPTIONS_LANGUAGE_LABEL, IDC_LANG, IDC_OPTIONS_GENRE_CATALOG_LABEL, IDC_GENRE_CATALOG);
	separateRows(IDC_OPTIONS_GENRE_CATALOG_LABEL, IDC_GENRE_CATALOG, IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME);
	separateRows(IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME, IDC_INTERFACE_PALETTE_LABEL, IDC_INTERFACE_PALETTE);
	CRect copyButton = bounds(IDC_INTERFACE_PALETTE_COPY);
	const int copyButtonHeight = copyButton.Height();
	copyButton.left = comboLeft;
	copyButton.right = interfaceComboRight;
	copyButton.top = max(bounds(IDC_INTERFACE_PALETTE_LABEL).bottom, bounds(IDC_INTERFACE_PALETTE).bottom) + rowGap;
	copyButton.bottom = copyButton.top + max(copyButtonHeight, measure(IDC_INTERFACE_PALETTE_COPY, copyButton.Width(), false).cy + padding);
	place(IDC_INTERFACE_PALETTE_COPY, copyButton);
	CRect importButton = bounds(IDC_PALETTE_IMPORT), exportButton = bounds(IDC_PALETTE_EXPORT);
	const int actionHeight = max(importButton.Height(), exportButton.Height());
	const int actionTop = copyButton.bottom + rowGap;
	const int actionMiddle = comboLeft + (interfaceComboRight - comboLeft) / 2;
	importButton.SetRect(comboLeft, actionTop, actionMiddle - gap / 2, actionTop + actionHeight);
	exportButton.SetRect(actionMiddle + gap / 2, actionTop, interfaceComboRight, actionTop + actionHeight);
	place(IDC_PALETTE_IMPORT, importButton); place(IDC_PALETTE_EXPORT, exportButton);
	CRect renameButton = bounds(IDC_PALETTE_RENAME), deleteButton = bounds(IDC_PALETTE_DELETE);
	const int editHeight = max(renameButton.Height(), deleteButton.Height());
	const int editTop = max(importButton.bottom, exportButton.bottom) + rowGap;
	renameButton.SetRect(comboLeft, editTop, actionMiddle - gap / 2, editTop + editHeight);
	deleteButton.SetRect(actionMiddle + gap / 2, editTop, interfaceComboRight, editTop + editHeight);
	place(IDC_PALETTE_RENAME, renameButton); place(IDC_PALETTE_DELETE, deleteButton);
	separateRows(IDC_PALETTE_RENAME, IDC_PALETTE_DELETE, IDC_PALETTE_ROLE_LABEL, IDC_PALETTE_ROLE);
	CRect choose = bounds(IDC_PALETTE_CHOOSE_COLOR), reset = bounds(IDC_PALETTE_RESET_COLOR);
	const int buttonHeight = max(choose.Height(), reset.Height());
	const int buttonTop = max(bounds(IDC_PALETTE_ROLE_LABEL).bottom, bounds(IDC_PALETTE_ROLE).bottom) + rowGap;
	choose.SetRect(comboLeft, buttonTop, interfaceComboRight, buttonTop + buttonHeight);
	reset.SetRect(comboLeft, choose.bottom + rowGap, interfaceComboRight, choose.bottom + rowGap + buttonHeight);
	place(IDC_PALETTE_CHOOSE_COLOR, choose); place(IDC_PALETTE_RESET_COLOR, reset);
	CRect contrast = bounds(IDC_PALETTE_CONTRAST);
	contrast.left = interfaceGroup.left + margin; contrast.right = interfaceComboRight;
	contrast.top = reset.bottom + rowGap; contrast.bottom = contrast.top + 3 * minimumTextHeight + padding;
	place(IDC_PALETTE_CONTRAST, contrast);
	separateRows(IDC_PALETTE_CONTRAST, IDC_PALETTE_CONTRAST, IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE);
	separateRows(IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE, IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE);
	separateRows(IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE, IDC_DESCRIPTION_LAYOUT_LABEL, IDC_DESCRIPTION_LAYOUT);
	grow(IDC_OPTIONS_INTERFACE_GROUP, { IDC_DESCRIPTION_LAYOUT_LABEL, IDC_DESCRIPTION_LAYOUT });
	if(!m_darkThemeSupported) {
		CRect note = bounds(IDC_INTERFACE_THEME_NOTE);
		note.left = interfaceGroup.left + margin; note.right = interfaceGroup.right - margin;
		note.top = max(bounds(IDC_DESCRIPTION_LAYOUT_LABEL).bottom, bounds(IDC_DESCRIPTION_LAYOUT).bottom) + rowGap;
		note.bottom = note.top + max(minimumTextHeight, measure(IDC_INTERFACE_THEME_NOTE, note.Width(), true).cy + padding);
		place(IDC_INTERFACE_THEME_NOTE, note);
		grow(IDC_OPTIONS_INTERFACE_GROUP, {IDC_INTERFACE_THEME_NOTE});
	}
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

void CSettingsGeneralPage::UpdateThemeComboItemHeight(HWND window)
{
	HDC dc = ::GetDC(window); if(!dc) return;
	HFONT font = reinterpret_cast<HFONT>(::SendMessage(window, WM_GETFONT, 0, 0));
	HGDIOBJ previous = font ? ::SelectObject(dc, font) : NULL;
	TEXTMETRICW metrics = {}; const bool measured = ::GetTextMetricsW(dc, &metrics) != FALSE;
	if(previous) ::SelectObject(dc, previous);
	::ReleaseDC(window, dc);
	if(measured) ::SendMessage(window, CB_SETITEMHEIGHT, 0, metrics.tmHeight + UiMetrics::ScaleForDpi(6, UiMetrics::DpiForWindow(window)));
}

LRESULT CALLBACK CSettingsGeneralPage::ThemeComboSubclassProc(HWND window, UINT message, WPARAM key, LPARAM parameter, UINT_PTR id, DWORD_PTR data)
{
	CSettingsGeneralPage* page = reinterpret_cast<CSettingsGeneralPage*>(data);
	if(message == WM_NCDESTROY) {
		::RemoveWindowSubclass(window, ThemeComboSubclassProc, id);
		return ::DefSubclassProc(window, message, key, parameter);
	}
	if(message == WM_SETFONT) {
		const LRESULT result = ::DefSubclassProc(window, message, key, parameter);
		UpdateThemeComboItemHeight(window);
		return result;
	}
	if(page && !page->m_darkThemeSupported && message == WM_KEYDOWN && (key == VK_UP || key == VK_DOWN)) {
		const int count = static_cast<int>(::SendMessage(window, CB_GETCOUNT, 0, 0));
		const int current = static_cast<int>(::SendMessage(window, CB_GETCURSEL, 0, 0));
		const int step = key == VK_DOWN ? 1 : -1;
		const bool mode = window == page->m_interfaceTheme.m_hWnd;
		for(int index = current + step; index >= 0 && index < count; index += step) {
			if(mode ? index != INTERFACE_THEME_DARK : page->PaletteItemAvailable(index)) {
				::SendMessage(window, CB_SETCURSEL, index, 0);
				::SendMessage(page->m_hWnd, WM_COMMAND, MAKEWPARAM(::GetDlgCtrlID(window), CBN_SELCHANGE), reinterpret_cast<LPARAM>(window));
				break;
			}
		}
		return 0;
	}
	return ::DefSubclassProc(window, message, key, parameter);
}

LRESULT CALLBACK CSettingsGeneralPage::ThemeListSubclassProc(HWND window, UINT message, WPARAM key, LPARAM parameter, UINT_PTR id, DWORD_PTR data)
{
	CSettingsGeneralPage* page = reinterpret_cast<CSettingsGeneralPage*>(data);
	if(message == WM_NCDESTROY) {
		::RemoveWindowSubclass(window, ThemeListSubclassProc, id);
		return ::DefSubclassProc(window, message, key, parameter);
	}
	if(page && message == WM_MOUSEMOVE) {
		const LRESULT hit = ::SendMessage(window, LB_ITEMFROMPOINT, 0, parameter);
		const int index = HIWORD(hit) ? -1 : LOWORD(hit);
		const bool themeList = id == 1;
		const bool unavailable = !page->m_darkThemeSupported && index >= 0 &&
			(themeList ? index == INTERFACE_THEME_DARK : !page->PaletteItemAvailable(index));
		const CString text = unavailable ?
			FbeLoadRuntimeStringByKey(L"fbe.settings.palette.unavailable", L"This dark palette requires Windows 10 (1809) or newer.") :
			(themeList ? FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.general.interface_theme", L"Automatic follows the Windows app theme.") :
				FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.general.interface_palette", L"Choose interface colours independently of the Windows mode. High Contrast uses system colours."));
		page->m_tooltips.UpdateText(window, text);
	}
	return ::DefSubclassProc(window, message, key, parameter);
}

bool CSettingsGeneralPage::PaletteItemAvailable(int index) const
{
	if(m_darkThemeSupported) return true;
	if(index >= 0 && index < INTERFACE_PALETTE_COUNT)
		return !ThemeManager::NamedPaletteIsDark(static_cast<InterfacePalette>(index));
	const int custom = index - INTERFACE_PALETTE_COUNT;
	if(custom < 0 || custom >= static_cast<int>(m_customPalettes.size())) return false;
	const auto base = m_customPalettes[custom].base;
	return base != ThemeManager::CustomPaletteBase::Dark && base != ThemeManager::CustomPaletteBase::Graphite && base != ThemeManager::CustomPaletteBase::Navy;
}

LRESULT CSettingsGeneralPage::OnDrawThemeItem(UINT, WPARAM, LPARAM parameter, BOOL& handled)
{
	const DRAWITEMSTRUCT* item = reinterpret_cast<const DRAWITEMSTRUCT*>(parameter);
	if(!item || (item->CtlID != IDC_INTERFACE_THEME && item->CtlID != IDC_INTERFACE_PALETTE)) { handled = FALSE; return 0; }
	const bool selected = (item->itemState & ODS_SELECTED) != 0;
	const int index = item->itemID == static_cast<UINT>(-1) ? -1 : static_cast<int>(item->itemID);
	const bool available = index < 0 || (item->CtlID == IDC_INTERFACE_THEME ? m_darkThemeSupported || index != INTERFACE_THEME_DARK : PaletteItemAvailable(index));
	const COLORREF background = selected ? ThemeManager::SelectionBackgroundColor() : ThemeManager::ControlColor();
	const COLORREF foreground = !available ? ThemeManager::DisabledTextColor() : selected ? ThemeManager::SelectionTextColor() : ThemeManager::TextColor();
	::SetBkColor(item->hDC, background); ::SetTextColor(item->hDC, foreground);
	::ExtTextOutW(item->hDC, 0, 0, ETO_OPAQUE, &item->rcItem, NULL, 0, NULL);
	if(index >= 0) {
		CComboBox combo(item->hwndItem); CString text; combo.GetLBText(index, text);
		RECT rect = item->rcItem; rect.left += UiMetrics::ScaleForDpi(4, UiMetrics::DpiForWindow(m_hWnd)); rect.right -= 2;
		::DrawTextW(item->hDC, text, text.GetLength(), &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
	}
	if(item->itemState & ODS_FOCUS) ::DrawFocusRect(item->hDC, &item->rcItem);
	return TRUE;
}

void CSettingsGeneralPage::ConfigureInterfaceTheme(bool darkThemeSupported)
{
	m_darkThemeSupported = darkThemeSupported;
	m_originalTheme = _Settings.GetInterfaceTheme();
	m_preserveUnavailableDark = !darkThemeSupported && m_originalTheme == INTERFACE_THEME_DARK;
	m_interfaceTheme.ResetContent();
	m_interfaceTheme.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.automatic", L"Automatic \x2014 follow Windows"));
	m_interfaceTheme.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.light", L"Light"));
	m_interfaceTheme.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.dark", L"Dark"));
	m_acceptedThemeIndex = m_preserveUnavailableDark ? INTERFACE_THEME_LIGHT : static_cast<int>(m_originalTheme);
	m_interfaceTheme.SetCurSel(m_acceptedThemeIndex);
	UpdateThemeNotice();
}

void CSettingsGeneralPage::UpdateThemeNotice()
{
	const bool supported = m_darkThemeSupported;
	CString notice;
	if(!supported) {
		notice = FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.unavailable", L"The dark interface theme requires Windows 10 (1809) or newer. This system uses the standard light appearance.");
		if(m_preserveUnavailableDark) notice += L" " + FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.preserved", L"Your saved dark preference is preserved until you choose another theme.");
	}
	::SetDlgItemText(m_hWnd, IDC_INTERFACE_THEME_NOTE, notice);
	::ShowWindow(GetDlgItem(IDC_INTERFACE_THEME_NOTE), supported ? SW_HIDE : SW_SHOW);
	CString themeTip = FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.general.interface_theme", L"Automatic follows the Windows app theme.");
	CString paletteTip = FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.general.interface_palette", L"Choose interface colours independently of the Windows mode. High Contrast uses system colours.");
	if(!supported) {
		themeTip += L" " + FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.unavailable", L"The dark interface theme requires Windows 10 (1809) or newer. This system uses the standard light appearance.");
		paletteTip += L" " + FbeLoadRuntimeStringByKey(L"fbe.settings.palette.unavailable", L"This dark palette requires Windows 10 (1809) or newer.");
		if(!m_unavailableListTooltips) {
			for(HWND combo : {m_interfaceTheme.m_hWnd, m_interfacePalette.m_hWnd}) {
				COMBOBOXINFO info = {}; info.cbSize = sizeof(info);
				if(::GetComboBoxInfo(combo, &info) && info.hwndList) {
					const bool themeList = combo == m_interfaceTheme.m_hWnd;
					m_tooltips.Add(info.hwndList, themeList ? L"fbe.settings.tooltip.general.interface_theme" : L"fbe.settings.tooltip.general.interface_palette",
						themeList ? L"Automatic follows the Windows app theme." : L"Choose interface colours independently of the Windows mode. High Contrast uses system colours.");
					::SetWindowSubclass(info.hwndList, ThemeListSubclassProc, themeList ? 1 : 2, reinterpret_cast<DWORD_PTR>(this));
				}
			}
			m_unavailableListTooltips = true;
		}
	}
	for(HWND control : {m_interfaceTheme.m_hWnd, GetDlgItem(IDC_INTERFACE_THEME_LABEL).m_hWnd}) m_tooltips.UpdateText(control, themeTip);
	for(HWND control : {m_interfacePalette.m_hWnd, GetDlgItem(IDC_INTERFACE_PALETTE_LABEL).m_hWnd}) m_tooltips.UpdateText(control, paletteTip);
	m_interfaceTheme.Invalidate(); m_interfacePalette.Invalidate();
}

LRESULT CSettingsGeneralPage::OnInterfaceThemeChanged(WORD notification, WORD, HWND, BOOL&)
{
	const int selection = m_interfaceTheme.GetCurSel();
	if(!m_darkThemeSupported && selection == INTERFACE_THEME_DARK) { m_interfaceTheme.SetCurSel(m_acceptedThemeIndex); return 0; }
	if(selection >= INTERFACE_THEME_AUTOMATIC && selection <= INTERFACE_THEME_DARK)
	{
		m_acceptedThemeIndex = selection;
		if(notification == CBN_SELENDOK || !m_interfaceTheme.GetDroppedState()) {
			m_preserveUnavailableDark = false;
			UpdateThemeNotice();
		}
		ThemeManager::SetSelectedTheme(static_cast<InterfaceTheme>(selection));
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
	}
	return 0;
}

LRESULT CSettingsGeneralPage::OnPaletteDropdown(WORD, WORD, HWND, BOOL&)
{
	if(!m_reopeningPaletteDropdown) {
		m_paletteDropdownStart = m_acceptedPaletteIndex;
		m_paletteChangedBeforeDropdown = m_paletteChanged;
	}
	return 0;
}

LRESULT CSettingsGeneralPage::OnPaletteDropdownCancel(WORD, WORD, HWND, BOOL&)
{
	if(m_paletteDropdownStart < 0 || m_paletteDropdownStart >= m_interfacePalette.GetCount()) return 0;
	m_interfacePalette.SetCurSel(m_paletteDropdownStart);
	BOOL handled = FALSE;
	m_cancellingPaletteDropdown = true;
	OnInterfacePaletteChanged(CBN_SELCHANGE, IDC_INTERFACE_PALETTE, m_interfacePalette, handled);
	m_cancellingPaletteDropdown = false;
	m_paletteChanged = m_paletteChangedBeforeDropdown;
	m_paletteDropdownStart = -1;
	return 0;
}

LRESULT CSettingsGeneralPage::OnInterfacePaletteChanged(WORD, WORD, HWND, BOOL&)
{
	const int selection = m_interfacePalette.GetCurSel();
	if(selection < 0 || selection >= INTERFACE_PALETTE_COUNT + static_cast<int>(m_customPalettes.size())) return 0;
	const bool keepDropdown = !m_cancellingPaletteDropdown && m_interfacePalette.GetDroppedState() != FALSE;
	const bool hadFocus = ::GetFocus() == m_interfacePalette.m_hWnd;
	bool applied = false;
	if(selection < INTERFACE_PALETTE_COUNT) {
		if(m_darkThemeSupported || !ThemeManager::NamedPaletteIsDark(static_cast<InterfacePalette>(selection)))
			applied = ThemeManager::SetNamedPalette(static_cast<InterfacePalette>(selection));
	} else {
		ThemeManager::CustomPalette palette; std::wstring error;
		const std::wstring& id = m_customPalettes[selection - INTERFACE_PALETTE_COUNT].id;
		bool loaded = false;
		for(const auto& pending : m_pendingPalettes) if(pending.id == id) { palette = pending; loaded = true; break; }
		if(!loaded) loaded = ThemeManager::LoadCustomPaletteFromProfile(id, palette, error);
		if(!loaded) {
			m_interfacePalette.SetCurSel(m_acceptedPaletteIndex);
			CString message = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.load_failed", L"Could not load the selected interface palette.");
			if(!error.empty()) { message += L"\n\n"; message += error.c_str(); }
			MessageBox(message, FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONWARNING);
			return 0;
		}
		const bool dark = palette.base == ThemeManager::CustomPaletteBase::Dark || palette.base == ThemeManager::CustomPaletteBase::Graphite || palette.base == ThemeManager::CustomPaletteBase::Navy;
		if(m_darkThemeSupported || !dark) applied = ThemeManager::SetCustomPalette(palette);
	}
	if(!applied) {
		m_interfacePalette.SetCurSel(m_acceptedPaletteIndex);
		return 0;
	}
	m_acceptedPaletteIndex = selection;
	UpdatePaletteColorControls();
	m_paletteChanged = true;
	ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
	// Retheming native controls can close an open combo popup. Keep keyboard
	// navigation active without replacing the selection captured on opening.
	if(keepDropdown && !m_interfacePalette.GetDroppedState()) {
		if(hadFocus) m_interfacePalette.SetFocus();
		m_reopeningPaletteDropdown = true;
		m_interfacePalette.ShowDropDown(TRUE);
		m_reopeningPaletteDropdown = false;
	}
	return 0;
}

LRESULT CSettingsGeneralPage::OnPaletteThemeChanged(UINT, WPARAM, LPARAM, BOOL& handled)
{
	if(GetDlgItem(IDC_PALETTE_CONTRAST).IsWindow()) UpdatePaletteContrast();
	handled = FALSE;
	return 0;
}

void CSettingsGeneralPage::UpdatePaletteColorControls()
{
	const bool editable = m_acceptedPaletteIndex >= INTERFACE_PALETTE_COUNT && ThemeManager::IsCustomPaletteActive();
	for(int id : {IDC_PALETTE_ROLE_LABEL, IDC_PALETTE_ROLE, IDC_PALETTE_CHOOSE_COLOR, IDC_PALETTE_RESET_COLOR, IDC_PALETTE_EXPORT, IDC_PALETTE_RENAME, IDC_PALETTE_DELETE}) GetDlgItem(id).EnableWindow(editable);
	UpdatePaletteContrast();
}

void CSettingsGeneralPage::UpdatePaletteContrast()
{
	if(!ThemeManager::IsCustomPaletteActive()) { SetDlgItemText(IDC_PALETTE_CONTRAST, L""); return; }
	auto luminance = [](COLORREF color) {
		auto channel = [](BYTE value) { const double v = value / 255.0; return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
		return 0.2126 * channel(GetRValue(color)) + 0.7152 * channel(GetGValue(color)) + 0.0722 * channel(GetBValue(color));
	};
	auto enough = [&](ThemeColorRole foreground, ThemeColorRole background, double threshold) {
		const double a = luminance(ThemeManager::Color(foreground)), b = luminance(ThemeManager::Color(background));
		return ((std::max)(a, b) + 0.05) / ((std::min)(a, b) + 0.05) >= threshold;
	};
	const bool readable = enough(THEME_COLOR_TEXT, THEME_COLOR_WINDOW, 4.5) && enough(THEME_COLOR_TEXT, THEME_COLOR_CONTROL, 4.5) &&
		enough(THEME_COLOR_SELECTION_TEXT, THEME_COLOR_SELECTION_BACKGROUND, 4.5) && enough(THEME_COLOR_TEXT, THEME_COLOR_HOVER, 4.5) &&
		enough(THEME_COLOR_TEXT, THEME_COLOR_PRESSED, 4.5) && enough(THEME_COLOR_DISABLED_TEXT, THEME_COLOR_CONTROL, 3.0);
	SetDlgItemText(IDC_PALETTE_CONTRAST, readable ?
		FbeLoadRuntimeStringByKey(L"fbe.settings.palette.contrast_ok", L"Text contrast check passed.") :
		FbeLoadRuntimeStringByKey(L"fbe.settings.palette.contrast_low", L"Warning: some text may be difficult to read against its background."));
}

LRESULT CSettingsGeneralPage::OnChoosePaletteColor(WORD, WORD, HWND, BOOL&)
{
	const int role = m_paletteRole.GetCurSel();
	if(role < 0 || role >= THEME_COLOR_COUNT || !ThemeManager::IsCustomPaletteActive()) return 0;
	static COLORREF customColors[16] = {};
	CHOOSECOLORW chooser = {};
	chooser.lStructSize = sizeof(chooser); chooser.hwndOwner = m_hWnd;
	chooser.rgbResult = ThemeManager::Color(static_cast<ThemeColorRole>(role));
	chooser.lpCustColors = customColors; chooser.Flags = CC_RGBINIT | CC_FULLOPEN;
	if(::ChooseColorW(&chooser)) StagePaletteColor(static_cast<ThemeColorRole>(role), chooser.rgbResult, true);
	return 0;
}

LRESULT CSettingsGeneralPage::OnResetPaletteColor(WORD, WORD, HWND, BOOL&)
{
	const int role = m_paletteRole.GetCurSel();
	if(role >= 0 && role < THEME_COLOR_COUNT) StagePaletteColor(static_cast<ThemeColorRole>(role), 0, false);
	return 0;
}

bool CSettingsGeneralPage::StagePaletteCopy(const CString& name)
{
	const int selection = m_interfacePalette.GetCurSel();
	if(selection < 0 || selection >= INTERFACE_PALETTE_COUNT + static_cast<int>(m_customPalettes.size())) return false;
	ThemeManager::CustomPalette copy;
	if(selection >= INTERFACE_PALETTE_COUNT) { if(!ThemeManager::GetCustomPalette(copy)) return false; }
	else if(ThemeManager::CopyNamedPaletteColors(static_cast<InterfacePalette>(selection), copy)) {}
	else copy.base = selection == INTERFACE_PALETTE_GRAPHITE ? ThemeManager::CustomPaletteBase::Graphite :
		selection == INTERFACE_PALETTE_NAVY ? ThemeManager::CustomPaletteBase::Navy :
		selection == INTERFACE_PALETTE_SEPIA ? ThemeManager::CustomPaletteBase::Sepia :
		ThemeManager::IsDark() ? ThemeManager::CustomPaletteBase::Dark : ThemeManager::CustomPaletteBase::Light;
	GUID guid = {};
	wchar_t text[40] = {};
	if(FAILED(::CoCreateGuid(&guid)) || !::StringFromGUID2(guid, text, _countof(text))) return false;
	CString id(text); id.Trim(L"{}"); id.MakeLower(); id = L"copy-" + id;
	copy.id = static_cast<LPCWSTR>(id); copy.name = static_cast<LPCWSTR>(name);
	ThemeManager::CustomPalette validated; std::wstring error;
	if(!ThemeManager::ParseCustomPalette(ThemeManager::SerializeCustomPalette(copy), validated, error) || validated.name != copy.name) return false;
	const CString label = name + L" (" + id + L")";
	const int index = m_interfacePalette.AddString(label);
	if(index < 0) return false;
	m_customPalettes.push_back(copy); m_pendingPalettes.push_back(copy); m_newPaletteIds.push_back(copy.id);
	m_interfacePalette.SetCurSel(index);
	BOOL handled = FALSE;
	OnInterfacePaletteChanged(CBN_SELCHANGE, IDC_INTERFACE_PALETTE, m_interfacePalette, handled);
	return m_acceptedPaletteIndex == index;
}

bool CSettingsGeneralPage::StagePaletteColor(ThemeColorRole role, COLORREF color, bool custom)
{
	const int index = m_interfacePalette.GetCurSel() - INTERFACE_PALETTE_COUNT;
	if(index < 0 || index >= static_cast<int>(m_customPalettes.size()) || role < 0 || role >= THEME_COLOR_COUNT) return false;
	ThemeManager::CustomPalette draft;
	if(!ThemeManager::GetCustomPalette(draft) || draft.id != m_customPalettes[index].id) return false;
	draft.present[role] = custom;
	draft.colors[role] = color;
	if(!ThemeManager::SetCustomPalette(draft)) return false;
	bool found = false;
	for(auto& pending : m_pendingPalettes) if(pending.id == draft.id) { pending = draft; found = true; break; }
	if(!found) m_pendingPalettes.push_back(draft);
	m_customPalettes[index] = draft;
	m_paletteChanged = true;
	UpdatePaletteContrast();
	ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
	return true;
}

bool CSettingsGeneralPage::StagePaletteImport(const CString& path, CString& error)
{
	ThemeManager::CustomPalette imported; std::wstring detail;
	if(!ThemeManager::LoadCustomPaletteFromFile(path, imported, detail)) { error = detail.c_str(); return false; }
	for(const auto& palette : m_customPalettes) if(palette.id == imported.id) {
		error = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.id_exists", L"A palette with this ID already exists."); return false;
	}
	if(!ThemeManager::CustomPaletteIdAvailable(imported.id)) {
		error = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.id_exists", L"A palette with this ID already exists or its destination is unavailable."); return false;
	}
	const bool dark = imported.base == ThemeManager::CustomPaletteBase::Dark || imported.base == ThemeManager::CustomPaletteBase::Graphite || imported.base == ThemeManager::CustomPaletteBase::Navy;
	if(dark && !m_darkThemeSupported) {
		error = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.unavailable", L"This dark palette requires Windows 10 (1809) or newer."); return false;
	}
	const CString label = CString(imported.name.c_str()) + L" (" + imported.id.c_str() + L")";
	const int index = m_interfacePalette.AddString(label);
	if(index < 0) { error = L"Cannot add palette to the list."; return false; }
	if(!ThemeManager::SetCustomPalette(imported)) {
		m_interfacePalette.DeleteString(index);
		error = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.unavailable", L"This dark palette requires Windows 10 (1809) or newer."); return false;
	}
	m_customPalettes.push_back(imported); m_pendingPalettes.push_back(imported); m_newPaletteIds.push_back(imported.id);
	m_interfacePalette.SetCurSel(index); m_acceptedPaletteIndex = index; m_paletteChanged = true;
	UpdatePaletteColorControls(); ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
	return true;
}

LRESULT CSettingsGeneralPage::OnImportPalette(WORD, WORD, HWND, BOOL&)
{
	const CString filter = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.json_filter", L"Interface palettes (*.json)");
	const COMDLG_FILTERSPEC filters[] = {{filter, L"*.json"}};
	ModernFileDialog::Request request; request.fileMustExist = true; request.pathMustExist = true; request.filters = filters; request.filterCount = _countof(filters);
	const auto result = ModernFileDialog::Show(m_hWnd, request);
	if(result.outcome == ModernFileDialog::Outcome::Failed) {
		CString detail; detail.Format(L"0x%08X", static_cast<unsigned>(result.error));
		MessageBox(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.load_failed", L"Could not load the selected interface palette.") + L"\n\n" + detail,
			FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONERROR); return 0;
	}
	if(result.outcome != ModernFileDialog::Outcome::Accepted || result.paths.empty()) return 0;
	CString error;
	if(!StagePaletteImport(result.paths.front().c_str(), error)) {
		CString message = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.load_failed", L"Could not load the selected interface palette.");
		if(!error.IsEmpty()) message += L"\n\n" + error;
		MessageBox(message, FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONWARNING);
	}
	return 0;
}

LRESULT CSettingsGeneralPage::OnExportPalette(WORD, WORD, HWND, BOOL&)
{
	ThemeManager::CustomPalette palette;
	if(!ThemeManager::GetCustomPalette(palette)) return 0;
	const CString filter = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.json_filter", L"Interface palettes (*.json)");
	const COMDLG_FILTERSPEC filters[] = {{filter, L"*.json"}};
	ModernFileDialog::Request request; request.save = true; request.pathMustExist = true; request.overwritePrompt = true;
	request.defaultExtension = L"json"; request.initialFileName = palette.id + L".json"; request.filters = filters; request.filterCount = _countof(filters);
	const auto result = ModernFileDialog::Show(m_hWnd, request);
	if(result.outcome == ModernFileDialog::Outcome::Failed) {
		CString detail; detail.Format(L"0x%08X", static_cast<unsigned>(result.error));
		MessageBox(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.export_failed", L"Could not export the interface palette.") + L"\n\n" + detail,
			FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONERROR); return 0;
	}
	if(result.outcome != ModernFileDialog::Outcome::Accepted || result.paths.empty()) return 0;
	std::wstring error;
	if(!ThemeManager::ExportCustomPaletteToFile(result.paths.front().c_str(), palette, error)) {
		CString message = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.export_failed", L"Could not export the interface palette.");
		if(!error.empty()) { message += L"\n\n"; message += error.c_str(); }
		MessageBox(message, FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONERROR);
	}
	return 0;
}

bool CSettingsGeneralPage::StagePaletteRename(const CString& name)
{
	const int selection = m_interfacePalette.GetCurSel();
	const int index = selection - INTERFACE_PALETTE_COUNT;
	if(index < 0 || index >= static_cast<int>(m_customPalettes.size())) return false;
	ThemeManager::CustomPalette previous;
	if(!ThemeManager::GetCustomPalette(previous) || previous.id != m_customPalettes[index].id) return false;
	ThemeManager::CustomPalette draft = previous; draft.name = static_cast<LPCWSTR>(name);
	ThemeManager::CustomPalette validated; std::wstring error;
	if(!ThemeManager::ParseCustomPalette(ThemeManager::SerializeCustomPalette(draft), validated, error) || validated.name != draft.name) return false;
	if(previous.name == draft.name) return true;
	const CString oldLabel = CString(previous.name.c_str()) + L" (" + previous.id.c_str() + L")";
	const CString label = name + L" (" + draft.id.c_str() + L")";
	m_interfacePalette.DeleteString(selection);
	const int inserted = m_interfacePalette.InsertString(selection, label);
	if(inserted != selection || !ThemeManager::SetCustomPalette(draft)) {
		if(inserted >= 0) m_interfacePalette.DeleteString(inserted);
		m_interfacePalette.InsertString(selection, oldLabel); m_interfacePalette.SetCurSel(selection);
		return false;
	}
	m_interfacePalette.SetCurSel(selection);
	m_customPalettes[index] = draft;
	bool found = false;
	for(auto& pending : m_pendingPalettes) if(pending.id == draft.id) { pending = draft; found = true; break; }
	if(!found) m_pendingPalettes.push_back(draft);
	m_paletteChanged = true;
	return true;
}

bool CSettingsGeneralPage::StagePaletteDelete()
{
	const int selection = m_interfacePalette.GetCurSel();
	const int index = selection - INTERFACE_PALETTE_COUNT;
	if(index < 0 || index >= static_cast<int>(m_customPalettes.size())) return false;
	const std::wstring id = m_customPalettes[index].id;
	const auto created = std::find(m_newPaletteIds.begin(), m_newPaletteIds.end(), id);
	if(created == m_newPaletteIds.end()) m_deletedPaletteIds.push_back(id);
	else m_newPaletteIds.erase(created);
	m_pendingPalettes.erase(std::remove_if(m_pendingPalettes.begin(), m_pendingPalettes.end(), [&](const ThemeManager::CustomPalette& value) { return value.id == id; }), m_pendingPalettes.end());
	m_customPalettes.erase(m_customPalettes.begin() + index);
	m_interfacePalette.DeleteString(selection);
	m_interfacePalette.SetCurSel(INTERFACE_PALETTE_CLASSIC);
	BOOL handled = FALSE;
	OnInterfacePaletteChanged(CBN_SELCHANGE, IDC_INTERFACE_PALETTE, m_interfacePalette, handled);
	return true;
}

LRESULT CSettingsGeneralPage::OnRenamePalette(WORD, WORD, HWND, BOOL&)
{
	ThemeManager::CustomPalette palette;
	if(!ThemeManager::GetCustomPalette(palette)) return 0;
	CString name(palette.name.c_str());
	if(AU::InputBoxOwned(name, FbeLoadRuntimeStringByKey(L"fbe.settings.palette.rename", L"Rename..."),
		FbeLoadRuntimeStringByKey(L"fbe.settings.palette.rename_name", L"New name of the interface palette:"), m_hWnd) != IDYES) return 0;
	name.Trim();
	if(!StagePaletteRename(name)) MessageBox(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.rename_failed", L"Could not rename the palette. Enter a name of up to 100 characters without control characters."),
		FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONWARNING);
	return 0;
}

LRESULT CSettingsGeneralPage::OnDeletePalette(WORD, WORD, HWND, BOOL&)
{
	StagePaletteDelete();
	return 0;
}

LRESULT CSettingsGeneralPage::OnCopyPalette(WORD, WORD, HWND, BOOL&)
{
	CString name;
	if(AU::InputBoxOwned(name, FbeLoadRuntimeStringByKey(L"fbe.settings.palette.copy", L"Create copy..."),
		FbeLoadRuntimeStringByKey(L"fbe.settings.palette.copy_name", L"Name of the new interface palette:"), m_hWnd) != IDYES) return 0;
	name.Trim();
	if(!StagePaletteCopy(name)) MessageBox(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.invalid_name", L"Could not create the palette. Enter a name of up to 100 characters without control characters."),
		FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONWARNING);
	return 0;
}

bool CSettingsGeneralPage::PrepareCommit(SettingsFileTransaction& files, CString& error)
{
	for(const auto& id : m_deletedPaletteIds) {
		std::wstring detail;
		if(!ThemeManager::PrepareCustomPaletteDelete(id, files, detail)) { error = detail.c_str(); return false; }
	}
	for(const auto& palette : m_pendingPalettes) {
		std::wstring detail;
		const bool created = std::find(m_newPaletteIds.begin(), m_newPaletteIds.end(), palette.id) != m_newPaletteIds.end();
		if(!ThemeManager::PrepareCustomPaletteSave(palette, !created, files, detail)) { error = detail.c_str(); return false; }
	}
	return true;
}

void CSettingsGeneralPage::FinishCommit()
{
	if(m_paletteChanged && m_acceptedPaletteIndex >= INTERFACE_PALETTE_COUNT)
		_Settings.SetInterfacePalette(CString(L"custom:") + m_customPalettes[m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT].id.c_str());
}

bool CSettingsGeneralPage::Validate() { return true; }
void CSettingsGeneralPage::Commit()
{
	if(m_paletteChanged) {
		if(m_acceptedPaletteIndex < INTERFACE_PALETTE_COUNT) _Settings.SetInterfacePalette(PaletteId(m_acceptedPaletteIndex));
		else {
			const auto& selected = m_customPalettes[m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT];
			_Settings.SetInterfacePalette(CString(L"custom:") + selected.id.c_str());
			for(const auto& pending : m_pendingPalettes) if(pending.id == selected.id) { ThemeManager::SetCustomPalette(pending); break; }
		}
	}
	const int iconSize = m_scriptIconSize.GetCurSel();
	if(iconSize != CB_ERR) _Settings.SetScriptIconSize(static_cast<int>(m_scriptIconSize.GetItemData(iconSize)));
	const int mainIconSize = m_mainToolbarIconSize.GetCurSel();
	if(mainIconSize != CB_ERR) _Settings.SetMainToolbarIconSize(static_cast<int>(m_mainToolbarIconSize.GetItemData(mainIconSize)));
	const int interfaceTheme = m_interfaceTheme.GetCurSel();
	if(interfaceTheme >= INTERFACE_THEME_AUTOMATIC && interfaceTheme <= INTERFACE_THEME_DARK &&
		(!m_preserveUnavailableDark || interfaceTheme != INTERFACE_THEME_LIGHT))
		_Settings.SetInterfaceTheme(static_cast<InterfaceTheme>(interfaceTheme));
	else if(m_preserveUnavailableDark) ThemeManager::SetSelectedTheme(m_originalTheme);
	const int encodingIndex = m_defaultEncoding.GetCurSel();
	if(encodingIndex != CB_ERR) { CString encoding; m_defaultEncoding.GetLBText(encodingIndex, encoding); _Settings.SetDefaultEncoding(encoding); }
	_Settings.SetKeepEncoding(m_keepEncoding.GetCheck() == BST_CHECKED);
	_Settings.SetRestoreFilePosition(m_restorePosition.GetCheck() == BST_CHECKED);
	_Settings.SetCreateBackupFile(IsDlgButtonChecked(IDC_CREATE_BACKUP_FILE) == BST_CHECKED);
	_Settings.SetShowFullPathInWindowTitle(IsDlgButtonChecked(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE) == BST_CHECKED);
	_Settings.SetUpdateChannel(m_updateChannel.GetCurSel() == 1 ? UpdateChannel::Prerelease : UpdateChannel::Stable);
	_Settings.SetDescriptionLayout(m_descriptionLayout.GetCurSel() == 1 ? L"classic" : L"modern");
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
	if(m_originalCustomPaletteActive) ThemeManager::SetCustomPalette(m_originalCustomPalette);
	else ThemeManager::SetNamedPalette(m_originalPalette);
	ThemeManager::SetSelectedTheme(m_originalTheme);
	ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
	return true;
}
