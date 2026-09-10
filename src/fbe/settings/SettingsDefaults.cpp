#include "stdafx.h"
#include "..\Settings.h"
#include "SettingsNormalization.h"
#include "SettingsDefaults.h"
#include "..\XmlSourceThemes.h"

const wchar_t DEFAULT_ENCODING[] = L"utf-8";
const wchar_t DEFAULT_FONT[] = L"Trebuchet MS";
const wchar_t DEFAULT_SRCFONT[] = L"Lucida Console";
const wchar_t DEFAULT_SCRIPTS_FOLDER[] = L"Scripts";

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

