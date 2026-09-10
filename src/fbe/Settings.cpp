#include "stdafx.h"
#include "XmlSourceThemes.h"
#include "settings\\SettingsNormalization.h"
#include "settings\\hotkeys\\HotkeyStore.h"
#include "settings\\hotkeys\\HotkeyDefaults.h"
#include "settings\\words\\WordsStore.h"
#include "settings\\SettingsStore.h"

enum KEY_TYPE
{
	KEY_INT,
	KEY_UINT,
	KEY_ULONG,
	KEY_BOOL,
	KEY_STRING,
	KEY_STRUCT
};

CString GetStringedProperty(void* member, KEY_TYPE type)
{
	switch(type)
	{
	case KEY_INT:
		{
			CString temp;
			temp.Format(L"%d", *(int*)member);
			return temp;
		}
	case KEY_UINT:
	case KEY_ULONG:
		{
			CString temp;
			temp.Format(L"%u", *(unsigned long*)member);
			return temp;
		}
	case KEY_BOOL:
		{
			CString temp;
			temp.Format(L"%s", *(bool*)member ? L"true" : L"false");
			return temp;
		}
	case KEY_STRING:
		return *(CString*)member;
	default:
		return CString();
	}
}

bool StrToBool(CString sValue)
{
	return sValue == L"true";
}

// Settings XML nodes
const wchar_t KEEP_ENCODING_KEY[]		= L"KeepEncoding";
const wchar_t DEFAULT_ENCODING_KEY[]	= L"DefaultSaveEncoding";
const wchar_t SEARCH_OPTIONS_KEY[]		= L"SearchOptions";
const wchar_t COLOR_BG_KEY[]			= L"ColorBG";
const wchar_t COLOR_FG_KEY[]			= L"ColorFG";
const wchar_t FONT_SIZE_KEY[]			= L"FontSize";
const wchar_t XML_SRC_WRAP_KEY[]		= L"XMLSrcWrap";
const wchar_t XML_SRC_SYNTAX_HL_KEY[]	= L"XMLSrcSyntaxHL";
const wchar_t XML_SRC_COLOR_PALETTE_KEY[] = L"XMLSrcColorPalette";
const wchar_t XML_SRC_THEME_ID_KEY[] = L"XMLSrcThemeId";
const wchar_t XML_SRC_COLOR_TEXT_KEY[] = L"XMLSrcColorText";
const wchar_t XML_SRC_COLOR_TAG_KEY[] = L"XMLSrcColorTag";
const wchar_t XML_SRC_COLOR_ATTRIBUTE_KEY[] = L"XMLSrcColorAttribute";
const wchar_t XML_SRC_COLOR_STRING_KEY[] = L"XMLSrcColorString";
const wchar_t XML_SRC_COLOR_COMMENT_KEY[] = L"XMLSrcColorComment";
const wchar_t XML_SRC_COLOR_BACKGROUND_KEY[] = L"XMLSrcColorBackground";
const wchar_t XML_SRC_TAG_HL_KEY[]		= L"XMLSrcTagHL";
const wchar_t XML_SRC_TAG_HL_MODE_KEY[] = L"XMLSrcTagHLMode";
const wchar_t XML_SRC_TAG_HL_ATTRIBUTES_KEY[] = L"XMLSrcTagHLAttributes";
const wchar_t XML_SRC_TAG_HL_ERRORS_KEY[] = L"XMLSrcTagHLErrors";
const wchar_t XML_SRC_SHOW_EOL_KEY[]	= L"XMLSrcShowEOL";
const wchar_t XML_SRC_SHOW_SPACE_KEY[]	= L"XMLSrcShowSpace";
const wchar_t XML_SRC_SHOW_SPECIAL_CHARS_KEY[] = L"XMLSrcShowSpecialChars";
const wchar_t XML_SRC_SPECIAL_CHARS_STYLE_KEY[] = L"XMLSrcSpecialCharsStyle";
const wchar_t FAST_MODE_KEY[]			= L"FastMode";
const wchar_t FONT_KEY[]				= L"Font";
const wchar_t EDITOR_BACKGROUND_KIND_KEY[] = L"EditorBackgroundKind";
const wchar_t EDITOR_BACKGROUND_ID_KEY[] = L"EditorBackgroundId";
const wchar_t EDITOR_BACKGROUND_CUSTOM_PATH_KEY[] = L"EditorBackgroundCustomPath";
const wchar_t EDITOR_BACKGROUND_LAYOUT_KEY[] = L"EditorBackgroundLayout";
const wchar_t SRC_FONT_KEY[]			= L"SrcFont";
const wchar_t VIEW_STATUS_BAR_KEY[]		= L"ViewStatusBar";
const wchar_t STATUS_BAR_PANES_KEY[]		= L"StatusBarPanes";
const wchar_t VIEW_DOCUMENT_TREE_KEY[]	= L"ViewDocumentTree";
const wchar_t SPLITTER_POS_KEY[]		= L"SplitterPos";
const wchar_t FIND_RESULTS_PANE_HEIGHT_KEY[] = L"FindResultsPaneHeight";
const wchar_t TOOLBARS_SETTINGS_KEY[]	= L"Toolbars";
const wchar_t SCRIPT_COMMAND_IDS_KEY[] = L"ScriptCommandIds";
const wchar_t SCRIPTS_TOOLBAR_CUSTOMIZE_SIZE_KEY[] = L"ScriptsToolbarCustomizeSize";
const wchar_t SCRIPTS_TOOLBAR_CUSTOMIZE_PLACEMENT_KEY[] = L"ScriptsToolbarCustomizePlacement";
const wchar_t RESTORE_FILE_POS_KEY[]	= L"RestoreFilePosition";
const wchar_t INTERFACE_LANG_KEY[]		= L"IntefaceLangID";
const wchar_t GENRE_CATALOG_KEY[]       = L"GenreCatalog";
const wchar_t SCRIPTS_FOLDER_KEY[]		= L"ScriptsFolder";

// Added by SeNS
const wchar_t USESPELLER_CHECK_KEY[]	= L"UseSpellChecker";
const wchar_t HIGHLIGHT_CHECK_KEY[]		= L"HighlightMisspells";
const wchar_t CUSTOM_DICT_KEY[]			= L"CustomDict";
const wchar_t CUSTOM_DICT_CODEPAGE_KEY[]= L"CustomDictCodePage";
const wchar_t NBSPCHAR_KEY[]			= L"NBSPChar";
const wchar_t CHANGE_KEYBD_CHECK_KEY[]	= L"ChangeKeybLayout";
const wchar_t KEYB_LAYOUT_KEY[]			= L"KeyboardLayout";
const wchar_t KEYB_LAYOUT_ID_KEY[]		= L"KeyboardLayoutId";
const wchar_t SHOW_LINE_NUMBERS_KEY[]	= L"XMLSrcShowLineNumbers";
const wchar_t IMAGE_TYPE_KEY[]			= L"PasteImageType";
const wchar_t JPEG_QUALITY_KEY[]		= L"JpegQuality";
const wchar_t IMAGE_IMPORT_FORMAT_KEY[] = L"ImageImportFormat";
const wchar_t IMAGE_IMPORT_JPEG_QUALITY_KEY[] = L"ImageImportJpegQuality";
const wchar_t IMAGE_IMPORT_KEEP_SUPPORTED_KEY[] = L"ImageImportKeepSupported";
// 

const wchar_t INSIMAGE_ASKING[]			= L"InsImageDialog";
const wchar_t SCRIPTS_HKEY_ERR_NTF[]	= L"ScrHkErrDialog";
const wchar_t INS_CLEAR_IMAGE[]			= L"InsClearImage";
const wchar_t CREATE_BACKUP_FILE_KEY[]		= L"CreateBackupFile";
const wchar_t SHOW_FULL_PATH_IN_WINDOW_TITLE_KEY[] = L"ShowFullPathInWindowTitle";
const wchar_t UPDATE_CHANNEL_KEY[] = L"UpdateChannel";
const wchar_t WINDOW_POSITION[]			= L"WindowPosition";
const wchar_t WORDS_DLG_POSITION[]		= L"WordsDlgPosition";
const wchar_t SHOW_WORDS_EXCLUSIONS[]	= L"ShowWordsExclusions";

// Default values for string settings
const wchar_t DEFAULT_ENCODING[]		= L"utf-8";
const wchar_t DEFAULT_FONT[]			= L"Trebuchet MS";
const wchar_t DEFAULT_SRCFONT[]			= L"Lucida Console";
const wchar_t DEFAULT_SCRIPTS_FOLDER[]	= L"Scripts";

#include "Settings.h"

#include "ElementDescMnr.h"
extern CElementDescMnr _EDMnr;

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

// ISerializable interface
int CSettings::GetProperties(std::vector<CString>& properties)
{
	properties.push_back(KEEP_ENCODING_KEY);
	properties.push_back(DEFAULT_ENCODING_KEY);
	properties.push_back(SEARCH_OPTIONS_KEY);
	properties.push_back(COLOR_BG_KEY);
	properties.push_back(COLOR_FG_KEY);
	properties.push_back(FONT_SIZE_KEY);
	properties.push_back(XML_SRC_WRAP_KEY);
	properties.push_back(XML_SRC_SYNTAX_HL_KEY);
	properties.push_back(XML_SRC_COLOR_PALETTE_KEY);
	properties.push_back(XML_SRC_THEME_ID_KEY);
	properties.push_back(XML_SRC_COLOR_TEXT_KEY);
	properties.push_back(XML_SRC_COLOR_TAG_KEY);
	properties.push_back(XML_SRC_COLOR_ATTRIBUTE_KEY);
	properties.push_back(XML_SRC_COLOR_STRING_KEY);
	properties.push_back(XML_SRC_COLOR_COMMENT_KEY);
	properties.push_back(XML_SRC_TAG_HL_KEY);
	properties.push_back(XML_SRC_TAG_HL_MODE_KEY);
	properties.push_back(XML_SRC_TAG_HL_ATTRIBUTES_KEY);
	properties.push_back(XML_SRC_TAG_HL_ERRORS_KEY);
	properties.push_back(XML_SRC_SHOW_EOL_KEY);
	properties.push_back(XML_SRC_SHOW_SPACE_KEY);
	properties.push_back(XML_SRC_SHOW_SPECIAL_CHARS_KEY);
	properties.push_back(XML_SRC_SPECIAL_CHARS_STYLE_KEY);
	properties.push_back(FAST_MODE_KEY);
	properties.push_back(FONT_KEY);
	properties.push_back(EDITOR_BACKGROUND_KIND_KEY);
	properties.push_back(EDITOR_BACKGROUND_ID_KEY);
	properties.push_back(EDITOR_BACKGROUND_CUSTOM_PATH_KEY);
	properties.push_back(EDITOR_BACKGROUND_LAYOUT_KEY);
	properties.push_back(SRC_FONT_KEY);
	properties.push_back(VIEW_STATUS_BAR_KEY);
	properties.push_back(STATUS_BAR_PANES_KEY);
	properties.push_back(VIEW_DOCUMENT_TREE_KEY);
	properties.push_back(SPLITTER_POS_KEY);
	properties.push_back(FIND_RESULTS_PANE_HEIGHT_KEY);
	properties.push_back(TOOLBARS_SETTINGS_KEY);
	properties.push_back(SCRIPT_COMMAND_IDS_KEY);
	properties.push_back(SCRIPTS_TOOLBAR_CUSTOMIZE_SIZE_KEY);
	properties.push_back(RESTORE_FILE_POS_KEY);
	properties.push_back(INTERFACE_LANG_KEY);
	properties.push_back(GENRE_CATALOG_KEY);
	properties.push_back(SCRIPTS_FOLDER_KEY);
	// SeNS
	properties.push_back(USESPELLER_CHECK_KEY);
	properties.push_back(HIGHLIGHT_CHECK_KEY);
	properties.push_back(CUSTOM_DICT_KEY);
	properties.push_back(CUSTOM_DICT_CODEPAGE_KEY);
	properties.push_back(NBSPCHAR_KEY);
	properties.push_back(CHANGE_KEYBD_CHECK_KEY);
	properties.push_back(KEYB_LAYOUT_KEY);
	properties.push_back(KEYB_LAYOUT_ID_KEY);
	properties.push_back(SHOW_LINE_NUMBERS_KEY);
	properties.push_back(IMAGE_TYPE_KEY);
	properties.push_back(JPEG_QUALITY_KEY);
	properties.push_back(IMAGE_IMPORT_FORMAT_KEY);
	properties.push_back(IMAGE_IMPORT_JPEG_QUALITY_KEY);
	properties.push_back(IMAGE_IMPORT_KEEP_SUPPORTED_KEY);

	properties.push_back(INSIMAGE_ASKING);
	properties.push_back(INS_CLEAR_IMAGE);
	properties.push_back(CREATE_BACKUP_FILE_KEY);
	properties.push_back(SHOW_FULL_PATH_IN_WINDOW_TITLE_KEY);
	properties.push_back(UPDATE_CHANNEL_KEY);
	properties.push_back(WINDOW_POSITION);
	properties.push_back(WORDS_DLG_POSITION);
	properties.push_back(SHOW_WORDS_EXCLUSIONS);
	properties.emplace_back(m_desc.GetClassName());
	properties.emplace_back(m_tree_items.GetClassName());

	return properties.size();
}

bool CSettings::GetPropertyValue(const CString& sProperty, CProperty& property)
{
	if(sProperty == KEEP_ENCODING_KEY)
	{
		property = GetStringedProperty(&m_keep_encoding, KEY_BOOL);
		return true;
	}
	else if(sProperty == DEFAULT_ENCODING_KEY)
	{
		property = m_default_encoding;
		return true;
	}
	else if(sProperty == SEARCH_OPTIONS_KEY)
	{
		property = GetStringedProperty(&m_search_options, KEY_INT);
		return true;
	}
	else if(sProperty == COLOR_BG_KEY)
	{
		property = GetStringedProperty(&m_collorBG, KEY_ULONG);
		return true;
	}
	else if(sProperty == COLOR_FG_KEY)
	{
		property = GetStringedProperty(&m_collorFG, KEY_ULONG);
		return true;
	}
	else if(sProperty == FONT_SIZE_KEY)
	{
		property = GetStringedProperty(&m_font_size, KEY_INT);
		return true;
	}
	else if(sProperty == XML_SRC_WRAP_KEY)
	{
		property = GetStringedProperty(&m_xml_src_wrap, KEY_BOOL);
		return true;
	}
	else if(sProperty == XML_SRC_SYNTAX_HL_KEY)
	{
		property = GetStringedProperty(&m_xml_src_syntaxHL, KEY_BOOL);
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_PALETTE_KEY)
	{
		property = GetStringedProperty(&m_xml_src_color_palette, KEY_INT);
		return true;
	}
	else if(sProperty == XML_SRC_THEME_ID_KEY)
	{
		property = GetStringedProperty(&m_xml_src_theme_id, KEY_STRING);
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_TEXT_KEY)
	{
		property = GetStringedProperty(&m_xml_src_colors[XML_SRC_COLOR_TEXT], KEY_ULONG);
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_TAG_KEY)
	{
		property = GetStringedProperty(&m_xml_src_colors[XML_SRC_COLOR_TAG], KEY_ULONG);
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_ATTRIBUTE_KEY)
	{
		property = GetStringedProperty(&m_xml_src_colors[XML_SRC_COLOR_ATTRIBUTE], KEY_ULONG);
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_STRING_KEY)
	{
		property = GetStringedProperty(&m_xml_src_colors[XML_SRC_COLOR_STRING], KEY_ULONG);
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_COMMENT_KEY)
	{
		property = GetStringedProperty(&m_xml_src_colors[XML_SRC_COLOR_COMMENT], KEY_ULONG);
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_BACKGROUND_KEY)
	{
		property = GetStringedProperty(&m_xml_src_colors[XML_SRC_COLOR_BACKGROUND], KEY_ULONG);
		return true;
	}
	else if(sProperty == XML_SRC_TAG_HL_KEY)
	{
		property = GetStringedProperty(&m_xml_src_tagHL, KEY_BOOL);
		return true;
	}
	else if(sProperty == XML_SRC_SHOW_EOL_KEY)
	{
		property = GetStringedProperty(&m_xml_src_showEOL, KEY_BOOL);
		return true;
	}
	else if(sProperty == XML_SRC_SHOW_SPACE_KEY)
	{
		property = GetStringedProperty(&m_xml_src_showSpace, KEY_BOOL);
		return true;
	}
	else if(sProperty == XML_SRC_SHOW_SPECIAL_CHARS_KEY)
	{
		property = GetStringedProperty(&m_xml_src_showSpecialChars, KEY_BOOL);
		return true;
	}
	else if(sProperty == XML_SRC_SPECIAL_CHARS_STYLE_KEY)
	{
		property = GetStringedProperty(&m_xml_src_specialCharsStyle, KEY_INT);
		return true;
	}
	else if(sProperty == FAST_MODE_KEY)
	{
		property = GetStringedProperty(&m_fast_mode, KEY_BOOL);
		return true;
	}
	else if(sProperty == FONT_KEY)
	{
		property = m_font;
		return true;
	}
	else if(sProperty == XML_SRC_TAG_HL_MODE_KEY) { property = GetStringedProperty(&m_xml_src_tagHL_mode, KEY_ULONG); return true; }
	else if(sProperty == XML_SRC_TAG_HL_ATTRIBUTES_KEY) { property = GetStringedProperty(&m_xml_src_tagHL_attributes, KEY_BOOL); return true; }
	else if(sProperty == XML_SRC_TAG_HL_ERRORS_KEY) { property = GetStringedProperty(&m_xml_src_tagHL_errors, KEY_BOOL); return true; }
	else if(sProperty == EDITOR_BACKGROUND_KIND_KEY) { property = m_editor_background_kind; return true; }
	else if(sProperty == EDITOR_BACKGROUND_ID_KEY) { property = m_editor_background_id; return true; }
	else if(sProperty == EDITOR_BACKGROUND_CUSTOM_PATH_KEY) { property = m_editor_background_custom_path; return true; }
	else if(sProperty == EDITOR_BACKGROUND_LAYOUT_KEY) { property = m_editor_background_layout; return true; }
	else if(sProperty == SRC_FONT_KEY)
	{
		property = m_srcfont;
		return true;
	}
	else if(sProperty == VIEW_STATUS_BAR_KEY)
	{
		property = GetStringedProperty(&m_view_status_bar, KEY_BOOL);
		return true;
	}
	else if(sProperty == VIEW_DOCUMENT_TREE_KEY)
	{
		property = GetStringedProperty(&m_view_doc_tree, KEY_BOOL);
		return true;
	}
	else if(sProperty == SPLITTER_POS_KEY)
	{
		property = GetStringedProperty(&m_splitter_pos, KEY_INT);
		return true;
	}
	else if(sProperty == FIND_RESULTS_PANE_HEIGHT_KEY)
	{
		property = GetStringedProperty(&m_find_results_pane_height, KEY_INT);
		return true;
	}
	else if(sProperty == TOOLBARS_SETTINGS_KEY)
	{
		property = m_toolbars_settings;
		return true;
	}
	else if(sProperty == SCRIPT_COMMAND_IDS_KEY)
	{
		property = m_script_command_ids;
		return true;
	}
	else if(sProperty == SCRIPTS_TOOLBAR_CUSTOMIZE_SIZE_KEY)
	{
		CString value; value.Format(L"%u;%u", m_scripts_toolbar_customize_width, m_scripts_toolbar_customize_height);
		property = value;
		return true;
	}
	else if(sProperty == RESTORE_FILE_POS_KEY)
	{
		property = GetStringedProperty(&m_restore_file_position, KEY_BOOL);
		return true;
	}
	else if(sProperty == INTERFACE_LANG_KEY)
	{
		property = GetStringedProperty(&m_interface_lang_id, KEY_INT);
		return true;
	}
	else if(sProperty == STATUS_BAR_PANES_KEY)
	{
		property = GetStringedProperty(&m_status_bar_panes, KEY_INT);
		return true;
	}
	else if(sProperty == GENRE_CATALOG_KEY)
	{
		property = m_genre_catalog == GenreCatalog::Librusec ? L"Librusec" : L"Standard";
		return true;
	}
	else if(sProperty == SCRIPTS_FOLDER_KEY)
	{
		property = m_scripts_folder;
		return true;
	}
	// added SeNS
	else if(sProperty == USESPELLER_CHECK_KEY)
	{
		property = GetStringedProperty(&m_usespell_check, KEY_BOOL);
		return true;
	}
	else if(sProperty == HIGHLIGHT_CHECK_KEY)
	{
		property = GetStringedProperty(&m_highlght_check, KEY_BOOL);
		return true;
	}
	else if(sProperty == CUSTOM_DICT_KEY)
	{
		property = m_custom_dict;
		return true;
	}
	else if(sProperty == CUSTOM_DICT_CODEPAGE_KEY)
	{
		property = GetStringedProperty(&m_custom_dict_codepage, KEY_INT); 
		return true;
	}
	else if(sProperty == NBSPCHAR_KEY)
	{
		property = m_nbsp_char;
		return true;
	}
	else if(sProperty == CHANGE_KEYBD_CHECK_KEY)
	{
		property = GetStringedProperty(&m_change_kbd_layout_check, KEY_BOOL);
		return true;
	}
	else if(sProperty == KEYB_LAYOUT_KEY)
	{
		property = GetStringedProperty(&m_keyb_layout, KEY_INT);
		return true;
	}
	else if(sProperty == SHOW_LINE_NUMBERS_KEY)
	{
		property = GetStringedProperty(&m_show_line_numbers, KEY_BOOL);
		return true;
	}
	else if(sProperty == IMAGE_TYPE_KEY)
	{
		property = GetStringedProperty(&m_image_type, KEY_INT);
		return true;
	}
	else if(sProperty == JPEG_QUALITY_KEY)
	{
		property = GetStringedProperty(&m_jpeg_quality, KEY_INT);
		return true;
	}
	else if(sProperty == IMAGE_IMPORT_FORMAT_KEY) { property = GetStringedProperty(&m_image_import_format, KEY_INT); return true; }
	else if(sProperty == IMAGE_IMPORT_JPEG_QUALITY_KEY) { property = GetStringedProperty(&m_image_import_jpeg_quality, KEY_INT); return true; }
	else if(sProperty == IMAGE_IMPORT_KEEP_SUPPORTED_KEY) { property = GetStringedProperty(&m_image_import_keep_supported, KEY_BOOL); return true; }
	///
	else if(sProperty == INSIMAGE_ASKING)
	{
		property = GetStringedProperty(&m_insimage_ask, KEY_BOOL);
		return true;
	}
	else if(sProperty == INS_CLEAR_IMAGE)
	{
		property = GetStringedProperty(&m_ins_clear_image, KEY_BOOL);
		return true;
	}
	else if(sProperty == CREATE_BACKUP_FILE_KEY)
	{
		property = GetStringedProperty(&m_create_backup_file, KEY_BOOL);
		return true;
	}
	else if(sProperty == SHOW_FULL_PATH_IN_WINDOW_TITLE_KEY)
	{
		property = GetStringedProperty(&m_show_full_path_in_window_title, KEY_BOOL);
		return true;
	}
	else if(sProperty == KEYB_LAYOUT_ID_KEY)
	{
		property = m_keyboard_layout_id;
		return true;
	}
	else if(sProperty == UPDATE_CHANNEL_KEY)
	{
		property = m_update_channel == UpdateChannel::Prerelease ? L"prerelease" : L"stable";
		return true;
	}
	else if(sProperty == WORDS_DLG_POSITION)
	{
		CString temp;
		temp.Format(L"%u;%u;%u;%ld;%ld;%ld;%ld;%ld;%ld;%ld;%ld",
			m_words_dlg_placement.length,
			m_words_dlg_placement.flags,
			m_words_dlg_placement.showCmd,
			m_words_dlg_placement.ptMinPosition.x,
			m_words_dlg_placement.ptMinPosition.y,
			m_words_dlg_placement.ptMaxPosition.x,
			m_words_dlg_placement.ptMaxPosition.y,
			m_words_dlg_placement.rcNormalPosition.bottom,
			m_words_dlg_placement.rcNormalPosition.left,
			m_words_dlg_placement.rcNormalPosition.top,
			m_words_dlg_placement.rcNormalPosition.right);
		property = temp;
		return true;
	}
	else if(sProperty == SCRIPTS_TOOLBAR_CUSTOMIZE_PLACEMENT_KEY)
	{
		CString temp;
		temp.Format(L"%u;%u;%u;%ld;%ld;%ld;%ld;%ld;%ld;%ld;%ld",
			m_scripts_toolbar_customize_placement.length, m_scripts_toolbar_customize_placement.flags,
			m_scripts_toolbar_customize_placement.showCmd, m_scripts_toolbar_customize_placement.ptMinPosition.x,
			m_scripts_toolbar_customize_placement.ptMinPosition.y, m_scripts_toolbar_customize_placement.ptMaxPosition.x,
			m_scripts_toolbar_customize_placement.ptMaxPosition.y, m_scripts_toolbar_customize_placement.rcNormalPosition.bottom,
			m_scripts_toolbar_customize_placement.rcNormalPosition.left, m_scripts_toolbar_customize_placement.rcNormalPosition.top,
			m_scripts_toolbar_customize_placement.rcNormalPosition.right);
		property = temp;
		return true;
	}
	else if(sProperty == SHOW_WORDS_EXCLUSIONS)
	{
		property = GetStringedProperty(&m_show_words_excls, KEY_BOOL);
		return true;
	}
	else if(sProperty == WINDOW_POSITION)
	{
		CString temp;
		temp.Format(L"%u;%u;%u;%ld;%ld;%ld;%ld;%ld;%ld;%ld;%ld",
					m_wnd_placement.length,
					m_wnd_placement.flags,
					m_wnd_placement.showCmd,
					m_wnd_placement.ptMinPosition.x,
					m_wnd_placement.ptMinPosition.y,
					m_wnd_placement.ptMaxPosition.x,
					m_wnd_placement.ptMaxPosition.y,
					m_wnd_placement.rcNormalPosition.bottom,
					m_wnd_placement.rcNormalPosition.left,
					m_wnd_placement.rcNormalPosition.top,
					m_wnd_placement.rcNormalPosition.right);
		property = temp;
		return true;
	}
	else if(sProperty == m_desc.GetClassName())
	{
		property = (ISerializable*)&m_desc;
		property.SetFactory(&m_desc);
		return true;
	}
	else if(sProperty == m_tree_items.GetClassName())
	{
		property = (ISerializable*)&m_tree_items;
		property.SetFactory(&m_tree_items);
		return true;
	}

	return false;
}

bool CSettings::SetPropertyValue(const CString& sProperty, CProperty& sValue)
{
	if(sProperty == KEEP_ENCODING_KEY)
	{
		m_keep_encoding = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == DEFAULT_ENCODING_KEY)
	{
		m_default_encoding = sValue.GetStringValue();
		return true;
	}
	else if(sProperty == SEARCH_OPTIONS_KEY)
	{
		m_search_options = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == COLOR_BG_KEY)
	{
		m_collorBG = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == COLOR_FG_KEY)
	{
		m_collorFG = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == FONT_SIZE_KEY)
	{
		m_font_size = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_WRAP_KEY)
	{
		m_xml_src_wrap = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_SYNTAX_HL_KEY)
	{
		m_xml_src_syntaxHL = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_PALETTE_KEY)
	{
		SetXmlSrcColorPalette(StrToInt(sValue.GetStringValue()));
		return true;
	}
	else if(sProperty == XML_SRC_THEME_ID_KEY)
	{
		SetXmlSrcThemeId(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_TEXT_KEY)
	{
		m_xml_src_colors[XML_SRC_COLOR_TEXT] = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_TAG_KEY)
	{
		m_xml_src_colors[XML_SRC_COLOR_TAG] = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_ATTRIBUTE_KEY)
	{
		m_xml_src_colors[XML_SRC_COLOR_ATTRIBUTE] = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_STRING_KEY)
	{
		m_xml_src_colors[XML_SRC_COLOR_STRING] = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_COMMENT_KEY)
	{
		// XML comments are not preserved by the document model.  A historical
		// override therefore must not survive invisibly without a UI to edit it.
		m_xml_src_colors[XML_SRC_COLOR_COMMENT] = XML_SRC_COLOR_DEFAULT;
		return true;
	}
	else if(sProperty == XML_SRC_COLOR_BACKGROUND_KEY)
	{
		m_xml_src_colors[XML_SRC_COLOR_BACKGROUND] = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_TAG_HL_KEY)
	{
		m_xml_src_tagHL = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_SHOW_EOL_KEY)
	{
		m_xml_src_showEOL = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_SHOW_SPACE_KEY)
	{
		m_xml_src_showSpace = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_SHOW_SPECIAL_CHARS_KEY)
	{
		m_xml_src_showSpecialChars = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == XML_SRC_SPECIAL_CHARS_STYLE_KEY)
	{
		m_xml_src_specialCharsStyle = StrToInt(sValue.GetStringValue()) == XML_SRC_SPECIAL_CHARS_TEXT_LABELS ? XML_SRC_SPECIAL_CHARS_TEXT_LABELS : XML_SRC_SPECIAL_CHARS_WORD_LIKE;
		return true;
	}
	else if(sProperty == FAST_MODE_KEY)
	{
		m_fast_mode = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == FONT_KEY)
	{
		m_font = sValue.GetStringValue();
		return true;
	}
	else if(sProperty == XML_SRC_TAG_HL_MODE_KEY) { m_xml_src_tagHL_mode = StrToInt(sValue.GetStringValue()); return true; }
	else if(sProperty == XML_SRC_TAG_HL_ATTRIBUTES_KEY) { m_xml_src_tagHL_attributes = StrToBool(sValue.GetStringValue()); return true; }
	else if(sProperty == XML_SRC_TAG_HL_ERRORS_KEY) { m_xml_src_tagHL_errors = StrToBool(sValue.GetStringValue()); return true; }
	else if(sProperty == EDITOR_BACKGROUND_KIND_KEY) { SetEditorBackgroundKind(sValue.GetStringValue()); return true; }
	else if(sProperty == EDITOR_BACKGROUND_ID_KEY) { m_editor_background_id = sValue.GetStringValue(); return true; }
	else if(sProperty == EDITOR_BACKGROUND_CUSTOM_PATH_KEY) { m_editor_background_custom_path = sValue.GetStringValue(); return true; }
	else if(sProperty == EDITOR_BACKGROUND_LAYOUT_KEY) { SetEditorBackgroundLayout(sValue.GetStringValue()); return true; }
	else if(sProperty == SRC_FONT_KEY)
	{
		m_srcfont = sValue.GetStringValue();
		return true;
	}
	else if(sProperty == VIEW_STATUS_BAR_KEY)
	{
		m_view_status_bar = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == VIEW_DOCUMENT_TREE_KEY)
	{
		m_view_doc_tree = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == SPLITTER_POS_KEY)
	{
		m_splitter_pos = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == FIND_RESULTS_PANE_HEIGHT_KEY)
	{
		m_find_results_pane_height = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == TOOLBARS_SETTINGS_KEY)
	{
		m_toolbars_settings = sValue.GetStringValue();
		return true;
	}
	else if(sProperty == SCRIPT_COMMAND_IDS_KEY)
	{
		m_script_command_ids = sValue.GetStringValue();
		return true;
	}
	else if(sProperty == SCRIPTS_TOOLBAR_CUSTOMIZE_SIZE_KEY)
	{
		unsigned int width = 0, height = 0;
		if(swscanf_s(sValue.GetStringValue(), L"%u;%u", &width, &height) == 2 && width >= 300 && height >= 200) {
			m_scripts_toolbar_customize_width = width; m_scripts_toolbar_customize_height = height;
		}
		return true;
	}
	else if(sProperty == RESTORE_FILE_POS_KEY)
	{
		m_restore_file_position = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == INTERFACE_LANG_KEY)
	{
	m_interface_lang_id = FbeSettings::NormalizeInterfaceLanguageID(StrToInt(sValue.GetStringValue()));
		return true;
	}
	else if(sProperty == STATUS_BAR_PANES_KEY)
	{
		m_status_bar_panes = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == GENRE_CATALOG_KEY)
	{
		m_genre_catalog = sValue.GetStringValue().CompareNoCase(L"Librusec") == 0
			? GenreCatalog::Librusec : GenreCatalog::Standard;
		return true;
	}
	else if(sProperty == SCRIPTS_FOLDER_KEY)
	{
	m_scripts_folder = FbeSettings::NormalizeScriptsFolderStoredPath(sValue.GetStringValue());
		return true;
	}
	// SeNS
	else if(sProperty == USESPELLER_CHECK_KEY)
	{
		m_usespell_check = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == HIGHLIGHT_CHECK_KEY)
	{
		m_highlght_check = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == CUSTOM_DICT_KEY)
	{
		m_custom_dict = sValue.GetStringValue();
		return true;
	}
	else if(sProperty == CUSTOM_DICT_CODEPAGE_KEY)
	{
		m_custom_dict_codepage = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == NBSPCHAR_KEY)
	{
		m_nbsp_char = sValue.GetStringValue();
		return true;
	}
	else if(sProperty == CHANGE_KEYBD_CHECK_KEY)
	{
		m_change_kbd_layout_check = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == KEYB_LAYOUT_KEY)
	{
		m_keyb_layout = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == SHOW_LINE_NUMBERS_KEY)
	{
		m_show_line_numbers = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == IMAGE_TYPE_KEY)
	{
		m_image_type = FbeSettings::NormalizeImageType(StrToInt(sValue.GetStringValue()));
		return true;
	}
	else if(sProperty == JPEG_QUALITY_KEY)
	{
	m_jpeg_quality = FbeSettings::NormalizeJpegQuality(StrToInt(sValue.GetStringValue()));
		return true;
	}
	else if(sProperty == IMAGE_IMPORT_FORMAT_KEY) { m_image_import_format = min(2u, StrToInt(sValue.GetStringValue())); return true; }
	else if(sProperty == IMAGE_IMPORT_JPEG_QUALITY_KEY) { m_image_import_jpeg_quality = max(1u, min(100u, StrToInt(sValue.GetStringValue()))); return true; }
	else if(sProperty == IMAGE_IMPORT_KEEP_SUPPORTED_KEY) { m_image_import_keep_supported = StrToBool(sValue.GetStringValue()); return true; }
	///
	else if(sProperty == INSIMAGE_ASKING)
	{
		m_insimage_ask = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == INS_CLEAR_IMAGE)
	{
		m_ins_clear_image = StrToBool(sValue);
		return true;
	}
	else if(sProperty == CREATE_BACKUP_FILE_KEY)
	{
		m_create_backup_file = StrToBool(sValue);
		return true;
	}
	else if(sProperty == SHOW_FULL_PATH_IN_WINDOW_TITLE_KEY)
	{
		m_show_full_path_in_window_title = StrToBool(sValue);
		return true;
	}
	else if(sProperty == KEYB_LAYOUT_ID_KEY)
	{
		CString id(sValue.GetStringValue()); id.Trim(); id.MakeUpper();
		m_keyboard_layout_id = id.GetLength() == 8 && id.SpanIncluding(L"0123456789ABCDEF").GetLength() == 8 ? id : CString();
		return true;
	}
	else if(sProperty == UPDATE_CHANNEL_KEY)
	{
		m_update_channel = sValue.GetStringValue().CompareNoCase(L"prerelease") == 0
			? UpdateChannel::Prerelease : UpdateChannel::Stable;
		return true;
	}
	else if(sProperty == SHOW_WORDS_EXCLUSIONS)
	{
		m_show_words_excls = StrToBool(sValue);
		return true;
	}
	else if(sProperty == WORDS_DLG_POSITION)
	{
		CString str = sValue.GetStringValue();
		int n = 0, curPos = 0;

		while(!str.Tokenize(L";", curPos).IsEmpty())
			n++;

		CString* tokens = new CString[n];
		curPos = n =0;

		CString temp;
		while(!(temp = str.Tokenize(L";", curPos)).IsEmpty())
		{
			tokens[n] = temp;
			n++;
		}

		if(n == 11)
		{
			m_words_dlg_placement.length = StrToInt(tokens[0]);
			m_words_dlg_placement.flags = StrToInt(tokens[1]);
			m_words_dlg_placement.showCmd = StrToInt(tokens[2]);
			m_words_dlg_placement.ptMinPosition.x = StrToInt(tokens[3]);
			m_words_dlg_placement.ptMinPosition.y = StrToInt(tokens[4]);
			m_words_dlg_placement.ptMaxPosition.x = StrToInt(tokens[5]);
			m_words_dlg_placement.ptMaxPosition.y = StrToInt(tokens[6]);
			m_words_dlg_placement.rcNormalPosition.bottom = StrToInt(tokens[7]);
			m_words_dlg_placement.rcNormalPosition.left = StrToInt(tokens[8]);
			m_words_dlg_placement.rcNormalPosition.top = StrToInt(tokens[9]);
			m_words_dlg_placement.rcNormalPosition.right = StrToInt(tokens[10]);
		}

		delete[] tokens;

		return true;
	}
	else if(sProperty == SCRIPTS_TOOLBAR_CUSTOMIZE_PLACEMENT_KEY)
	{
		CString str = sValue.GetStringValue(); int n = 0, curPos = 0;
		while(!str.Tokenize(L";", curPos).IsEmpty()) n++;
		CString* tokens = new CString[n]; curPos = n = 0; CString temp;
		while(!(temp = str.Tokenize(L";", curPos)).IsEmpty()) { tokens[n] = temp; n++; }
		if(n == 11)
		{
			m_scripts_toolbar_customize_placement.length = StrToInt(tokens[0]);
			m_scripts_toolbar_customize_placement.flags = StrToInt(tokens[1]);
			m_scripts_toolbar_customize_placement.showCmd = StrToInt(tokens[2]);
			m_scripts_toolbar_customize_placement.ptMinPosition.x = StrToInt(tokens[3]);
			m_scripts_toolbar_customize_placement.ptMinPosition.y = StrToInt(tokens[4]);
			m_scripts_toolbar_customize_placement.ptMaxPosition.x = StrToInt(tokens[5]);
			m_scripts_toolbar_customize_placement.ptMaxPosition.y = StrToInt(tokens[6]);
			m_scripts_toolbar_customize_placement.rcNormalPosition.bottom = StrToInt(tokens[7]);
			m_scripts_toolbar_customize_placement.rcNormalPosition.left = StrToInt(tokens[8]);
			m_scripts_toolbar_customize_placement.rcNormalPosition.top = StrToInt(tokens[9]);
			m_scripts_toolbar_customize_placement.rcNormalPosition.right = StrToInt(tokens[10]);
		}
		delete[] tokens;
		return true;
	}
	else if(sProperty == WINDOW_POSITION)
	{
		CString str = sValue.GetStringValue();
		int n = 0, curPos = 0;

		while(!str.Tokenize(L";", curPos).IsEmpty())
			n++;

		CString* tokens = new CString[n];
		curPos = n =0;

		CString temp;
		while(!(temp = str.Tokenize(L";", curPos)).IsEmpty())
		{
			tokens[n] = temp;
			n++;
		}
		
		if(n == 11)
		{
			m_wnd_placement.length = StrToInt(tokens[0]);
			m_wnd_placement.flags = StrToInt(tokens[1]);
			m_wnd_placement.showCmd = StrToInt(tokens[2]);
			m_wnd_placement.ptMinPosition.x = StrToInt(tokens[3]);
			m_wnd_placement.ptMinPosition.y = StrToInt(tokens[4]);
			m_wnd_placement.ptMaxPosition.x = StrToInt(tokens[5]);
			m_wnd_placement.ptMaxPosition.y = StrToInt(tokens[6]);
			m_wnd_placement.rcNormalPosition.bottom = StrToInt(tokens[7]);
			m_wnd_placement.rcNormalPosition.left = StrToInt(tokens[8]);
			m_wnd_placement.rcNormalPosition.top = StrToInt(tokens[9]);
			m_wnd_placement.rcNormalPosition.right = StrToInt(tokens[10]);
		}
		
		delete[] tokens;

		return true;
	}
	else if(sProperty == m_desc.GetClassName())
	{
		DESCSHOWINFO* pdesc = (DESCSHOWINFO*)(sValue.GetObject());
		m_desc.elements = pdesc->elements;
		sValue.GetFactory()->Destroy(pdesc);

		return true;
	}
	else if(sProperty == m_tree_items.GetClassName())
	{
		TREEITEMSHOWINFO* pti = (TREEITEMSHOWINFO*)(sValue.GetObject());
		m_tree_items.items = pti->items;
		sValue.GetFactory()->Destroy(pti);

		return true;
	}

	return false;
}

bool CSettings::HasMultipleInstances()
{
	return false;
}

CString CSettings::GetClassName()
{
	return L"Settings";
}

CString CSettings::GetID()
{
	return L"0";
}

// IObjectFactory interface
ISerializable* CSettings::Create()
{
	return new CSettings;
}

void CSettings::Destroy(ISerializable* obj)
{
	delete obj;
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

void CSettings::SetDefaults()
{
	m_keep_encoding			= true;
	m_default_encoding		= DEFAULT_ENCODING;
	m_search_options		= 0;
	m_collorBG				= CLR_DEFAULT;
	m_collorFG				= CLR_DEFAULT;
	m_font_size				= 12;
	m_xml_src_wrap			= true;
	m_xml_src_syntaxHL		= true;
	m_xml_src_color_palette = XML_SRC_COLOR_PALETTE_SYSTEM;
	m_xml_src_theme_id = XmlSourceThemes::GetThemeIdForPalette(m_xml_src_color_palette);
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
		m_xml_src_colors[i] = XML_SRC_COLOR_DEFAULT;
	m_xml_src_tagHL			= true;
	m_xml_src_tagHL_mode = 0;
	m_xml_src_tagHL_attributes = false;
	m_xml_src_tagHL_errors = true;
	m_xml_src_showEOL		= false;
	m_xml_src_showSpace		= false;
	m_xml_src_showSpecialChars = false;
	m_xml_src_specialCharsStyle = XML_SRC_SPECIAL_CHARS_WORD_LIKE;
	m_fast_mode				= false;
	m_font					= DEFAULT_FONT;
	m_srcfont				= DEFAULT_SRCFONT;
	m_editor_background_kind = L"none";
	m_editor_background_id.Empty();
	m_editor_background_custom_path.Empty();
	m_editor_background_layout = L"tile";
	m_view_status_bar		= true;
	m_status_bar_panes		= 0x3f;
	m_view_doc_tree			= true;
	m_splitter_pos			= 200;
	m_find_results_pane_height = 180;
	m_toolbars_settings.Empty();
	m_script_command_ids.Empty();
	m_scripts_toolbar_customize_width = 700;
	m_scripts_toolbar_customize_height = 500;
	m_restore_file_position	= false;
	m_interface_lang_id		= FBE_INTERFACE_LANGUAGE_AUTO;
	m_genre_catalog			= GenreCatalog::Standard;
	m_scripts_folder		= GetDefaultScriptsFolderStored();
	m_insimage_ask			= true;
	m_ins_clear_image		= false;
	m_create_backup_file		= true;
	m_show_full_path_in_window_title = false;
	m_update_channel = UpdateChannel::Stable;
	m_show_words_excls		= true;
	// added by SeNS
	m_usespell_check		= true;
	m_highlght_check		= true;
	m_custom_dict           = L"custom.dic";
	m_custom_dict_codepage	= 1251;
	m_nbsp_char				= L"\u00A0";
	m_change_kbd_layout_check = false;
	m_keyb_layout = 0;
	m_keyboard_layout_id.Empty();
	m_show_line_numbers		= false;
	m_image_type			= 1;
	m_jpeg_quality			= 75;
	m_image_import_format		= 0;
	m_image_import_jpeg_quality	= 90;
	m_image_import_keep_supported	= true;

	::ZeroMemory(&m_wnd_placement, sizeof(WINDOWPLACEMENT));
	::ZeroMemory(&m_scripts_toolbar_customize_placement, sizeof(WINDOWPLACEMENT));
	m_desc.SetDefaults();
}

int DESCSHOWINFO::GetProperties(std::vector<CString>& properties)
{
	std::map<CString, bool>::iterator iter = elements.begin();
	while(iter != elements.end())
	{
		properties.push_back(iter->first);
		++iter;
	}

	return properties.size();
}

bool DESCSHOWINFO::GetPropertyValue(const CString& sProperty, CProperty& property)
{
	std::map<CString, bool>::iterator iter = elements.begin();
	while(iter != elements.end())
	{
		if(iter->first == sProperty)
		{
			property = GetStringedProperty(&elements[iter->first], KEY_BOOL);
			return true;
		}
		else
			++iter;
	}

	return false;
}

bool DESCSHOWINFO::SetPropertyValue(const CString& sProperty, CProperty& sValue)
{
	std::map<CString, bool>::iterator iter = elements.begin();
	while(iter != elements.end())
	{
		if(iter->first == sProperty)
		{
			iter->second = StrToBool(sValue.GetStringValue());
			return true;
		}
		else
			++iter;
	}

	return false;
}

bool DESCSHOWINFO::HasMultipleInstances()
{
	return false;
}

CString DESCSHOWINFO::GetClassName()
{
	return L"Description";
}

CString DESCSHOWINFO::GetID()
{
	return L"";
}

ISerializable* DESCSHOWINFO::Create()
{
	return new DESCSHOWINFO;
}

void DESCSHOWINFO::Destroy(ISerializable* obj)
{
	delete obj;
}

TREEITEMSHOWINFO::TREEITEMSHOWINFO()
{
	SetDefaults();
}

// Default fields showing in description
void TREEITEMSHOWINFO::SetDefaults()
{
	_EDMnr.InitStandartEDs();
	int edCount = _EDMnr.GetStEDsCount();
	for(int i = 0; i < edCount; ++i)
	{
		CElementDescriptor* ed = _EDMnr.GetStED(i);
		items[ed->GetCaption()] = ed->ViewInTree();
	}
}

int TREEITEMSHOWINFO::GetProperties(std::vector<CString>& properties)
{
	std::map<CString, bool>::iterator iter = items.begin();
	while(iter != items.end())
	{
		properties.push_back(iter->first);
		++iter;
	}

	return properties.size();
}

bool TREEITEMSHOWINFO::GetPropertyValue(const CString& sProperty, CProperty& property)
{
	std::map<CString, bool>::iterator iter = items.begin();
	while(iter != items.end())
	{
		if(iter->first == sProperty)
		{
			property = GetStringedProperty(&items[iter->first], KEY_BOOL);
			return true;
		}
		else
			++iter;
	}

	return false;
}

bool TREEITEMSHOWINFO::SetPropertyValue(const CString& sProperty, CProperty& sValue)
{
	std::map<CString, bool>::iterator iter = items.begin();
	while(iter != items.end())
	{
		if(iter->first == sProperty)
		{
			iter->second = StrToBool(sValue.GetStringValue());
			return true;
		}
		else
			++iter;
	}

	return false;
}

bool TREEITEMSHOWINFO::HasMultipleInstances()
{
	return false;
}

CString TREEITEMSHOWINFO::GetClassName()
{
	return L"TreeItems";
}

CString TREEITEMSHOWINFO::GetID()
{
	return L"";
}

ISerializable* TREEITEMSHOWINFO::Create()
{
	return new TREEITEMSHOWINFO;
}

void TREEITEMSHOWINFO::Destroy(ISerializable* obj)
{
	delete obj;
}
