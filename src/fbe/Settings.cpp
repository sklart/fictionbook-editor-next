#include "stdafx.h"
#include "..\\common\\DeploymentContext.h"
#include "XmlSourceThemes.h"

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
const wchar_t XML_SRC_SHOW_EOL_KEY[]	= L"XMLSrcShowEOL";
const wchar_t XML_SRC_SHOW_SPACE_KEY[]	= L"XMLSrcShowSpace";
const wchar_t XML_SRC_SHOW_SPECIAL_CHARS_KEY[] = L"XMLSrcShowSpecialChars";
const wchar_t XML_SRC_SPECIAL_CHARS_STYLE_KEY[] = L"XMLSrcSpecialCharsStyle";
const wchar_t FAST_MODE_KEY[]			= L"FastMode";
const wchar_t FONT_KEY[]				= L"Font";
const wchar_t SRC_FONT_KEY[]			= L"SrcFont";
const wchar_t VIEW_STATUS_BAR_KEY[]		= L"ViewStatusBar";
const wchar_t VIEW_DOCUMENT_TREE_KEY[]	= L"ViewDocumentTree";
const wchar_t SPLITTER_POS_KEY[]		= L"SplitterPos";
const wchar_t TOOLBARS_SETTINGS_KEY[]	= L"Toolbars";
const wchar_t SCRIPT_COMMAND_IDS_KEY[] = L"ScriptCommandIds";
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

// XML serialization filenames
const wchar_t SETTINGS_XML_FILE[] = L"Settings.xml";
const wchar_t HOTKEYS_XML_FILE[] = L"Hotkeys.xml";
const wchar_t WORDS_XML_FILE[] = L"Words.xml";

#include "Settings.h"

#include "ElementDescMnr.h"
extern CElementDescMnr _EDMnr;

static DWORD NormalizeInterfaceLanguageID(DWORD langId)
{
	switch(langId)
	{
	case FBE_INTERFACE_LANGUAGE_AUTO:
	case FBE_INTERFACE_LANGUAGE_ENGLISH:
	case FBE_INTERFACE_LANGUAGE_RUSSIAN:
	case FBE_INTERFACE_LANGUAGE_UKRAINIAN:
	case FBE_INTERFACE_LANGUAGE_GERMAN:
	case FBE_INTERFACE_LANGUAGE_FRENCH:
	case FBE_INTERFACE_LANGUAGE_SPANISH:
	case FBE_INTERFACE_LANGUAGE_ITALIAN:
	case FBE_INTERFACE_LANGUAGE_POLISH:
	case FBE_INTERFACE_LANGUAGE_PORTUGUESE:
	case FBE_INTERFACE_LANGUAGE_DUTCH:
	case FBE_INTERFACE_LANGUAGE_CZECH:
	case FBE_INTERFACE_LANGUAGE_BULGARIAN:
		return langId;
	}

	// Миграция старых настроек: раньше здесь хранились WinAPI LANG_*,
	// которые не являются стабильными идентификаторами UI-локалей FBE.
	switch(PRIMARYLANGID(langId))
	{
	case LANG_RUSSIAN:
		return FBE_INTERFACE_LANGUAGE_RUSSIAN;
	case LANG_UKRAINIAN:
		return FBE_INTERFACE_LANGUAGE_UKRAINIAN;
	case LANG_GERMAN:
		return FBE_INTERFACE_LANGUAGE_GERMAN;
	case LANG_FRENCH:
		return FBE_INTERFACE_LANGUAGE_FRENCH;
	case LANG_SPANISH:
		return FBE_INTERFACE_LANGUAGE_SPANISH;
	case LANG_ITALIAN:
		return FBE_INTERFACE_LANGUAGE_ITALIAN;
	case LANG_POLISH:
		return FBE_INTERFACE_LANGUAGE_POLISH;
	case LANG_PORTUGUESE:
		return FBE_INTERFACE_LANGUAGE_PORTUGUESE;
	case LANG_DUTCH:
		return FBE_INTERFACE_LANGUAGE_DUTCH;
	case LANG_CZECH:
		return FBE_INTERFACE_LANGUAGE_CZECH;
	case LANG_BULGARIAN:
		return FBE_INTERFACE_LANGUAGE_BULGARIAN;
	case LANG_ENGLISH:
	default:
		return FBE_INTERFACE_LANGUAGE_ENGLISH;
	}
}

static DWORD InterfaceLanguageFromLocaleName(LPCWSTR localeName)
{
	if(localeName == NULL || localeName[0] == 0)
		return FBE_INTERFACE_LANGUAGE_ENGLISH;

	if(::lstrcmpiW(localeName, L"ru-RU") == 0)
		return FBE_INTERFACE_LANGUAGE_RUSSIAN;
	if(::lstrcmpiW(localeName, L"uk-UA") == 0)
		return FBE_INTERFACE_LANGUAGE_UKRAINIAN;
	if(::lstrcmpiW(localeName, L"de-DE") == 0)
		return FBE_INTERFACE_LANGUAGE_GERMAN;
	if(::lstrcmpiW(localeName, L"fr-FR") == 0)
		return FBE_INTERFACE_LANGUAGE_FRENCH;
	if(::lstrcmpiW(localeName, L"es-ES") == 0)
		return FBE_INTERFACE_LANGUAGE_SPANISH;
	if(::lstrcmpiW(localeName, L"it-IT") == 0)
		return FBE_INTERFACE_LANGUAGE_ITALIAN;
	if(::lstrcmpiW(localeName, L"pl-PL") == 0)
		return FBE_INTERFACE_LANGUAGE_POLISH;
	if(::lstrcmpiW(localeName, L"pt-PT") == 0)
		return FBE_INTERFACE_LANGUAGE_PORTUGUESE;
	if(::lstrcmpiW(localeName, L"nl-NL") == 0)
		return FBE_INTERFACE_LANGUAGE_DUTCH;
	if(::lstrcmpiW(localeName, L"cs-CZ") == 0)
		return FBE_INTERFACE_LANGUAGE_CZECH;
	if(::lstrcmpiW(localeName, L"bg-BG") == 0)
		return FBE_INTERFACE_LANGUAGE_BULGARIAN;
	return FBE_INTERFACE_LANGUAGE_ENGLISH;
}

CSettings::CSettings():m_need_restart(false), keycodes(0)
{

}

CSettings::~CSettings()
{
}

void CSettings::Init()
{
	const TCHAR* appname = L"FictionBook Editor Next";
	m_key_path = L"Software\\FBETeam\\";
	m_key_path += appname;
	// Portable copies deliberately retain a key path for legacy readers but do
	// not create an FBE-owned registry branch or persist settings there.
	if (DeploymentContext::RegistryPersistenceAllowed())
		m_key.Create(HKEY_CURRENT_USER, m_key_path);
}

void CSettings::InitHotkeyGroups()
{
	// File group hotkeys
	CHotkeysGroup file_hotkeys_group(L"File", IDS_HOTKEY_GROUP_FILE);

	// Open
	CHotkey FileOpen(L"Open", IDS_HOTKEY_FILE_OPEN, FCONTROL, ID_FILE_OPEN, U::StringToKeycode(L"O"));
	file_hotkeys_group.m_hotkeys.push_back(FileOpen);

	// Save
	CHotkey FileSave(L"Save", IDS_HOTKEY_FILE_SAVE, NULL, ID_FILE_SAVE, VK_F2);
	file_hotkeys_group.m_hotkeys.push_back(FileSave);

	// Save as...
	CHotkey FileSaveAs(L"SaveAs", IDS_HOTKEY_FILE_SAVEAS, FSHIFT, ID_FILE_SAVE_AS, VK_F2);
	file_hotkeys_group.m_hotkeys.push_back(FileSaveAs);

	// Validate
	CHotkey FileValidate(L"Validate", IDS_HOTKEY_FILE_VALIDATE, NULL, ID_FILE_VALIDATE, VK_F8);
	file_hotkeys_group.m_hotkeys.push_back(FileValidate);

	//Edit group hotkeys
	CHotkeysGroup edit_hotkeys_group(L"Edit", IDS_HOTKEY_GROUP_EDIT);

	// Add annotation
	CHotkey EditAddAnnotation(L"AddAnnotation",
								IDS_HOTKEY_EDIT_ADD_ANNOTATION,
								FCONTROL,
								ID_EDIT_ADD_ANN,
								U::StringToKeycode(L"J"));
	edit_hotkeys_group.m_hotkeys.push_back(EditAddAnnotation);

	// Add body
	CHotkey EditAddBody(L"AddBody", IDS_HOTKEY_EDIT_ADD_BODY, FALT+FSHIFT, ID_EDIT_ADD_BODY, U::StringToKeycode(L"B"));
	edit_hotkeys_group.m_hotkeys.push_back(EditAddBody);

	// Add epigraph
	CHotkey EditAddEpigraph(L"AddEpigraph",
							IDS_HOTKEY_EDIT_ADD_EPIGRAPH,
							FCONTROL,
							ID_EDIT_ADD_EPIGRAPH,
							U::StringToKeycode(L"N"));
	edit_hotkeys_group.m_hotkeys.push_back(EditAddEpigraph);

	// Add section image
	CHotkey EditAddSectionImage(L"AddSectionImage", IDS_HOTKEY_EDIT_ADD_IMAGE, FCONTROL, ID_EDIT_ADD_IMAGE,
		U::StringToKeycode(L"G"));
	edit_hotkeys_group.m_hotkeys.push_back(EditAddSectionImage);

	// Add text author
	CHotkey EditAddTextAuthor(L"AddTextAuthor", IDS_HOTKEY_EDIT_ADD_TA, FCONTROL, ID_EDIT_ADD_TA, U::StringToKeycode(L"D"));
	edit_hotkeys_group.m_hotkeys.push_back(EditAddTextAuthor);

	// Add title
	CHotkey EditAddTitle(L"AddTitle", IDS_HOTKEY_EDIT_ADD_TITLE, FCONTROL, ID_EDIT_ADD_TITLE,U::StringToKeycode(L"T"));
	edit_hotkeys_group.m_hotkeys.push_back(EditAddTitle);

	// Bold
	CHotkey EditBold(L"Bold", IDS_HOTKEY_EDIT_BOLD, FCONTROL, ID_EDIT_BOLD, U::StringToKeycode(L"B"));
	edit_hotkeys_group.m_hotkeys.push_back(EditBold);

	// Clone
	CHotkey EditClone(L"Clone", IDS_HOTKEY_EDIT_CLONE, FCONTROL, ID_EDIT_CLONE, VK_RETURN);
	edit_hotkeys_group.m_hotkeys.push_back(EditClone);

	// Copy
	CHotkey EditCopy(L"Copy", IDS_HOTKEY_EDIT_COPY, FCONTROL, ID_EDIT_COPY, U::StringToKeycode(L"C"));
	edit_hotkeys_group.m_hotkeys.push_back(EditCopy);

	// Cut
	CHotkey EditCut(L"Cut", IDS_HOTKEY_EDIT_CUT, FCONTROL, ID_EDIT_CUT, U::StringToKeycode(L"X"));
	edit_hotkeys_group.m_hotkeys.push_back(EditCut);

	// Find
	CHotkey EditFind(L"Find", IDS_HOTKEY_EDIT_FIND, FCONTROL, ID_EDIT_FIND, U::StringToKeycode(L"F"));
	edit_hotkeys_group.m_hotkeys.push_back(EditFind);

	// Find next
	CHotkey EditFindNext(L"FindNext", IDS_HOTKEY_EDIT_FIND_NEXT, NULL, ID_EDIT_FINDNEXT, VK_F3);
	edit_hotkeys_group.m_hotkeys.push_back(EditFindNext);

	// Incremental search
	CHotkey EditIncrementalSearch(L"IncrementalSearch",
									IDS_HOTKEY_EDIT_INCREMENTAL_SEARCH,
									FALT,
									ID_EDIT_INCSEARCH,
									U::StringToKeycode(L"I"));
	edit_hotkeys_group.m_hotkeys.push_back(EditIncrementalSearch);

	// Insert cite
	CHotkey EditInsertCite(L"InsertCite",
							IDS_HOTKEY_EDIT_INSERT_CITE,
							FALT,
							ID_EDIT_INS_CITE,
							U::StringToKeycode(L"C"));
	edit_hotkeys_group.m_hotkeys.push_back(EditInsertCite);

	// Insert image
	CHotkey EditInsertImage(L"InsertImage",
							IDS_HOTKEY_EDIT_INSERT_IMAGE,
							FCONTROL,
							ID_EDIT_INS_IMAGE,
							U::StringToKeycode(L"M"));
	edit_hotkeys_group.m_hotkeys.push_back(EditInsertImage);


	// Insert inline image - added by SeNS
	CHotkey EditInsertInlineImage(L"InsertInlineImage",
							IDS_HOTKEY_EDIT_INSERT_INLINEIMAGE,
							FALT,
							ID_EDIT_INS_INLINEIMAGE,
							U::StringToKeycode(L"M"));
	edit_hotkeys_group.m_hotkeys.push_back(EditInsertInlineImage);


	// Insert poem
	CHotkey EditInsertPoem(L"InsertPoem",
							IDS_HOTKEY_EDIT_INSERT_POEM,
							FCONTROL,
							ID_EDIT_INS_POEM,
							U::StringToKeycode(L"P"));
	edit_hotkeys_group.m_hotkeys.push_back(EditInsertPoem);

	// Italic
	CHotkey EditItalic(L"Italic", IDS_HOTKEY_EDIT_ITALIC, FCONTROL, ID_EDIT_ITALIC, U::StringToKeycode(L"I"));
	edit_hotkeys_group.m_hotkeys.push_back(EditItalic);

	// Merge
	CHotkey EditMerge(L"Merge", IDS_HOTKEY_EDIT_MERGE, FALT, ID_EDIT_MERGE, VK_DELETE);
	edit_hotkeys_group.m_hotkeys.push_back(EditMerge);

	// Added by SeNS
	CHotkey EditSub(L"Subscript", IDS_HOTKEY_EDIT_SUB, NULL, ID_EDIT_SUB, NULL);
	edit_hotkeys_group.m_hotkeys.push_back(EditSub);

	CHotkey EditSup(L"Superscript", IDS_HOTKEY_EDIT_SUP, NULL, ID_EDIT_SUP, NULL);
	edit_hotkeys_group.m_hotkeys.push_back(EditSup);

	// Paste : changed by SeNS
	CHotkey EditPaste(L"Paste", IDS_HOTKEY_EDIT_PASTE, FSHIFT, ID_EDIT_PASTE, VK_INSERT);
	edit_hotkeys_group.m_hotkeys.push_back(EditPaste);

	// Redo
	CHotkey EditRedo(L"Redo", IDS_HOTKEY_EDIT_REDO, FCONTROL, ID_EDIT_REDO, U::StringToKeycode(L"Y"));
	edit_hotkeys_group.m_hotkeys.push_back(EditRedo);

	// Replace
	CHotkey EditReplace(L"Replace", IDS_HOTKEY_EDIT_REPLACE, FCONTROL, ID_EDIT_REPLACE, U::StringToKeycode(L"H"));
	edit_hotkeys_group.m_hotkeys.push_back(EditReplace);

	// Split
	CHotkey EditSplit(L"Split", IDS_HOTKEY_EDIT_SPLIT, FSHIFT, ID_EDIT_SPLIT, VK_RETURN);
	edit_hotkeys_group.m_hotkeys.push_back(EditSplit);

	// Undo
	CHotkey EditUndo(L"Undo", IDS_HOTKEY_EDIT_UNDO, FCONTROL, ID_EDIT_UNDO, U::StringToKeycode(L"Z"));
	edit_hotkeys_group.m_hotkeys.push_back(EditUndo);

	// Insert table
	CHotkey EditInsertTable(L"InsertTable",
							IDS_HOTKEY_EDIT_INSERT_TABLE,
							FALT,
							ID_INSERT_TABLE,
							U::StringToKeycode(L"T"));
	edit_hotkeys_group.m_hotkeys.push_back(EditInsertTable);

	// Remove outer section
	CHotkey RemoveOuterSection(L"RemoveOuterSection",
		IDS_HOTKEY_EDIT_REMOVE_OUTER_SECTION,
		FALT | FCONTROL,
		ID_EDIT_REMOVE_OUTER_SECTION,
		VK_SPACE);
	edit_hotkeys_group.m_hotkeys.push_back(RemoveOuterSection);

	//Navigation group hotkeys
	CHotkeysGroup navigation_hotkeys_group(L"Navigation", IDS_HOTKEY_GROUP_NAVIGATION);

	// Goto reference
	CHotkey NavigationGotoFootnote (L"GotoFootnote",
									IDS_HOTKEY_NAVIGATION_GOTO_FOOTNOTE,
									FCONTROL,
									ID_GOTO_FOOTNOTE,
									VK_BACK);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationGotoFootnote);

	// Goto matched tag
	CHotkey NavigationGotoMatchingTag (L"GotoMatchingTag",
								  		IDS_HOTKEY_NAVIGATION_GOTO_MATCHTAG,
										FALT,
									    ID_GOTO_MATCHTAG,
										U::StringToKeycode(L":"));
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationGotoMatchingTag);

	// Goto wrong tag
	CHotkey NavigationGotoWrongTag (L"GotoWrongTag",
								  	 IDS_HOTKEY_NAVIGATION_GOTO_WRONGTAG,
									 FCONTROL,
									 ID_GOTO_WRONGTAG,
									 U::StringToKeycode(L":"));
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationGotoWrongTag);

	// Next item
	CHotkey NavigationNextItem(L"NextItem",
									IDS_HOTKEY_NAVIGATION_NEXT_ITEM,
									FCONTROL,
									ID_NEXT_ITEM,
									VK_TAB);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationNextItem);

	// Collapse tree 1 level
	CHotkey NavigationCollapse1(L"Collapse1",
		IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE1,
		NULL,
		ID_SCI_COLLAPSE1,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationCollapse1);

	// Collapse tree 2 levels
	CHotkey NavigationCollapse2(L"Collapse2",
		IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE2,
		NULL,
		ID_SCI_COLLAPSE2,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationCollapse2);

	// Collapse tree 3 levels
	CHotkey NavigationCollapse3(L"Collapse3",
		IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE3,
		NULL,
		ID_SCI_COLLAPSE3,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationCollapse3);

	// Collapse tree 4 levels
	CHotkey NavigationCollapse4(L"Collapse4",
		IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE4,
		NULL,
		ID_SCI_COLLAPSE4,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationCollapse4);

	// Collapse tree 5 levels
	CHotkey NavigationCollapse5(L"Collapse5",
		IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE5,
		NULL,
		ID_SCI_COLLAPSE5,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationCollapse5);

	// Collapse tree 6 levels
	CHotkey NavigationCollapse6(L"Collapse6",
		IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE6,
		NULL,
		ID_SCI_COLLAPSE6,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationCollapse6);

	// Collapse tree 7 levels
	CHotkey NavigationCollapse7(L"Collapse7",
		IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE7,
		NULL,
		ID_SCI_COLLAPSE7,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationCollapse7);

	// Collapse tree 8 levels
	CHotkey NavigationCollapse8(L"Collapse8",
		IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE8,
		NULL,
		ID_SCI_COLLAPSE8,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationCollapse8);

	// Collapse tree 9 levels
	CHotkey NavigationCollapse9(L"Collapse9",
		IDS_HOTKEY_NAVIGATION_SCI_COLLAPSE9,
		NULL,
		ID_SCI_COLLAPSE9,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationCollapse9);

	// Expand tree 1 level
	CHotkey NavigationExpand1(L"Expand1",
		IDS_HOTKEY_NAVIGATION_SCI_EXPAND1,
		NULL,
		ID_SCI_EXPAND1,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationExpand1);

	// Expand tree 2 levels
	CHotkey NavigationExpand2(L"Expand2",
		IDS_HOTKEY_NAVIGATION_SCI_EXPAND2,
		NULL,
		ID_SCI_EXPAND2,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationExpand2);

	// Expand tree 3 levels
	CHotkey NavigationExpand3(L"Expand3",
		IDS_HOTKEY_NAVIGATION_SCI_EXPAND3,
		NULL,
		ID_SCI_EXPAND3,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationExpand3);

	// Expand tree 4 levels
	CHotkey NavigationExpand4(L"Expand4",
		IDS_HOTKEY_NAVIGATION_SCI_EXPAND4,
		NULL,
		ID_SCI_EXPAND4,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationExpand4);

	// Expand tree 5 levels
	CHotkey NavigationExpand5(L"Expand5",
		IDS_HOTKEY_NAVIGATION_SCI_EXPAND5,
		NULL,
		ID_SCI_EXPAND5,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationExpand5);

	// Expand tree 6 levels
	CHotkey NavigationExpand6(L"Expand6",
		IDS_HOTKEY_NAVIGATION_SCI_EXPAND6,
		NULL,
		ID_SCI_EXPAND6,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationExpand6);

	// Expand tree 7 levels
	CHotkey NavigationExpand7(L"Expand7",
		IDS_HOTKEY_NAVIGATION_SCI_EXPAND7,
		NULL,
		ID_SCI_EXPAND7,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationExpand7);

	// Expand tree 8 levels
	CHotkey NavigationExpand8(L"Expand8",
		IDS_HOTKEY_NAVIGATION_SCI_EXPAND8,
		NULL,
		ID_SCI_EXPAND8,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationExpand8);

	// Expand tree 9 levels
	CHotkey NavigationExpand9(L"Expand9",
		IDS_HOTKEY_NAVIGATION_SCI_EXPAND9,
		NULL,
		ID_SCI_EXPAND9,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationExpand9);

	// Select href
	CHotkey NavigationSelectHref(L"SelectHref",
		IDS_HOTKEY_NAVIGATION_SELECT_HREF,
		FALT,
		ID_SELECT_HREF,
		U::StringToKeycode(L"H"));
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationSelectHref);

	// Select ID
	CHotkey NavigationSelectID(L"SelectID",
		IDS_HOTKEY_NAVIGATION_SELECT_ID,
		NULL,
		ID_SELECT_ID,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationSelectID);

	// Select table ID
	CHotkey NavigationSelectTableID(L"SelectTableID",
		IDS_HOTKEY_NAVIGATION_SELECT_ID_TABLE,
		NULL,
		ID_SELECT_IDT,
		NULL);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationSelectTableID);

	// Select text
	CHotkey NavigationSelectText(L"SelectText",
		IDS_HOTKEY_NAVIGATION_SELECT_TEXT,
		NULL,
		ID_SELECT_TEXT,
		VK_ESCAPE);
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationSelectText);

	// Select tree
	CHotkey NavigationSelectTree(L"SelectTree",
		IDS_HOTKEY_NAVIGATION_SELECT_TREE,
		FALT,
		ID_SELECT_TREE,
		U::StringToKeycode(L"Q"));
	navigation_hotkeys_group.m_hotkeys.push_back(NavigationSelectTree);

	//Style group hotkeys
	CHotkeysGroup style_hotkeys_group(L"Style", IDS_HOTKEY_GROUP_STYLE);

	// Link
	CHotkey StyleLink(L"Link", IDS_HOTKEY_STYLE_LINK, FCONTROL, ID_STYLE_LINK, U::StringToKeycode(L"L"));
	style_hotkeys_group.m_hotkeys.push_back(StyleLink);

	// No link
	CHotkey StyleNoLink(L"NoLink", IDS_HOTKEY_STYLE_NO_LINK, FCONTROL, ID_STYLE_NOLINK, U::StringToKeycode(L"U"));
	style_hotkeys_group.m_hotkeys.push_back(StyleNoLink);

	// Normal
	CHotkey StyleNormal(L"Normal", IDS_HOTKEY_STYLE_NORMAL, FALT, ID_STYLE_NORMAL, U::StringToKeycode(L"N"));
	style_hotkeys_group.m_hotkeys.push_back(StyleNormal);

	// Note
	CHotkey StyleNote(L"Note", IDS_HOTKEY_STYLE_NOTE, FCONTROL, ID_STYLE_NOTE, U::StringToKeycode(L"W"));
	style_hotkeys_group.m_hotkeys.push_back(StyleNote);

	// Subtitle
	CHotkey StyleSubtitle(L"Subtitle", IDS_HOTKEY_STYLE_SUBTITLE, FALT, ID_STYLE_SUBTITLE, U::StringToKeycode(L"S"));
	style_hotkeys_group.m_hotkeys.push_back(StyleSubtitle);

	// Text author
	CHotkey StyleTextAuthor(L"TextAuthor",
								IDS_HOTKEY_STYLE_TEXT_AUTHOR,
								FALT,
								ID_STYLE_TEXTAUTHOR,
								U::StringToKeycode(L"A"));
	style_hotkeys_group.m_hotkeys.push_back(StyleTextAuthor);

	// View group hotkeys
	CHotkeysGroup view_hotkeys_group(L"View", IDS_HOTKEY_GROUP_VIEW);

	// View body
	CHotkey ViewBody(L"Body", IDS_HOTKEY_VIEW_BODY, FALT, ID_VIEW_BODY, VK_F2);
	view_hotkeys_group.m_hotkeys.push_back(ViewBody);

	// View description
	CHotkey ViewDescription(L"Description", IDS_HOTKEY_VIEW_DESCRIPTION, FALT, ID_VIEW_DESC, VK_F1);
	view_hotkeys_group.m_hotkeys.push_back(ViewDescription);

	// View source
	CHotkey ViewSource(L"Source", IDS_HOTKEY_VIEW_SOURCE, FALT, ID_VIEW_SOURCE, VK_F3);
	view_hotkeys_group.m_hotkeys.push_back(ViewSource);

	// Added by SeNS
	// Fast mode
	CHotkey FastMode(L"Fast mode", IDS_HOTKEY_FASTMODE, NULL, ID_VIEW_FASTMODE, VK_F5);
	view_hotkeys_group.m_hotkeys.push_back(FastMode);

	CHotkey ViewTree(L"Toggle Tree View", IDS_HOTKEY_TREEVIEW, FCONTROL, ID_VIEW_TREE, VK_F5);
	view_hotkeys_group.m_hotkeys.push_back(ViewTree);

	// Scripts group hotkeys
	CHotkeysGroup scripts_hotkeys_group(L"Scripts", IDS_HOTKEY_GROUP_SCRIPTS);

	// Last script
	CHotkey ScriptsLastScript(L"LastScript",
								IDS_HOTKEY_SCRIPTS_LAST_SCRIPT,
								FCONTROL,
								ID_LAST_SCRIPT,
								VK_OEM_3);
	scripts_hotkeys_group.m_hotkeys.push_back(ScriptsLastScript);

	// Plugins group hotkeys
	CHotkeysGroup plugins_hotkeys_group(L"Plugins", IDS_HOTKEY_GROUP_PLUGINS);

	// Last plugin
	CHotkey PluginsLastPlugin(L"LastPlugin",
		IDS_HOTKEY_PLUGINS_LAST_PLUGIN,
		FALT,
		ID_LAST_PLUGIN,
		VK_OEM_3);
	plugins_hotkeys_group.m_hotkeys.push_back(PluginsLastPlugin);

	// Tools group hotkeys
	CHotkeysGroup tools_hotkeys_group(L"Tools", IDS_HOTKEY_GROUP_TOOLS);

	// Words
	CHotkey ToolsWords(L"Words", IDS_HOTKEY_TOOLS_WORDS, FALT, ID_TOOLS_WORDS, U::StringToKeycode(L"W"));
	tools_hotkeys_group.m_hotkeys.push_back(ToolsWords);

	// Settings: Ctrl+, is a layout-independent physical OEM key and is free
	// among the built-in defaults.  It remains fully user-configurable.
	CHotkey ToolsOptions(L"Options", IDS_HOTKEY_TOOLS_OPTIONS, FCONTROL, ID_VIEW_OPTIONS, VK_OEM_COMMA);
	tools_hotkeys_group.m_hotkeys.push_back(ToolsOptions);

	// Added by SeNS
	CHotkey ToolsSpell(L"Spell check", IDS_HOTKEY_TOOLS_SPELL, NULL, ID_TOOLS_SPELLCHECK, VK_F7);
	tools_hotkeys_group.m_hotkeys.push_back(ToolsSpell);

	CHotkey ToolsSpellHighlight(L"Toggle highlight", IDS_HOTKEY_TOOLS_SPELLHIGHLIGHT, FSHIFT, ID_TOOLS_SPELLCHECK_HIGHLIGHT, VK_F7);
	tools_hotkeys_group.m_hotkeys.push_back(ToolsSpellHighlight);

	CHotkey ToolsSpellAddToDict(L"Add to dictionary", IDC_SPELL_ADD2DICT, NULL, IDC_SPELL_ADD2DICT, NULL);
	tools_hotkeys_group.m_hotkeys.push_back(ToolsSpellAddToDict);

	CHotkey ToolsSpellIgnore(L"Ignore", IDC_SPELL_IGNOREALL, NULL, IDC_SPELL_IGNOREALL, NULL);
	tools_hotkeys_group.m_hotkeys.push_back(ToolsSpellIgnore);

	// Symbols group hotkeys
	CHotkeysGroup symbols_hotkeys_group(L"Symbols", IDS_HOTKEY_GROUP_SYMBOLS);

	wchar_t vals[32767 + 1];
	ZeroMemory(vals, sizeof(vals));

	int valcount = ::GetPrivateProfileSection(L"symbols", vals, 32767, U::GetProgDir() + L"symbols.ini");

	CSimpleMap<CString, CString> mapSymbs;
	int k = 0;
	while(k < valcount && vals[k] != 0)
	{
		CString str = &vals[k];
		CString resKey, resVal;
		int curPos = 0;

		resKey = str.Tokenize(L"=", curPos);
		resVal = str.Tokenize(L"=", curPos);

		if(!resKey.IsEmpty() && !resVal.IsEmpty())
			mapSymbs.Add(resKey + L"=", resVal);

		k += (wcslen(&vals[k]) + 1);
	}

	for(int i = 1; i < 100; ++i)
	{
		CString pattern, langPatt;
		pattern.Format(L"%s%d=", L"sym", i);
		langPatt.Format(L"%s%d_%d=", L"sym", i, static_cast<int>(m_interface_lang_id));
		if(!mapSymbs.Lookup(pattern).IsEmpty())
		{
			int val = _wtoi(mapSymbs.Lookup(pattern).GetBuffer());
			CString desc;
			if(!mapSymbs.Lookup(langPatt).IsEmpty())
				desc = mapSymbs.Lookup(langPatt) + L" ";
			// special case for combining chars
			if ((val>=0x0300 && val<=0x036F) || (val>=0x1DC0 && val<=0x1DFF) || 
				(val>=0x20D0 && val<=0x20FF) || (val>=0xFE20 && val<=0xFE2F)) desc += CString(L"( "); 
			else desc += CString(L"(");
			
			if (val==160) desc += GetNBSPChar() + CString(L")"); else desc += (wchar_t)val + CString(L")");

			// special fix for nbsp
			if (val==160) val = m_nbsp_char[0];

			CHotkey Symbol(mapSymbs.Lookup(pattern).GetBuffer(),
				desc,
				wchar_t(val),
				NULL,
				ID_EDIT_INS_SYMBOL + i,
				NULL);
			symbols_hotkeys_group.m_hotkeys.push_back(Symbol);
		}
	}

	// Collect all hotkey groups and sort
	m_hotkey_groups.push_back(file_hotkeys_group);
	m_hotkey_groups.push_back(edit_hotkeys_group);
	m_hotkey_groups.push_back(navigation_hotkeys_group);
	m_hotkey_groups.push_back(style_hotkeys_group);
	m_hotkey_groups.push_back(tools_hotkeys_group);
	m_hotkey_groups.push_back(view_hotkeys_group);
	m_hotkey_groups.push_back(plugins_hotkeys_group);
	m_hotkey_groups.push_back(scripts_hotkeys_group);
	m_hotkey_groups.push_back(symbols_hotkeys_group);

	std::sort(m_hotkey_groups.begin(), m_hotkey_groups.end());
}

void CSettings::Close()
{
	m_key.Close();
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
	properties.push_back(XML_SRC_SHOW_EOL_KEY);
	properties.push_back(XML_SRC_SHOW_SPACE_KEY);
	properties.push_back(XML_SRC_SHOW_SPECIAL_CHARS_KEY);
	properties.push_back(XML_SRC_SPECIAL_CHARS_STYLE_KEY);
	properties.push_back(FAST_MODE_KEY);
	properties.push_back(FONT_KEY);
	properties.push_back(SRC_FONT_KEY);
	properties.push_back(VIEW_STATUS_BAR_KEY);
	properties.push_back(VIEW_DOCUMENT_TREE_KEY);
	properties.push_back(SPLITTER_POS_KEY);
	properties.push_back(TOOLBARS_SETTINGS_KEY);
	properties.push_back(SCRIPT_COMMAND_IDS_KEY);
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
	else if(sProperty == RESTORE_FILE_POS_KEY)
	{
		m_restore_file_position = StrToBool(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == INTERFACE_LANG_KEY)
	{
		m_interface_lang_id = NormalizeInterfaceLanguageID(StrToInt(sValue.GetStringValue()));
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
		m_scripts_folder = sValue.GetStringValue();
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
		m_image_type = StrToInt(sValue.GetStringValue());
		return true;
	}
	else if(sProperty == JPEG_QUALITY_KEY)
	{
		m_jpeg_quality = StrToInt(sValue.GetStringValue());
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
	CString fullpath = U::GetSettingsDir() + SETTINGS_XML_FILE;
	CXMLSerializer ser(fullpath, L"FBE", false);

	ser.Serialize(this);
}

void CSettings::Load()
{
	CString fullpath = U::GetSettingsDir() + SETTINGS_XML_FILE;
	CXMLSerializer ser(fullpath, L"FBE", true);


	SetDefaults();
	if(!ser.Deserialize(this, this))
		Save();
}

CHotkeysGroup* CSettings::GetGroupByName(const CString& name)
{
	for(unsigned int i = 0; i < m_hotkey_groups.size(); ++i)
	{
		if(m_hotkey_groups[i].m_reg_name == name)
			return &m_hotkey_groups[i];
	}

	return NULL;
}

CHotkey* CSettings::GetHotkeyByName(const CString& name, CHotkeysGroup& group)
{
	for(unsigned int i = 0; i < group.m_hotkeys.size(); ++i)
	{
		if(group.m_hotkeys[i].m_reg_name == name)
			return &group.m_hotkeys[i];
	}

	return NULL;
}

void CSettings::SaveHotkeyGroups()
{
	CXMLSerializer ser(U::GetSettingsDir() + HOTKEYS_XML_FILE, L"FBE", false);

	std::vector<void*> hkGroupsPtr;
	for(unsigned int i = 0; i < m_hotkey_groups.size(); ++i)
	{
		hkGroupsPtr.push_back(&m_hotkey_groups[i]);
	}

	ser.Serialize(hkGroupsPtr);
}

void CSettings::LoadHotkeyGroups()
{
	CXMLSerializer ser(U::GetSettingsDir() + HOTKEYS_XML_FILE, L"FBE", true);

	CHotkeysGroup group;
	std::vector<void*> objects;

	ser.Deserialize(&group, objects);

	for(unsigned int i = 0; i < objects.size(); ++i)
	{
		group = *(CHotkeysGroup*)objects[i];
		if(CHotkeysGroup* foundGr = GetGroupByName(group.m_reg_name))
		{
			for(unsigned int j = 0; j < group.m_hotkeys.size(); ++j)
			{
				if(CHotkey* foundHk = GetHotkeyByName(group.m_hotkeys[j].m_reg_name, *foundGr))
				{
					foundHk->m_accel.fVirt = group.m_hotkeys[j].m_accel.fVirt;
					foundHk->m_accel.key = group.m_hotkeys[j].m_accel.key;
				}
			}
		}
	}

	for(unsigned int i = 0; i < m_hotkey_groups.size(); ++i)
	{
		for(unsigned int j = 0; j < m_hotkey_groups[i].m_hotkeys.size(); ++j)
		{
			ACCEL accel = m_hotkey_groups[i].m_hotkeys[j].m_accel;

			if(accel.fVirt != NULL && accel.key != NULL && accel.cmd != NULL)
				keycodes++;

			if(m_hotkey_groups.at(i).m_reg_name == L"Scripts" || m_hotkey_groups.at(i).m_reg_name == L"Plugins")
			{
				ACCEL def_accel = m_hotkey_groups.at(i).m_hotkeys.at(j).m_def_accel;
				if(accel.fVirt != def_accel.fVirt || accel.key != def_accel.key)
				{
					m_hotkey_groups[i].m_hotkeys[j].m_def_accel.fVirt = m_hotkey_groups[i].m_hotkeys[j].m_accel.fVirt;
					m_hotkey_groups[i].m_hotkeys[j].m_def_accel.key = m_hotkey_groups[i].m_hotkeys[j].m_accel.key;
					m_hotkey_groups[i].m_hotkeys[j].m_accel.cmd = m_hotkey_groups[i].m_hotkeys[j].m_def_accel.cmd;
				}
			}
		}
	}
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
DWORD CSettings::GetSplitterPos()const
{
	return m_splitter_pos;
}
CString CSettings::GetToolbarsSettings()const
{
	return m_toolbars_settings;
}
CString CSettings::GetScriptCommandIds()const
{
	return m_script_command_ids;
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
	return NormalizeInterfaceLanguageID(m_interface_lang_id);
}

DWORD CSettings::GetEffectiveInterfaceLanguageID()const
{
	const DWORD langId = GetInterfaceLanguageID();
	if(langId != FBE_INTERFACE_LANGUAGE_AUTO)
		return langId;

	wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = {};
	if(::GetUserDefaultLocaleName(localeName, _countof(localeName)) > 0)
		return InterfaceLanguageFromLocaleName(localeName);

	return NormalizeInterfaceLanguageID(PRIMARYLANGID(GetUserDefaultLangID()));
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
	return m_scripts_folder;
}

CString CSettings::GetDefaultScriptsFolder()
{
	return U::GetProgDir() + DEFAULT_SCRIPTS_FOLDER + L"\\";
}

bool CSettings::IsDefaultScriptsFolder()
{
	return GetScriptsFolder().CompareNoCase(GetDefaultScriptsFolder()) == 0;
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
	lang_id = NormalizeInterfaceLanguageID(lang_id);
	if(m_interface_lang_id != lang_id)
	{
		m_interface_lang_id = lang_id;
		if(apply)
			Save();
	}
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
	if(apply)
	{
		if(m_scripts_folder != fullpath)
		{
			m_scripts_folder = fullpath;
		}
	}
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

void CSettings::SetXMLSrcShowLineNumbers(const bool value, bool apply)
{
	m_show_line_numbers = value;
	if (apply) Save();
}

void CSettings::SetImageType(const DWORD value, bool apply)
{
	m_image_type = value;
	if (apply) Save();
}

void CSettings::SetJpegQuality(const DWORD value, bool apply)
{
	m_jpeg_quality = value;
	if (apply) Save();
}


// Predicate for std::sort
class sortComp { public: bool operator()(void* x, void* y) {
	return (reinterpret_cast<WordsItem*>(x)->m_word.Compare(reinterpret_cast<WordsItem*>(y)->m_word) < 0); }
};
//
void CSettings::LoadWords()
{
	CXMLSerializer ser(U::GetUserDataFile(WORDS_XML_FILE), L"FBE", true);

	WordsItem word;
	std::vector<void*> objects;
	ser.Deserialize(&word, objects);

	// Deserialization creates one temporary WordsItem per XML node.  Keep only
	// the deduplicated values in the persistent model and release every
	// temporary object before returning; large Words.xml files otherwise retain
	// tens of thousands of unnecessary allocations.
	m_words.clear();
	std::sort (objects.begin(), objects.end(), sortComp());
	m_words.reserve(objects.size());
	CString previousWord;
	bool havePreviousWord = false;
	for(std::vector<void*>::iterator item = objects.begin(); item != objects.end(); ++item)
	{
		WordsItem* loadedWord = reinterpret_cast<WordsItem*>(*item);
		if(!havePreviousWord || previousWord.Compare(loadedWord->m_word) != 0)
		{
			m_words.push_back(*loadedWord);
			previousWord = loadedWord->m_word;
			havePreviousWord = true;
		}
		word.Destroy(loadedWord);
	}
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

void CSettings::SaveWords()
{
	// changed by SeNS: extremely slow serialization replaced by fast and simple code
	MSXML2::IXMLDOMDocument2Ptr pXMLDoc;
	HRESULT hr = pXMLDoc.CreateInstance(__uuidof(DOMDocument));
	if (!FAILED(hr))
	{
		CString xml(L"<FBE>\n\t<Words>\n");
		// store all words
		for (unsigned int i=0; i<m_words.size(); i++)
		{
			xml += L"\t\t<Word>\n\t\t\t<Value>" + m_words[i].m_word + L"</Value>\n";
			CString count;
			count.Format(L"%d",m_words[i].m_count);
			xml += L"\t\t\t<Counted>" + count + L"</Counted>\n\t\t</Word>";
		}
		xml += L"\t</Words>\n</FBE>";
		pXMLDoc->loadXML(xml.AllocSysString());

		MSXML2::IXMLDOMElementPtr pXMLRootElem = pXMLDoc->GetdocumentElement();
		MSXML2::IXMLDOMProcessingInstructionPtr pXMLProcessingNode = pXMLDoc->createProcessingInstruction(L"xml", L" version='1.0' encoding='UTF-8'");

		_variant_t vtObject;
		vtObject.vt = VT_DISPATCH;
		vtObject.pdispVal = pXMLRootElem;
		vtObject.pdispVal->AddRef();
		pXMLDoc->insertBefore(pXMLProcessingNode,vtObject);

		CString fileName(U::GetSettingsDir()+WORDS_XML_FILE);
		CString temporaryFile(fileName + L".tmp");
		::DeleteFileW(temporaryFile);
		if (pXMLDoc->save(temporaryFile.AllocSysString()) == S_OK)
		{
			if (!::MoveFileExW(temporaryFile, fileName, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				::DeleteFileW(temporaryFile);
		}
	}
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
	m_xml_src_showEOL		= false;
	m_xml_src_showSpace		= false;
	m_xml_src_showSpecialChars = false;
	m_xml_src_specialCharsStyle = XML_SRC_SPECIAL_CHARS_WORD_LIKE;
	m_fast_mode				= false;
	m_font					= DEFAULT_FONT;
	m_srcfont				= DEFAULT_SRCFONT;
	m_view_status_bar		= true;
	m_view_doc_tree			= true;
	m_splitter_pos			= 200;
	m_toolbars_settings.Empty();
	m_script_command_ids.Empty();
	m_restore_file_position	= false;
	m_interface_lang_id		= FBE_INTERFACE_LANGUAGE_AUTO;
	m_genre_catalog			= GenreCatalog::Standard;
	m_scripts_folder		= GetDefaultScriptsFolder();
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
	m_show_line_numbers		= false;
	m_image_type			= 1;
	m_jpeg_quality			= 75;
	m_image_import_format		= 0;
	m_image_import_jpeg_quality	= 90;
	m_image_import_keep_supported	= true;

	::ZeroMemory(&m_wnd_placement, sizeof(WINDOWPLACEMENT));
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
