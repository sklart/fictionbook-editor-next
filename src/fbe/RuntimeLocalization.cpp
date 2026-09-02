#include "stdafx.h"
#include "resource.h"
#include "RuntimeLocalization.h"
#include "..\common\RuntimeLocalizationCommon.h"
#include "..\common\DeploymentContext.h"

#include <map>
#include <string>
#include <vector>

struct RuntimeStringBinding {
	UINT id;
	const wchar_t* key;
};

struct RuntimeDialogBinding {
	UINT dialogId;
	UINT controlId; // 0 means the dialog caption.
	const wchar_t* key;
};

// Binding metadata only; fbe-small-dialogs.json remains the source of all
// translations.  Every control below has a stable resource ID (never
// IDC_STATIC), so an English dialog template can be localized at runtime.
static const RuntimeDialogBinding g_runtimeDialogBindings[] = {
	{ IDD_TABLE, 0, L"fbe.dialog.idd_table.caption" },
	{ IDD_TABLE, IDOK, L"fbe.dialog.idd_table.ok" },
	{ IDD_TABLE, IDCANCEL, L"fbe.dialog.idd_table.cancel" },
	{ IDD_TABLE, IDC_TABLE_ROWS_LABEL, L"fbe.dialog.idd_table.rows_label" },
	{ IDD_TABLE, IDC_TABLE_COLUMNS_LABEL, L"fbe.dialog.idd_table.columns_label" },
	{ IDD_TABLE, IDC_CHECK_TABLE_TITLE, L"fbe.dialog.idd_table.header_row" },
	{ IDD_INPUTBOX, 0, L"fbe.dialog.idd_inputbox.caption" },
	{ IDD_INPUTBOX, IDC_PROMPT, L"fbe.dialog.idd_inputbox.prompt" },
	{ IDD_INPUTBOX, IDYES, L"fbe.dialog.idd_inputbox.yes" },
	{ IDD_INPUTBOX, IDNO, L"fbe.dialog.idd_inputbox.no" },
	{ IDD_INPUTBOX, IDCANCEL, L"fbe.dialog.idd_inputbox.cancel" },
	{ IDD_ADDIMAGE, 0, L"fbe.dialog.idd_addimage.caption" },
	{ IDD_ADDIMAGE, IDYES, L"fbe.dialog.idd_addimage.yes" },
	{ IDD_ADDIMAGE, IDCANCEL, L"fbe.dialog.idd_addimage.no" },
	{ IDD_ADDIMAGE, IDC_ADDIMAGE_ASKAGAIN, L"fbe.dialog.idd_addimage.ask_again" },
	{ IDD_ADDIMAGE, IDS_ADD_CLEARIMG_TEXT, L"fbe.dialog.idd_addimage.text" },
	{ IDD_TOOLS_SETTINGS, 0, L"fbe.dialog.idd_tools_settings.caption" },
	{ IDD_TOOLS_SETTINGS, IDOK, L"fbe.dialog.idd_tools_settings.ok" },
	{ IDD_TOOLS_SETTINGS, IDCANCEL, L"fbe.dialog.idd_tools_settings.cancel" },
	{ IDD_SPELL_CHECK, IDC_SPELL_NOT_IN_DICTIONARY, L"fbe.dialog.idd_spell_check.not_in_dictionary" },
	{ IDD_SPELL_CHECK, IDC_SPELL_CHANGE_TO, L"fbe.dialog.idd_spell_check.replace_with" },
	{ IDD_SPELL_CHECK, IDC_SPELL_SUGGESTIONS, L"fbe.dialog.idd_spell_check.suggestions" },
	{ IDD_SETTINGS_IMAGES, IDC_SETTINGS_OTHER_DIALOGS, L"fbe.dialog.idd_setting_other.images" },
	{ IDD_SETTINGS_IMAGES, IDC_SETTINGS_ASKIMAGE, L"fbe.dialog.idd_setting_other.ask_image" },
	{ IDD_SETTINGS_IMAGES, IDC_OPTIONS_CLEARIMGS, L"fbe.dialog.idd_setting_other.clear_images" },
	{ IDD_SETTINGS_IMAGES, IDC_SETTINGS_OTHER_PASTE, L"fbe.dialog.idd_setting_other.image_settings" },
	{ IDD_SETTINGS_IMAGES, IDC_SETTINGS_OTHER_FORMAT, L"fbe.dialog.idd_setting_other.format" },
	{ IDD_SETTINGS_IMAGES, IDC_SETTINGS_OTHER_QUALITY, L"fbe.dialog.idd_setting_other.jpeg_quality" },
	{ IDD_SETTINGS_IMAGES, IDC_SETTINGS_OTHER_IMPORT, L"fbe.dialog.idd_setting_other.image_import" },
	{ IDD_SETTINGS_IMAGES, IDC_SETTINGS_OTHER_OUTPUT, L"fbe.dialog.idd_setting_other.output_format" },
	{ IDD_SETTINGS_IMAGES, IDC_SETTINGS_OTHER_IMPORT_QUALITY, L"fbe.dialog.idd_setting_other.jpeg_quality" },
	{ IDD_SETTINGS_IMAGES, IDC_IMAGE_IMPORT_KEEP_SUPPORTED, L"fbe.dialog.idd_setting_other.keep_supported" },
	{ IDD_ABOUTBOX, 0, L"fbe.dialog.idd_aboutbox.caption" },
	{ IDD_ABOUTBOX, IDOK, L"fbe.dialog.idd_aboutbox.ok" },
	{ IDD_ABOUTBOX, IDC_STATIC_BUILD, L"fbe.dialog.idd_aboutbox.build" },
	{ IDD_ABOUTBOX, IDC_SYSLINK_AB_LINKS, L"fbe.dialog.idd_aboutbox.link" },
	{ IDD_ABOUTBOX, IDC_UPDATE, L"fbe.dialog.idd_aboutbox.update" },
	{ IDD_WORDS, 0, L"fbe.dialog.idd_words.caption" },
	{ IDD_WORDS, IDC_WLIST, L"fbe.dialog.idd_words.list" },
	{ IDD_WORDS, IDOK, L"fbe.dialog.idd_words.ok" },
	{ IDD_WORDS, IDCANCEL, L"fbe.dialog.idd_words.cancel" },
	{ IDD_WORDS, IDC_CHECK_SHOWHIDE_EXCLS, L"fbe.dialog.idd_words.show_hide_exclusions" },
	{ IDD_WORDS, IDC_WORDS_ACTIONS_GROUP, L"fbe.dialog.idd_words.actions" },
	{ IDD_WORDS, IDC_WORDS_SELECTION_GROUP, L"fbe.dialog.idd_words.selection" },
	{ IDD_WORDS, IDC_BUTTON_DESEL, L"fbe.dialog.idd_words.deselect" },
	{ IDD_WORDS, IDC_BUTTON_REMOVEHLREPL, L"fbe.dialog.idd_words.remove_replacement" },
	{ IDD_WORDS, IDC_BUTTON_ADDHLTOEXCLS, L"fbe.dialog.idd_words.add_to_exclusions" },
	{ IDD_WORDS, IDC_BUTTON_SELALL, L"fbe.dialog.idd_words.select_all" },
	{ IDD_WORDS, IDC_BUTTON_SETHLREPL, L"fbe.dialog.idd_words.set_replacement" },
	{ IDD_WORDS, IDC_BUTTON_SELALLREPL, L"fbe.dialog.idd_words.select_all_replacements" },
	{ IDD_WORDS, IDC_WORDS_FR_GBOX_CURWORD, L"fbe.dialog.idd_words.current_word" },
	{ IDD_WORDS, IDC_WORDS_FR_TEXT_WORD, L"fbe.dialog.idd_words.word" },
	{ IDD_WORDS, IDC_WORDS_FR_TEXT_REPL, L"fbe.dialog.idd_words.replacement" },
	{ IDD_WORDS, IDC_WORDS_FR_BTN_FIND, L"fbe.dialog.idd_words.find" },
	{ IDD_WORDS, IDC_WORDS_FR_BTN_REPL, L"fbe.dialog.idd_words.replace" },
};

static const RuntimeStringBinding g_runtimeStringBindings[] = {
	{ IDS_UPDATE_CHECK, L"fbe.update.checking" },
	{ IDS_UPDATE_DOWNLOADERROR, L"fbe.update.download_error" },
	{ IDS_UPDATE_CONNECTING, L"fbe.update.connecting" },
	{ IDS_UPDATE_CANTCONNECT, L"fbe.update.cant_connect" },
	{ IDS_UPDATE_DOWNLOADCOMPLETE, L"fbe.update.download_complete" },
	{ IDS_UPDATE_404ERROR, L"fbe.update.http_404" },
	{ IDS_UPDATE_403ERROR, L"fbe.update.http_403" },
	{ IDS_UPDATE_407ERROR, L"fbe.update.http_407" },
	{ IDS_UPDATE_NOTSUPPORTEDRANGE, L"fbe.update.range_not_supported" },
	{ IDS_UPDATE_DOWNLOADERRORSTATUS, L"fbe.update.download_error_status" },
	{ IDS_UPDATE_INCORRECTMD5, L"fbe.update.incorrect_sha256" },
	{ IDS_UPDATE_NEWVERSIONAVAILABLE, L"fbe.update.new_version_available" },
	{ IDS_UPDATE_HAVELATESTVERSION, L"fbe.update.latest_version" },
	{ IDS_UPDATE_DOWNLOADEDFROM, L"fbe.update.downloaded_from" },
	{ IDS_UPDATE_DOWNLOADED, L"fbe.update.downloaded" },
	{ IDS_UPDATE_DOWNLOADREADY, L"fbe.update.download_ready" },
	{ IDS_UPDATE_CLOSE, L"fbe.update.close_to_install" },
	{ IDS_SEARCH_END_MSG, L"fbe.search.finished" },
	{ IDS_READONLY_SAVE_MSG, L"fbe.save.readonly_warning" },
	{ IDS_SAVE_DLG_MSG, L"fbe.save.confirm_changes" },
	{ IDS_ERRMSGBOX_CAPTION, L"fbe.error.caption" },
	{ IDS_VALIDATION_FAIL_MSG, L"fbe.validation.failed.message" },
	{ IDS_VALIDATION_FAIL_CPT, L"fbe.validation.failed.caption" },
	{ IDS_OUT_OF_MEM_MSG, L"fbe.error.out_of_memory" },
	{ IDS_IMPORT_ERR_CPT, L"fbe.import.caption" },
	{ IDS_IMPORT_ERR_MSG, L"fbe.import.unsupported_interface" },
	{ IDS_EXPORT_ERR_CPT, L"fbe.export.caption" },
	{ IDS_EXPORT_ERR_MSG, L"fbe.export.unsupported_interface" },
	{ IDS_FILE_CHANGED_CPT, L"fbe.file_changed.caption" },
	{ IDS_FILE_CHANGED_MSG, L"fbe.file_changed.reload" },
	{ IDS_NO_SCRIPTS, L"fbe.scripts.none" },
	{ IDS_SB_SAVED_NO_ERR, L"fbe.status.saved.no_errors" },
	{ IDS_GENRES_LIST_MSG, L"fbe.genres.open_failed" },
	{ IDS_GOTO_REF_FAIL_MSG, L"fbe.navigation.reference_not_found" },
	{ IDS_IMPORT_XML_ERR_MSG, L"fbe.import.xml_interface_missing" },
	{ IDS_SCINTILLA_LOAD_ERR_MSG, L"fbe.scintilla.load_failed" },
	{ IDS_ABOUT_LOGOCAPTION, L"fbe.about.logo_caption" },
	{ IDS_ABOUT_WINDOW_CAPTION, L"fbe.about.window_caption" },
	{ IDS_ABOUT_BUILD_LABEL, L"fbe.about.build_label" },
	{ IDS_ABOUT_UPDATE_NOW, L"fbe.about.update_now" },
	{ IDS_BAD_XML_MSG, L"fbe.xml.invalid_source_warning" },
	{ IDS_MB_OK, L"fbe.messagebox.ok" },
	{ IDS_MB_CANCEL, L"fbe.messagebox.cancel" },
	{ IDS_MB_ABORT, L"fbe.messagebox.abort" },
	{ IDS_MB_RETRY, L"fbe.messagebox.retry" },
	{ IDS_MB_IGNORE, L"fbe.messagebox.ignore" },
	{ IDS_MB_YES, L"fbe.messagebox.yes" },
	{ IDS_MB_NO, L"fbe.messagebox.no" },
	{ IDS_MB_CLOSE, L"fbe.messagebox.close" },
	{ IDS_LANG_UKRAINIAN, L"fbe.language.ukrainian" },
	{ IDS_LANG_SYSTEM_DEFAULT, L"fbe.language.system_default" },
	{ IDS_LANG_GERMAN, L"fbe.language.german" },
	{ IDS_LANG_FRENCH, L"fbe.language.french" },
	{ IDS_LANG_SPANISH, L"fbe.language.spanish" },
	{ IDS_LANG_ITALIAN, L"fbe.language.italian" },
	{ IDS_LANG_POLISH, L"fbe.language.polish" },
	{ IDS_LANG_PORTUGUESE, L"fbe.language.portuguese" },
	{ IDS_LANG_DUTCH, L"fbe.language.dutch" },
	{ IDS_LANG_CZECH, L"fbe.language.czech" },
	{ IDS_LANG_BULGARIAN, L"fbe.language.bulgarian" },
	{ IDS_UPDATEEXISTS, L"fbe.update.file_exists" },
	{ IDS_RECOVERY_CAPTION, L"fbe.recovery.caption" },
	{ IDS_RECOVERY_MSG, L"fbe.recovery.prompt" },
	{ IDS_CML_ARGS_MSG, L"fbe.command_line.argument_required" },
	{ IDS_INVALID_CML_MSG, L"fbe.command_line.invalid_option" },
	{ IDS_TB_CAPT_COLSPAN, L"fbe.table.caption.colspan" },
	{ IDS_TB_CAPT_IMAGE_TITLE, L"fbe.table.caption.image_title" },
	{ IDS_TB_CAPT_ROWSPAN, L"fbe.table.caption.rowspan" },
	{ IDS_TB_CAPT_SECTION_ID, L"fbe.table.caption.section_id" },
	{ IDS_TB_CAPT_STYLE, L"fbe.table.caption.style" },
	{ IDS_TB_CAPT_TABLE_ID, L"fbe.table.caption.table_id" },
	{ IDS_TB_CAPT_TABLE_STYLE, L"fbe.table.caption.table_style" },
	{ IDS_TB_CAPT_TD_ALIGN, L"fbe.table.caption.td_align" },
	{ IDS_TB_CAPT_TD_VALIGN, L"fbe.table.caption.td_valign" },
	{ IDS_TB_CAPT_TR_ALIGN, L"fbe.table.caption.tr_align" },
	{ IDS_CHOOSE_SCRIPTS_FLD, L"fbe.scripts.choose_folder" },
	{ IDS_HOTKEY_GROUP_EDIT, L"fbe.hotkey.group.edit" },
	{ IDS_HOTKEY_GROUP_NAVIGATION, L"fbe.hotkey.group.navigation" },
	{ IDS_HOTKEY_EDIT_SUB, L"fbe.hotkey.edit.subscript" },
	{ IDS_HOTKEY_EDIT_SUP, L"fbe.hotkey.edit.superscript" },
	{ IDS_HOTKEY_EDIT_MERGE, L"fbe.hotkey.edit.merge" },
	{ IDS_HOTKEY_EDIT_PASTE, L"fbe.hotkey.edit.paste" },
	{ IDS_HOTKEY_EDIT_REDO, L"fbe.hotkey.edit.redo" },
	{ IDS_HOTKEY_EDIT_REPLACE, L"fbe.hotkey.edit.replace" },
	{ IDS_HOTKEY_EDIT_SPLIT, L"fbe.hotkey.edit.split" },
	{ IDS_HOTKEY_EDIT_UNDO, L"fbe.hotkey.edit.undo" },
	{ IDS_HOTKEY_NAVIGATION_NEXT_ITEM, L"fbe.hotkey.navigation.next_item" },
	{ IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE1, L"fbe.hotkey.navigation.collapse1" },
	{ IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE2, L"fbe.hotkey.navigation.collapse2" },
	{ IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE3, L"fbe.hotkey.navigation.collapse3" },
	{ IDS_HOTKEY_EDIT_INSERT_INLINEIMAGE, L"fbe.hotkey.edit.insert_inline_image" },
	{ IDS_HOTKEY_FASTMODE, L"fbe.hotkey.fast_mode" },
	{ IDS_HOTKEY_NAVIGATION_GOTO_FOOTNOTE, L"fbe.hotkey.navigation.goto_footnote" },
	{ IDS_HOTKEY_NAVIGATION_GOTO_MATCHTAG, L"fbe.hotkey.navigation.goto_matching_tag" },
	{ IDS_HOTKEY_NAVIGATION_GOTO_WRONGTAG, L"fbe.hotkey.navigation.goto_wrong_tag" },
	{ IDS_HOTKEY_PLUGINS_LAST_PLUGIN, L"fbe.hotkey.plugins.last_plugin" },
	{ IDS_HOTKEY_TOOLS_SPELL, L"fbe.hotkey.tools.spell" },
	{ IDS_HOTKEY_TOOLS_SPELLHIGHLIGHT, L"fbe.hotkey.tools.spell_highlight" },
	{ IDS_HOTKEY_TOOLS_ADD_TO_DICTIONARY, L"fbe.hotkey.tools.add_to_dictionary" },
	{ IDS_HOTKEY_TOOLS_IGNORE_ALL, L"fbe.hotkey.tools.ignore_all" },
	{ IDS_HOTKEY_TREEVIEW, L"fbe.hotkey.treeview.toggle" },
	{ IDS_HOTKEY_ASSIGN_COLLISION, L"fbe.hotkey.assign.collision" },
	{ IDS_HOTKEY_ASSIGN_NO_COLLISION, L"fbe.hotkey.assign.no_collision" },
	{ IDS_HOTKEY_DEFAULT_COLLISION, L"fbe.hotkey.default.collision" },
	{ IDS_HOTKEY_EDIT_REMOVE_OUTER_SECTION, L"fbe.hotkey.edit.remove_outer_section" },
	{ IDS_HOTKEY_GROUP_PLUGINS, L"fbe.hotkey.group.plugins" },
	{ IDS_HOTKEY_GROUP_TOOLS, L"fbe.hotkey.group.tools" },
	{ IDS_HOTKEY_TOOLS_WORDS, L"fbe.hotkey.tools.words" },
	{ IDS_HOTKEY_TOOLS_OPTIONS, L"fbe.hotkey.tools.options" },
	{ IDS_HOTKEY_WRONG, L"fbe.hotkey.wrong" },
	{ IDS_HOTKEY_EDIT_ADD_ANNOTATION, L"fbe.hotkey.edit.add_annotation" },
	{ IDS_HOTKEY_EDIT_ADD_BODY, L"fbe.hotkey.edit.add_body" },
	{ IDS_HOTKEY_FILE_OPEN, L"fbe.hotkey.file.open" },
	{ IDS_HOTKEY_FILE_SAVE, L"fbe.hotkey.file.save" },
	{ IDS_HOTKEY_FILE_SAVEAS, L"fbe.hotkey.file.save_as" },
	{ IDS_HOTKEY_FILE_VALIDATE, L"fbe.hotkey.file.validate" },
	{ IDS_HOTKEY_GROUP_FILE, L"fbe.hotkey.group.file" },
	{ IDS_HOTKEY_EDIT_ADD_EPIGRAPH, L"fbe.hotkey.edit.add_epigraph" },
	{ IDS_HOTKEY_EDIT_ADD_IMAGE, L"fbe.hotkey.edit.add_section_image" },
	{ IDS_HOTKEY_EDIT_ADD_TA, L"fbe.hotkey.edit.add_text_author" },
	{ IDS_HOTKEY_EDIT_ADD_TITLE, L"fbe.hotkey.edit.add_title" },
	{ IDS_HOTKEY_EDIT_BOLD, L"fbe.hotkey.edit.bold" },
	{ IDS_HOTKEY_EDIT_CLONE, L"fbe.hotkey.edit.clone" },
	{ IDS_HOTKEY_EDIT_COPY, L"fbe.hotkey.edit.copy" },
	{ IDS_HOTKEY_EDIT_CUT, L"fbe.hotkey.edit.cut" },
	{ IDS_HOTKEY_EDIT_FIND, L"fbe.hotkey.edit.find" },
	{ IDS_HOTKEY_EDIT_FIND_NEXT, L"fbe.hotkey.edit.find_next" },
	{ IDS_HOTKEY_EDIT_INCREMENTAL_SEARCH, L"fbe.hotkey.edit.incremental_search" },
	{ IDS_HOTKEY_EDIT_INSERT_CITE, L"fbe.hotkey.edit.insert_cite" },
	{ IDS_HOTKEY_EDIT_INSERT_IMAGE, L"fbe.hotkey.edit.insert_image" },
	{ IDS_HOTKEY_EDIT_INSERT_POEM, L"fbe.hotkey.edit.insert_poem" },
	{ IDS_HOTKEY_EDIT_ITALIC, L"fbe.hotkey.edit.italic" },
	{ IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE4, L"fbe.hotkey.navigation.collapse4" },
	{ IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE5, L"fbe.hotkey.navigation.collapse5" },
	{ IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE6, L"fbe.hotkey.navigation.collapse6" },
	{ IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE7, L"fbe.hotkey.navigation.collapse7" },
	{ IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE8, L"fbe.hotkey.navigation.collapse8" },
	{ IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE9, L"fbe.hotkey.navigation.collapse9" },
	{ IDS_HOTKEY_NAVIGATION_SCI_EXPAND1, L"fbe.hotkey.navigation.expand1" },
	{ IDS_HOTKEY_NAVIGATION_SCI_EXPAND2, L"fbe.hotkey.navigation.expand2" },
	{ IDS_HOTKEY_NAVIGATION_SCI_EXPAND3, L"fbe.hotkey.navigation.expand3" },
	{ IDS_HOTKEY_NAVIGATION_SCI_EXPAND4, L"fbe.hotkey.navigation.expand4" },
	{ IDS_HOTKEY_NAVIGATION_SCI_EXPAND5, L"fbe.hotkey.navigation.expand5" },
	{ IDS_HOTKEY_NAVIGATION_SCI_EXPAND6, L"fbe.hotkey.navigation.expand6" },
	{ IDS_HOTKEY_NAVIGATION_SCI_EXPAND7, L"fbe.hotkey.navigation.expand7" },
	{ IDS_HOTKEY_NAVIGATION_SCI_EXPAND8, L"fbe.hotkey.navigation.expand8" },
	{ IDS_HOTKEY_NAVIGATION_SCI_EXPAND9, L"fbe.hotkey.navigation.expand9" },
	{ IDS_HOTKEY_NAVIGATION_SELECT_HREF, L"fbe.hotkey.navigation.select_href" },
	{ IDS_HOTKEY_EDIT_INSERT_TABLE, L"fbe.hotkey.edit.insert_table" },
	{ IDS_HOTKEY_GROUP_STYLE, L"fbe.hotkey.group.style" },
	{ IDS_HOTKEY_GROUP_VIEW, L"fbe.hotkey.group.view" },
	{ IDS_HOTKEY_NAVIGATION_SELECT_ID, L"fbe.hotkey.navigation.select_id" },
	{ IDS_HOTKEY_NAVIGATION_SELECT_ID_TABLE, L"fbe.hotkey.navigation.select_table_id" },
	{ IDS_HOTKEY_NAVIGATION_SELECT_TEXT, L"fbe.hotkey.navigation.select_text" },
	{ IDS_HOTKEY_NAVIGATION_SELECT_TREE, L"fbe.hotkey.navigation.select_tree" },
	{ IDS_HOTKEY_STYLE_LINK, L"fbe.hotkey.style.link" },
	{ IDS_HOTKEY_STYLE_NO_LINK, L"fbe.hotkey.style.no_link" },
	{ IDS_HOTKEY_STYLE_NORMAL, L"fbe.hotkey.style.normal" },
	{ IDS_HOTKEY_STYLE_NOTE, L"fbe.hotkey.style.note" },
	{ IDS_HOTKEY_STYLE_SUBTITLE, L"fbe.hotkey.style.subtitle" },
	{ IDS_HOTKEY_STYLE_TEXT_AUTHOR, L"fbe.hotkey.style.text_author" },
	{ IDS_HOTKEY_VIEW_BODY, L"fbe.hotkey.view.body" },
	{ IDS_HOTKEY_VIEW_DESCRIPTION, L"fbe.hotkey.view.description" },
	{ IDS_HOTKEY_VIEW_SOURCE, L"fbe.hotkey.view.source" },
	{ IDS_HOTKEY_GROUP_SCRIPTS, L"fbe.hotkey.group.scripts" },
	{ IDS_HOTKEY_SCRIPTS_LAST_SCRIPT, L"fbe.hotkey.scripts.last_script" },
	{ IDS_HOTKEY_GROUP_SYMBOLS, L"fbe.hotkey.group.symbols" },
	{ IDS_SCRIPT_HOTKEY_CONFLICT, L"fbe.scripts.hotkey_conflict" },
	{ IDS_SPELL_CHECK_COMPLETED, L"fbe.spell.check_completed" },
	{ IDS_SPELL_CONTINUE, L"fbe.spell.continue" },
	{ IDS_CTXMENU_COPY, L"fbe.context.copy" },
	{ IDS_CTXMENU_CUT, L"fbe.context.cut" },
	{ IDS_CTXMENU_IMG_SAVEAS, L"fbe.context.image_save_as" },
	{ IDS_CTXMENU_PASTE, L"fbe.context.paste" },
	{ IDS_CTXMENU_SELECT, L"fbe.context.select" },
	{ IDS_DOCTREE_MENU_ELEMENTS, L"fbe.document_tree.menu.elements" },
	{ IDS_DOCTREE_MENU_SCRIPTS, L"fbe.document_tree.menu.scripts" },
	{ IDS_SETTINGS_NEED_RESTART, L"fbe.settings.need_restart" },
	{ IDS_DOC_TREE_CLEANUP, L"fbe.document_tree.cleanup" },
	{ IDS_PANE_INS, L"fbe.status.insert_mode" },
	{ IDS_PANE_OVR, L"fbe.status.overwrite_mode" },
	{ IDS_STATUS_POSITION, L"fbe.status.position" },
	{ IDS_STATUS_SELECTION, L"fbe.status.selection" },
	{ IDS_STATUS_PANE_POSITION, L"fbe.status_pane.position" },
	{ IDS_STATUS_PANE_SELECTION, L"fbe.status_pane.selection" },
	{ IDS_STATUS_PANE_CHARACTER, L"fbe.status_pane.character" },
	{ IDS_STATUS_PANE_ENCODING, L"fbe.status_pane.encoding" },
	{ IDS_STATUS_PANE_VALIDATION, L"fbe.status_pane.validation" },
	{ IDS_STATUS_PANE_INSERT_MODE, L"fbe.status_pane.insert_mode" },
	{ IDS_DMS_AUTHOR, L"fbe.dms.author_nickname" },
	{ IDS_DMS_CI, L"fbe.dms.custom_info" },
	{ IDS_DMS_DI, L"fbe.dms.document_info" },
	{ IDS_DMS_GENRE_M, L"fbe.dms.genre_match" },
	{ IDS_DMS_ID, L"fbe.dms.id" },
	{ IDS_DMS_KW, L"fbe.dms.keywords" },
	{ IDS_DMS_STI, L"fbe.dms.source_title_info" },
	{ IDS_DMS_TI, L"fbe.dms.title_info" },
	{ IDS_LANG_ENGLISH, L"fbe.language.english" },
	{ IDS_LANG_RUSSIAN, L"fbe.language.russian" },
	{ IDS_SB_NO_ERR, L"fbe.status.no_errors" },
	{ IDS_SETTINGS_OTHER_CAPTION, L"fbe.settings.other.caption" },
	{ IDS_SETTINGS_VIEW_CAPTION, L"fbe.settings.view.caption" },
	{ IDS_SETTINGS_HOTKEYS_CAPTION, L"fbe.settings.hotkeys.caption" },
	{ IDS_SETTINGS_WORDS_CAPTION, L"fbe.settings.words.caption" },
	{ IDS_TB_CAPT_HREF, L"fbe.table.caption.href" },
	{ IDS_TB_CAPT_ID, L"fbe.table.caption.id" },
	{ IDS_DOCUMENT_TREE_CAPTION, L"fbe.document_tree.caption" },
	{ IDS_ENCODINGS, L"fbe.encodings.list" },
	{ IDS_SCRIPT_MSG_CPT, L"fbe.script.message.caption" },
	{ IDS_SCRIPT_ERRX_MSG, L"fbe.script.error_hresult" },
	{ IDS_SCRIPT_ERRD_MSG, L"fbe.script.error_description" },
	{ IDS_SCRIPT_MSG, L"fbe.script.error_unknown" },
	{ IDS_SCRIPT_LOAD_ERR_MSG, L"fbe.script.load_error" },
	{ IDS_SCRIPT_PARSE_DIAGNOSTIC_MSG, L"fbe.script.diagnostic_parse" },
	{ IDS_SCRIPT_RUNTIME_DIAGNOSTIC_MSG, L"fbe.script.diagnostic_runtime" },
	{ IDS_SCRIPT_LOAD_DIAGNOSTIC_MSG, L"fbe.script.diagnostic_load" },
	{ IDS_SCRIPT_COPY_DETAILS, L"fbe.script.copy_details" },
	{ IDS_SCRIPT_CLOSE_DETAILS, L"fbe.script.close_details" },
	{ IDS_COM_ERR_CPT, L"fbe.com.error.caption" },
	{ IDS_XML_PARSE_ERR_CPT, L"fbe.xml.parse.caption" },
	{ IDS_XML_PARSE_ERR_MSG, L"fbe.xml.parse.location" },
	{ IDS_XML_PARSE_ERRQ_MSG, L"fbe.xml.parse.quick" },
	{ IDS_REPL_ALL_CAPT, L"fbe.replace.all.caption" },
	{ IDS_REPL_DONE_MSG, L"fbe.replace.done" },
	{ IDS_SEARCH_FAIL_MSG, L"fbe.search.fail" },
	{ IDS_REPL_WORDS_CPT, L"fbe.replace.words.caption" },
	{ IDS_REPL_WORDS_MSG, L"fbe.replace.words.done" },
	{ IDS_ADD_CLEARIMG_TEXT, L"fbe.image.insert_clear.prompt" },
	{ IDS_ADD_CLEARIMG_CAPTION, L"fbe.image.add.caption" },
	{ IDS_ADD_IMAGE_FILEDLG, L"fbe.image.choose_file" },
	{ IDS_ADD_BINARIES_FILEDLG, L"fbe.binaries.choose_files" },
	{ IDS_SETTINGS_WLIST_COUNTED, L"fbe.settings.words.list.counted" },
	{ IDS_SETTINGS_WLIST_WORD, L"fbe.settings.words.list.word" },
	{ IDS_SETTINGS_WORDS_ADD_ERR_TEXT, L"fbe.settings.words.add_error.text" },
	{ IDS_SETTINGS_WORDS_ADD_ERR_CAP, L"fbe.settings.words.add_error.caption" },
	{ IDS_SETTINGS_WORDS_ADD_ERR_SYM, L"fbe.settings.words.add_error.symbols" },
	{ IDS_WORDS_WLIST_WORD, L"fbe.words.list.word" },
	{ IDS_WORDS_WLIST_REPLACEMENT, L"fbe.words.list.replacement" },
	{ IDS_WORDS_WLIST_COUNTED, L"fbe.words.list.counted" },
	{ IDS_WORDS_FR_BTN_FIND0, L"fbe.words.find.button.find" },
	{ IDS_WORDS_FR_BTN_FIND1, L"fbe.words.find.button.next" },
};

static std::map<UINT, CStringW> g_runtimeStrings;
static std::map<std::wstring, CStringW> g_runtimeStringsByKey;
static bool g_runtimeInitialized = false;


static const wchar_t kRuntimeLocaleEnvironment[] = L"FBE_NEXT_UI_LOCALE";
static const wchar_t kRuntimeLocaleFileName[] = L"interface-locale.txt";

static bool GetRuntimeLocaleFilePath(CPath& localePath)
{
	// DeploymentContext preserves the installed %LOCALAPPDATA%\FBE Next path
	// while portable copies use Data\Settings beside FBE.exe.
	const std::wstring settingsDirectory = DeploymentContext::SettingsDirectory();
	if (settingsDirectory.empty()) return false;
	localePath = settingsDirectory.c_str();
	localePath.Append(kRuntimeLocaleFileName);
	return true;
}

static bool WritePublishedRuntimeLocaleName(const wchar_t* localeName)
{
	if (!FbeRuntimeLocalization::IsKnownRuntimeLocaleName(localeName))
		return false;

	CPath localePath;
	if (!GetRuntimeLocaleFilePath(localePath))
		return false;

	CPath localeDir(localePath);
	localeDir.RemoveFileSpec();
	::CreateDirectoryW(localeDir, NULL);

	char utf8[64] = {};
	const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, localeName, -1, utf8, static_cast<int>(sizeof(utf8)), NULL, NULL);
	if (bytes <= 1)
		return false;

	CPath temporaryPath(localePath);
	temporaryPath += L".tmp";
	HANDLE file = ::CreateFileW(temporaryPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return false;

	DWORD written = 0;
	const BOOL ok = ::WriteFile(file, utf8, static_cast<DWORD>(bytes - 1), &written, NULL) &&
		::FlushFileBuffers(file);
	::CloseHandle(file);
	if (!ok || written != static_cast<DWORD>(bytes - 1)) {
		::DeleteFileW(temporaryPath);
		return false;
	}
	if (!::MoveFileExW(temporaryPath, localePath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		::DeleteFileW(temporaryPath);
		return false;
	}
	return true;
}


static void EnsureFbeRuntimeStrings()
{
	if (g_runtimeInitialized)
		return;

	g_runtimeInitialized = true;
	FbeRuntimeLocalization::LoadRuntimeStringFiles(_Module.GetModuleInstance(), L"fbe.json", g_runtimeStringBindings, _countof(g_runtimeStringBindings), g_runtimeStrings);
	FbeRuntimeLocalization::LoadRuntimeStringFiles(_Module.GetModuleInstance(), L"fbe.json", g_runtimeStringsByKey);
}

int FbeLoadRuntimeString(UINT id, wchar_t* buffer, int bufferChars)
{
	if (buffer == NULL || bufferChars <= 0)
		return 0;

	buffer[0] = L'\0';
	EnsureFbeRuntimeStrings();

	std::map<UINT, CStringW>::const_iterator it = g_runtimeStrings.find(id);
	if (it == g_runtimeStrings.end())
		return 0;

	wcsncpy_s(buffer, bufferChars, it->second, _TRUNCATE);
	return static_cast<int>(wcslen(buffer));
}

CString FbeLoadRuntimeString(UINT id, LPCWSTR fallback)
{
	EnsureFbeRuntimeStrings();

	std::map<UINT, CStringW>::const_iterator it = g_runtimeStrings.find(id);
	if (it != g_runtimeStrings.end())
		return it->second;

	wchar_t buffer[4096] = {};
	if (::LoadStringW(_Module.GetResourceInstance(), id, buffer, _countof(buffer)) > 0)
		return CString(buffer);

	return fallback != NULL ? CString(fallback) : CString();
}

CString FbeLoadRuntimeStringByKey(LPCWSTR key, LPCWSTR fallback)
{
	if (key == NULL || key[0] == 0)
		return fallback != NULL ? CString(fallback) : CString();

	EnsureFbeRuntimeStrings();

	std::map<std::wstring, CStringW>::const_iterator it = g_runtimeStringsByKey.find(std::wstring(key));
	if (it != g_runtimeStringsByKey.end())
		return it->second;

    return fallback != NULL ? CString(fallback) : CString();
}

void FbeApplyRuntimeDialogLocalization(HWND dialog, UINT dialogId)
{
	if (dialog == NULL)
		return;

	for (size_t index = 0; index < _countof(g_runtimeDialogBindings); ++index)
	{
		const RuntimeDialogBinding& binding = g_runtimeDialogBindings[index];
		if (binding.dialogId != dialogId)
			continue;
		const CString text = FbeLoadRuntimeStringByKey(binding.key);
		if (text.IsEmpty())
			continue;
		if (binding.controlId == 0)
			::SetWindowText(dialog, text);
		else {
			HWND control = ::GetDlgItem(dialog, binding.controlId);
			if (control != NULL)
				::SetWindowText(control, text);
		}
	}
}

bool FbeIsRuntimeLocaleInstalled(LPCWSTR localeName)
{
    return FbeRuntimeLocalization::RuntimeStringFileExists(
        _Module.GetModuleInstance(), localeName, L"fbe.json");
}

void FbePublishRuntimeLocaleName(LPCWSTR localeName)
{
	if (!FbeRuntimeLocalization::IsKnownRuntimeLocaleName(localeName))
		return;

	::SetEnvironmentVariableW(kRuntimeLocaleEnvironment, localeName);
	WritePublishedRuntimeLocaleName(localeName);
}

void FbeResetRuntimeLocalization()
{
	g_runtimeStrings.clear();
	g_runtimeStringsByKey.clear();
	g_runtimeInitialized = false;
}
