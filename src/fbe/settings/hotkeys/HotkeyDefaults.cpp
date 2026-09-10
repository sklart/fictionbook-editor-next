#include "stdafx.h"
#include "HotkeyDefaults.h"
#include "..\..\resource.h"
#include "..\..\res1.h"
#include "..\..\utils\utils.h"

#include <algorithm>

namespace FbeSettings { namespace Hotkeys {
void BuildDefaults(std::vector<CHotkeysGroup>& groups, DWORD interfaceLanguageId, const CString& nbspChar)
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

	CHotkey ToolsSpellAddToDict(L"Add to dictionary", IDS_HOTKEY_TOOLS_ADD_TO_DICTIONARY, NULL, IDC_SPELL_ADD2DICT, NULL);
	tools_hotkeys_group.m_hotkeys.push_back(ToolsSpellAddToDict);

	CHotkey ToolsSpellIgnore(L"Ignore", IDS_HOTKEY_TOOLS_IGNORE_ALL, NULL, IDC_SPELL_IGNOREALL, NULL);
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
		static_assert(ID_EDIT_INS_SYMBOL + 99 <= 0xffff, "Symbol command IDs must fit in WM_COMMAND");
		CString pattern, langPatt;
		pattern.Format(L"%s%d=", L"sym", i);
		langPatt.Format(L"%s%d_%d=", L"sym", i, static_cast<int>(interfaceLanguageId));
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
			
			if (val==160) desc += nbspChar + CString(L")"); else desc += (wchar_t)val + CString(L")");

			// special fix for nbsp
			if (val==160) val = nbspChar[0];

			CHotkey Symbol(mapSymbs.Lookup(pattern).GetBuffer(),
				desc,
				wchar_t(val),
				NULL,
				static_cast<WORD>(ID_EDIT_INS_SYMBOL + i),
				NULL);
			symbols_hotkeys_group.m_hotkeys.push_back(Symbol);
		}
	}

	// Collect all hotkey groups and sort
	groups.push_back(file_hotkeys_group);
	groups.push_back(edit_hotkeys_group);
	groups.push_back(navigation_hotkeys_group);
	groups.push_back(style_hotkeys_group);
	groups.push_back(tools_hotkeys_group);
	groups.push_back(view_hotkeys_group);
	groups.push_back(plugins_hotkeys_group);
	groups.push_back(scripts_hotkeys_group);
	groups.push_back(symbols_hotkeys_group);

	std::sort(groups.begin(), groups.end());
}

} }
