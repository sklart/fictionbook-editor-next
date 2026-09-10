#include "stdafx.h"
#include "..\Settings.h"
#include "..\XmlSourceThemes.h"
#include "SettingsNormalization.h"
#include "..\ElementDescMnr.h"

extern CElementDescMnr _EDMnr;

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

