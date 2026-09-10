#include "stdafx.h"
#include "XmlSourceThemes.h"
#include "settings\\SettingsNormalization.h"
#include "settings\\SettingsDefaults.h"
#include "settings\\hotkeys\\HotkeyStore.h"
#include "settings\\hotkeys\\HotkeyDefaults.h"
#include "settings\\words\\WordsStore.h"
#include "settings\\SettingsStore.h"

#include "Settings.h"

CSettings::CSettings():m_need_restart(false), keycodes(0)
{

}

CSettings::~CSettings()
{
}

void CSettings::Init()
{
	FbeSettings::Store::InitializeRegistry(m_key, m_key_path);
}

CString NormalizeScriptsFolderStoredPath(const CString& sourcePath)
{
	return FbeSettings::NormalizeScriptsFolderStoredPath(sourcePath);
}

CString ResolveScriptsFolderPath(const CString& storedPath)
{
	return FbeSettings::ResolveScriptsFolderPath(storedPath);
}

void CSettings::InitHotkeyGroups()
{
	FbeSettings::Hotkeys::BuildDefaults(m_hotkey_groups, m_interface_lang_id, GetNBSPChar());
}

void CSettings::Close()
{
	FbeSettings::Store::CloseRegistry(m_key);
}

void CSettings::Save()
{
	FbeSettings::Store::Save(*this);
}

void CSettings::Load()
{
	FbeSettings::Store::Load(*this);
}

CHotkeysGroup* CSettings::GetGroupByName(const CString& name)
{
	return FbeSettings::Hotkeys::FindGroup(m_hotkey_groups, name);
}

CHotkey* CSettings::GetHotkeyByName(const CString& name, CHotkeysGroup& group)
{
	return FbeSettings::Hotkeys::FindHotkey(group, name);
}

void CSettings::SaveHotkeyGroups()
{
	FbeSettings::Hotkeys::Save(m_hotkey_groups);
}

void CSettings::LoadHotkeyGroups()
{
	FbeSettings::Hotkeys::Load(m_hotkey_groups, keycodes);
}

bool CSettings::KeepEncoding()const
{
	return m_keep_encoding;
}
bool CSettings::XmlSrcWrap()const
{
	return m_xml_src_wrap;
}
bool CSettings::XmlSrcSyntaxHL()const
{
	return m_xml_src_syntaxHL;
}
DWORD CSettings::GetXmlSrcColorPalette()const
{
	if(m_xml_src_color_palette == XML_SRC_COLOR_PALETTE_LEGACY_CONTRAST)
		return XML_SRC_COLOR_PALETTE_FBE_LIGHT;
	if(m_xml_src_color_palette == XML_SRC_COLOR_PALETTE_LEGACY_HIGH_CONTRAST_DARK)
		return XML_SRC_COLOR_PALETTE_FBE_DARK;
	if(m_xml_src_color_palette == XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_LIGHT)
		return XML_SRC_COLOR_PALETTE_FBE_LIGHT;
	if(m_xml_src_color_palette == XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_DARK)
		return XML_SRC_COLOR_PALETTE_FBE_DARK;
	return m_xml_src_color_palette <= XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_DARK
		? m_xml_src_color_palette : XML_SRC_COLOR_PALETTE_FBE_LIGHT;
}
CString CSettings::GetXmlSrcThemeId()const
{
	return XmlSourceThemes::NormalizeThemeId(m_xml_src_theme_id);
}
CString CSettings::GetStoredXmlSrcThemeId()const
{
	return m_xml_src_theme_id;
}
DWORD CSettings::GetXmlSrcDefaultColor(DWORD palette, XmlSrcColorGroup group)
{
	static const XmlSrcStyleToken tokens[XML_SRC_COLOR_GROUP_COUNT] = {
		XML_SRC_STYLE_XML_TEXT,
		XML_SRC_STYLE_XML_TAG_NAME,
		XML_SRC_STYLE_XML_ATTRIBUTE_NAME,
		XML_SRC_STYLE_XML_ATTRIBUTE_VALUE,
		XML_SRC_STYLE_XML_COMMENT,
		XML_SRC_STYLE_EDITOR_BACKGROUND,
	};
	return GetXmlSrcThemeColor(palette,
		tokens[group < XML_SRC_COLOR_GROUP_COUNT ? group : XML_SRC_COLOR_TEXT]);
}

DWORD CSettings::GetXmlSrcThemeColor(DWORD palette, XmlSrcStyleToken token)
{
	if(palette == XML_SRC_COLOR_PALETTE_SYSTEM)
	{
		// Значение AppsUseLightTheme существует в Windows 10/11. В Windows 7
		// и при любой ошибке чтения выбираем светлую FBE Light.
		DWORD appsUseLightTheme = 1;
		DWORD valueSize = sizeof(appsUseLightTheme);
		const LONG result = ::RegGetValue(HKEY_CURRENT_USER,
			L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
			L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &appsUseLightTheme, &valueSize);
		palette = result == ERROR_SUCCESS && appsUseLightTheme == 0
			? XML_SRC_COLOR_PALETTE_FBE_DARK
			: XML_SRC_COLOR_PALETTE_FBE_LIGHT;
	}
	// Старые значения 1 и 4 больше не отображаются в списке схем. Если они
	// сохранены ранней сборкой, безопасно переводим их в ближайшие FBE-темы.
	if(palette == XML_SRC_COLOR_PALETTE_LEGACY_CONTRAST)
		palette = XML_SRC_COLOR_PALETTE_FBE_LIGHT;
	else if(palette == XML_SRC_COLOR_PALETTE_LEGACY_HIGH_CONTRAST_DARK)
		palette = XML_SRC_COLOR_PALETTE_FBE_DARK;
	else if(palette == XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_LIGHT)
		palette = XML_SRC_COLOR_PALETTE_FBE_LIGHT;
	else if(palette == XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_DARK)
		palette = XML_SRC_COLOR_PALETTE_FBE_DARK;
	else if(palette > XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_DARK)
		palette = XML_SRC_COLOR_PALETTE_FBE_LIGHT;
	if(token >= XML_SRC_STYLE_TOKEN_COUNT)
		token = XML_SRC_STYLE_EDITOR_FOREGROUND;

	DWORD color = 0;
	if(XmlSourceThemes::GetThemeColor(XmlSourceThemes::GetThemeIdForPalette(palette), token, color))
		return color;
	return RGB(32,34,36);

}
DWORD CSettings::GetXmlSrcColor(XmlSrcColorGroup group)const
{
	if(group >= XML_SRC_COLOR_GROUP_COUNT)
		group = XML_SRC_COLOR_TEXT;
	if(m_xml_src_colors[group] != XML_SRC_COLOR_DEFAULT)
		return m_xml_src_colors[group];

	static const XmlSrcStyleToken tokens[XML_SRC_COLOR_GROUP_COUNT] = {
		XML_SRC_STYLE_XML_TEXT,
		XML_SRC_STYLE_XML_TAG_NAME,
		XML_SRC_STYLE_XML_ATTRIBUTE_NAME,
		XML_SRC_STYLE_XML_ATTRIBUTE_VALUE,
		XML_SRC_STYLE_XML_COMMENT,
		XML_SRC_STYLE_EDITOR_BACKGROUND,
	};
	DWORD color = 0;
	if(XmlSourceThemes::GetThemeColor(GetXmlSrcThemeId(), tokens[group], color))
		return color;
	return GetXmlSrcDefaultColor(m_xml_src_color_palette, group);
}
bool CSettings::HasXmlSrcCustomColor(XmlSrcColorGroup group)const
{
	return group < XML_SRC_COLOR_GROUP_COUNT && m_xml_src_colors[group] != XML_SRC_COLOR_DEFAULT;
}
XmlSrcColorGroup CSettings::GetXmlSrcColorGroup(XmlSrcStyleToken token)
{
	switch(token)
	{
	case XML_SRC_STYLE_EDITOR_BACKGROUND:
		return XML_SRC_COLOR_BACKGROUND;
	case XML_SRC_STYLE_EDITOR_FOREGROUND:
	case XML_SRC_STYLE_XML_TEXT:
	case XML_SRC_STYLE_XML_ENTITY:
		return XML_SRC_COLOR_TEXT;
	case XML_SRC_STYLE_XML_TAG_NAME:
	case XML_SRC_STYLE_XML_TAG_DELIMITER:
		return XML_SRC_COLOR_TAG;
	case XML_SRC_STYLE_XML_ATTRIBUTE_NAME:
	case XML_SRC_STYLE_XML_NAMESPACE:
		return XML_SRC_COLOR_ATTRIBUTE;
	case XML_SRC_STYLE_XML_ATTRIBUTE_VALUE:
		return XML_SRC_COLOR_STRING;
	case XML_SRC_STYLE_XML_COMMENT:
		return XML_SRC_COLOR_COMMENT;
	default:
		return XML_SRC_COLOR_GROUP_COUNT;
	}
}

DWORD CSettings::GetXmlSrcStyleColor(XmlSrcStyleToken token)const
{
	const XmlSrcColorGroup group = GetXmlSrcColorGroup(token);
	// Group colors are overrides only.  Without an override every token keeps
	// its own exact color from the selected .fbetheme.
	if(group < XML_SRC_COLOR_GROUP_COUNT && HasXmlSrcCustomColor(group))
		return m_xml_src_colors[group];
	DWORD color = 0;
	if(XmlSourceThemes::GetThemeColor(GetXmlSrcThemeId(), token, color))
		return color;
	return GetXmlSrcThemeColor(m_xml_src_color_palette, token);
}

bool CSettings::XmlSrcTagHL()const
{
	return m_xml_src_tagHL;
}
bool CSettings::XmlSrcShowEOL()const
{
	return m_xml_src_showEOL;
}
bool CSettings::XmlSrcShowSpace()const
{
	return m_xml_src_showSpace;
}
bool CSettings::XmlSrcShowSpecialChars()const
{
	return m_xml_src_showSpecialChars;
}
DWORD CSettings::XmlSrcSpecialCharsStyle()const
{
	return m_xml_src_specialCharsStyle;
}
bool CSettings::FastMode()const
{
	return m_fast_mode;
}

bool CSettings::ViewStatusBar()const
{
	return m_view_status_bar;
}

bool CSettings::ViewDocumentTree()const
{
	return m_view_doc_tree;
}

bool CSettings::RestoreFilePosition()const
{
	return m_restore_file_position;
}

bool CSettings::NeedRestart()const
{
	return m_need_restart;
}

DWORD CSettings::GetSearchOptions()const
{
	return m_search_options;
}
DWORD CSettings::GetFontSize()const
{
	return m_font_size;
}
CString CSettings::GetFont()const
{
	return m_font;
}
CString CSettings::GetSrcFont()const
{
	return m_srcfont;
}
DWORD CSettings::XmlSrcTagHighlightMode()const { return m_xml_src_tagHL_mode; }
bool CSettings::XmlSrcTagHighlightAttributes()const { return m_xml_src_tagHL_attributes; }
bool CSettings::XmlSrcTagHighlightErrors()const { return m_xml_src_tagHL_errors; }
CString CSettings::GetEditorBackgroundKind()const { return m_editor_background_kind; }
CString CSettings::GetEditorBackgroundId()const { return m_editor_background_id; }
CString CSettings::GetEditorBackgroundCustomPath()const { return m_editor_background_custom_path; }
CString CSettings::GetEditorBackgroundLayout()const { return m_editor_background_layout; }
DWORD CSettings::GetSplitterPos()const
{
	return m_splitter_pos;
}
DWORD CSettings::GetFindResultsPaneHeight()const
{
	return m_find_results_pane_height;
}
CString CSettings::GetToolbarsSettings()const
{
	return m_toolbars_settings;
}
CString CSettings::GetScriptCommandIds()const
{
	return m_script_command_ids;
}

CSize CSettings::GetScriptsToolbarCustomizeSize() const
{
	return CSize(static_cast<int>(m_scripts_toolbar_customize_width), static_cast<int>(m_scripts_toolbar_customize_height));
}
bool CSettings::GetScriptsToolbarCustomizePlacement(WINDOWPLACEMENT& wpl) const
{
	if(m_scripts_toolbar_customize_placement.length != sizeof(WINDOWPLACEMENT)) return false;
	wpl = m_scripts_toolbar_customize_placement;
	wpl.showCmd = SW_SHOWNORMAL;
	return true;
}
CString CSettings::GetKeyPath()const
{
	return m_key_path;
}

const CRegKey& CSettings::GetKey()const
{
	return m_key;
}

// SeNS
bool CSettings::GetUseSpellChecker()const
{
	return m_usespell_check;
}

bool CSettings::GetHighlightMisspells()const
{
	return m_highlght_check;
}

CString CSettings::GetCustomDict()const
{
	return m_custom_dict;
}

DWORD CSettings::GetCustomDictCodepage()const
{
	return m_custom_dict_codepage;
}

CString CSettings::GetNBSPChar()const
{
	return m_nbsp_char;
}

CString CSettings::GetOldNBSPChar()const
{
	// В текстовом DOM неразрывный пробел хранится самим символом U+00A0,
	// а не XML-сущностью &nbsp;. Возвращаем фактическое предыдущее значение,
	// иначе первая замена после выбора другого обозначения ничего не меняет.
	return m_old_nbsp;
}

bool CSettings::GetChangeKeybLayout()const
{
	return m_change_kbd_layout_check;
}

DWORD CSettings::GetKeybLayout()const
{
	return m_keyb_layout;
}

bool CSettings::XMLSrcShowLineNumbers() const
{
	return m_show_line_numbers;
}

DWORD CSettings::GetImageType() const
{
	return m_image_type;
}

DWORD CSettings::GetJpegQuality() const
{
	return m_jpeg_quality;
}

DWORD CSettings::GetImageImportFormat() const { return m_image_import_format; }
DWORD CSettings::GetImageImportJpegQuality() const { return m_image_import_jpeg_quality; }
bool CSettings::GetImageImportKeepSupported() const { return m_image_import_keep_supported; }

///
bool CSettings::GetExtElementStyle(const CString& elem)const
{
	std::map<CString, bool>::const_iterator member = m_desc.elements.find(elem);
	if(member == m_desc.elements.end())
		return false;
	else return member->second;
}

bool CSettings::GetWindowPosition(WINDOWPLACEMENT &wpl)const
{
	if(m_wnd_placement.length != sizeof(WINDOWPLACEMENT))
		return false;

	wpl = m_wnd_placement;
	if(wpl.showCmd == SW_HIDE)
		wpl.showCmd = SW_SHOWNORMAL;
	return true;
}

bool CSettings::GetWordsDlgPosition(WINDOWPLACEMENT &wpl)const
{
	if(m_words_dlg_placement.length != sizeof(WINDOWPLACEMENT))
		return false;

	wpl = m_words_dlg_placement;
	if(wpl.showCmd == SW_HIDE)
		wpl.showCmd = SW_SHOWNORMAL;
	return true;
}

CString CSettings::GetDefaultEncoding()const
{
	return m_default_encoding;
}

DWORD CSettings::GetColorBG()const
{
	return m_collorBG;
}

DWORD CSettings::GetColorFG()const
{
	return m_collorFG;
}

DWORD CSettings::GetInterfaceLanguageID()const
{
	return FbeSettings::NormalizeInterfaceLanguageID(m_interface_lang_id);
}

DWORD CSettings::GetEffectiveInterfaceLanguageID()const
{
	const DWORD langId = GetInterfaceLanguageID();
	if(langId != FBE_INTERFACE_LANGUAGE_AUTO)
		return langId;

	wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = {};
	if(::GetUserDefaultLocaleName(localeName, _countof(localeName)) > 0)
		return FbeSettings::InterfaceLanguageFromLocaleName(localeName);

	return FbeSettings::NormalizeInterfaceLanguageID(PRIMARYLANGID(GetUserDefaultLangID()));
}

CString CSettings::GetInterfaceLocaleName()const
{
	if(m_interface_lang_id == FBE_INTERFACE_LANGUAGE_AUTO)
	{
		wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = {};
		if(::GetUserDefaultLocaleName(localeName, _countof(localeName)) > 0 && localeName[0] != 0)
		{
			if(::lstrcmpiW(localeName, L"en-US") == 0 || ::lstrcmpiW(localeName, L"ru-RU") == 0 ||
				::lstrcmpiW(localeName, L"uk-UA") == 0 || ::lstrcmpiW(localeName, L"de-DE") == 0 ||
				::lstrcmpiW(localeName, L"fr-FR") == 0 || ::lstrcmpiW(localeName, L"es-ES") == 0 ||
				::lstrcmpiW(localeName, L"it-IT") == 0 || ::lstrcmpiW(localeName, L"pl-PL") == 0 ||
				::lstrcmpiW(localeName, L"pt-PT") == 0 || ::lstrcmpiW(localeName, L"nl-NL") == 0 ||
				::lstrcmpiW(localeName, L"cs-CZ") == 0 || ::lstrcmpiW(localeName, L"bg-BG") == 0)
				return localeName;
		}
		return L"en-US";
	}

	switch(GetInterfaceLanguageID())
	{
	case FBE_INTERFACE_LANGUAGE_RUSSIAN:
		return L"ru-RU";
	case FBE_INTERFACE_LANGUAGE_UKRAINIAN:
		return L"uk-UA";
	case FBE_INTERFACE_LANGUAGE_GERMAN:
		return L"de-DE";
	case FBE_INTERFACE_LANGUAGE_FRENCH:
		return L"fr-FR";
	case FBE_INTERFACE_LANGUAGE_SPANISH:
		return L"es-ES";
	case FBE_INTERFACE_LANGUAGE_ITALIAN:
		return L"it-IT";
	case FBE_INTERFACE_LANGUAGE_POLISH:
		return L"pl-PL";
	case FBE_INTERFACE_LANGUAGE_PORTUGUESE:
		return L"pt-PT";
	case FBE_INTERFACE_LANGUAGE_DUTCH:
		return L"nl-NL";
	case FBE_INTERFACE_LANGUAGE_CZECH:
		return L"cs-CZ";
	case FBE_INTERFACE_LANGUAGE_BULGARIAN:
		return L"bg-BG";
	case FBE_INTERFACE_LANGUAGE_ENGLISH:
	default:
		return L"en-US";
	}
}

CString CSettings::GetLocalizedGenresFileName()const
{
	switch(GetEffectiveInterfaceLanguageID())
	{
	case FBE_INTERFACE_LANGUAGE_RUSSIAN:
		return L"genres.rus.txt";
	case FBE_INTERFACE_LANGUAGE_UKRAINIAN:
		return L"genres.ukr.txt";
	default:
		return L"genres.txt";
	}
}

DWORD CSettings::StatusBarPanes()const
{
	return m_status_bar_panes;
}

CString CSettings::GetKeyboardLayoutId() const { return m_keyboard_layout_id; }

GenreCatalog CSettings::GetGenreCatalog()const
{
	return m_genre_catalog;
}

CString CSettings::GetGenreCatalogFileName()const
{
	if(m_genre_catalog != GenreCatalog::Librusec)
		return GetLocalizedGenresFileName();

	// There is no Ukrainian Librusec payload. Keep the selection explicit and
	// deliberately fall back to the localized standard list.
	switch(GetEffectiveInterfaceLanguageID())
	{
	case FBE_INTERFACE_LANGUAGE_RUSSIAN:
		return L"genres.rus.librusec.txt";
	case FBE_INTERFACE_LANGUAGE_UKRAINIAN:
		return L"genres.ukr.txt";
	default:
		return L"genres.librusec.txt";
	}
}

CString CSettings::GetGenreCatalogLegacyFileName()const
{
	if(m_genre_catalog != GenreCatalog::Librusec)
		return CString();

	switch(GetEffectiveInterfaceLanguageID())
	{
	case FBE_INTERFACE_LANGUAGE_RUSSIAN:
		return L"genres.rus.txt_L";
	case FBE_INTERFACE_LANGUAGE_UKRAINIAN:
		return CString();
	default:
		return L"genres.txt_L";
	}
}

CString CSettings::ResolveGenreCatalogFileName()const
{
	const CString primary = GetGenreCatalogFileName();
	if(::GetFileAttributes(U::GetProgDirFile(primary)) != INVALID_FILE_ATTRIBUTES)
		return primary;

	const CString legacy = GetGenreCatalogLegacyFileName();
	if(!legacy.IsEmpty() && ::GetFileAttributes(U::GetProgDirFile(legacy)) != INVALID_FILE_ATTRIBUTES)
		return legacy;

	return primary;
}

CString CSettings::GetInterfaceLanguageName()const
{
	switch(GetEffectiveInterfaceLanguageID())
	{
	case FBE_INTERFACE_LANGUAGE_RUSSIAN:
		return L"russian";
	case FBE_INTERFACE_LANGUAGE_UKRAINIAN:
		return L"ukrainian";
	case FBE_INTERFACE_LANGUAGE_GERMAN:
		return L"german";
	case FBE_INTERFACE_LANGUAGE_FRENCH:
		return L"french";
	case FBE_INTERFACE_LANGUAGE_SPANISH:
		return L"spanish";
	case FBE_INTERFACE_LANGUAGE_ITALIAN:
		return L"italian";
	case FBE_INTERFACE_LANGUAGE_POLISH:
		return L"polish";
	case FBE_INTERFACE_LANGUAGE_PORTUGUESE:
		return L"portuguese";
	case FBE_INTERFACE_LANGUAGE_DUTCH:
		return L"dutch";
	case FBE_INTERFACE_LANGUAGE_CZECH:
		return L"czech";
	case FBE_INTERFACE_LANGUAGE_BULGARIAN:
		return L"bulgarian";
	default:
		return L"english";
	}
}

CString CSettings::GetScriptsFolder() const
{
	return GetResolvedScriptsFolder();
}

CString CSettings::GetScriptsFolderStored() const
{
	return m_scripts_folder;
}

CString CSettings::GetResolvedScriptsFolder() const
{
	return FbeSettings::ResolveScriptsFolderPath(m_scripts_folder);
}

CString CSettings::GetDefaultScriptsFolderStored() const
{
	return FbeSettings::NormalizeScriptsFolderStoredPath(DEFAULT_SCRIPTS_FOLDER);
}

CString CSettings::GetDefaultScriptsFolder()
{
	return FbeSettings::ResolveScriptsFolderPath(GetDefaultScriptsFolderStored());
}

bool CSettings::IsDefaultScriptsFolder()
{
	return GetResolvedScriptsFolder().CompareNoCase(GetDefaultScriptsFolder()) == 0;
}

bool CSettings::GetInsImageAsking() const
{
	return m_insimage_ask;
}

bool CSettings::GetIsInsClearImage() const
{
	return m_ins_clear_image;
}

bool CSettings::GetCreateBackupFile() const
{
	return m_create_backup_file;
}

bool CSettings::GetShowFullPathInWindowTitle() const
{
	return m_show_full_path_in_window_title;
}

UpdateChannel CSettings::GetUpdateChannel() const
{
	return m_update_channel;
}

bool CSettings::GetShowWordsExcls() const
{
	return m_show_words_excls;
}

bool CSettings::GetDocTreeItemState(const ATL::CString&item, bool default_state)
{
	std::map<CString, bool>::const_iterator member = m_tree_items.items.find(item);
	if(member == m_tree_items.items.end())
		return default_state;
	else return member->second;
}

void CSettings::SetKeepEncoding(bool keep, bool apply)
{
	m_keep_encoding = keep;
	if(apply)
		Save();
}

void CSettings::SetSearchOptions(DWORD opt, bool apply)
{
	m_search_options = opt;
	if(apply)
		Save();
}

void CSettings::SetFontSize(DWORD size, bool apply)
{
	m_font_size = size;
	if(apply)
		Save();
}

void CSettings::SetXmlSrcWrap(bool wrap, bool apply)
{
	m_xml_src_wrap = wrap;
	if(apply)
		Save();
}

void CSettings::SetXmlSrcSyntaxHL(bool hl, bool apply)
{
	m_xml_src_syntaxHL = hl;
	if(apply)
		Save();
}

void CSettings::SetXmlSrcColorPalette(DWORD palette, bool apply)
{
	if(palette == XML_SRC_COLOR_PALETTE_LEGACY_CONTRAST)
		palette = XML_SRC_COLOR_PALETTE_FBE_LIGHT;
	else if(palette == XML_SRC_COLOR_PALETTE_LEGACY_HIGH_CONTRAST_DARK)
		palette = XML_SRC_COLOR_PALETTE_FBE_DARK;
	else if(palette == XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_LIGHT)
		palette = XML_SRC_COLOR_PALETTE_FBE_LIGHT;
	else if(palette == XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_DARK)
		palette = XML_SRC_COLOR_PALETTE_FBE_DARK;
	m_xml_src_color_palette = palette <= XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_DARK ? palette : XML_SRC_COLOR_PALETTE_FBE_LIGHT;
	m_xml_src_theme_id = XmlSourceThemes::GetThemeIdForPalette(m_xml_src_color_palette);
	if(apply)
		Save();
}

void CSettings::SetXmlSrcThemeId(const CString& id, bool apply)
{
	m_xml_src_theme_id = XmlSourceThemes::NormalizeThemeId(id);
	m_xml_src_color_palette = XmlSourceThemes::GetPaletteForThemeId(m_xml_src_theme_id);
	if(apply)
		Save();
}

void CSettings::SetXmlSrcColor(XmlSrcColorGroup group, DWORD color, bool apply)
{
	if(group < XML_SRC_COLOR_GROUP_COUNT)
		m_xml_src_colors[group] = color;
	if(apply)
		Save();
}

void CSettings::SetXmlSrcTagHL(bool hl, bool apply)
{
	m_xml_src_tagHL = hl;
	if(apply)
		Save();
}
void CSettings::SetXmlSrcShowEOL(bool eol, bool apply)
{
	m_xml_src_showEOL = eol;
	if(apply)
		Save();
}
void CSettings::SetXmlSrcShowSpace(bool eol, bool apply)
{
	m_xml_src_showSpace = eol;
	if(apply)
		Save();
}
void CSettings::SetXmlSrcShowSpecialChars(bool show, bool apply)
{
	m_xml_src_showSpecialChars = show;
	if(apply)
		Save();
}
void CSettings::SetXmlSrcSpecialCharsStyle(DWORD style, bool apply)
{
	m_xml_src_specialCharsStyle = style == XML_SRC_SPECIAL_CHARS_TEXT_LABELS ? XML_SRC_SPECIAL_CHARS_TEXT_LABELS : XML_SRC_SPECIAL_CHARS_WORD_LIKE;
	if(apply)
		Save();
}
void CSettings::SetFastMode(bool mode,  bool apply)
{
	m_fast_mode = mode;
	if(apply)
		Save();
}

void CSettings::SetFont(const CString& font, bool apply)
{
	m_font = font;
	if(apply)
		Save();
}

void CSettings::SetSrcFont(const CString& font, bool apply)
{
	m_srcfont = font;
	if(apply)
		Save();
}
void CSettings::SetXmlSrcTagHighlightMode(DWORD mode, bool apply) { m_xml_src_tagHL_mode = mode ? 1 : 0; if(apply) Save(); }
void CSettings::SetXmlSrcTagHighlightAttributes(bool enabled, bool apply) { m_xml_src_tagHL_attributes = enabled; if(apply) Save(); }
void CSettings::SetXmlSrcTagHighlightErrors(bool enabled, bool apply) { m_xml_src_tagHL_errors = enabled; if(apply) Save(); }
void CSettings::SetEditorBackgroundKind(const CString& value, bool apply) { m_editor_background_kind = value == L"builtin" || value == L"custom" ? value : L"none"; if(apply) Save(); }
void CSettings::SetEditorBackgroundId(const CString& value, bool apply) { m_editor_background_id = value; if(apply) Save(); }
void CSettings::SetEditorBackgroundCustomPath(const CString& value, bool apply) { m_editor_background_custom_path = value; if(apply) Save(); }
void CSettings::SetEditorBackgroundLayout(const CString& value, bool apply) { m_editor_background_layout = value == L"center" || value == L"contain" || value == L"cover" ? value : L"tile"; if(apply) Save(); }

void CSettings::SetViewStatusBar(bool view, bool apply)
{
	m_view_status_bar = view;
	if(apply)
		Save();
}

void CSettings::SetViewDocumentTree(bool view, bool apply)
{
	m_view_doc_tree = view;
	if(apply)
		Save();
}

void CSettings::SetSplitterPos(DWORD pos, bool apply)
{
	m_splitter_pos = pos;
	if(apply)
		Save();
}
void CSettings::SetFindResultsPaneHeight(DWORD height, bool apply)
{
	m_find_results_pane_height = height;
	if(apply)
		Save();
}

void CSettings::SetToolbarsSettings(CString& settings, bool apply)
{
	m_toolbars_settings = settings;
	if(apply)
		Save();
}

void CSettings::SetExtElementStyle(const CString& elem, bool ext, bool apply)
{
	m_desc.elements[elem] = ext;
	if(apply)
		Save();
}

void CSettings::SetWindowPosition(const WINDOWPLACEMENT &wpl, bool apply)
{
	m_wnd_placement = wpl;
	if(m_wnd_placement.showCmd == SW_HIDE)
		m_wnd_placement.showCmd = SW_SHOWNORMAL;
	if(apply)
		Save();
}

void CSettings::SetWordsDlgPosition(const WINDOWPLACEMENT &wpl, bool apply)
{
	m_words_dlg_placement = wpl;
	if(m_words_dlg_placement.showCmd == SW_HIDE)
		m_words_dlg_placement.showCmd = SW_SHOWNORMAL;
	if(apply)
		Save();
}

void CSettings::SetDefaultEncoding(const CString &enc, bool apply)
{
	m_default_encoding = enc;
	if(apply)
		Save();
}

void CSettings::SetColorBG(DWORD col, bool apply)
{
	m_collorBG = col;
	if(apply)
		Save();
}

void CSettings::SetColorFG(DWORD col, bool apply)
{
	m_collorFG = col;
	if(apply)
		Save();
}

void CSettings::SetRestoreFilePosition(bool restore, bool apply)
{
	m_restore_file_position = restore;
	if(apply)
		Save();
}

void CSettings::SetInterfaceLanguage(DWORD lang_id, bool apply)
{
	lang_id = FbeSettings::NormalizeInterfaceLanguageID(lang_id);
	if(m_interface_lang_id != lang_id)
	{
		m_interface_lang_id = lang_id;
		if(apply)
			Save();
	}
}

void CSettings::SetStatusBarPanes(DWORD panes, bool apply)
{
	m_status_bar_panes = panes;
	if(apply)
		Save();
}

void CSettings::SetGenreCatalog(GenreCatalog catalog, bool apply)
{
	if(m_genre_catalog != catalog)
	{
		m_genre_catalog = catalog;
		if(apply)
			Save();
	}
}

void CSettings::SetScriptsFolder(const CString& fullpath, bool apply)
{
	const CString normalized = FbeSettings::NormalizeScriptsFolderStoredPath(fullpath);
	if(m_scripts_folder.CompareNoCase(normalized) != 0)
	{
		m_scripts_folder = normalized;
	}
	if(apply) Save();
}

void CSettings::SetInsImageAsking(bool ask, bool apply)
{
	m_insimage_ask = ask;
	if(apply)
		Save();
}

void CSettings::SetIsInsClearImage(bool clear, bool apply)
{
	m_ins_clear_image = clear;
	if(apply)
		Save();
}

void CSettings::SetCreateBackupFile(bool createBackup, bool apply)
{
	m_create_backup_file = createBackup;
	if(apply)
		Save();
}

void CSettings::SetShowFullPathInWindowTitle(bool show, bool apply)
{
	m_show_full_path_in_window_title = show;
	if(apply)
		Save();
}

void CSettings::SetUpdateChannel(UpdateChannel channel, bool apply)
{
	m_update_channel = channel == UpdateChannel::Prerelease ? UpdateChannel::Prerelease : UpdateChannel::Stable;
	if(apply) Save();
}

void CSettings::SetShowWordsExcls(bool show, bool apply)
{
	m_show_words_excls = show;
	if(apply)
		Save();
}

void CSettings::SetNeedRestart()
{
	m_need_restart = true;
}

void CSettings::SetDocTreeItemState(const ATL::CString &item, bool state)
{
	m_tree_items.items[item] = state;
	Save();
}

// SeNS
void CSettings::SetUseSpellChecker(const bool value, bool apply)
{
	m_usespell_check = value;
	if (!value) 
		SetHighlightMisspells(value, apply);
	if (apply) Save();
}

void CSettings::SetHighlightMisspells(const bool value, bool apply)
{
	m_highlght_check = value;
	if (apply) Save();
}

void CSettings::SetCustomDict(const ATL::CString &value, bool apply)
{
	m_custom_dict.SetString(value);
	if (apply) Save();
}

void CSettings::SetCustomDictCodepage(const DWORD value, bool apply)
{
	m_custom_dict_codepage = value;
	if (apply) Save();
}

void CSettings::SetNBSPChar(const ATL::CString &value, bool apply)
{
	if (value.Compare(m_nbsp_char) != 0)
	{
		m_old_nbsp.SetString(m_nbsp_char);
		m_nbsp_char.SetString(value);
		if (apply) Save();
	}
}

void CSettings::SetChangeKeybLayout(const bool value, bool apply)
{
	m_change_kbd_layout_check = value;
	if (apply) Save();
}

void CSettings::SetKeybLayout(const DWORD value, bool apply)
{
	m_keyb_layout = value;
	if (apply) Save();
}

void CSettings::SetKeyboardLayoutId(const CString& value, bool apply)
{
	CString id(value); id.Trim(); id.MakeUpper();
	if(id.GetLength() == 8 && id.SpanIncluding(L"0123456789ABCDEF").GetLength() == 8)
		m_keyboard_layout_id = id;
	if(apply) Save();
}

void CSettings::SetXMLSrcShowLineNumbers(const bool value, bool apply)
{
	m_show_line_numbers = value;
	if (apply) Save();
}

void CSettings::SetImageType(const DWORD value, bool apply)
{
	m_image_type = FbeSettings::NormalizeImageType(value);
	if (apply) Save();
}

void CSettings::SetJpegQuality(const DWORD value, bool apply)
{
	m_jpeg_quality = FbeSettings::NormalizeJpegQuality(value);
	if (apply) Save();
}


void CSettings::LoadWords()
{
	FbeSettings::Words::Load(m_words);
}

void CSettings::SetImageImportFormat(const DWORD value, bool apply) { m_image_import_format = min(2u, value); if(apply) Save(); }
void CSettings::SetImageImportJpegQuality(const DWORD value, bool apply) { m_image_import_jpeg_quality = max(1u, min(100u, value)); if(apply) Save(); }
void CSettings::SetImageImportKeepSupported(const bool value, bool apply) { m_image_import_keep_supported = value; if(apply) Save(); }

void CSettings::SetScriptCommandIds(const CString& ids, bool apply)
{
	m_script_command_ids = ids;
	if(apply)
		Save();
}

void CSettings::SetScriptsToolbarCustomizeSize(const CSize& size, bool apply)
{
	if(size.cx >= 300 && size.cy >= 200)
	{
		m_scripts_toolbar_customize_width = static_cast<DWORD>(size.cx);
		m_scripts_toolbar_customize_height = static_cast<DWORD>(size.cy);
	}
	if(apply) Save();
}
void CSettings::SetScriptsToolbarCustomizePlacement(const WINDOWPLACEMENT& wpl, bool apply)
{
	m_scripts_toolbar_customize_placement = wpl;
	m_scripts_toolbar_customize_placement.length = sizeof(WINDOWPLACEMENT);
	m_scripts_toolbar_customize_placement.showCmd = SW_SHOWNORMAL;
	m_scripts_toolbar_customize_placement.flags = 0;
	if(apply) Save();
}

void CSettings::SaveWords()
{
	FbeSettings::Words::Save(m_words);
}
