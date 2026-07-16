#include "stdafx.h"
#include "resource.h"
#include "RuntimeLocalization.h"
#include "..\common\RuntimeLocalizationCommon.h"

#include <map>
#include <string>
#include <vector>

struct RuntimeStringBinding {
	UINT id;
	const wchar_t* key;
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
	wchar_t localAppData[MAX_PATH] = {};
	const DWORD length = ::GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, _countof(localAppData));
	if (length == 0 || length >= _countof(localAppData))
		return false;

	localePath = localAppData;
	localePath.Append(L"FBE Next");
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

	HANDLE file = ::CreateFileW(localePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return false;

	DWORD written = 0;
	const BOOL ok = ::WriteFile(file, utf8, static_cast<DWORD>(bytes - 1), &written, NULL);
	::CloseHandle(file);
	return ok && written == static_cast<DWORD>(bytes - 1);
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
