// MainFrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "structure/BodyStructuralEditor.h"
#include "structure/StructuralTrace.h"
#include "document\DocumentLifecycleController.h"
#include "document\DocumentLoader.h"
#include "document\DocumentOpenSource.h"
#include "document\DocumentSavePlan.h"
#include "document\DocumentSaveController.h"
#include "document\ui\DocumentFileDialogs.h"
#include "archive\ui\ArchiveOpenCoordinator.h"

#include "MainFrm.h"
#include "AboutBox.h"
#include "..\\common\\ModernFileDialog.h"
#include "settings\\ui\\SettingsDlg.h"
#include "Settings.h"
#include "ThemeManager.h"
#include "settings\\EditorBackgrounds.h"
#include "utils.h"
#include "KeyboardLayoutSelection.h"
#include "RuntimeLocalization.h"
#include "ImageImport.h"
#include "FictionBookFileType.h"
#include "document\\ArchiveRecentDocuments.h"
#include "document\\recent\\RecentDocumentsStore.h"
#include "document\\recent\\RecentDocumentsManager.h"
#include "document\\FileFingerprint.h"
#include "archive\\ArchiveReader.h"
#include "archive\\ArchiveDocumentResolver.h"
#include "archive\\ArchiveDocumentWriter.h"
#include "recovery\\RecoveryService.h"
#include "xmlMatchedTagsHighlighter.h"
#include "StartupTrace.h"
#include "plugins\\PluginManager.h"
#include "plugins\\PluginApiV2.h"
#include "UiMetrics.h"
#include "ScriptsToolbarCustomizeDlg.h"
#include "ScriptToolbarManagerDlg.h"
#include "scripts\\ScriptCatalog.h"
#include "scripts\\ScriptCommandRegistry.h"
#include "source\\BodySourceSelectionTransfer.h"
#include "source\\SourceDocumentTransfer.h"
#include "source\\SourceViewDiagnostics.h"
#include "view\\ui\\EditorViewPresentationHost.h"
#include "navigation\\LinkDomNavigation.h"
#include "LinkNavigation.h"
#include "XmlDeclaration.h"
#include "..\\common\\DeploymentContext.h"
#include "..\\common\\RuntimeLocalizationCommon.h"
#include "toolbars\\PortableToolbarStore.h"
#include "toolbars\\ToolbarLayoutAdapter.h"
#include "toolbars\\ToolbarFactory.h"
#include "toolbars\\TableToolbarCommands.h"
#include "testing\RuntimeTestScenarioMode.h"
#include <string>
#include <vector>
#include <algorithm>
#include <psapi.h>



static const UINT_PTR RECOVERY_TIMER_ID = 0xFBE;
static const UINT_PTR IMAGE_IMPORT_TEST_TIMER_ID = 0xFBF;
static const UINT RECOVERY_INTERVAL_MS = 2 * 60 * 1000;
static SourceEditorConfig BuildSourceEditorConfig();
typedef FbeArchive::ResolvedDocument ResolvedOpenDocument;


namespace
{
using ToolbarFactory::AutoSizeToolbar;
using ToolbarFactory::ImageListHasMaskPlane;
using ToolbarFactory::SetDialogFontForToolbarRow;
const int SCRIPT_COMMAND_COUNT = 999;
const int SCRIPT_FOLDER_MENU_ID_BASE = ID_EDIT_INS_SYMBOL + 101;
const int SCRIPT_FOLDER_MENU_ID_COUNT = 999;
static_assert(ID_SCRIPT_BASE + SCRIPT_COMMAND_COUNT < SCRIPT_FOLDER_MENU_ID_BASE, "Script and folder menu IDs overlap");
static_assert(ID_LAST_PLUGIN < ID_SPELL_REPLACE_FIRST, "Plug-in and spell suggestion command IDs overlap");
static_assert(ID_PLUGIN_IMPORT_LAST < ID_PLUGIN_EXPORT_FIRST, "Import and export command ranges overlap");
static_assert(ID_SCRIPT_BASE + SCRIPT_COMMAND_COUNT < ID_PLUGIN_IMPORT_FIRST, "Plug-in and script command ranges overlap");
static_assert(ID_PLUGIN_EXPORT_LAST < ID_LAST_SCRIPT, "Plug-in and regular command ranges overlap");
static_assert(ID_SCRIPT_BASE + 999 < ID_SPELL_REPLACE_FIRST, "Script and spell suggestion command IDs overlap");
static_assert(ID_SPELL_REPLACE_LAST < ID_SCI_COLLAPSE_BASE, "Scintilla and spell suggestion command IDs overlap");
static_assert(ID_SPELL_REPLACE_LAST < 0xffff, "Spell suggestion command IDs must fit in WM_COMMAND");
static_assert(ID_FILE_MRU_LAST <= 0xffff, "MRU command IDs must fit in WM_COMMAND");
static_assert(SCRIPT_FOLDER_MENU_ID_BASE > ID_EDIT_INS_SYMBOL + 100, "Folder menu IDs overlap symbol commands");
static_assert(SCRIPT_FOLDER_MENU_ID_BASE + SCRIPT_FOLDER_MENU_ID_COUNT < ID_NEXT_ITEM, "Folder menu IDs overlap regular commands");


static WORD MruCommandId(int offset)
{
	ATLASSERT(offset >= 0 && ID_FILE_MRU_FIRST + offset <= ID_FILE_MRU_LAST);
	return static_cast<WORD>(static_cast<UINT>(ID_FILE_MRU_FIRST) + static_cast<UINT>(offset));
}

static bool AddCommandBarBitmapFromModule(CCommandBarCtrl& commandBar, HINSTANCE module,
	UINT bitmapResourceId, UINT commandId)
{
	HBITMAP bitmap = static_cast<HBITMAP>(::LoadImage(module, MAKEINTRESOURCE(bitmapResourceId),
		IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
	if(bitmap == NULL)
		return false;

	const BOOL added = commandBar.AddBitmap(bitmap, commandId);
	::DeleteObject(bitmap);
	return added != FALSE;
}

static CString StripMenuMnemonics(const CString& text)
{
	CString result;
	for (int index = 0; index < text.GetLength(); ++index)
	{
		if (text[index] != L'&') { result += text[index]; continue; }
		if (index + 1 < text.GetLength() && text[index + 1] == L'&') result += text[++index];
	}
	return result;
}

}

extern CSettings _Settings;

struct RuntimeMenuCommandBinding
{
	UINT commandId;
	LPCWSTR key;
};

static const RuntimeMenuCommandBinding kMainFrameMenuCommandBindings[] = {
	{ ID_FILE_NEW, L"fbe.menu.idr_mainframe.file.new" },
	{ ID_FILE_OPEN, L"fbe.menu.idr_mainframe.file.open" },
	{ ID_FILE_SAVE, L"fbe.menu.idr_mainframe.file.save" },
	{ ID_FILE_SAVE_AS, L"fbe.menu.idr_mainframe.file.save_as" },
	{ ID_FILE_VALIDATE, L"fbe.menu.idr_mainframe.file.validate" },
	{ ID_APP_EXIT, L"fbe.menu.idr_mainframe.file.exit" },
	{ ID_EDIT_UNDO, L"fbe.menu.idr_mainframe.edit.undo" },
	{ ID_EDIT_REDO, L"fbe.menu.idr_mainframe.edit.redo" },
	{ ID_EDIT_CUT, L"fbe.menu.idr_mainframe.edit.cut" },
	{ ID_EDIT_COPY, L"fbe.menu.idr_mainframe.edit.copy" },
	{ ID_EDIT_PASTE, L"fbe.menu.idr_mainframe.edit.paste" },
	{ ID_EDIT_FIND, L"fbe.menu.idr_mainframe.edit.find" },
	{ ID_EDIT_FINDNEXT, L"fbe.menu.idr_mainframe.edit.find_next" },
	{ ID_EDIT_REPLACE, L"fbe.menu.idr_mainframe.edit.replace" },
	{ ID_GOTO_FOOTNOTE, L"fbe.menu.idr_mainframe.edit.goto_footnote" },
	{ ID_GOTO_MATCHTAG, L"fbe.menu.idr_mainframe.edit.goto_matching_tag" },
	{ ID_GOTO_WRONGTAG, L"fbe.menu.idr_mainframe.edit.goto_wrong_tag" },
	{ ID_EDIT_CLONE, L"fbe.menu.idr_mainframe.edit.clone" },
	{ ID_EDIT_SPLIT, L"fbe.menu.idr_mainframe.edit.split" },
	{ ID_EDIT_MERGE, L"fbe.menu.idr_mainframe.edit.merge" },
	{ ID_EDIT_REMOVE_OUTER_SECTION, L"fbe.menu.idr_mainframe.edit.remove_outer_section" },
	{ 60161, L"fbe.menu.idr_mainframe.view.toolbar" },
	{ 60162, L"fbe.menu.idr_mainframe.view.scripts_bar" },
	{ 60163, L"fbe.menu.idr_mainframe.view.links_bar" },
	{ 60164, L"fbe.menu.idr_mainframe.view.tables_bar" },
	{ ID_VIEW_STATUS_BAR, L"fbe.menu.idr_mainframe.view.status_bar" },
	{ ID_VIEW_TREE, L"fbe.menu.idr_mainframe.view.tree" },
	{ ID_VIEW_DESC, L"fbe.menu.idr_mainframe.view.description" },
	{ ID_VIEW_BODY, L"fbe.menu.idr_mainframe.view.body" },
	{ ID_VIEW_SOURCE, L"fbe.menu.idr_mainframe.view.source" },
	{ ID_VIEW_FASTMODE, L"fbe.menu.idr_mainframe.view.fast_mode" },
	{ ID_EDIT_ADD_BODY, L"fbe.menu.idr_mainframe.insert.body" },
	{ ID_EDIT_ADD_TITLE, L"fbe.menu.idr_mainframe.insert.title" },
	{ ID_EDIT_ADD_EPIGRAPH, L"fbe.menu.idr_mainframe.insert.epigraph" },
	{ ID_EDIT_ADD_ANN, L"fbe.menu.idr_mainframe.insert.annotation" },
	{ ID_EDIT_ADD_TA, L"fbe.menu.idr_mainframe.insert.text_author" },
	{ ID_EDIT_INS_IMAGE, L"fbe.menu.idr_mainframe.insert.image" },
	{ ID_EDIT_INS_INLINEIMAGE, L"fbe.menu.idr_mainframe.insert.inline_image" },
	{ ID_EDIT_INS_POEM, L"fbe.menu.idr_mainframe.insert.poem" },
	{ ID_EDIT_INS_CITE, L"fbe.menu.idr_mainframe.insert.cite" },
	{ ID_INSERT_TABLE, L"fbe.menu.idr_mainframe.insert.table" },
	{ ID_EDIT_ADD_IMAGE, L"fbe.menu.idr_mainframe.insert.section_image" },
	{ ID_EDIT_ADDBINARY, L"fbe.menu.idr_mainframe.insert.binary" },
	{ ID_STYLE_NORMAL, L"fbe.menu.idr_mainframe.style.normal" },
	{ ID_STYLE_TEXTAUTHOR, L"fbe.menu.idr_mainframe.style.text_author" },
	{ ID_STYLE_SUBTITLE, L"fbe.menu.idr_mainframe.style.subtitle" },
	{ ID_STYLE_LINK, L"fbe.menu.idr_mainframe.style.link" },
	{ ID_STYLE_NOTE, L"fbe.menu.idr_mainframe.style.note" },
	{ ID_STYLE_NOLINK, L"fbe.menu.idr_mainframe.style.remove_link" },
	{ ID_TOOLS_WORDS, L"fbe.menu.idr_mainframe.tools.words" },
	{ ID_VIEW_OPTIONS, L"fbe.menu.idr_mainframe.tools.options" },
	{ ID_TOOLS_SPELLCHECK, L"fbe.menu.idr_mainframe.tools.spellcheck" },
	{ ID_TOOLS_DIAGNOSTIC_TRACE, L"fbe.menu.idr_mainframe.tools.diagnostic_trace" },
	{ ID_TOOLS_OPEN_DIAGNOSTIC_LOG, L"fbe.menu.idr_mainframe.tools.open_diagnostic_log" },
	{ ID_TOOLS_OPEN_DIAGNOSTIC_FOLDER, L"fbe.menu.idr_mainframe.tools.open_diagnostic_folder" },
	{ ID_TOOLS_COPY_DIAGNOSTIC_LOG_PATH, L"fbe.menu.idr_mainframe.tools.copy_diagnostic_log_path" },
	{ ID_TOOLS_CLEAR_DIAGNOSTIC_LOGS, L"fbe.menu.idr_mainframe.tools.clear_diagnostic_logs" },
	{ ID_TOOLS_CREATE_DIAGNOSTIC_PACKAGE, L"fbe.menu.idr_mainframe.tools.create_diagnostic_package" },
	{ ID_APP_ABOUT, L"fbe.menu.idr_mainframe.help.about" },
};

static CString GetDiagnosticTraceText(LPCWSTR key, LPCWSTR fallback)
{
	return FbeLoadRuntimeStringByKey(key, fallback);
}

static LPCWSTR FindRuntimeMainFrameMenuCommandKey(UINT commandId)
{
	for(size_t i = 0; i < _countof(kMainFrameMenuCommandBindings); ++i)
	{
		if(kMainFrameMenuCommandBindings[i].commandId == commandId)
			return kMainFrameMenuCommandBindings[i].key;
	}
	return NULL;
}

static CString GetRuntimeToolbarToolTipText(UINT commandId)
{
	for (size_t index = 0; index < kTableToolbarCommandCount; ++index)
	{
		const TableToolbarCommand& command = kTableToolbarCommands[index];
		if (command.commandId == commandId)
			return FbeLoadRuntimeStringByKey(command.localizationKey, command.fallbackText);
	}
	LPCWSTR toolbarKey = NULL;
	switch(commandId)
	{
	case ID_EDIT_BOLD: toolbarKey = L"fbe.hotkey.edit.bold"; break;
	case ID_EDIT_ITALIC: toolbarKey = L"fbe.hotkey.edit.italic"; break;
	case ID_EDIT_SUP: toolbarKey = L"fbe.hotkey.edit.superscript"; break;
	case ID_EDIT_SUB: toolbarKey = L"fbe.hotkey.edit.subscript"; break;
	case ID_EDIT_STRIK: toolbarKey = L"fbe.toolbar.strikethrough"; break;
	case ID_EDIT_CODE: toolbarKey = L"fbe.toolbar.code"; break;
	}

	wchar_t resourceText[MAX_LOAD_STRING + 1] = {};
	if (!FbeLoadString(_Module.GetResourceInstance(), commandId, resourceText, MAX_LOAD_STRING))
		return CString();

	const wchar_t* fallback = wcschr(resourceText, L'\n');
	fallback = (fallback != NULL) ? fallback + 1 : resourceText;
	const LPCWSTR key = toolbarKey != NULL ? toolbarKey : FindRuntimeMainFrameMenuCommandKey(commandId);
	const CString localized = key != NULL ? FbeLoadRuntimeStringByKey(key, fallback) : CString(fallback);
	return StripMenuMnemonics(localized);
}

static LPCWSTR GetCommandTraceSource(LPARAM lParam)
{
	if(lParam == 0)
		return L"menu/hotkey/internal";

	wchar_t className[64] = {};
	if(::GetClassName((HWND)lParam, className, _countof(className)) > 0 &&
		_wcsicmp(className, TOOLBARCLASSNAME) == 0)
		return L"toolbar";

	return L"control";
}

static void TraceMainFrameCommand(WPARAM wParam, LPARAM lParam)
{
	const UINT commandId = LOWORD(wParam);
	const WORD notificationCode = HIWORD(wParam);
	if(notificationCode != 0 && notificationCode != 1)
		return;

	const LPCWSTR key = FindRuntimeMainFrameMenuCommandKey(commandId);
	if(key == NULL)
		return;
	CString trace;
	trace.Format(L"ui-command-id=%s; command-id=%u; source=%s", key,
		commandId, GetCommandTraceSource(lParam));
	StartupTrace::Event(L"command", L"C100", trace);
}

static bool IsAcceleratorModifierPressed(int virtualKey)
{
	return (::GetKeyState(virtualKey) & 0x8000) != 0;
}

static bool MatchesHotkeyMessage(const ACCEL& accelerator, const MSG* message)
{
	if((accelerator.fVirt & FVIRTKEY) == 0 || accelerator.key != message->wParam)
		return false;

	const bool controlPressed = IsAcceleratorModifierPressed(VK_CONTROL);
	const bool shiftPressed = IsAcceleratorModifierPressed(VK_SHIFT);
	const bool altPressed = IsAcceleratorModifierPressed(VK_MENU);
	return ((accelerator.fVirt & FCONTROL) != 0) == controlPressed &&
		((accelerator.fVirt & FSHIFT) != 0) == shiftPressed &&
		((accelerator.fVirt & FALT) != 0) == altPressed;
}

static CString GetHotkeyText(const ACCEL& accelerator)
{
	CString text;
	if((accelerator.fVirt & FCONTROL) != 0)
		text += L"Ctrl+";
	if((accelerator.fVirt & FALT) != 0)
		text += L"Alt+";
	if((accelerator.fVirt & FSHIFT) != 0)
		text += L"Shift+";

	wchar_t keyName[64] = {};
	const UINT scanCode = ::MapVirtualKey(accelerator.key, MAPVK_VK_TO_VSC);
	if(scanCode != 0 && ::GetKeyNameText(static_cast<LONG>(scanCode << 16), keyName, _countof(keyName)) > 0)
		text += keyName;
	else
	{
		CString fallback;
		fallback.Format(L"VK_%u", accelerator.key);
		text += fallback;
	}
	return text;
}

static void TraceMainFrameHotkey(const MSG* message)
{
	if(message->message != WM_KEYDOWN && message->message != WM_SYSKEYDOWN)
		return;
	if((message->lParam & 0x40000000) != 0)
		return;

	for(size_t groupIndex = 0; groupIndex < _Settings.m_hotkey_groups.size(); ++groupIndex)
	{
		const CHotkeysGroup& group = _Settings.m_hotkey_groups[groupIndex];
		for(size_t hotkeyIndex = 0; hotkeyIndex < group.m_hotkeys.size(); ++hotkeyIndex)
		{
			const CHotkey& hotkey = group.m_hotkeys[hotkeyIndex];
			if(!MatchesHotkeyMessage(hotkey.m_accel, message))
				continue;
			CString trace;
			trace.Format(L"command-id=%u; virtual-key=%u", hotkey.m_accel.cmd, hotkey.m_accel.key);
			StartupTrace::Event(L"command", L"C110", trace);
			return;
		}
	}
}

// Обновляет уже существующие пункты встроенных плагинов. При смене языка не
// нужно заново создавать плагины, скрипты, значки меню и кнопки toolbar: такой
// путь накапливал GDI-ресурсы и добавлял повторные элементы интерфейса.
static void RefreshBundledPluginMenuTexts(const PluginManager& manager, HMENU menu, const TCHAR* type, UINT commandBase)
{
	if(menu == NULL)
		return;
	int commandOffset = 0;
	const int commandCapacity = static_cast<int>((commandBase == ID_IMPORT_BASE ? ID_PLUGIN_IMPORT_LAST : ID_PLUGIN_EXPORT_LAST) - commandBase + 1);
	const std::vector<PluginDescriptor>& plugins = manager.GetPlugins();
	for(size_t index = 0; index < plugins.size() && commandOffset < commandCapacity; ++index)
	{
		const PluginDescriptor& plugin = plugins[index];
		if(plugin.type != type) continue;
		const CString text = FbeLoadRuntimeStringByKey(plugin.menuKey, plugin.menu);
		MENUITEMINFO itemInfo = {};
		itemInfo.cbSize = sizeof(itemInfo);
		itemInfo.fMask = MIIM_STRING;
		itemInfo.dwTypeData = const_cast<LPTSTR>(static_cast<LPCTSTR>(text));
		itemInfo.cch = text.GetLength();
		::SetMenuItemInfo(menu, commandBase + commandOffset, FALSE, &itemInfo);
		++commandOffset;
	}
}

static void SetRuntimeMenuItemTextByPosition(HMENU menu, UINT position, LPCWSTR key)
{
	if(menu == NULL || key == NULL)
		return;

	HMENU subMenu = ::GetSubMenu(menu, position);
	if(subMenu == NULL)
		return;

	CString text = FbeLoadRuntimeStringByKey(key);
	if(text.IsEmpty())
		return;

	::ModifyMenu(menu, position, MF_BYPOSITION | MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>(subMenu), text);
}

static void SetRuntimePlainMenuItemTextByPosition(HMENU menu, UINT position, LPCWSTR key)
{
	if(menu == NULL || key == NULL)
		return;

	MENUITEMINFO itemInfo = {};
	itemInfo.cbSize = sizeof(itemInfo);
	itemInfo.fMask = MIIM_ID | MIIM_STATE | MIIM_FTYPE;
	if(!::GetMenuItemInfo(menu, position, TRUE, &itemInfo))
		return;
	if(itemInfo.hSubMenu != NULL || (itemInfo.fType & MFT_SEPARATOR))
		return;

	CString text = FbeLoadRuntimeStringByKey(key);
	if(text.IsEmpty())
		return;

	::ModifyMenu(menu, position, MF_BYPOSITION | MF_STRING | (itemInfo.fState & (MFS_DISABLED | MFS_GRAYED)), itemInfo.wID, text);
}

// MRU command IDs are reused by real documents.  Its empty-state is therefore
// owned by CRecentDocumentList, never by the general command-id localizer.
static void RefreshMruEmptyStateText(CRecentDocumentList& mru)
{
	const CString text = FbeLoadRuntimeStringByKey(
		L"fbe.menu.idr_mainframe.recent.empty", L"No Recent Files");
	ATL::Checked::tcsncpy_s(mru.m_szNoEntries, _countof(mru.m_szNoEntries), text, _TRUNCATE);
}

static void ApplyRuntimeMenuCommandTexts(HMENU menu)
{
	if(menu == NULL)
		return;

	const int count = ::GetMenuItemCount(menu);
	for(int i = 0; i < count; ++i)
	{
		HMENU subMenu = ::GetSubMenu(menu, i);
		if(subMenu != NULL)
			ApplyRuntimeMenuCommandTexts(subMenu);

		const UINT commandId = ::GetMenuItemID(menu, i);
		if(commandId == static_cast<UINT>(-1) || commandId == 0 || commandId == IDCANCEL)
			continue;
		// These are dynamic command slots.  A non-empty MRU item must retain its
		// document caption and ID_FILE_MRU_FIRST must remain executable.
		if(commandId >= ID_FILE_MRU_FIRST && commandId <= ID_FILE_MRU_LAST)
			continue;

		LPCWSTR key = FindRuntimeMainFrameMenuCommandKey(commandId);
		if(key == NULL)
			continue;

		CString text = FbeLoadRuntimeStringByKey(key);
		if(!text.IsEmpty())
			::ModifyMenu(menu, i, MF_BYPOSITION | MF_STRING, commandId, text);
	}
}

static int FindMenuPositionByCommand(HMENU menu, UINT commandId)
{
	if(menu == NULL)
		return -1;
	const int count = ::GetMenuItemCount(menu);
	for(int position = 0; position < count; ++position)
		if(::GetMenuItemID(menu, position) == commandId)
			return position;
	return -1;
}

static int FindTopLevelMenuPositionByCommand(HMENU menu, UINT commandId)
{
	if(menu == NULL)
		return -1;
	const int count = ::GetMenuItemCount(menu);
	for(int position = 0; position < count; ++position)
		if(FindMenuPositionByCommand(::GetSubMenu(menu, position), commandId) >= 0)
			return position;
	return -1;
}

static bool MenuContainsScriptCommand(HMENU menu)
{
	if(menu == NULL)
		return false;
	const int count = ::GetMenuItemCount(menu);
	for(int position = 0; position < count; ++position)
	{
		const UINT commandId = ::GetMenuItemID(menu, position);
		if(commandId >= ID_SCRIPT_BASE && commandId < ID_SCRIPT_BASE + SCRIPT_COMMAND_COUNT)
			return true;
		if(MenuContainsScriptCommand(::GetSubMenu(menu, position)))
			return true;
	}
	return false;
}

static int FindTopLevelScriptsMenuPosition(HMENU menu)
{
	if(menu == NULL)
		return -1;
	const int count = ::GetMenuItemCount(menu);
	for(int position = 0; position < count; ++position)
	{
		HMENU subMenu = ::GetSubMenu(menu, position);
		if(subMenu == NULL)
			continue;
		if(::GetMenuItemID(subMenu, 0) == IDCANCEL)
			return position;
		if(MenuContainsScriptCommand(subMenu))
			return position;
	}
	return -1;
}
static void ApplyRuntimeMainFrameMenuLocalization(HMENU menu)
{
	if(menu == NULL)
		return;

	SetRuntimeMenuItemTextByPosition(menu, 0, L"fbe.menu.idr_mainframe.popup.file");
	SetRuntimeMenuItemTextByPosition(menu, 1, L"fbe.menu.idr_mainframe.popup.edit");
	SetRuntimeMenuItemTextByPosition(menu, 2, L"fbe.menu.idr_mainframe.popup.view");
	SetRuntimeMenuItemTextByPosition(menu, 3, L"fbe.menu.idr_mainframe.popup.insert");
	SetRuntimeMenuItemTextByPosition(menu, 4, L"fbe.menu.idr_mainframe.popup.style");
	SetRuntimeMenuItemTextByPosition(menu, 5, L"fbe.menu.idr_mainframe.popup.tools");
	const int diagnosticTopPosition = FindTopLevelMenuPositionByCommand(menu, ID_TOOLS_DIAGNOSTIC_TRACE);
	if(diagnosticTopPosition >= 0)
		SetRuntimeMenuItemTextByPosition(menu, diagnosticTopPosition, L"fbe.menu.idr_mainframe.popup.diagnostics");
	const int scriptsPosition = FindTopLevelScriptsMenuPosition(menu);
	if(scriptsPosition >= 0)
		SetRuntimeMenuItemTextByPosition(menu, scriptsPosition, L"fbe.menu.idr_mainframe.popup.scripts");
	const int helpPosition = FindTopLevelMenuPositionByCommand(menu, ID_APP_ABOUT);
	if(helpPosition >= 0)
	{
		SetRuntimeMenuItemTextByPosition(menu, helpPosition, L"fbe.menu.idr_mainframe.popup.help");
		HMENU helpMenu = ::GetSubMenu(menu, helpPosition);
		const int diagnosticsPosition = FindTopLevelMenuPositionByCommand(helpMenu, ID_TOOLS_DIAGNOSTIC_TRACE);
		if(diagnosticsPosition >= 0)
			SetRuntimeMenuItemTextByPosition(helpMenu, diagnosticsPosition, L"fbe.menu.idr_mainframe.popup.diagnostics");
	}

	HMENU fileMenu = ::GetSubMenu(menu, 0);
	if(fileMenu != NULL)
	{
		SetRuntimeMenuItemTextByPosition(fileMenu, 6, L"fbe.menu.idr_mainframe.popup.import");
		SetRuntimeMenuItemTextByPosition(fileMenu, 7, L"fbe.menu.idr_mainframe.popup.export");
		SetRuntimeMenuItemTextByPosition(fileMenu, 9, L"fbe.menu.idr_mainframe.popup.recent_documents");

		HMENU importMenu = ::GetSubMenu(fileMenu, 6);
		HMENU exportMenu = ::GetSubMenu(fileMenu, 7);
		if(importMenu != NULL && ::GetMenuItemID(importMenu, 0) == IDCANCEL)
			SetRuntimePlainMenuItemTextByPosition(importMenu, 0, L"fbe.menu.idr_mainframe.plugins.none.import");
		if(exportMenu != NULL && ::GetMenuItemID(exportMenu, 0) == IDCANCEL)
			SetRuntimePlainMenuItemTextByPosition(exportMenu, 0, L"fbe.menu.idr_mainframe.plugins.none.export");
	}

	HMENU scriptsMenu = scriptsPosition >= 0 ? ::GetSubMenu(menu, scriptsPosition) : NULL;
	if(scriptsMenu != NULL && ::GetMenuItemID(scriptsMenu, 0) == IDCANCEL)
		SetRuntimePlainMenuItemTextByPosition(scriptsMenu, 0, L"fbe.menu.idr_mainframe.scripts.empty");

	ApplyRuntimeMenuCommandTexts(menu);
}

// Снимок параметров, которые действительно требуют перенастройки редактора.
// Смена только языка не должна повторно инициализировать MSHTML, Scintilla и
// проверку орфографии: это заметно задерживает интерфейс и не влияет на их работу.
struct EditorConfigurationSnapshot
{
	CString font;
	CString sourceFont;
	CString customDictionary;
	CString nbsp;
	DWORD fontSize;
	DWORD foreground;
	DWORD background;
	CString editorBackgroundKind;
	CString editorBackgroundId;
	CString editorBackgroundCustomPath;
	CString editorBackgroundLayout;
	DWORD sourceColorPalette;
	CString sourceThemeId;
	DWORD sourceColors[XML_SRC_COLOR_GROUP_COUNT];
	DWORD customDictionaryCodepage;
	bool sourceWrap;
	bool sourceSyntaxHighlight;
	bool sourceTagHighlight;
	bool sourceShowEol;
	bool sourceShowWhitespace;
	bool sourceShowSpecialChars;
	DWORD sourceSpecialCharsStyle;
	bool sourceShowLineNumbers;
	bool fastMode;
	bool useSpellChecker;
	bool highlightMisspells;

	bool operator==(const EditorConfigurationSnapshot& other) const
	{
		return font == other.font && sourceFont == other.sourceFont &&
			customDictionary == other.customDictionary && nbsp == other.nbsp &&
			fontSize == other.fontSize && foreground == other.foreground &&
			background == other.background && editorBackgroundKind == other.editorBackgroundKind &&
			editorBackgroundId == other.editorBackgroundId && editorBackgroundCustomPath == other.editorBackgroundCustomPath &&
			editorBackgroundLayout == other.editorBackgroundLayout && sourceColorPalette == other.sourceColorPalette &&
			sourceThemeId == other.sourceThemeId &&
			memcmp(sourceColors, other.sourceColors, sizeof(sourceColors)) == 0 &&
			customDictionaryCodepage == other.customDictionaryCodepage &&
			sourceWrap == other.sourceWrap && sourceSyntaxHighlight == other.sourceSyntaxHighlight &&
			sourceTagHighlight == other.sourceTagHighlight && sourceShowEol == other.sourceShowEol &&
			sourceShowWhitespace == other.sourceShowWhitespace && sourceShowSpecialChars == other.sourceShowSpecialChars &&
			sourceSpecialCharsStyle == other.sourceSpecialCharsStyle &&
			sourceShowLineNumbers == other.sourceShowLineNumbers &&
			fastMode == other.fastMode && useSpellChecker == other.useSpellChecker &&
			highlightMisspells == other.highlightMisspells;
	}
};

static EditorConfigurationSnapshot CaptureEditorConfigurationSnapshot()
{
	EditorConfigurationSnapshot snapshot = {};
	snapshot.font = _Settings.GetFont();
	snapshot.sourceFont = _Settings.GetSrcFont();
	snapshot.customDictionary = _Settings.GetCustomDict();
	snapshot.nbsp = _Settings.GetNBSPChar();
	snapshot.fontSize = _Settings.GetFontSize();
	snapshot.foreground = _Settings.GetColorFG();
	snapshot.background = _Settings.GetColorBG();
	snapshot.editorBackgroundKind = _Settings.GetEditorBackgroundKind();
	snapshot.editorBackgroundId = _Settings.GetEditorBackgroundId();
	snapshot.editorBackgroundCustomPath = _Settings.GetEditorBackgroundCustomPath();
	snapshot.editorBackgroundLayout = _Settings.GetEditorBackgroundLayout();
	snapshot.sourceColorPalette = _Settings.GetXmlSrcColorPalette();
	snapshot.sourceThemeId = _Settings.GetXmlSrcThemeId();
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
		snapshot.sourceColors[i] = _Settings.GetXmlSrcColor(static_cast<XmlSrcColorGroup>(i));
	snapshot.customDictionaryCodepage = _Settings.GetCustomDictCodepage();
	snapshot.sourceWrap = _Settings.XmlSrcWrap();
	snapshot.sourceSyntaxHighlight = _Settings.XmlSrcSyntaxHL();
	snapshot.sourceTagHighlight = _Settings.XmlSrcTagHL();
	snapshot.sourceShowEol = _Settings.XmlSrcShowEOL();
	snapshot.sourceShowWhitespace = _Settings.XmlSrcShowSpace();
	snapshot.sourceShowSpecialChars = _Settings.XmlSrcShowSpecialChars();
	snapshot.sourceSpecialCharsStyle = _Settings.XmlSrcSpecialCharsStyle();
	snapshot.sourceShowLineNumbers = _Settings.XMLSrcShowLineNumbers();
	snapshot.fastMode = _Settings.FastMode();
	snapshot.useSpellChecker = _Settings.GetUseSpellChecker();
	snapshot.highlightMisspells = _Settings.GetHighlightMisspells();
	return snapshot;
}

static bool HasDocumentStyleConfigurationChanged(const EditorConfigurationSnapshot& before,
	const EditorConfigurationSnapshot& after)
{
	return before.font != after.font || before.fontSize != after.fontSize ||
		before.foreground != after.foreground || before.background != after.background ||
		before.editorBackgroundKind != after.editorBackgroundKind || before.editorBackgroundId != after.editorBackgroundId ||
		before.editorBackgroundCustomPath != after.editorBackgroundCustomPath || before.editorBackgroundLayout != after.editorBackgroundLayout ||
		before.fastMode != after.fastMode;
}
static bool HasOnlyEditorBackgroundConfigurationChanged(const EditorConfigurationSnapshot& before,
	const EditorConfigurationSnapshot& after)
{
	const bool backgroundChanged = before.editorBackgroundKind != after.editorBackgroundKind ||
		before.editorBackgroundId != after.editorBackgroundId ||
		before.editorBackgroundCustomPath != after.editorBackgroundCustomPath ||
		before.editorBackgroundLayout != after.editorBackgroundLayout;
	if(!backgroundChanged) return false;
	EditorConfigurationSnapshot withoutBackgroundChanges = after;
	withoutBackgroundChanges.editorBackgroundKind = before.editorBackgroundKind;
	withoutBackgroundChanges.editorBackgroundId = before.editorBackgroundId;
	withoutBackgroundChanges.editorBackgroundCustomPath = before.editorBackgroundCustomPath;
	withoutBackgroundChanges.editorBackgroundLayout = before.editorBackgroundLayout;
	return before == withoutBackgroundChanges;
}
static bool HasOnlySourceEditorConfigurationChanged(const EditorConfigurationSnapshot& before,
	const EditorConfigurationSnapshot& after)
{
	return before.font == after.font && before.foreground == after.foreground &&
		before.background == after.background && before.fontSize == after.fontSize &&
		before.editorBackgroundKind == after.editorBackgroundKind && before.editorBackgroundId == after.editorBackgroundId &&
		before.editorBackgroundCustomPath == after.editorBackgroundCustomPath && before.editorBackgroundLayout == after.editorBackgroundLayout &&
		before.customDictionary == after.customDictionary && before.nbsp == after.nbsp &&
		before.customDictionaryCodepage == after.customDictionaryCodepage &&
		before.fastMode == after.fastMode && before.useSpellChecker == after.useSpellChecker &&
		before.highlightMisspells == after.highlightMisspells &&
		!(before == after);
}
static UINT GetWindowDpi(HWND window)
{
	typedef UINT (WINAPI* GetDpiForWindowProc)(HWND);
	HMODULE user32 = ::GetModuleHandle(L"user32.dll");
	GetDpiForWindowProc getDpiForWindow = user32
		? reinterpret_cast<GetDpiForWindowProc>(::GetProcAddress(user32, "GetDpiForWindow")) : NULL;
	if (getDpiForWindow)
		return getDpiForWindow(window);

	HDC dc = ::GetDC(window);
	const UINT dpi = dc ? static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSX)) : 96;
	if (dc)
		::ReleaseDC(window, dc);
	return dpi ? dpi : 96;
}

static int EstimateSourceLineCount(const CString& text)
{
	int lines = 1;
	bool previousWasCarriageReturn = false;
	for(int i = 0; i < text.GetLength(); ++i)
	{
		const wchar_t ch = text[i];
		if(ch == L'\r')
		{
			++lines;
			previousWasCarriageReturn = true;
		}
		else if(ch == L'\n')
		{
			if(!previousWasCarriageReturn)
				++lines;
			previousWasCarriageReturn = false;
		}
		else
		{
			previousWasCarriageReturn = false;
		}
	}
	return lines;
}

static bool IsHighContrastEnabled()
{
	HIGHCONTRAST highContrast = {};
	highContrast.cbSize = sizeof(highContrast);
	return ::SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(highContrast),
		&highContrast, 0) && (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

// MessageBox localization
HHOOK hCBTHook;
HWND activatedWnd = 0;
LRESULT CALLBACK CBTProc(INT nCode, WPARAM wParam, LPARAM lParam)
{
	HWND  hChildWnd;    // msgbox is "child"
	CString s;
	// notification that a window is about to be activated
	// window handle is wParam
	if (nCode == HCBT_ACTIVATE)
	{
		// set window handles
		hChildWnd  = (HWND)wParam;
		if (activatedWnd != (HWND)wParam && ::GetProp(hChildWnd, L"FBE_SKIP_SYSTEM_DIALOG_LOCALIZATION") == NULL)
		{
			activatedWnd = hChildWnd;

			if(GetDlgItem(hChildWnd,IDOK)!=NULL)
			{
				s = FbeLoadCString(IDS_MB_OK);
				SetDlgItemText(hChildWnd,IDOK,s);
			}
			if(GetDlgItem(hChildWnd,IDCANCEL)!=NULL)
			{
				s = FbeLoadCString(IDS_MB_CANCEL);
				SetDlgItemText(hChildWnd,IDCANCEL,s);
			}
			if(GetDlgItem(hChildWnd,IDABORT)!=NULL)
			{
				s = FbeLoadCString(IDS_MB_ABORT);
				SetDlgItemText(hChildWnd,IDABORT,s);
			}
			if(GetDlgItem(hChildWnd,IDRETRY)!=NULL)
			{
				s = FbeLoadCString(IDS_MB_RETRY);
				SetDlgItemText(hChildWnd,IDRETRY,s);
			}
			if(GetDlgItem(hChildWnd,IDIGNORE)!=NULL)
			{
				s = FbeLoadCString(IDS_MB_IGNORE);
				SetDlgItemText(hChildWnd,IDIGNORE,s);
			}
			if(GetDlgItem(hChildWnd,IDYES)!=NULL)
			{
				s = FbeLoadCString(IDS_MB_YES);
				SetDlgItemText(hChildWnd,IDYES,s);
			}
			if(GetDlgItem(hChildWnd,IDNO)!=NULL)
			{
				s = FbeLoadCString(IDS_MB_NO);
				SetDlgItemText(hChildWnd,IDNO,s);
			}
		}
	}
	if (nCode == HCBT_DESTROYWND)
	{
		if (activatedWnd == (HWND)wParam)
			activatedWnd = 0;
	}
	// otherwise, continue with any possible chained hooks
	return CallNextHookEx(hCBTHook, nCode, wParam, lParam);
}
void HookSysDialogs()
{
	hCBTHook = SetWindowsHookEx(WH_CBT, &CBTProc, 0, GetCurrentThreadId());
}

void UnhookSysDialogs()
{
	UnhookWindowsHookEx(hCBTHook);
}
// utility methods
bool  CMainFrame::IsBandVisible(int id) {
  int nBandIndex = m_rebar.IdToIndex(id);
  REBARBANDINFO	rbi;
  rbi.cbSize=sizeof(rbi);
  rbi.fMask=RBBIM_STYLE;
  m_rebar.GetBandInfo(nBandIndex,&rbi);
  return (rbi.fStyle&RBBS_HIDDEN)==0;
}

void CMainFrame::AttachDocument(FB::Doc *doc)
{
	if (!doc || !doc->m_body.HasDoc())
	{
		StartupTrace::Warning(L"mainframe", L"M125", L"document attach deferred: HTML document is not ready");
		return;
	}
	/*if (IsSourceActive()) {
	UIEnable(ID_VIEW_TREE, 1);
	UISetCheck(ID_VIEW_TREE, m_save_sp_mode);
	m_splitter.SetSinglePaneMode(m_save_sp_mode ? SPLIT_PANE_NONE : SPLIT_PANE_RIGHT);
	}*/
	m_view.AttachWnd(doc->m_body);
	UISetCheck(ID_VIEW_BODY, 1);
	UISetCheck(ID_VIEW_DESC, 0);
	UISetCheck(ID_VIEW_SOURCE, 0);
	m_view.ActivateWnd(doc->m_body);
	m_editor_view_state.Reset(EditorView::Body, EditorView::Description);
	m_editor_selection_state.Reset();
	m_cb_updated=false;
	m_need_title_update=m_sel_changed=true;
	if(_Settings.ViewDocumentTree())
	{
		m_document_tree.GetDocumentStructure(doc->m_body.Document());
		m_document_tree.HighlightItemAtPos(doc->m_body.SelectionContainer());
	}
	// added by SeNS
	if (m_Speller && m_Speller->Enabled())
	{
		m_Speller->SetFrame(m_hWnd);

		const CString custDictName = U::GetUserDataFile(_Settings.GetCustomDict(), doc->m_body.m_file_path);

		m_Speller->SetCustomDictionary(custDictName, _Settings.GetCustomDictCodepage());
		m_Speller->AttachDocument(doc->m_body.Document());
	}
    ShowView(DESC);
    ShowView(BODY);
	m_view.ActivateWnd(doc->m_body);
}

CString CMainFrame::GetOpenFileName() { const DocumentFileDialogs::OpenResult result = DocumentFileDialogs::ShowOpen(m_hWnd); return result.accepted ? result.path : CString(); }

CString	CMainFrame::GetSaveFileName(CString& encoding) {
	if (RuntimeTests::IsScenario(L"save-as-cancel-runtime")) return CString();
	// Runtime integration uses an explicitly supplied output only in this
	// narrowly scoped test mode; normal Save As always shows the native dialog.
	if (RuntimeTests::IsScenario(L"archive-rar-save-runtime") || RuntimeTests::IsScenario(L"save-as-failure-runtime"))
	{
		wchar_t testPath[MAX_PATH] = {};
		const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SAVE_PATH", testPath, _countof(testPath));
		if (length && length < _countof(testPath))
		{
			encoding = RuntimeTests::IsScenario(L"save-as-failure-runtime") ? CString(L"windows-1251") : (_Settings.KeepEncoding() ? m_doc->m_encoding : _Settings.GetDefaultEncoding());
			return CString(testPath);
		}
	}
	DocumentFileDialogs::SaveRequest request;
	bstr_t filename = m_doc->m_filename;
	if (!filename || (filename == bstr_t(L"Untitled.fb2")))
		filename = L"";
	request.initialFileName = static_cast<const wchar_t*>(filename);
	request.currentFileName = m_doc->m_filename;
	request.selectedEncoding = _Settings.KeepEncoding() ? m_doc->m_encoding : _Settings.GetDefaultEncoding();
	wchar_t encodingBuffer[1024] = {};
	FbeLoadString(_Module.GetResourceInstance(), IDS_ENCODINGS, encodingBuffer, _countof(encodingBuffer));
	request.encodingList = encodingBuffer;
	const DocumentFileDialogs::SaveResult result = DocumentFileDialogs::ShowSave(m_hWnd, request);
	if (!result.accepted) return CString();
	encoding = result.encoding;
	return result.path;
}

bool	CMainFrame::DocChanged() {
	return m_doc && m_doc->DocChanged() || IsSourceActive() && m_source.SendMessage(SCI_GETMODIFY);
}

bool	CMainFrame::DiscardChanges() {
  U::SaveFileSelectedPos(m_doc->m_filename, m_doc->GetSelectedPos());

  if (DocChanged())
  {
    switch (U::MessageBox(MB_YESNOCANCEL|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SAVE_DLG_MSG, static_cast<LPCWSTR>(m_doc->m_filename)))
    {
    case IDYES:
		{
			bool ret = (SaveFile(false)==OK);
				if(!ret) _Settings.Load();
			return ret;
		}
    case IDNO:
      return true;
    case IDCANCEL:
		{
			_Settings.Load();
			return false;
		}
    }
  }
  return true;
}

void  CMainFrame::SetIsText() {
  RefreshStatusMainPane();
}

void  CMainFrame::StopIncSearch(bool fCancel) {
  if (!m_incsearch)
    return;
  m_incsearch=0;
  m_sel_changed=true; // will cause status line update soon
  if (fCancel)
    m_doc->m_body.CancelIncSearch();
  else
    m_doc->m_body.StopIncSearch();
  RefreshStatusMainPane();
}

CMainFrame::FILE_OP_STATUS CMainFrame::SaveFile(bool askname) {
  ATLASSERT(m_doc!=NULL);

  // force consistent html view
  if ((IsSourceActive() && CommitSourceDocument() != EditorSourceOperationResult::Success) || m_bad_xml) // added by SeNS: do not save bad xml!
    return FAIL;

  const DocumentSavePlan savePlan = DocumentSavePlan::Create(askname, m_doc->m_namevalid, m_document_session.Location());

  DocumentSaveController saveController;
  if (savePlan.target == DocumentSaveTarget::CurrentArchive)
  {
	const DocumentSaveResult result = saveController.SaveCurrent(*m_doc, m_document_session, m_document_session.Location());
	if (!result.Succeeded())
	{
		if (result.failure == DocumentSaveFailureKind::ArchiveWrite)
		{
			if (RuntimeTests::IsScenario(L"archive-recovery-external-verify"))
			{
				wchar_t diagnostic[16] = {};
				swprintf_s(diagnostic, _countof(diagnostic), L"%d", static_cast<int>(result.archiveError.code));
				::SetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SAVE_ERROR", diagnostic);
			}
			FbeArchiveUi::ShowError(m_hWnd, result.archiveError);
		}
		return FAIL;
	}
	CommitSuccessfulSave();
	return OK;
  }

  if (!askname && m_doc->m_namevalid) {
    const DWORD attributes = ::GetFileAttributes(m_doc->m_filename);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY)) {
      if (U::MessageBox(MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON1,
            IDR_MAINFRAME, IDS_READONLY_SAVE_MSG, static_cast<LPCWSTR>(m_doc->m_filename)) == IDYES)
        return SaveFile(true);
      return CANCELLED;
    }
  }

  if (savePlan.target == DocumentSaveTarget::SaveAs) { // ask user about save file name
    CString encoding;
    CString filename(GetSaveFileName(encoding));
    if (filename.IsEmpty())
      return CANCELLED;
    const bool wasFbd = m_doc->GetDocumentFileType() == FictionBookFileType::Fbd;
	DocumentSaveAsRequest request; request.filename = filename; request.encoding = encoding;
    if (saveController.SaveAsNormal(*m_doc, m_document_session, request).Succeeded()) {
	  if (wasFbd != IsFbdFile(filename)) ResetValidationStatus();
	  U::SetCurrentDirectoryToFile(filename);
	  m_recentDocuments.OnSavedAsNormal(filename);
	  CommitSuccessfulSave();
	  UpdateStatusBar();
      return OK;
    }
    return FAIL;
  }
  const DocumentSaveResult currentSave = saveController.SaveCurrent(*m_doc, m_document_session, m_document_session.Location());

  if(currentSave.Succeeded())
  {
	  CommitSuccessfulSave();
	  return OK;
  }
  else
  {
	const HRESULT saveError = m_doc->GetLastSaveError();
	const bool accessDenied = saveError == E_ACCESSDENIED || HRESULT_CODE(saveError) == ERROR_ACCESS_DENIED;
	if (accessDenied && U::MessageBox(MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON1,
		IDR_MAINFRAME, IDS_SAVE_ACCESS_DENIED_MSG, static_cast<LPCWSTR>(m_doc->m_filename)) == IDYES)
		return SaveFile(true);
	return FAIL;
  }
}

void CMainFrame::CommitSuccessfulSave()
{
	m_doc->MarkSavePoint();
	if (IsSourceActive()) m_source.SendMessage(SCI_SETSAVEPOINT);
	m_recovery.DeleteIfWritten();
}

CMainFrame::FILE_OP_STATUS  CMainFrame::LoadFile(const wchar_t *initfilename, const DocumentLocation* preferredArchiveLocation)
{
  CString filename(initfilename);
  if (filename.IsEmpty())
    filename = GetOpenFileName();
  if (filename.IsEmpty())
    return CANCELLED;

  ResolvedOpenDocument resolved;
  const bool archive = DetectDocumentContainerKind(filename) != DocumentContainerKind::None;
  FbeArchive::Error archiveError;
  if (archive && !FbeArchiveUi::ResolveOpenRequest(filename, resolved, preferredArchiveLocation, &archiveError))
  {
    if (archiveError.code != FbeArchive::ErrorCode::None) FbeArchiveUi::ShowError(m_hWnd, archiveError);
    return CANCELLED;
  }

	if (!RuntimeTests::IsScenario(L"archive-mru-runtime") && !DiscardChanges())
	    return CANCELLED;

  EnableWindow(FALSE);
  m_status.SetPaneText(ID_DEFAULT_PANE, FbeLoadRuntimeString(IDS_STATUS_LOADING));
	DocumentOpenSource source = archive ? DocumentOpenSource() : DocumentOpenSource::Normal(filename);
	if (archive) { source.location = resolved.location; source.rawBytes = resolved.rawBytes; }
	DocumentLifecycleController lifecycle(*this, m_doc, m_document_session, m_view);
	const DocumentLifecycleResult lifecycleResult = lifecycle.Open(source);
  EnableWindow(TRUE);
  if (!lifecycleResult.Succeeded())
  {
	  if (LoadToScintilla(filename)) return OK;
	  return FAIL;
  }

  AttachDocument(m_doc);
  m_bad_xml = false;
  ResetStatusForDocument();
  return OK;
}

void  CMainFrame::GetDocumentStructure() {
  m_doc_changed=false;
  m_document_tree.GetDocumentStructure(m_doc->m_body.Document());
}

void  CMainFrame::GoTo(MSHTML::IHTMLElement *e) {
  try {
    m_doc->m_body.GoTo(e);
   // ShowView();
  }
  catch (_com_error&) {
  }
}

// message handlers
BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
	// reset ctrl tab
	if (pMsg->message == WM_KEYUP && pMsg->wParam == VK_CONTROL)
	{
		m_editor_view_state.SetCtrlTabActive(false);
	}
	TraceMainFrameHotkey(pMsg);

	// well, if we are doing an incremental search, then swallow WM_CHARS
	if (m_incsearch && pMsg->hwnd != *this)
	{
		BOOL tmp;
		if(pMsg->message == WM_CHAR)
		{
			OnChar(WM_CHAR, pMsg->wParam, 0, tmp);
			return TRUE;
		}
		if ((pMsg->message == WM_KEYDOWN || pMsg->message == WM_KEYUP) &&
			(pMsg->wParam == VK_BACK || pMsg->wParam == VK_RETURN))
		{
			if (pMsg->message == WM_KEYDOWN)
				OnChar(WM_CHAR, pMsg->wParam, 0, tmp);
			return TRUE;
		}
	}

	// let other windows do their translations
	if(CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	// this is needed to pass certain keys to the web browser
	HWND hWndFocus = ::GetFocus();
	if(m_doc)
	{
		if(::IsChild(m_doc->m_body,hWndFocus))
		{
			if (m_doc->m_body.PreTranslateMessage(pMsg))
				return TRUE;
			/*    } else if (::IsChild(m_doc->m_desc,hWndFocus)) {
			if (m_doc->m_desc.PreTranslateMessage(pMsg))
			return TRUE;*/
		}
	}

	return FALSE;
}

LRESULT CMainFrame::OnPreCommand(UINT, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = FALSE;
	TraceMainFrameCommand(wParam, lParam);
	if((HIWORD(wParam) == 0 || HIWORD(wParam) == 1) && LOWORD(wParam) != ID_EDIT_INCSEARCH)
		StopIncSearch(true);
	return 0;
}

void  CMainFrame::UIUpdateViewCmd(CFBEView& view, WORD wID, OLECMD& oc, const wchar_t *hk)
{
	CString fbuf;
	fbuf.Format(L"%s\t%s", (const TCHAR*)view.QueryCmdText(oc.cmdID), hk);
	UISetText(wID, fbuf);
	UIEnable(wID, (oc.cmdf & OLECMDF_ENABLED) != 0);
}

BOOL CMainFrame::OnIdle()
{
	// LoadFromHTML pumps messages before DocumentComplete.  Do not run the
	// command-update path until its MSHTML document is available.
	if (!m_doc || !m_doc->m_body.HasDoc())
	  return false;

	if(CheckFileTimeStamp())
	{
		return true;
	}

	if (IsSourceActive())
	{
		static WORD disabled_commands[] =
		{
			ID_EDIT_BOLD,
			ID_EDIT_ITALIC,
			ID_EDIT_STRIK,
			ID_EDIT_SUP,
			ID_EDIT_SUB,
			ID_EDIT_CODE,
			ID_EDIT_CLONE,
			ID_EDIT_SPLIT,
			ID_EDIT_MERGE,
			ID_EDIT_REMOVE_OUTER_SECTION,
			ID_STYLE_NORMAL,
			ID_STYLE_TEXTAUTHOR,
			ID_STYLE_SUBTITLE,
			ID_STYLE_LINK,
			ID_STYLE_NOTE,
			ID_STYLE_NOLINK,
			ID_EDIT_ADD_BODY,
			ID_EDIT_ADD_TITLE,
			ID_EDIT_ADD_EPIGRAPH,
			ID_EDIT_ADD_IMAGE,
			ID_EDIT_ADD_ANN,
			ID_EDIT_ADD_TA,
			ID_EDIT_INS_IMAGE,
			ID_EDIT_INS_INLINEIMAGE,
			ID_EDIT_INS_POEM,
			ID_EDIT_INS_CITE,
			ID_EDIT_ADDBINARY,
			ID_INSERT_TABLE,
			ID_VIEW_TREE,
			ID_GOTO_REFERENCE,
			ID_GOTO_FOOTNOTE,
		};

		for (int i = 0; i < sizeof(disabled_commands)/sizeof(disabled_commands[0]); ++i)
			UIEnable(disabled_commands[i], FALSE);

		HMENU scripts = GetSubMenu(m_MenuBar.GetMenu(), 7);
		for(int i = 0; i < m_scripts.Menu().Count(); ++i)
		{
			if(!m_scripts.Menu().Item(i).isFolder)
			{
				::EnableMenuItem(scripts, ID_SCRIPT_BASE + m_scripts.Menu().Item(i).commandId, MF_BYCOMMAND | MF_GRAYED);
			}
		}

		m_contextAttributeBars.SetLinkAvailability(LinkAttributeAvailability{ false, false, false, false });
		m_contextAttributeBars.SetTableAvailability(TableAttributeAvailability{ false, false, false, false, false, false, false, false, false });

		bool fCanCC = m_source.SendMessage(SCI_GETSELECTIONSTART) != m_source.SendMessage(SCI_GETSELECTIONEND);
		UIEnable(ID_EDIT_COPY, fCanCC);
		UIEnable(ID_EDIT_CUT, fCanCC);
		UIEnable(ID_EDIT_PASTE, m_source.SendMessage(SCI_CANPASTE));
		UIEnable(ID_EDIT_PASTE2, m_source.SendMessage(SCI_CANPASTE));

		UIEnable(ID_GOTO_WRONGTAG, true);

		if(m_source.SendMessage(SCI_CANUNDO))
		{
			UISetText(ID_EDIT_UNDO, FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.edit.undo", L"&Undo"));
			UIEnable(ID_EDIT_UNDO, 1);
		}
		else
		{
			UISetText(ID_EDIT_UNDO, FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.edit.undo", L"&Undo"));
			UIEnable(ID_EDIT_UNDO, 0);
		}

		if(m_source.SendMessage(SCI_CANREDO))
		{
			UISetText(ID_EDIT_REDO, FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.edit.redo", L"&Redo"));
			UIEnable(ID_EDIT_REDO, 1);
		}
		else
		{
			UISetText(ID_EDIT_REDO, FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.edit.redo", L"&Redo"));
			UIEnable(ID_EDIT_REDO, 0);
		}

		m_last_sci_ovr = m_source.SendMessage(SCI_GETOVERTYPE);
		m_status.SetPaneText(ID_PANE_INS, CurrentOverwriteMode() ? strOVR : strINS);

	RefreshLocalizedToolbarButtonTexts(m_CmdToolbar);
	RefreshLocalizedToolbarButtonTexts(m_ScriptsToolbar);

		// Added by SeNS: issue (wish) #127
		DisplayCharCode();
	}
	// BODY view
	else
	{
		HMENU scripts = GetSubMenu(m_MenuBar.GetMenu(), 7);
		for (int i = 0; i < m_scripts.Menu().Count(); ++i)
		{
			if(!m_scripts.Menu().Item(i).isFolder)
			{
				::EnableMenuItem(scripts, ID_SCRIPT_BASE + m_scripts.Menu().Item(i).commandId, MF_BYCOMMAND | MF_ENABLED);
			}
		}

		// check if editing commands can be performed

		CFBEView& view = ActiveView();

		static OLECMD mshtml_commands[] =
		{
			{IDM_REDO},				// 0
			{IDM_UNDO},				// 1
			{IDM_COPY},				// 2
			{IDM_CUT},				// 3
			{IDM_PASTE},			// 4
			{IDM_UNLINK},			// 5
			{IDM_BOLD},				// 6
			{IDM_ITALIC},			// 7
			{IDM_STRIKETHROUGH},	// 8
			{IDM_SUPERSCRIPT},		// 9
			{IDM_SUBSCRIPT},		// 10
		};
		view.QueryStatus(mshtml_commands, sizeof(mshtml_commands)/sizeof(mshtml_commands[0]));

		static WORD	fbe_commands[] =
		{
			ID_EDIT_REDO,
			ID_EDIT_UNDO,
			ID_EDIT_COPY,
			ID_EDIT_CUT,
			ID_EDIT_PASTE,
			ID_STYLE_NOLINK,
			ID_EDIT_BOLD,
			ID_EDIT_ITALIC,
			ID_EDIT_STRIK,
			ID_EDIT_SUP,
			ID_EDIT_SUB,
			ID_EDIT_CODE,
			ID_GOTO_REFERENCE,
			ID_GOTO_FOOTNOTE
		};

		for (int jj=0; jj < sizeof(mshtml_commands)/sizeof(mshtml_commands[0]); ++jj)
		{
			DWORD flags = mshtml_commands[jj].cmdf;
			WORD cmd = fbe_commands[jj];
			UIEnable(cmd, (flags & OLECMDF_ENABLED) != 0);
			UISetCheck(cmd, (flags & OLECMDF_LATCHED) != 0);
		}
		UIUpdateViewCmd(view, ID_EDIT_REDO, mshtml_commands[0], L"Ctrl+Y");
		UIUpdateViewCmd(view, ID_EDIT_UNDO, mshtml_commands[1], L"Ctrl+Z");

		UIEnable(ID_EDIT_FINDNEXT, view.CanFindNext());

		UIUpdateViewCmd(view, ID_STYLE_LINK);
		UIUpdateViewCmd(view, ID_STYLE_NOTE);
		UIUpdateViewCmd(view, ID_STYLE_NORMAL);
		UIUpdateViewCmd(view, ID_STYLE_SUBTITLE);
		UIUpdateViewCmd(view, ID_STYLE_TEXTAUTHOR);
		UIUpdateViewCmd(view, ID_EDIT_ADD_TITLE);
		UIUpdateViewCmd(view, ID_EDIT_ADD_BODY);
		UIUpdateViewCmd(view, ID_EDIT_ADD_TA);
		UIUpdateViewCmd(view, ID_EDIT_CLONE);
		UIUpdateViewCmd(view, ID_EDIT_INS_IMAGE);
		UIUpdateViewCmd(view, ID_EDIT_INS_INLINEIMAGE);
		UIUpdateViewCmd(view, ID_EDIT_ADD_IMAGE);
		UIUpdateViewCmd(view, ID_EDIT_ADD_EPIGRAPH);
		UIUpdateViewCmd(view, ID_EDIT_ADD_ANN);
		UIUpdateViewCmd(view, ID_EDIT_SPLIT);
		UIUpdateViewCmd(view, ID_EDIT_INS_POEM);
		UIUpdateViewCmd(view, ID_EDIT_INS_CITE);
		UIUpdateViewCmd(view, ID_EDIT_CODE);
		UISetCheckCmd(view, ID_EDIT_CODE);
		UIUpdateViewCmd(view, ID_INSERT_TABLE);
		UIUpdateViewCmd(view, ID_TABLE_INSERT_ROW_ABOVE);
		UIUpdateViewCmd(view, ID_TABLE_INSERT_ROW_BELOW);
		UIUpdateViewCmd(view, ID_TABLE_DELETE_ROW);
		UIUpdateViewCmd(view, ID_TABLE_INSERT_COLUMN_LEFT);
		UIUpdateViewCmd(view, ID_TABLE_INSERT_COLUMN_RIGHT);
		UIUpdateViewCmd(view, ID_TABLE_DELETE_COLUMN);
		UIUpdateViewCmd(view, ID_TABLE_TOGGLE_HEADER_CELL);
		UIUpdateViewCmd(view, ID_TABLE_MAKE_HEADER_CELLS);
		UIUpdateViewCmd(view, ID_TABLE_MAKE_NORMAL_CELLS);
		UIUpdateViewCmd(view, ID_GOTO_FOOTNOTE);
		UIUpdateViewCmd(view, ID_GOTO_REFERENCE);
		UIUpdateViewCmd(view, ID_EDIT_MERGE);
		UIUpdateViewCmd(view, ID_EDIT_REMOVE_OUTER_SECTION);

		UIEnable(ID_GOTO_MATCHTAG, false);
		UIEnable(ID_GOTO_WRONGTAG, false);

		// Added by SeNS: process bitmap paste
		UIEnable(ID_EDIT_PASTE, m_source.SendMessage(SCI_CANPASTE) || BitmapInClipboard());

		if (m_sel_changed && /*GetCurView()*/m_editor_view_state.Current() != DESC)
		{
			SetStatusContext(m_doc->m_body.SelPath());
			UpdateStatusBar();

			// update links and IDs
			try
			{
				LinkAttributeState linkState;
				LinkAttributeAvailability linkAvailability = {};
				TableAttributeState tableState;
				TableAttributeAvailability tableAvailability = {};
				MSHTML::IHTMLElementPtr an(m_doc->m_body.SelectionAnchor());
				_variant_t    href;

				if(an)
					href = an->getAttribute(L"href", 2);

				if((bool)an && V_VT(&href)==VT_BSTR)
				{
					linkAvailability.href = true;
					linkState.href = CString(V_BSTR(&href));
					if(linkState.href.Find(L"file") == 0) linkState.href = linkState.href.Mid(linkState.href.ReverseFind(L'#'), 1024);
					bool img = (U::scmp(an->tagName, L"DIV") == 0) || (U::scmp(an->tagName, L"SPAN") == 0);
					if(img != m_cb_last_images)
						m_cb_updated = false;
					m_cb_last_images = img;
				}

				MSHTML::IHTMLElementPtr	sc(m_doc->m_body.SelectionStructCon());
				if(sc)
				{
					linkAvailability.id = true;
					if(U::scmp(sc->id, L"fbw_body"))
						linkState.id = static_cast<const wchar_t*>(sc->id);
				}

				MSHTML::IHTMLElementPtr	  im(m_doc->m_body.SelectionStructImage());
				if(im)
				{
					linkAvailability.imageTitle = true;
					linkState.imageTitle = static_cast<const wchar_t*>(im->title);
				}

				// ??????????? ID ??? ????? <section>
				MSHTML::IHTMLElementPtr scstn(m_doc->m_body.SelectionStructSection());
				if(scstn)
				{
					linkAvailability.section = true;
					linkState.section = static_cast<const wchar_t*>(scstn->id);
				}
				// ??????????? ID ??? ????? <table>
				MSHTML::IHTMLElementPtr sct(m_doc->m_body.SelectionStructTable());
				if(sct)
				{
					tableAvailability.tableId = true;
					tableState.tableId = static_cast<const wchar_t*>(sct->id);
				}

				// ??????????? ID ??? ????? <tr>, <th>, <td>
				MSHTML::IHTMLElementPtr sctc(m_doc->m_body.SelectionStructTableCon());
				if (sctc) {
					tableAvailability.cellId = true;
					tableState.id = static_cast<const wchar_t*>(sctc->id);
				}

				// ??????????? style ??? ????? <table>
				_bstr_t	styleT("");
				MSHTML::IHTMLElementPtr scsT(m_doc->m_body.SelectionsStyleTB(styleT));
				if(scsT)
				{
					tableAvailability.tableStyle = true;
					if(U::scmp(styleT,L"") != 0) tableState.tableStyle = static_cast<const wchar_t*>(styleT);
				}

				// ??????????? style ??? ????? <th>, <td>
				_bstr_t	style("");
				MSHTML::IHTMLElementPtr scs(m_doc->m_body.SelectionsStyleB(style));
				if(scs)
				{
					tableAvailability.cellStyle = true;
					if(U::scmp(style,L"") != 0) tableState.style = static_cast<const wchar_t*>(style);
				}

				// ??????????? colspan ??? ????? <th>, <td>
				_bstr_t colspan("");
				MSHTML::IHTMLElementPtr scc(m_doc->m_body.SelectionsColspanB(colspan));
				if(scc)
				{
					tableAvailability.colspan = true;
					if(U::scmp(colspan, L"") != 0) tableState.colspan = static_cast<const wchar_t*>(colspan);
				}

				// ??????????? rowspan ??? ????? <th>, <td>
				_bstr_t rowspan("");
				MSHTML::IHTMLElementPtr scr(m_doc->m_body.SelectionsRowspanB(rowspan));
				if(scr)
				{
					tableAvailability.rowspan = true;
					if(U::scmp(rowspan,L"") != 0) tableState.rowspan = static_cast<const wchar_t*>(rowspan);
				}

				// ??????????? align ??? ????? <tr>
				_bstr_t alignTR("");
				MSHTML::IHTMLElementPtr scaTR(m_doc->m_body.SelectionsAlignTRB(alignTR));
				if(scaTR)
				{
					tableAvailability.rowAlign = true;
					if(U::scmp(alignTR,L"") != 0) tableState.rowAlign = static_cast<const wchar_t*>(alignTR);
				}

				// ??????????? align ??? ????? <th>, <td>
				_bstr_t align("");
				MSHTML::IHTMLElementPtr sca(m_doc->m_body.SelectionsAlignB(align));
				if(sca)
				{
					tableAvailability.align = true;
					if(U::scmp(align,L"") != 0) tableState.align = static_cast<const wchar_t*>(align);
				}

				// ??????????? valign ??? ????? <th>, <td>
				_bstr_t valign("");
				MSHTML::IHTMLElementPtr scva(m_doc->m_body.SelectionsVAlignB(valign));
				if(scva)
				{
					tableAvailability.valign = true;
					if(U::scmp(valign,L"") != 0) tableState.valign = static_cast<const wchar_t*>(valign);
				}
				m_ignore_cb_changes = true;
				m_contextAttributeBars.ApplySelectionState(linkState, linkAvailability, tableState, tableAvailability);
				m_ignore_cb_changes = false;
			}
			catch(_com_error&)
			{

			}

			// update current tree node
			if (!m_doc_changed && _Settings.ViewDocumentTree())
				m_document_tree.HighlightItemAtPos(m_doc->m_body.SelectionContainer()); // locate appropriate tree node

			m_sel_changed = false;
		}

		// insert/overwrite mode
		OLECMD oc = {IDM_OVERWRITE};
		view.QueryStatus(&oc, 1);
		bool fOvr = (oc.cmdf & OLECMDF_LATCHED) != 0;
		if (fOvr != m_last_ie_ovr)
		{
			m_last_ie_ovr = fOvr;
			m_status.SetPaneText(ID_PANE_INS, fOvr ? strOVR : strINS);
		}

		// added by SeNS: strange bug woraround - restore position on loaded from command line file
		if (m_restore_pos_cmdline)
		{
			m_restore_pos_cmdline = false;
			int saved_pos = U::GetFileSelectedPos(m_doc->m_filename);
			GoTo(saved_pos);
			m_view.SetFocus();
		}
	}

	// added by SeNS
	// detect page scrolling, run a background spellcheck if necessary
	if (m_Speller && m_Speller->Enabled() && m_editor_view_state.Current() == BODY)
	{
		if (!m_Speller->Available())
			UIEnable(ID_TOOLS_SPELLCHECK, false, true);
		else
		{
			UIEnable(ID_TOOLS_SPELLCHECK, true, true);
			m_Speller->CheckScroll();
		}
	}
	else UIEnable(ID_TOOLS_SPELLCHECK, false, true);

	const bool tableCommandEnabled = m_editor_view_state.Current() == BODY && m_doc && m_doc->m_body.SelectionStructTableCon();
	const UINT tableCommands[] = {
		ID_TABLE_INSERT_ROW_ABOVE, ID_TABLE_INSERT_ROW_BELOW, ID_TABLE_DELETE_ROW,
		ID_TABLE_INSERT_COLUMN_LEFT, ID_TABLE_INSERT_COLUMN_RIGHT, ID_TABLE_DELETE_COLUMN,
		ID_TABLE_MAKE_HEADER_CELLS, ID_TABLE_MAKE_NORMAL_CELLS
	};
	for (size_t index = 0; index < _countof(tableCommands); ++index) {
		UIEnable(tableCommands[index], tableCommandEnabled);
	}

	// update UI
	UIUpdateToolBar();

	// update document tree
	if (m_doc_changed)
	{
		MSHTML::IHTMLDOMNodePtr chp(m_doc->m_body.GetChangedNode());
		if ((bool)chp && m_document_tree.IsWindowVisible())
		{
			m_document_tree.UpdateDocumentStructure(m_doc->m_body.Document(), chp);
			m_document_tree.HighlightItemAtPos(m_doc->m_body.SelectionContainer());
		}
		m_doc_changed = false;
	}

	// focus some stupid control if requested
	BOOL tmp;
	switch (m_want_focus)
	{
		case IDC_ID:
			OnSelectCtl(0, ID_SELECT_ID, 0, tmp);
			break;
		case IDC_HREF:
			OnSelectCtl(0, ID_SELECT_HREF, 0, tmp);
			break;
		case IDC_IMAGE_TITLE:
			OnSelectCtl(0, ID_SELECT_IMAGE, 0, tmp);
			break;
		case IDC_SECTION:
			OnSelectCtl(0, ID_SELECT_SECTION, 0, tmp);
			break;
		case IDC_IDT:
			OnSelectCtl(0, ID_SELECT_IDT, 0, tmp);
			break;
		case IDC_STYLET:
			OnSelectCtl(0, ID_SELECT_STYLET, 0, tmp);
			break;
		case IDC_STYLE:
			OnSelectCtl(0, ID_SELECT_STYLE, 0, tmp);
			break;
		case IDC_COLSPAN:
			OnSelectCtl(0, ID_SELECT_COLSPAN, 0, tmp);
			break;
		case IDC_ROWSPAN:
			OnSelectCtl(0, ID_SELECT_ROWSPAN, 0, tmp);
			break;
		case IDC_ALIGNTR:
			OnSelectCtl(0, ID_SELECT_ALIGNTR, 0, tmp);
			break;
		case IDC_ALIGN:
			OnSelectCtl(0, ID_SELECT_ALIGN, 0, tmp);
			break;
		case IDC_VALIGN:
			OnSelectCtl(0, ID_SELECT_VALIGN, 0, tmp);
			break;
	}
	m_want_focus = 0;

	// install a posted status line message
	const DWORD statusNow = ::GetTickCount();
	if (m_status_state.PromoteQueuedMessage(statusNow))
		RefreshStatusMainPane();
	if (m_status_state.ClearTransientIfExpired(statusNow))
		RefreshStatusMainPane();

	// see if we need to update title
	if(m_need_title_update || m_change_state != DocChanged())
	{
		m_need_title_update = false;
		m_change_state = DocChanged();
		CString tt;
		if (_Settings.GetShowFullPathInWindowTitle() && m_doc->m_namevalid)
		{
			CString fullPath(U::GetFullPathName(m_doc->m_filename));
			CClientDC dc(m_hWnd);
			CRect clientRect;
			GetClientRect(&clientRect);
			const int maxPathWidth = max(160, clientRect.Width() - 240);

			SIZE pathSize = {};
			::GetTextExtentPoint32W(dc, fullPath, fullPath.GetLength(), &pathSize);
			if (pathSize.cx <= maxPathWidth)
			{
				tt = fullPath;
			}
			else
			{
				CString root;
				if (fullPath.GetLength() >= 3 && fullPath[1] == L':' && fullPath[2] == L'\\')
					root = fullPath.Left(3);
				else if (fullPath.Left(2) == L"\\\\")
				{
					const int serverEnd = fullPath.Find(L'\\', 2);
					const int shareEnd = serverEnd >= 0 ? fullPath.Find(L'\\', serverEnd + 1) : -1;
					if (shareEnd >= 0)
						root = fullPath.Left(shareEnd + 1);
				}

				CString fileName(U::GetFileTitle(fullPath));
				tt = root + L"...\\" + fileName;
				::GetTextExtentPoint32W(dc, tt, tt.GetLength(), &pathSize);
				while (pathSize.cx > maxPathWidth && fileName.GetLength() > 1)
				{
					fileName = fileName.Mid(1);
					tt = L"..." + fileName;
					::GetTextExtentPoint32W(dc, tt, tt.GetLength(), &pathSize);
				}
			}
		}
		else
			tt = U::GetFileTitle(m_doc->m_filename);
		if (m_document_session.Location().IsArchive())
		{
			const CString entryName(U::GetFileTitle(m_document_session.Location().entryPath));
			const CString containerName(U::GetFileTitle(m_document_session.Location().storagePath));
			tt = entryName + L" :: " + containerName;
		}
		tt += m_change_state ? L" +" : L" -";
		CString title(tt + L" FB Editor Next");
		if (StartupTrace::Enabled())
			title += GetDiagnosticTraceText(L"fbe.trace.title_suffix", L" [Диагностика]");
		SetWindowText(title);
	}

	return FALSE;
}

void CMainFrame::AddTbButton(HWND hWnd, const TCHAR *text, const int idCommand, const BYTE bState, const HICON icon)
{
    CToolBarCtrl tb = hWnd;
	int iImage = I_IMAGENONE;
	BYTE bStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	if (icon)
	{
		CImageList iList = tb.GetImageList();
		if (iList) iImage = iList.AddIcon(icon);
	}

	tb.AddButton(idCommand, bStyle, bState, iImage, text, 0);
	// custom added command
	if (icon)
	{
		int idx = tb.CommandToIndex(idCommand);
		TBBUTTON tbButton;
		tb.GetButton(idx, &tbButton);
		AddToolbarButton(tb,tbButton, text);
		// move button to unassigned
		tb.DeleteButton(idx);
	}
	tb.AutoSize();
}

void CMainFrame::ShowScriptsToolbarCustomizeDialog()
{
	if(!::IsWindow(m_ScriptsToolbar)) return;
	TBBUTTONS catalog, defaults;
	if(!GetAvailableButtons(m_ScriptsToolbar, catalog) || !GetDefaultButtons(m_ScriptsToolbar, defaults)) return;
	std::vector<ScriptsToolbarCommand> commands;
	auto addCommand = [&](int command, const CString& name, const CString& relativePath) {
		for(size_t existing = 0; existing < commands.size(); ++existing)
			if(commands[existing].command == command) return;
		ScriptsToolbarCommand item = {}; item.command = command; item.name = name; item.relativePath = relativePath;
		item.button.iBitmap = I_IMAGENONE; item.button.idCommand = command;
		item.button.fsState = TBSTATE_ENABLED; item.button.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
		for(int index = 0; index < catalog.GetSize(); ++index)
			if(catalog[index].idCommand == command) { item.button = catalog[index]; break; }
		commands.push_back(item);
	};
	for(int index = 0; index < catalog.GetSize(); ++index) {
		if(catalog[index].idCommand == 0 || (catalog[index].fsStyle & TBSTYLE_SEP)) continue;
		CString text;
		if(catalog[index].idCommand == ID_LAST_SCRIPT)
			text = FbeLoadRuntimeStringByKey(L"fbe.hotkey.scripts.last_script", L"Last script");
		else if(!GetButtonText(catalog[index], text)) continue;
		addCommand(catalog[index].idCommand, text, CString());
	}
	for(int index = 0; index < m_scripts.Menu().Count(); ++index) {
		const ScriptDescriptor& script = m_scripts.Menu().Item(index);
		if(!script.isFolder && script.commandId > 0)
			addCommand(ID_SCRIPT_BASE + script.commandId, script.name, script.relativePath);
	}
	std::sort(commands.begin(), commands.end(), [](const ScriptsToolbarCommand& left, const ScriptsToolbarCommand& right) {
		return left.name.CompareNoCase(right.name) < 0;
	});
	std::vector<ScriptsToolbarTarget> panels; for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) if(m_scriptToolbars.Items()[index].window != NULL) { ScriptsToolbarTarget target = { m_scriptToolbars.Items()[index].definition.name, m_scriptToolbars.Items()[index].window }; panels.push_back(target); }
	CScriptsToolbarCustomizeDlg dialog(m_ScriptsToolbar, commands, defaults, _Settings, panels);
	dialog.DoModal(m_hWnd);
}

void CMainFrame::ShowScriptToolbarManagerDialog()
{
	m_scriptToolbarManager.Collection().Items().clear();
	for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) m_scriptToolbarManager.Collection().Items().push_back(m_scriptToolbars.Items()[index].definition);
	CScriptToolbarManagerDlg dialog(m_scriptToolbarManager, [this](const std::vector<ScriptToolbarDefinition>& previous, const std::vector<ScriptToolbarDefinition>& current) {
		return ApplyScriptToolbarDefinitions(previous, current);
	});
	dialog.DoModal(m_hWnd);
}

bool CMainFrame::ApplyScriptToolbarDefinitions(const std::vector<ScriptToolbarDefinition>& previous, const std::vector<ScriptToolbarDefinition>& current)
{
	PortableToolbarLayout layout; PortableToolbarStore::Load(layout);
	layout.scriptToolbars = current; layout.scriptsToolbarPresent = true;
	if(!PortableToolbarStore::Save(layout)) return false;
	if(InitializeScripts()) return true;
	layout.scriptToolbars = previous;
	if(!PortableToolbarStore::Save(layout)) return false;
	InitializeScripts();
	return false;
}

void CMainFrame::RefreshScriptToolbarViewMenu()
{
	HMENU view = ::GetSubMenu(m_MenuBar.GetMenu(), 2);
	if(view == NULL) return;
	for(int index = ::GetMenuItemCount(view) - 1; index >= 0; --index)
	{
		const UINT command = ::GetMenuItemID(view, index);
		if(command >= ID_VIEW_SCRIPT_TOOLBAR_DYNAMIC_FIRST && command <= ID_VIEW_SCRIPT_TOOLBAR_DYNAMIC_LAST)
			::RemoveMenu(view, index, MF_BYPOSITION);
	}
	m_scriptToolbarMenuIds.clear();
	int insertion = ::GetMenuItemCount(view);
	for(int index = 0; index < ::GetMenuItemCount(view); ++index)
		if(::GetMenuItemID(view, index) == ID_VIEW_SCRIPT_TOOLBARS_MANAGE) { insertion = index; break; }
	for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index)
	{
		const ScriptToolbarDefinition& definition = m_scriptToolbars.Items()[index].definition;
		if(definition.id == L"scripts-main") continue;
		const UINT command = ID_VIEW_SCRIPT_TOOLBAR_DYNAMIC_FIRST + static_cast<UINT>(m_scriptToolbarMenuIds.size());
		if(command > ID_VIEW_SCRIPT_TOOLBAR_DYNAMIC_LAST) break;
		::InsertMenu(view, insertion++, MF_BYPOSITION | MF_STRING | (definition.visible ? MF_CHECKED : MF_UNCHECKED), command, definition.name);
		m_scriptToolbarMenuIds.push_back(definition.id);
	}
}

LRESULT CMainFrame::OnViewScriptToolbarToggle(WORD, WORD command, HWND, BOOL&)
{
	const size_t index = static_cast<size_t>(command - ID_VIEW_SCRIPT_TOOLBAR_DYNAMIC_FIRST);
	if(index >= m_scriptToolbarMenuIds.size()) return 0;
	std::vector<ScriptToolbarDefinition> previous, current;
	for(size_t runtime = 0; runtime < m_scriptToolbars.Items().size(); ++runtime)
		previous.push_back(m_scriptToolbars.Items()[runtime].definition);
	current = previous;
	for(size_t definition = 0; definition < current.size(); ++definition)
		if(current[definition].id == m_scriptToolbarMenuIds[index]) { current[definition].visible = !current[definition].visible; break; }
	ApplyScriptToolbarDefinitions(previous, current);
	return 0;
}

LRESULT CMainFrame::OnToolbarDoubleClick(int, LPNMHDR hdr, BOOL& bHandled)
{
	if(hdr == NULL || hdr->hwndFrom != m_ScriptsToolbar) { bHandled = FALSE; return 0; }
	ShowScriptsToolbarCustomizeDialog();
	bHandled = TRUE;
	return 0;
}

LRESULT CALLBACK CMainFrame::ScriptsToolbarSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR reference)
{
	CMainFrame* frame = reinterpret_cast<CMainFrame*>(reference);
	if(frame != NULL && message == WM_LBUTTONDBLCLK)
	{
		frame->ShowScriptsToolbarCustomizeDialog();
		return 0;
	}
	return ::DefSubclassProc(window, message, wParam, lParam);
}

void CMainFrame::RestorePortableToolbarLayout(HWND toolbar, bool scriptsToolbar)
{
	PortableToolbarLayout layout;
	if(!PortableToolbarStore::Load(layout)) return;
	const bool toolbarPresent = scriptsToolbar ? layout.scriptsToolbarPresent : layout.commandToolbarPresent;
	if(!toolbarPresent) return;
	std::vector<PortableToolbarItem> saved = scriptsToolbar ? layout.scripts : layout.commands;

	CToolBarCtrl target = toolbar;
	if(scriptsToolbar)
	{
		TBBUTTONS available;
		if(!GetAvailableButtons(toolbar, available)) return;
		for(int scriptIndex = 0; scriptIndex < m_scripts.Menu().Count(); ++scriptIndex)
		{
			const ScriptDescriptor& script = m_scripts.Menu().Item(scriptIndex);
			if(script.isFolder || script.commandId < 1) continue;
			const int command = ID_SCRIPT_BASE + script.commandId;
			bool alreadyPresent = false;
			for(int buttonIndex = 0; buttonIndex < available.GetSize(); ++buttonIndex)
				if(available[buttonIndex].idCommand == command) { alreadyPresent = true; break; }
			if(alreadyPresent) continue;
			TBBUTTON button = {};
			button.iBitmap = I_IMAGENONE;
			button.idCommand = command;
			button.fsState = TBSTATE_ENABLED;
			button.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
			if(AddToolbarButton(toolbar, button, script.name)) available.Add(button);
		}
	}
	const int catalogIndex = m_aButtons.FindKey(toolbar);
	if(catalogIndex < 0) return;
	TBBUTTONS catalog = m_aButtons.GetValueAt(catalogIndex);
	for(size_t index = 0; index < saved.size(); ++index)
	{
		PortableToolbarItem& item = saved[index];
		if(item.separator) continue;

		int command = item.command;
		if(!item.scriptUid.IsEmpty() || !item.relativePath.IsEmpty())
		{
			command = 0;
			for(int scriptIndex = 0; scriptIndex < m_scripts.Menu().Count(); ++scriptIndex)
				if(!m_scripts.Menu().Item(scriptIndex).isFolder && (m_scripts.Menu().Item(scriptIndex).uid == item.scriptUid || (!item.relativePath.IsEmpty() && m_scripts.Menu().Item(scriptIndex).relativePath == item.relativePath)) && m_scripts.Menu().Item(scriptIndex).commandId > 0)
				{
					command = ID_SCRIPT_BASE + m_scripts.Menu().Item(scriptIndex).commandId;
					break;
				}
		}
		item.command = command; // deleted scripts remain unresolved and are ignored by the adapter.
	}
	std::vector<TBBUTTON> catalogButtons(catalog.GetSize());
	for(int index = 0; index < catalog.GetSize(); ++index) catalogButtons[index] = catalog[index];
	ToolbarLayoutAdapter::Apply(target, saved, catalogButtons);

	if(scriptsToolbar && !layout.lastScript.IsEmpty())
		for(int scriptIndex = 0; scriptIndex < m_scripts.Menu().Count(); ++scriptIndex)
			if(!m_scripts.Menu().Item(scriptIndex).isFolder && (m_scripts.Menu().Item(scriptIndex).uid == layout.lastScript || m_scripts.Menu().Item(scriptIndex).relativePath == layout.lastScript))
			{
				m_scripts.SetLastScript(m_scripts.Menu().Item(scriptIndex));
				break;
			}
}

namespace
{
bool IsUnavailableScriptToolbarItem(const PortableToolbarItem& item, const FbeScripts::UiController& scripts)
{
	if(item.separator || item.scriptUid.IsEmpty()) return false;
	for(int index = 0; index < scripts.Menu().Count(); ++index)
		if(!scripts.Menu().Item(index).isFolder && scripts.Menu().Item(index).uid == item.scriptUid) return false;
	return true;
}

bool SameToolbarItem(const PortableToolbarItem& left, const PortableToolbarItem& right)
{
	if(left.separator || right.separator) return left.separator == right.separator;
	if(!left.scriptUid.IsEmpty() || !right.scriptUid.IsEmpty()) return left.scriptUid == right.scriptUid;
	if(!left.relativePath.IsEmpty() || !right.relativePath.IsEmpty()) return left.relativePath == right.relativePath;
	return left.command == right.command;
}

std::vector<PortableToolbarItem> MergeUnavailableScriptToolbarItems(const std::vector<PortableToolbarItem>& current, const std::vector<PortableToolbarItem>& persisted, const FbeScripts::UiController& scripts)
{
	std::vector<PortableToolbarItem> merged;
	size_t cursor = 0;
	for(size_t previous = 0; previous < persisted.size(); ++previous)
	{
		if(IsUnavailableScriptToolbarItem(persisted[previous], scripts)) { merged.push_back(persisted[previous]); continue; }
		size_t found = cursor;
		while(found < current.size() && !SameToolbarItem(current[found], persisted[previous])) ++found;
		if(found == current.size()) continue;
		while(cursor <= found) merged.push_back(current[cursor++]);
	}
	while(cursor < current.size()) merged.push_back(current[cursor++]);
	return merged;
}
}

void CMainFrame::SavePortableToolbarLayout()
{
	PortableToolbarLayout layout;
	// Preserve v2 definitions that do not currently have a legacy WTL control.
	// This also keeps missing-script/orphaned UID entries round-trippable.
	PortableToolbarLayout persisted;
	if(PortableToolbarStore::Load(persisted)) layout.scriptToolbars = persisted.scriptToolbars;
	layout.commandToolbarPresent = true; layout.scriptsToolbarPresent = true;
	ToolbarLayoutAdapter::Capture(m_CmdToolbar, layout.commands);
	ToolbarLayoutAdapter::Capture(m_ScriptsToolbar, layout.scripts);
	for(size_t index = 0; index < layout.scripts.size(); ++index)
	{
		PortableToolbarItem& item = layout.scripts[index];
		if(item.separator || item.command < ID_SCRIPT_BASE + 1 || item.command > ID_SCRIPT_BASE + SCRIPT_COMMAND_COUNT) continue;
		const int scriptId = item.command - ID_SCRIPT_BASE;
		for(int scriptIndex = 0; scriptIndex < m_scripts.Menu().Count(); ++scriptIndex)
			if(!m_scripts.Menu().Item(scriptIndex).isFolder && m_scripts.Menu().Item(scriptIndex).commandId == scriptId) { item.command = 0; item.scriptUid = m_scripts.Menu().Item(scriptIndex).uid; break; }
	}
	// ToolbarLayoutAdapter cannot render a temporarily missing script, but its
	// UID must survive this save so the button reconnects after the file returns.
	for(size_t toolbarIndex = 0; toolbarIndex < persisted.scriptToolbars.size(); ++toolbarIndex)
		if(persisted.scriptToolbars[toolbarIndex].id == L"scripts-main")
			layout.scripts = MergeUnavailableScriptToolbarItems(layout.scripts, persisted.scriptToolbars[toolbarIndex].items, m_scripts);
	for(size_t toolbarIndex = 0; toolbarIndex < m_scriptToolbars.Items().size(); ++toolbarIndex)
	{
		const ScriptToolbarRuntime& runtime = m_scriptToolbars.Items()[toolbarIndex];
		if(runtime.window == NULL || runtime.definition.id == L"scripts-main") continue;
		std::vector<PortableToolbarItem> captured; ToolbarLayoutAdapter::Capture(runtime.window, captured);
		for(size_t itemIndex = 0; itemIndex < captured.size(); ++itemIndex) if(!captured[itemIndex].separator && captured[itemIndex].command >= ID_SCRIPT_BASE + 1 && captured[itemIndex].command <= ID_SCRIPT_BASE + SCRIPT_COMMAND_COUNT)
			for(int scriptIndex = 0; scriptIndex < m_scripts.Menu().Count(); ++scriptIndex) if(!m_scripts.Menu().Item(scriptIndex).isFolder && m_scripts.Menu().Item(scriptIndex).commandId == captured[itemIndex].command - ID_SCRIPT_BASE) { captured[itemIndex].command = 0; captured[itemIndex].scriptUid = m_scripts.Menu().Item(scriptIndex).uid; break; }
		bool found = false; for(size_t definitionIndex = 0; definitionIndex < layout.scriptToolbars.size(); ++definitionIndex) if(layout.scriptToolbars[definitionIndex].id == runtime.definition.id) { const std::vector<PortableToolbarItem> previous = layout.scriptToolbars[definitionIndex].items; layout.scriptToolbars[definitionIndex].items = MergeUnavailableScriptToolbarItems(captured, previous, m_scripts); layout.scriptToolbars[definitionIndex].visible = runtime.definition.visible; found = true; break; }
		if(!found) { ScriptToolbarDefinition definition = runtime.definition; definition.items = captured; layout.scriptToolbars.push_back(definition); }
	}
	bool mainFound = false;
	for(size_t index = 0; index < layout.scriptToolbars.size(); ++index)
		if(layout.scriptToolbars[index].id == L"scripts-main") { layout.scriptToolbars[index].items = layout.scripts; ScriptToolbarRuntime* runtime = m_scriptToolbars.Find(L"scripts-main"); if(runtime != NULL && runtime->rebarBandId != 0 && m_rebar.IdToIndex(runtime->rebarBandId) >= 0) layout.scriptToolbars[index].visible = IsBandVisible(runtime->rebarBandId); mainFound = true; break; }
	if(!mainFound) { ScriptToolbarDefinition main; main.id = L"scripts-main"; main.name = L"Scripts"; main.items = layout.scripts; layout.scriptToolbars.push_back(main); }
	layout.lastScript = m_scripts.LastScriptUid();
	PortableToolbarStore::Save(layout);
}

namespace
{
class ScriptDiscoveryRuntime
{
public:
	explicit ScriptDiscoveryRuntime(CMainFrame* frame) : m_started(StartScript(frame) == 0) {}
	~ScriptDiscoveryRuntime() { if (m_started) StopScript(); }
	bool Started() const { return m_started; }

private:
	bool m_started;
};
}

void CMainFrame::InitializeExtensionUi()
{
	InitializeScripts();
	InitializeBundledPlugins();
	InitializeRecentDocumentsMenu();
}

void CMainFrame::DestroyScriptToolbarRuntimeControls()
{
	for(size_t index = m_scriptToolbars.Items().size(); index > 0; --index)
	{
		ScriptToolbarRuntime& runtime = m_scriptToolbars.Items()[index - 1];
		if(runtime.window == NULL || runtime.window == m_ScriptsToolbar) continue;
		if(::IsWindow(m_rebar))
			for(int band = m_rebar.GetBandCount() - 1; band >= 0; --band)
			{
				REBARBANDINFO info = {}; info.cbSize = sizeof(info); info.fMask = RBBIM_CHILD;
				if(m_rebar.GetBandInfo(band, &info) && info.hwndChild == runtime.window)
					m_rebar.DeleteBand(band);
			}
		if(::IsWindow(runtime.window)) ::DestroyWindow(runtime.window);
		runtime.window = NULL;
		runtime.rebarBandId = 0;
	}
	if(::IsWindow(m_rebar)) { m_rebar.SendMessage(WM_SIZE); UpdateLayout(); }
}

bool CMainFrame::InitializeScripts()
{
	ReleaseScriptResources();
	DestroyScriptToolbarRuntimeControls();
	m_scriptToolbars.Reset();
	PortableToolbarLayout persistedToolbars;
	bool hasPersistedMainDefinition = false;
	if(PortableToolbarStore::Load(persistedToolbars))
		for(size_t index = 0; index < persistedToolbars.scriptToolbars.size(); ++index)
		{
			m_scriptToolbars.Add(persistedToolbars.scriptToolbars[index]);
			if(persistedToolbars.scriptToolbars[index].id == L"scripts-main") hasPersistedMainDefinition = true;
		}
	if(m_scriptToolbars.Find(L"scripts-main") == NULL) { ScriptToolbarDefinition main; main.id = L"scripts-main"; main.name = L"Scripts"; m_scriptToolbars.Add(main); }
	bool controlsCreated = true;
	for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index)
	{
		ScriptToolbarRuntime& runtime = m_scriptToolbars.Items()[index];
		if(runtime.definition.id == L"scripts-main")
		{
			runtime.window = m_ScriptsToolbar;
			if(::IsWindow(m_rebar))
				for(int band = 0; band < static_cast<int>(m_rebar.GetBandCount()); ++band)
				{
					REBARBANDINFO info = {}; info.cbSize = sizeof(info); info.fMask = RBBIM_CHILD | RBBIM_ID;
					if(m_rebar.GetBandInfo(band, &info) && info.hwndChild == m_ScriptsToolbar) { runtime.rebarBandId = info.wID; if(hasPersistedMainDefinition) m_rebar.ShowBand(band, runtime.definition.visible); break; }
				}
			continue;
		}
		if(!runtime.definition.visible) continue;
		runtime.window = CreateSimpleToolBarCtrl(m_hWnd, IDR_SCRIPTS, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST | CCS_ADJUSTABLE);
		if(runtime.window == NULL) { controlsCreated = false; break; }
		SetDialogFontForToolbarRow(runtime.window); CToolBarCtrl toolbar = runtime.window; toolbar.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS); InitToolBar(toolbar, IDR_SCRIPTS); UIAddToolBar(toolbar);
		if(!AddSimpleReBarBand(toolbar, 0, TRUE, 0, FALSE)) { ::DestroyWindow(runtime.window); runtime.window = NULL; controlsCreated = false; break; }
		const int band = m_rebar.GetBandCount() - 1;
		REBARBANDINFO info = {}; info.cbSize = sizeof(info); info.fMask = RBBIM_ID;
		if(band < 0 || !m_rebar.GetBandInfo(band, &info)) { DestroyScriptToolbarRuntimeControls(); controlsCreated = false; break; }
		runtime.rebarBandId = info.wID;
	}
	if(!controlsCreated) { DestroyScriptToolbarRuntimeControls(); return false; }
	StartupTrace::Event(L"plugin", L"P100", L"script directory resolved");
	CString serializedCommandIds;
	HMENU mainMenu = m_MenuBar.GetMenu();
	if(m_scripts.Initialize(_Settings.GetScriptsFolder(), _Settings.GetScriptCommandIds(), serializedCommandIds, ::GetSubMenu(mainMenu, 6),
		FbeLoadRuntimeStringByKey(L"fbe.menu.scripts.empty", L"No scripts"),
		[this](const CString& path) { ScriptDiscoveryRuntime runtime(this); return runtime.Started() && SUCCEEDED(ScriptLoad(path)) && ScriptFindFunc(L"Run"); },
		[this](const ScriptDescriptor& script, const FbeScripts::VisualResource& visual, UINT command) {
			if(!script.isFolder && visual.icon != NULL) AddTbButton(m_ScriptsToolbar, script.name, command, TBSTATE_ENABLED, visual.icon);
			for(size_t toolbarIndex = 0; !script.isFolder && toolbarIndex < m_scriptToolbars.Items().size(); ++toolbarIndex) if(m_scriptToolbars.Items()[toolbarIndex].window != NULL && m_scriptToolbars.Items()[toolbarIndex].window != m_ScriptsToolbar) AddTbButton(m_scriptToolbars.Items()[toolbarIndex].window, script.name, command, TBSTATE_ENABLED, visual.icon);
			if(!script.isFolder) { TBBUTTONS catalog; bool available = GetAvailableButtons(m_ScriptsToolbar, catalog); for(int index = 0; available && index < catalog.GetSize(); ++index) if(catalog[index].idCommand == static_cast<int>(command)) available = false; if(available) { TBBUTTON button = {}; button.iBitmap = I_IMAGENONE; button.idCommand = command; button.fsState = TBSTATE_ENABLED; button.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE; AddToolbarButton(m_ScriptsToolbar, button, script.name); } }
			if(visual.bitmap != NULL) m_MenuBar.AddBitmap(visual.bitmap, command); else if(visual.icon != NULL) m_MenuBar.AddIcon(visual.icon, command);
		},
		[this](ScriptDescriptor& script) { InitScriptHotkey(script); })) _Settings.SetScriptCommandIds(serializedCommandIds);
	for(size_t toolbarIndex = 0; toolbarIndex < m_scriptToolbars.Items().size(); ++toolbarIndex)
	{
		ScriptToolbarRuntime& runtime = m_scriptToolbars.Items()[toolbarIndex];
		if(runtime.window == NULL) continue;
		if(runtime.definition.id == L"scripts-main" && !hasPersistedMainDefinition) continue;
		TBBUTTONS available; if(!GetAvailableButtons(runtime.window, available)) continue;
		std::vector<TBBUTTON> catalog(available.GetSize()); for(int buttonIndex = 0; buttonIndex < available.GetSize(); ++buttonIndex) catalog[buttonIndex] = available[buttonIndex];
		std::vector<PortableToolbarItem> items = runtime.definition.items;
		for(size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex) if(!items[itemIndex].separator && !items[itemIndex].scriptUid.IsEmpty())
			for(int scriptIndex = 0; scriptIndex < m_scripts.Menu().Count(); ++scriptIndex) { const ScriptDescriptor& script = m_scripts.Menu().Item(scriptIndex); if(!script.isFolder && script.uid == items[itemIndex].scriptUid && script.commandId > 0) { items[itemIndex].command = ID_SCRIPT_BASE + script.commandId; break; } }
		ToolbarLayoutAdapter::Apply(runtime.window, items, catalog);
	}
	StartupTrace::Event(L"plugin", L"P120", L"scripts collected");
	StartupTrace::Event(L"plugin", L"P130", L"scripts sorted");
	ApplyRuntimeMainFrameMenuLocalization(mainMenu);
	RefreshScriptToolbarViewMenu();
	return true;
}

void CMainFrame::InitializeBundledPlugins()
{
	HMENU file = ::GetSubMenu(m_MenuBar.GetMenu(), 0);
	m_plugins.Initialize(::GetSubMenu(file, 6), ::GetSubMenu(file, 7),
		[](const PluginDescriptor& plugin) { return FbeLoadRuntimeStringByKey(plugin.menuKey, plugin.menu); },
		[this](const PluginDescriptor& plugin, UINT command, const CString& menu) {
			CString hotkeyText(menu); hotkeyText.Remove(L'&');
			const CString type = FbeLoadRuntimeStringByKey(plugin.type == L"Import" ? L"fbe.hotkey.plugins.import" : L"fbe.hotkey.plugins.export", plugin.type);
			RegisterPluginHotkey(plugin.clsidText, command, type + CString(L" | ") + hotkeyText);
		},
		[this](HICON icon, UINT command) { m_MenuBar.AddIcon(icon, command); });
	StartupTrace::Event(L"plugin", L"P140", L"import plugins initialized");
	StartupTrace::Event(L"plugin", L"P150", L"export plugins initialized");
}

void CMainFrame::InitializeRecentDocumentsMenu()
{
	HMENU file = ::GetSubMenu(m_MenuBar.GetMenu(), 0);
	HMENU sub = ::GetSubMenu(file, 9);
	m_recentDocuments.List().SetMenuHandle(sub);
	RefreshMruEmptyStateText(m_recentDocuments.List());
	m_recentDocuments.List().SetMaxEntries(m_recentDocuments.List().m_nMaxEntries_Max - 1);
	if (DeploymentContext::RegistryPersistenceAllowed()) m_recentDocuments.List().ReadFromRegistry(_Settings.GetKeyPath());
	else FbeRecentDocuments::ReadPortableMru(m_recentDocuments.List());
	m_recentDocuments.List().SetMaxEntries(m_recentDocuments.List().m_nMaxEntries_Max - 1);
	FbeRecentDocuments::RemoveLegacyArchiveMruEntries(m_recentDocuments.List());
	FbeRecentDocuments::AddArchiveMruRecordsToList(m_recentDocuments.List());
	StartupTrace::Event(L"plugin", L"P160", L"MRU initialized");
}

LRESULT CMainFrame::OnCreate(UINT, WPARAM, LPARAM, BOOL&)
{
	StartupTrace::AppendTestStartupBreadcrumb("mainframe-oncreate-enter");
	StartupTrace::AppendTestStartupBreadcrumb("mainframe-ui-create-start");
  StartupTrace::Event(L"mainframe", L"M100", L"OnCreate started");
  StartupTrace::Event(L"settings", L"G100", L"application settings applied");
	UiMetrics::UpdateForWindow(m_hWnd);
  m_editor_view_state.SetCtrlTabActive(false);

  // create command bar window
  m_MenuBar.SetAlphaImages(true);
	HWND hWndCmdBar = m_MenuBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
  // attach menu
  ApplyRuntimeMainFrameMenuLocalization(GetMenu());
  m_MenuBar.AttachMenu(GetMenu());
	::SendMessage(hWndCmdBar, WM_SETFONT, reinterpret_cast<WPARAM>(UiMetrics::MenuFont()), TRUE);
	m_MenuBar.AutoSize();
  // remove old menu
  SetMenu(NULL);
  // load command bar images
  m_MenuBar.LoadImages(IDR_MAINFRAME_SMALL);
  const HINSTANCE applicationModule = ATL::_AtlBaseModule.GetModuleInstance();
  AddCommandBarBitmapFromModule(m_MenuBar, applicationModule,
    IDB_TABLE_INSERT_ROW_ABOVE, ID_TABLE_INSERT_ROW_ABOVE);
  AddCommandBarBitmapFromModule(m_MenuBar, applicationModule,
    IDB_TABLE_INSERT_ROW_BELOW, ID_TABLE_INSERT_ROW_BELOW);
  AddCommandBarBitmapFromModule(m_MenuBar, applicationModule,
    IDB_TABLE_INSERT_COLUMN_LEFT, ID_TABLE_INSERT_COLUMN_LEFT);
  AddCommandBarBitmapFromModule(m_MenuBar, applicationModule,
    IDB_TABLE_INSERT_COLUMN_RIGHT, ID_TABLE_INSERT_COLUMN_RIGHT);
  AddCommandBarBitmapFromModule(m_MenuBar, applicationModule,
    IDB_TABLE_DELETE_ROW, ID_TABLE_DELETE_ROW);
  AddCommandBarBitmapFromModule(m_MenuBar, applicationModule,
    IDB_TABLE_DELETE_COLUMN, ID_TABLE_DELETE_COLUMN);
  AddCommandBarBitmapFromModule(m_MenuBar, applicationModule,
    IDB_TABLE_MAKE_HEADER_CELLS, ID_TABLE_MAKE_HEADER_CELLS);
  AddCommandBarBitmapFromModule(m_MenuBar, applicationModule,
    IDB_TABLE_MAKE_NORMAL_CELLS, ID_TABLE_MAKE_NORMAL_CELLS);

	m_CmdToolbar = ToolbarFactory::CreateCommandToolbarCtrl(m_hWnd, m_commandToolbarImages, IDR_MAINFRAME,
		ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST | CCS_ADJUSTABLE);
	if (!m_CmdToolbar || !InitToolBar(m_CmdToolbar, IDR_MAINFRAME))
	{
		StartupTrace::Error(L"toolbar", L"TB209", L"failed to create the application-owned command toolbar image list");
		if (m_CmdToolbar) m_CmdToolbar.SetImageList(NULL);
		m_commandToolbarImages.Destroy();
		if (m_CmdToolbar) m_CmdToolbar.DestroyWindow();
		return -1;
	}
	m_CmdToolbar.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS);
	for (size_t index = 0; index < kTableToolbarCommandCount; ++index)
	{
    m_table_toolbar_image_indices[index] = -1;
  }
  for (size_t index = 0; index < kTableToolbarCommandCount; ++index)
  {
    const TableToolbarCommand& command = kTableToolbarCommands[index];
		const int imageIndex = ToolbarFactory::AddBitmapFromModule(m_CmdToolbar, applicationModule, command.bitmapResourceId);
    m_table_toolbar_image_indices[index] = imageIndex;
    if (imageIndex < 0) continue;
    TBBUTTON button = {};
    button.iBitmap = imageIndex;
    button.idCommand = command.commandId;
    button.fsState = TBSTATE_ENABLED;
    button.fsStyle = TBSTYLE_BUTTON;
    button.iString = 1;
		AddToolbarButton(m_CmdToolbar, button, StripMenuMnemonics(FbeLoadRuntimeStringByKey(command.localizationKey, command.fallbackText)));
	}
	// Restore commands toolbar layout and position
	if (DeploymentContext::RegistryPersistenceAllowed())
		m_CmdToolbar.RestoreState(HKEY_CURRENT_USER, _Settings.GetKeyPath() + L"\\Toolbars", L"CommandToolbar");
	else
		RestorePortableToolbarLayout(m_CmdToolbar, false);
  for (size_t index = 0; index < kTableToolbarCommandCount; ++index)
  {
    if (m_table_toolbar_image_indices[index] < 0) continue;
    TBBUTTONINFO info = {};
    info.cbSize = sizeof(info);
    info.dwMask = TBIF_IMAGE;
    info.iImage = m_table_toolbar_image_indices[index];
    m_CmdToolbar.SetButtonInfo(kTableToolbarCommands[index].commandId, &info);
  }
  UIAddToolBar(m_CmdToolbar);

  m_ScriptsToolbar = CreateSimpleToolBarCtrl(m_hWnd, IDR_SCRIPTS, FALSE,  ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST | CCS_ADJUSTABLE);
	SetDialogFontForToolbarRow(m_ScriptsToolbar);
  m_ScriptsToolbar.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS);
  InitToolBar(m_ScriptsToolbar, IDR_SCRIPTS);
	CImageList scriptsToolbarImages = m_ScriptsToolbar.GetImageList();
	m_scriptsToolbarBaseImageCount = scriptsToolbarImages ? scriptsToolbarImages.GetImageCount() : 0;
	::SetWindowSubclass(m_ScriptsToolbar, ScriptsToolbarSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
  UIAddToolBar(m_ScriptsToolbar);

	if(!m_contextAttributeBars.Create(m_hWnd))
		return -1;
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AutoSizeToolbar(m_CmdToolbar);
	AutoSizeToolbar(m_ScriptsToolbar);
	AddSimpleReBarBand(hWndCmdBar, 0, TRUE, 0);
	AddSimpleReBarBand(m_CmdToolbar, 0, TRUE, 0, FALSE);
	AddSimpleReBarBand(m_ScriptsToolbar, 0, TRUE, 0, FALSE);
	AddSimpleReBarBand(m_contextAttributeBars.LinksBar(), 0, TRUE, 0, TRUE);
	AddSimpleReBarBand(m_contextAttributeBars.TableBar(), 0, TRUE, 0, TRUE);
	AddSimpleReBarBand(m_contextAttributeBars.TableBar2(), 0, TRUE, 0, TRUE);
	m_rebar = m_hWndToolBar;
	m_rebar.SendMessage(WM_SIZE);
	StartupTrace::Event(L"mainframe", L"M110", L"menus and toolbars created");

  // create status bar
  CreateSimpleStatusBar();
  m_status.SubclassWindow(m_hWndStatusBar);
  int panes[] =
  {
	  ID_DEFAULT_PANE,
	  ID_PANE_POSITION,
	  ID_PANE_SELECTION,
	  ID_PANE_CHAR,
	  ID_PANE_ENCODING,
	  ID_PANE_VALIDATION,
	  ID_PANE_INS
  };
  m_status.SetPanes(panes, sizeof(panes)/sizeof(panes[0]));
	m_status.SetFont(UiMetrics::DialogFont());
  m_current_dpi = GetWindowDpi(m_hWnd);
  m_status.SetPaneText(ID_PANE_POSITION, L"");
  m_status.SetPaneText(ID_PANE_SELECTION, L"");
  m_status.SetPaneText(ID_PANE_CHAR, L"");
  m_status.SetPaneText(ID_PANE_ENCODING, L"");
  m_status.SetPaneText(ID_PANE_VALIDATION, L"");

	// load insert/overwrite abbreviations
	FbeLoadString(_Module.GetResourceInstance(), IDS_PANE_INS, strINS, MAX_LOAD_STRING);
	FbeLoadString(_Module.GetResourceInstance(), IDS_PANE_OVR, strOVR, MAX_LOAD_STRING);
	UpdateStatusBar();

  // create splitter
  m_hWndClient = m_splitter.Create(m_hWnd,rcDefault,NULL,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN);
  m_splitter.SetSplitterExtendedStyle(0);

  // The outer splitter remains responsible for the document tree. Its right
  // pane is a horizontal editor/results splitter so Find All never creates a
  // floating top-level window.
  m_editor_results_splitter.Create(m_splitter, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
  m_editor_results_splitter.SetSplitterExtendedStyle(SPLIT_BOTTOMALIGNED);

  // create splitter contents
//  m_document_tree.Create(m_splitter);
//  m_document_tree.SetTitle(L"Document Tree");
  StartupTrace::AppendTestStartupBreadcrumb("mshtml-view-create-start");
  m_view.Create(m_editor_results_splitter,rcDefault,NULL,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN);
	StartupTrace::AppendTestStartupBreadcrumb("mshtml-view-create-complete");
	 m_find_results_pane.Create(m_editor_results_splitter, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

  // create a tree
  /*m_dummy_pane.Create(m_document_tree,rcDefault,NULL,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,WS_EX_CLIENTEDGE);
  m_document_tree.SetClient(m_dummy_pane);
  m_document_tree.Create(m_dummy_pane, rcDefault);
  m_document_tree.SetBkColor(::GetSysColor(COLOR_WINDOW));
  m_dummy_pane.SetSplitterPane(0,m_document_tree);
  m_dummy_pane.SetSinglePaneMode(SPLIT_PANE_LEFT);*/

  // create a source view
	if(!m_source.Create(m_view))
		return -1;
  m_view.AttachWnd(m_source);
	m_source.ApplyConfiguration(BuildSourceEditorConfig());
  StartupTrace::Event(L"mainframe", L"M120", L"editor controls created");

  // initialize a new blank document
  StartupTrace::AppendTestStartupBreadcrumb("document-create-start");
  m_doc=new FB::Doc(*this);
  FB::Doc::m_active_doc = m_doc;
	StartupTrace::AppendTestStartupBreadcrumb("document-create-complete");
  bool start_with_params = false;
  CString startupFileName;
  // ????????? ???? ?? ????????? ??????, ???? ?? ??? ???????.
  if (_ARGV.GetSize()>0 && !_ARGV[0].IsEmpty())
  {
    const DWORD fullPathLength = ::GetFullPathName(_ARGV[0], 0, NULL, NULL);
    if (fullPathLength > 0)
    {
      LPTSTR fullPath = startupFileName.GetBuffer(fullPathLength);
      const DWORD written = ::GetFullPathName(_ARGV[0], fullPathLength, fullPath, NULL);
      startupFileName.ReleaseBuffer(written > 0 ? written : 0);
      if (written == 0)
        startupFileName = _ARGV[0];
    }
    else
      startupFileName = _ARGV[0];

	StartupTrace::AppendTestStartupBreadcrumb("document-open-request");
	StartupTrace::AppendTestStartupBreadcrumb("document-load-start");
	ResolvedOpenDocument startupResolved;
	const bool startupArchive = DetectDocumentContainerKind(startupFileName) != DocumentContainerKind::None;
	FbeArchive::Error startupArchiveError;
	const bool startupResolvedOk = !startupArchive || FbeArchiveUi::ResolveOpenRequest(startupFileName, startupResolved, NULL, &startupArchiveError);
	if (!startupResolvedOk && startupArchiveError.code != FbeArchive::ErrorCode::None)
		FbeArchiveUi::ShowError(m_hWnd, startupArchiveError);
	DocumentOpenSource startupSource = startupArchive ? DocumentOpenSource() : DocumentOpenSource::Normal(startupFileName);
	if (startupArchive) { startupSource.location = startupResolved.location; startupSource.rawBytes = startupResolved.rawBytes; }
    if (startupResolvedOk && DocumentLoader::Load(*m_doc, m_view, startupSource))
	{
		StartupTrace::AppendTestStartupBreadcrumb("document-load-complete");
      start_with_params = true;
	  if (startupArchive) m_document_session.OpenArchive(startupResolved.location); else m_document_session.OpenNormal(startupFileName, m_doc->GetDocumentFileType());
	}
    else
	{
		StartupTrace::AppendTestStartupBreadcrumb("document-load-complete");
		// added by SeNS: create blank document, and load incorrect XML to Scintilla
		delete m_doc;
		m_doc=new FB::Doc(*this);
		FB::Doc::m_active_doc = m_doc;
		m_doc->CreateBlank(m_view);
		m_document_session.NewDocument();
		m_bad_xml = true;
	}
  } else
  {
	m_doc->CreateBlank(m_view);
	m_document_session.NewDocument();
  }

  StartupTrace::Event(L"mainframe", L"M130", L"document content created");

  if (_Settings.FastMode()) {
		m_doc->SetFastMode(true);
		UISetCheck(ID_VIEW_FASTMODE, TRUE);
  } else
    m_doc->SetFastMode(false);

  AttachDocument(m_doc);
	StartupTrace::AppendTestStartupBreadcrumb("document-attached");
  StartupTrace::Event(L"mainframe", L"M140", L"document attached");
  UISetCheck(ID_VIEW_BODY,1);

  StartupTrace::AppendTestStartupBreadcrumb("document-tree-create-start");
  m_document_tree.Create(m_splitter);
	StartupTrace::AppendTestStartupBreadcrumb("document-tree-create-complete");
  StartupTrace::Event(L"mainframe", L"M150", L"document tree initialized");

  if (AU::_ARGS.start_in_desc_mode)
	ShowView(DESC);

  // init plugins&MRU list
  StartupTrace::AppendTestStartupBreadcrumb("plugins-init-start");
  // The unattended background regression exercises the live MSHTML document,
  // not plug-in discovery.  In its disposable portable runtime, avoid touching
  // the optional plug-in/script/MRU surface: hosted workers can block there
  // before the scenario message is posted.  Normal editor startup and every
  // other scenario retain the full initialization path.
  if (RuntimeTests::IsScenario(L"editor-background-runtime"))
  {
    StartupTrace::AppendTestStartupBreadcrumb("plugins-init-skipped-runtime-test");
  }
  else
  {
	InitializeExtensionUi();
  }
  StartupTrace::AppendTestStartupBreadcrumb("plugins-init-complete");
  StartupTrace::Event(L"mainframe", L"M160", L"plugins and MRU initialized");

  StartupTrace::AppendTestStartupBreadcrumb("mainframe-layout-start");
  // setup splitter
	 m_editor_results_splitter.SetSplitterPanes(m_view, m_find_results_pane);
	 m_editor_results_splitter.SetSinglePaneMode(SPLIT_PANE_LEFT);
  m_splitter.SetSplitterPanes(m_document_tree, m_editor_results_splitter);

  // hide elements
  if (_Settings.ViewStatusBar())
  {
	  UISetCheck(ID_VIEW_STATUS_BAR, 1);
  }
  else
  {
	  m_status.ShowWindow(SW_HIDE);
	  UISetCheck(ID_VIEW_STATUS_BAR, FALSE);
  }

  if (_Settings.ViewDocumentTree())
  {
	  UISetCheck(ID_VIEW_TREE, 1);
  }
  else
  {
	  m_document_tree.ShowWindow(SW_HIDE);
	  UISetCheck(ID_VIEW_TREE, FALSE);
      m_splitter.SetSinglePaneMode(SPLIT_PANE_RIGHT);
  }

  // load toolbar settings
  for (int j=ATL_IDW_BAND_FIRST;j<ATL_IDW_BAND_FIRST+5;++j)
    UISetCheck(j,TRUE);
  REBARBANDINFO   rbi;
  memset(&rbi,0,sizeof(rbi));
  rbi.cbSize=sizeof(rbi);
  rbi.fMask=RBBIM_SIZE|RBBIM_STYLE;
  CString     tbs(_Settings.GetToolbarsSettings());
  const TCHAR *cp=tbs;
  for (int bn=0;;++bn) {
    const TCHAR	  *ce=_tcschr(cp,_T(';'));
    if (!ce)
      break;
    int	      id,style,cx;
    if (_stscanf(cp,_T("%d,%d,%d;"),&id,&style,&cx)!=3)
      break;
    cp=ce+1;
    int	      idx=m_rebar.IdToIndex(id);
	if(idx < 0)
	  continue; // A dynamic script toolbar may have been removed since this layout was saved.
    m_rebar.GetBandInfo(idx,&rbi);
    rbi.fStyle &= ~(RBBS_BREAK|RBBS_HIDDEN);
    style &= RBBS_BREAK|RBBS_HIDDEN;
    rbi.fStyle |= style;
    rbi.cx=cx;
    m_rebar.SetBandInfo(idx,&rbi);
    if (idx!=bn)
      m_rebar.MoveBand(idx,bn);
    UISetCheck(id,style & RBBS_HIDDEN ? FALSE : TRUE);
  }

  StartupTrace::AppendTestStartupBreadcrumb("mainframe-layout-complete");
  // register object for message filtering and idle updates
	StartupTrace::AppendTestStartupBreadcrumb("mainframe-message-hooks-start");
  CMessageLoop* pLoop = _Module.GetMessageLoop();
  ATLASSERT(pLoop != NULL);
  pLoop->AddMessageFilter(this);
  pLoop->AddIdleHandler(this);
	StartupTrace::AppendTestStartupBreadcrumb("mainframe-message-hooks-complete");

  // accept dropped files
  ::DragAcceptFiles(*this,TRUE);

  // Modification by Pilgrim
  BOOL bVisible = _Settings.ViewDocumentTree();
  m_document_tree.ShowWindow(bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
  UISetCheck(ID_VIEW_TREE, bVisible);
  m_splitter.SetSinglePaneMode(bVisible ? SPLIT_PANE_NONE : SPLIT_PANE_RIGHT);

  if(start_with_params)
  {
	  if (m_document_session.Location().IsArchive()) FbeRecentDocuments::RememberArchiveMruRecord(m_recentDocuments.List(), m_document_session.Location());
	  else FbeRecentDocuments::RememberNormalMruRecord(m_recentDocuments.List(), startupFileName);
  	  if(_Settings.RestoreFilePosition())
	  {
			m_restore_pos_cmdline = true;
	  }
  }

  StartupTrace::AppendTestStartupBreadcrumb("keyboard-layout-start");
  // Change keyboard layout
  if (_Settings.GetChangeKeybLayout())
  {
	  CString layout = _Settings.GetKeyboardLayoutId();
	  if(layout.IsEmpty()) layout = ResolveLegacyKeyboardLayoutId(_Settings.GetKeybLayout()).c_str();
	  if(!layout.IsEmpty() && !LoadKeyboardLayout(layout, KLF_ACTIVATE))
		  StartupTrace::Warning(L"startup", L"ST126", L"Configured keyboard layout could not be loaded.");
	}
	StartupTrace::AppendTestStartupBreadcrumb("keyboard-layout-complete");

  // added by SeNS: create blank document, and load incorrect XML to Scintilla
  if (m_bad_xml)
	if (!LoadToScintilla(startupFileName)) return -1;

  // Added by SeNS
  StartupTrace::AppendTestStartupBreadcrumb("speller-init-start");
  if (m_Speller && m_Speller->Enabled())
  {
	if (!m_Speller->Available())
		UIEnable(ID_TOOLS_SPELLCHECK, false, true);
	else
		UIEnable(ID_TOOLS_SPELLCHECK, true, true);
	m_Speller->SetHighlightMisspells(_Settings.GetHighlightMisspells());
  }
  else UIEnable(ID_TOOLS_SPELLCHECK, false, true);
	StartupTrace::AppendTestStartupBreadcrumb("speller-init-complete");

	// Restore scripts toolbar layout and position
	StartupTrace::AppendTestStartupBreadcrumb("scripts-toolbar-restore-start");
	RestorePortableToolbarLayout(m_ScriptsToolbar, true);
	StartupTrace::AppendTestStartupBreadcrumb("scripts-toolbar-restore-complete");

	// An unattended -b run has no user to answer this dialog.  Keep tracing
	// enabled for the report, but never turn diagnostics into a modal blocker.
	if (AU::_ARGS.source_memory_benchmark_path.IsEmpty() && StartupTrace::Enabled() && StartupTrace::IsEnabledByStoredNextLaunchPreference())
  {
	  const CString caption(GetDiagnosticTraceText(L"fbe.trace.caption", L"Диагностический журнал"));
	  const CString warning(GetDiagnosticTraceText(L"fbe.trace.warning",
		  L"FBE Next запущен в режиме диагностики. Запись диагностического журнала может замедлять работу программы и содержит сведения о действиях с книгами.\n\n"
		  L"Отключить диагностический режим для следующих запусков? Для применения потребуется перезапустить программу."));
	  if (::MessageBox(m_hWnd, warning, caption, MB_YESNO | MB_ICONWARNING) == IDYES)
	  {
		  if (m_diagnostic_commands.SetEnabledForNextLaunch(false))
		  {
			  ::MessageBox(m_hWnd,
				  GetDiagnosticTraceText(L"fbe.trace.disable.completed",
					  L"Диагностический режим будет отключён после перезапуска FBE Next."),
				  caption, MB_OK | MB_ICONINFORMATION);
		  }
		  else
		  {
			  ::MessageBox(m_hWnd,
				  GetDiagnosticTraceText(L"fbe.trace.change_failed",
					  L"Не удалось изменить настройку диагностического журнала."),
				  caption, MB_OK | MB_ICONERROR);
		  }
	  }
  }

  StartupTrace::AppendTestStartupBreadcrumb("mainframe-finalize-start");
	ThemeManager::ApplyToWindow(m_hWnd);
  m_need_title_update = true;
	StartupTrace::AppendTestStartupBreadcrumb("mainframe-finalize-complete");
  StartupTrace::Event(L"mainframe", L"M199", L"OnCreate completed");
	StartupTrace::AppendTestStartupBreadcrumb("mainframe-ui-create-complete");
	StartupTrace::AppendTestStartupBreadcrumb("mainframe-oncreate-exit");
  return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /* unused: uMsg */, WPARAM /* unused: wParam */, LPARAM /* unused: lParam */, BOOL& bHandled)
{
	if(::IsWindow(m_ScriptsToolbar)) ::RemoveWindowSubclass(m_ScriptsToolbar, ScriptsToolbarSubclassProc, 1);
	m_source.Destroy();
  KillTimer(RECOVERY_TIMER_ID);
  DestroyAcceleratorTable(m_hAccel);
	m_contextAttributeBars.Destroy();
	if (::IsWindow(m_CmdToolbar)) m_CmdToolbar.SetImageList(NULL);
	m_commandToolbarImages.Destroy();
	UiMetrics::Shutdown();
	// WTL's default CFrameWindowImpl handler posts WM_QUIT with code 1 for
	// every top-level window.  A normal editor close, including a successful
	// unattended Save, is a successful process termination.
	::PostQuitMessage(0);
	bHandled=TRUE;
  return 0;
}

LRESULT CMainFrame::OnQueryEndSession(UINT, WPARAM, LPARAM, BOOL&)
{
	if (DocChanged())
		SaveRecoveryNow();
	return TRUE;
}

LRESULT CMainFrame::OnEndSession(UINT, WPARAM wParam, LPARAM, BOOL&)
{
	if (wParam)
	{
		if (DocChanged())
			SaveRecoveryNow();
		KillTimer(RECOVERY_TIMER_ID);
	}
	return 0;
}
LRESULT CMainFrame::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
  // Batch jobs have neither an operator nor an interactive close contract.
  // Their individual scenario handlers have already saved or recorded failure
  // before posting WM_CLOSE, so never let DiscardChanges() show a modal prompt.
  const bool unattendedBatch = !AU::_ARGS.source_memory_benchmark_path.IsEmpty();
  if (unattendedBatch || DiscardChanges())
  {
	m_recovery.DeleteIfWritten();
	// added by SeNS
	if (m_Speller)
	{
		m_Speller->EndDocumentCheck();
		m_Speller->SetEnabled(false);
	}
	_Settings.SetViewStatusBar(m_status.IsWindowVisible() != 0);
	//_Settings.SetViewDocumentTree(IsSourceActive() ? m_document_tree.IsWindowVisible()==0 : !m_save_sp_mode);
    _Settings.SetSplitterPos(m_splitter.GetSplitterPos());
	if (m_editor_results_splitter.IsWindow() && m_editor_results_splitter.GetSinglePaneMode() == SPLIT_PANE_NONE)
	{
		RECT resultsClient = {}; m_editor_results_splitter.GetClientRect(&resultsClient);
		const int height = resultsClient.bottom - m_editor_results_splitter.GetSplitterPos();
		if (height > 0) _Settings.SetFindResultsPaneHeight(MulDiv(height, 96, m_current_dpi ? m_current_dpi : 96));
	}
    WINDOWPLACEMENT wpl;
    wpl.length=sizeof(wpl);
    GetWindowPlacement(&wpl);
	_Settings.SetWindowPosition(wpl);
	if (DeploymentContext::RegistryPersistenceAllowed())
		FbeRecentDocuments::WriteRegistryMruWithoutArchive(m_recentDocuments.List(), _Settings.GetKeyPath());
	else
		FbeRecentDocuments::WritePortableMru(m_recentDocuments.List());
    // save toolbars state
    CString tbs;
    REBARBANDINFO  rbi;
    memset(&rbi,0,sizeof(rbi));
    rbi.cbSize=sizeof(rbi);
    rbi.fMask=RBBIM_ID|RBBIM_SIZE|RBBIM_STYLE;
    int	  num_bands=m_rebar.GetBandCount();
    for (int i=0;i<num_bands;++i) {
      m_rebar.GetBandInfo(i,&rbi);
	  bool dynamicScriptBand = false;
	  for(size_t runtime = 0; runtime < m_scriptToolbars.Items().size(); ++runtime)
		if(m_scriptToolbars.Items()[runtime].definition.id != L"scripts-main" && m_scriptToolbars.Items()[runtime].rebarBandId == rbi.wID) { dynamicScriptBand = true; break; }
	  if(dynamicScriptBand) continue;
      CString	bi;
      bi.Format(_T("%d,%d,%d;"), static_cast<int>(rbi.wID), static_cast<int>(rbi.fStyle), static_cast<int>(rbi.cx));
      tbs+=bi;
    }

	// Save toolbar layout
	if (DeploymentContext::RegistryPersistenceAllowed())
	{
		m_CmdToolbar.SaveState(HKEY_CURRENT_USER, _Settings.GetKeyPath() + L"\\Toolbars", L"CommandToolbar");
	}
	SavePortableToolbarLayout();

    _Settings.SetToolbarsSettings(tbs);
	_Settings.SaveHotkeyGroups();
	_Settings.Save();
	_Settings.SaveWords();
	_Settings.Close();

	DefWindowProc(WM_CLOSE,0,0);
	// A handled, successful close must not be reported as a process failure.
	// In particular, unattended Save/benchmark runs use WM_CLOSE to finish.
	return 0;
  }
  return 0;
}

bool CMainFrame::SaveRecoveryNow()
{
	std::vector<char> sourceText;
	const bool sourceActive = IsSourceActive();
	if (sourceActive)
	{
		const LRESULT textLength = m_source.SendMessage(SCI_GETLENGTH);
		sourceText.resize(static_cast<size_t>(textLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, textLength + 1, reinterpret_cast<LPARAM>(sourceText.data()));
	}
	FbeRecovery::SnapshotRequest request; request.documentChanged = DocChanged(); request.sourceActive = sourceActive; request.sourceXmlInvalid = m_bad_xml;
	request.sourceText = sourceText.empty() ? NULL : sourceText.data(); request.sourceTextLength = sourceText.empty() ? 0 : sourceText.size() - 1; request.location = m_document_session.Location();
	return m_doc && m_recovery.Save(*m_doc, request);
}
void CMainFrame::TryRestoreRecovery()
{
	// Unattended runtime scenarios must not inherit or prompt for a previous
	// session.  The two recovery scenarios below are the sole explicit tests of
	// this UI path and retain its production behavior.
	wchar_t testMode[4] = {};
	const bool unattendedTest = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode)) == 1 && testMode[0] == L'1';
	// -b runs an unattended batch job.  A recovery prompt would leave that job
	// blocked forever, and the document passed on its command line is already
	// the caller's explicit recovery choice.
	const bool unattendedBatch = !AU::_ARGS.source_memory_benchmark_path.IsEmpty();
	if ((unattendedTest || unattendedBatch) && !RuntimeTests::IsScenario(L"archive-recovery-verify") && !RuntimeTests::IsScenario(L"archive-recovery-external-verify") && !RuntimeTests::IsScenario(L"normal-recovery-verify"))
		return;
	FbeRecovery::RestoreCandidate candidate;
	if (!m_recovery.GetRestoreCandidate(_ARGV.GetSize() > 0, candidate)) return;

	if (!RuntimeTests::IsScenario(L"archive-recovery-verify") && !RuntimeTests::IsScenario(L"archive-recovery-external-verify") && !RuntimeTests::IsScenario(L"normal-recovery-verify") && U::MessageBox(MB_YESNO | MB_ICONQUESTION, IDS_RECOVERY_CAPTION, IDS_RECOVERY_MSG) != IDYES)
		return;

	if (LoadFile(candidate.snapshotPath) == OK)
	{
		m_recovery.CommitRestoredIdentity(*m_doc, m_document_session, candidate);
		m_doc->ResetSavePoint();
		if (m_bad_xml)
			m_bad_filename = L"Untitled.fb2";
		m_recovery.CompleteRestore();
	}
}

LRESULT CMainFrame::OnSettingChange(UINT, WPARAM, LPARAM, BOOL&)
{
	ThemeManager::RefreshSystemTheme();
	UiMetrics::UpdateForWindow(m_hWnd);
	if (::IsWindow(m_MenuBar)) { ::SendMessage(m_MenuBar, WM_SETFONT, reinterpret_cast<WPARAM>(UiMetrics::MenuFont()), TRUE); m_MenuBar.AutoSize(); }
	if (::IsWindow(m_CmdToolbar)) { SetDialogFontForToolbarRow(m_CmdToolbar); AutoSizeToolbar(m_CmdToolbar); }
	if (::IsWindow(m_ScriptsToolbar)) { SetDialogFontForToolbarRow(m_ScriptsToolbar); AutoSizeToolbar(m_ScriptsToolbar); }
	m_contextAttributeBars.UpdateMetrics();
	if (::IsWindow(m_rebar)) m_rebar.SendMessage(WM_SIZE);
	if (::IsWindow(m_hWndStatusBar)) m_status.SetFont(UiMetrics::DialogFont());
	if (m_doc)
		m_doc->ApplyConfChanges();
	if (m_source.IsWindow())
	{
		m_source.UpdateMetrics(BuildSourceEditorConfig());
		m_source.SendMessage(SCI_COLOURISE, 0, -1);
	}
	if (m_document_tree.IsWindow())
	{
		if (m_document_tree.m_tree.m_tree.IsWindow())
			m_document_tree.m_tree.m_tree.SetBkColor(ThemeManager::WindowColor());
	}
	RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
	return 0;
}
LRESULT CMainFrame::OnDpiChanged(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
	const UINT newDpi = HIWORD(wParam);
	if (!newDpi || newDpi == m_current_dpi)
		return 0;

	const UINT oldDpi = m_current_dpi ? m_current_dpi : 96;
	const int splitterPosition = m_splitter.GetSplitterPos();
	const bool resultsVisible = m_editor_results_splitter.GetSinglePaneMode() == SPLIT_PANE_NONE;
	if (resultsVisible)
	{
		// Preserve the height the user actually dragged to before coordinates
		// become relative to the new DPI; do not restore an older setting.
		RECT resultsClient = {}; m_editor_results_splitter.GetClientRect(&resultsClient);
		const int height = resultsClient.bottom - m_editor_results_splitter.GetSplitterPos();
		if (height > 0) _Settings.SetFindResultsPaneHeight(MulDiv(height, 96, oldDpi));
	}
	const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
	if (suggested)
	{
		SetWindowPos(NULL, suggested->left, suggested->top,
			suggested->right - suggested->left, suggested->bottom - suggested->top,
			SWP_NOACTIVATE | SWP_NOZORDER);
	}

	m_current_dpi = newDpi;
	UiMetrics::UpdateForWindow(m_hWnd);
	if (::IsWindow(m_MenuBar)) { ::SendMessage(m_MenuBar, WM_SETFONT, reinterpret_cast<WPARAM>(UiMetrics::MenuFont()), TRUE); m_MenuBar.AutoSize(); }
	if (::IsWindow(m_CmdToolbar)) { SetDialogFontForToolbarRow(m_CmdToolbar); AutoSizeToolbar(m_CmdToolbar); }
	if (::IsWindow(m_ScriptsToolbar)) { SetDialogFontForToolbarRow(m_ScriptsToolbar); AutoSizeToolbar(m_ScriptsToolbar); }
	m_contextAttributeBars.UpdateMetrics();
	if (::IsWindow(m_hWndStatusBar)) m_status.SetFont(UiMetrics::DialogFont());
	if(m_source.IsWindow())
	{
		m_source.UpdateMetrics(BuildSourceEditorConfig());
	}
	if (splitterPosition >= 0)
		m_splitter.SetSplitterPos(MulDiv(splitterPosition, newDpi, oldDpi));
	if (resultsVisible)
		ApplyFindResultsPaneHeight();
	m_find_results_pane.ApplyDpi();

	m_rebar.SendMessage(WM_SIZE);
	m_status.SendMessage(WM_SIZE);
	UpdateLayout();
	UpdateStatusBarLayout();
	RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
	return 0;
}

void CMainFrame::ApplyFindResultsPaneHeight()
{
	if (!m_editor_results_splitter.IsWindow()) return;
	RECT client = {}; m_editor_results_splitter.GetClientRect(&client);
	const UINT dpi = m_current_dpi ? m_current_dpi : 96;
	const int total = (std::max)(0, static_cast<int>(client.bottom - client.top));
	const int savedHeight = (std::max)(120, static_cast<int>(_Settings.GetFindResultsPaneHeight()));
	const int preferred = MulDiv(savedHeight, dpi, 96);
	const int minResults = MulDiv(120, dpi, 96);
	const int minEditor = MulDiv(160, dpi, 96);
	// WTL's splitter has one minimum for both panes.  Use 160 logical pixels
	// when there is room (which is stricter than the 120px Results minimum),
	// otherwise reduce it symmetrically so a tiny window never gets a negative
	// splitter position.
	m_editor_results_splitter.m_cxyMin = (std::min)(minEditor, total / 2);
	const int maxResults = (std::max)(0, total - minEditor);
	const int resultsHeight = total >= minResults + minEditor
		? (std::min)((std::max)(minResults, preferred), maxResults)
		: total / 2;
	m_editor_results_splitter.SetSplitterPos((std::max)(0, total - resultsHeight));
}

void CMainFrame::ConstrainFindResultsPaneSplitter()
{
	if (!m_editor_results_splitter.IsWindow() || m_editor_results_splitter.GetSinglePaneMode() != SPLIT_PANE_NONE) return;
	RECT client = {}; m_editor_results_splitter.GetClientRect(&client);
	const UINT dpi = m_current_dpi ? m_current_dpi : 96;
	const int total = (std::max)(0, static_cast<int>(client.bottom - client.top));
	m_editor_results_splitter.m_cxyMin = (std::min)(MulDiv(160, dpi, 96), total / 2);
	const int position = m_editor_results_splitter.GetSplitterPos();
	if (position >= 0) m_editor_results_splitter.SetSplitterPos(position);
}

void CMainFrame::ShowFindResultsPane(CFBEView* view)
{
	if (view == NULL) return;
	m_find_results_pane.Attach(view);
	const bool wasHidden = m_editor_results_splitter.GetSinglePaneMode() != SPLIT_PANE_NONE;
	m_editor_results_splitter.SetSinglePaneMode(SPLIT_PANE_NONE);
	if (wasHidden) ApplyFindResultsPaneHeight();
	// Find All is modeless: do not steal focus from its dialog.
}

void CMainFrame::HideFindResultsPane()
{
	if (!m_editor_results_splitter.IsWindow()) return;
	RECT client = {}; m_editor_results_splitter.GetClientRect(&client);
	const int height = client.bottom - m_editor_results_splitter.GetSplitterPos();
	const UINT dpi = m_current_dpi ? m_current_dpi : 96;
	if (height > 0) _Settings.SetFindResultsPaneHeight(MulDiv(height, 96, dpi));
	m_editor_results_splitter.SetSinglePaneMode(SPLIT_PANE_LEFT);
}

void CMainFrame::RefreshFindResultsPane(CFBEView* view)
{
	if (m_editor_results_splitter.GetSinglePaneMode() == SPLIT_PANE_NONE && view != NULL && m_find_results_pane.AttachedView() == view)
		m_find_results_pane.Refresh();
}

LRESULT CMainFrame::OnShowFindResultsPane(UINT, WPARAM view, LPARAM, BOOL&) { ShowFindResultsPane(reinterpret_cast<CFBEView*>(view)); return 0; }
LRESULT CMainFrame::OnHideFindResultsPane(UINT, WPARAM, LPARAM, BOOL&) { HideFindResultsPane(); return 0; }
LRESULT CMainFrame::OnRefreshFindResultsPane(UINT, WPARAM view, LPARAM, BOOL&) { RefreshFindResultsPane(reinterpret_cast<CFBEView*>(view)); return 0; }
LRESULT CMainFrame::OnDetachFindResultsPane(UINT, WPARAM view, LPARAM, BOOL&)
{
	CFBEView* detached = reinterpret_cast<CFBEView*>(view);
	if (detached != NULL && m_find_results_pane.AttachedView() == detached)
	{
		m_find_results_pane.Detach(detached);
		HideFindResultsPane();
	}
	return 0;
}
LRESULT CMainFrame::OnTimer(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
{
	if (wParam == IMAGE_IMPORT_TEST_TIMER_ID)
	{
		KillTimer(IMAGE_IMPORT_TEST_TIMER_ID);
		PostMessage(AU::WM_SOURCE_MEMORY_BENCHMARK);
		return 0;
	}
	if (wParam != RECOVERY_TIMER_ID)
	{
		bHandled = FALSE;
		return 0;
	}

	SaveRecoveryNow();


	return 0;
}

LRESULT CMainFrame::OnPostCreate(UINT, WPARAM, LPARAM, BOOL&)
{
	StartupTrace::AppendTestStartupBreadcrumb("postcreate-enter");
	StartupTrace::AppendTestStartupBreadcrumb("postcreate-recovery-start");
	TryRestoreRecovery();
	StartupTrace::AppendTestStartupBreadcrumb("postcreate-recovery-complete");
	SetTimer(RECOVERY_TIMER_ID, RECOVERY_INTERVAL_MS);

	//SetSplitterPos works best after the default WM_CREATE has been handled
	m_splitter.SetSplitterPos(_Settings.GetSplitterPos());

	_Settings.LoadHotkeyGroups();
	DestroyAcceleratorTable(m_hAccel);

	LPACCEL lpaccelNew = new ACCEL[_Settings.keycodes];
	int HKentries = _Settings.keycodes;
	for(unsigned int i = 0; i < _Settings.m_hotkey_groups.size(); ++i)
	{
		CHotkeysGroup& group = _Settings.m_hotkey_groups[i];
		for(unsigned int j = 0; j < group.m_hotkeys.size(); ++j)
		{
			ACCEL accel = group.m_hotkeys[j].m_accel;
			if(accel.fVirt != NULL && accel.key != NULL && accel.cmd != NULL)
			{
				lpaccelNew[--HKentries] = accel;
			}
		}
	}

	m_hAccel = CreateAcceleratorTable(lpaccelNew, _Settings.keycodes);
	delete[] lpaccelNew;

	FillMenuWithHkeys(m_MenuBar.GetMenu());
	RunPortableStateTestScenario();
	StartupTrace::AppendTestStartupBreadcrumb("postcreate-before-runtime-dispatch");
	if (!AU::_ARGS.source_memory_benchmark_path.IsEmpty())
	{
		if (PostMessage(AU::WM_SOURCE_MEMORY_BENCHMARK))
			StartupTrace::AppendTestStartupBreadcrumb("runtime-message-posted");
		else
		{
			const DWORD error = ::GetLastError();
			StartupTrace::HResult(L"test", L"TST201", HRESULT_FROM_WIN32(error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error), L"failed to post runtime benchmark message");
		}
	}
	StartupTrace::AppendTestStartupBreadcrumb("postcreate-exit");
	return 0;
}

static bool WritePortableStateTestText(const CString& path, const char* text)
{
	HANDLE file = ::CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	const DWORD length = static_cast<DWORD>(strlen(text));
	const bool ok = ::WriteFile(file, text, length, &written, NULL) != FALSE && written == length;
	::CloseHandle(file);
	return ok;
}

static bool HasPortableStateToolbarWidth(const CString& settings, UINT bandId, int expectedWidth)
{
	int position = 0;
	CString entry = settings.Tokenize(L";", position);
	while (!entry.IsEmpty())
	{
		int id = 0, style = 0, width = 0;
		if (_stscanf(entry, L"%d,%d,%d", &id, &style, &width) == 3 &&
			id == static_cast<int>(bandId))
			return width == expectedWidth;
		entry = settings.Tokenize(L";", position);
	}
	return false;
}

#include "testing\RuntimeTestScenarios.inl"

#include "ui\MainFrameRuntimeUi.inl"
// search&replace in scintilla
CString	  SciSelection(CWindow source) {
  int	  start=source.SendMessage(SCI_GETSELECTIONSTART);
  int	  end=source.SendMessage(SCI_GETSELECTIONEND);

  if (start>=end)
    return CString();

  std::vector<char> buffer(end-start+1);
  if (buffer.empty())
    return CString();
  source.SendMessage(SCI_GETSELTEXT,0,(LPARAM)buffer.data());

  char	  *p=buffer.data();
  while (*p && *p!='\r' && *p!='\n')
    ++p;

  int	  wlen=::MultiByteToWideChar(CP_UTF8,0,buffer.data(),p-buffer.data(),NULL,0);
  if (wlen <= 0)
    return CString();

  CString ret;
  wchar_t *wp=ret.GetBuffer(wlen);
  ::MultiByteToWideChar(CP_UTF8, 0, buffer.data() ,p-buffer.data(), wp, wlen);
  ret.ReleaseBuffer(wlen);
  return ret;
}

static int BuildScintillaSearchFlags(int findFlags, bool useRegexp) {
  const int kFindWholeWord = 2; // FRF_WHOLE
  const int kFindMatchCase = 4; // FRF_CASE

  int flags=0;
  if (findFlags & kFindWholeWord)
    flags|=SCFIND_WHOLEWORD;
  if (findFlags & kFindMatchCase)
    flags|=SCFIND_MATCHCASE;
  if (useRegexp)
    flags|=SCFIND_REGEXP|SCFIND_CXX11REGEX;
  return flags;
}

struct ScopedMallocChar {
  char* value;
  explicit ScopedMallocChar(char* ptr = NULL) : value(ptr) {}
  ~ScopedMallocChar() { free(value); }
  char* get() const { return value; }
  char* release() { char* tmp = value; value = NULL; return tmp; }
private:
  ScopedMallocChar(const ScopedMallocChar&);
  ScopedMallocChar& operator=(const ScopedMallocChar&);
};

static bool PrepareScintillaRegexReplaceTarget(CWindow source, CString& patternText, int findFlags) {
  // ??? SCI_REPLACETARGETRE ????? ?????? ???????? ????? ?? ???????? target,
  // ????? Scintilla ????? ???????? ?????? ??????? ????? TARGETFROMSELECTION.
  if (_Settings.GetNBSPChar().Compare(L"\u00A0") != 0)
    patternText.Replace( L"\u00A0", _Settings.GetNBSPChar());

  int patlen = 0;
  ScopedMallocChar pattern(AU::ToUtf8(patternText, patlen));
  if (pattern.get() == NULL)
    return false;

  source.SendMessage(SCI_TARGETFROMSELECTION);
  const int targetStart = source.SendMessage(SCI_GETTARGETSTART);
  const int targetEnd = source.SendMessage(SCI_GETTARGETEND);
  source.SendMessage(SCI_SETSEARCHFLAGS,BuildScintillaSearchFlags(findFlags, true),0);
  const int matchPos = source.SendMessage(SCI_SEARCHINTARGET,patlen,(LPARAM)pattern.get());
  const bool readyToReplace = matchPos == targetStart &&
    source.SendMessage(SCI_GETTARGETEND) == targetEnd;
  return readyToReplace;
}

class CSciFindDlg : public CFindDlgBase {
public:
  CWindow	m_source;

  CSciFindDlg(CFBEView *view,HWND src) :
    CFindDlgBase(view), m_source(src)
  {
  }
  void UpdatePattern()
  {
	  m_view->m_fo.pattern=SciSelection(m_source);
  }

  virtual void	DoFind() {
    GetData();
    if (m_view->SciFindNext(m_source,false,true)) {
      SaveString();
      SaveHistory();
    }
  }
};

class CSciReplaceDlg : public CReplaceDlgBase {
public:
  CWindow	m_source;

  CSciReplaceDlg(CFBEView *view,HWND src) :
    CReplaceDlgBase(view), m_source(src)
  {
  }

	void UpdatePattern()
	{
		m_view->m_fo.pattern=SciSelection(m_source);
	}

  virtual void DoFind() {
    if (!m_view->SciFindNext(m_source,false,false))
	{
		U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_END_MSG, static_cast<LPCWSTR>(m_view->m_fo.pattern));
	}
    else {
      SaveString();
      SaveHistory();
      m_selvalid=true;
      MakeClose();
    }
  }
  virtual void DoReplace() {
    if (m_selvalid) { // replace
      bool readyToReplace = true;
      if (m_view->m_fo.fRegexp)
        readyToReplace = PrepareScintillaRegexReplaceTarget(m_source, m_view->m_fo.pattern, m_view->m_fo.flags);
      else
        m_source.SendMessage(SCI_TARGETFROMSELECTION);

      if (readyToReplace) {
        DWORD   len=::WideCharToMultiByte(CP_UTF8,0,
		m_view->m_fo.replacement,m_view->m_fo.replacement.GetLength(),
		NULL,0,NULL,NULL);
        std::vector<char> tmp(len+1);
        if (!tmp.empty()) {
	  ::WideCharToMultiByte(CP_UTF8,0,
		        m_view->m_fo.replacement,m_view->m_fo.replacement.GetLength(),
		        tmp.data(),len,NULL,NULL);
	  tmp[len]='\0';
	  if (m_view->m_fo.fRegexp)
	    m_source.SendMessage(SCI_REPLACETARGETRE,len,(LPARAM)tmp.data());
	  else
	    m_source.SendMessage(SCI_REPLACETARGET,len,(LPARAM)tmp.data());
        }
      }
      m_selvalid=false;
    }
    DoFind();
  }
  virtual void DoReplaceAll() {
    if (m_view->m_fo.pattern.IsEmpty())
      return;

    // setup search flags
    int flags = BuildScintillaSearchFlags(m_view->m_fo.flags, m_view->m_fo.fRegexp);
    m_source.SendMessage(SCI_SETSEARCHFLAGS,flags,0);

    // setup target range
    int	  end=m_source.SendMessage(SCI_GETLENGTH);
    m_source.SendMessage(SCI_SETTARGETSTART,0);
    m_source.SendMessage(SCI_SETTARGETEND,end);

    // convert search pattern and replacement to utf8
    int	  patlen, num_pat_nbsp = 0, num_rep_nbsp = 0;
	// added by SeNS
	if (_Settings.GetNBSPChar().Compare(L"\u00A0") != 0)
		num_pat_nbsp = m_view->m_fo.pattern.Replace( L"\u00A0", _Settings.GetNBSPChar());
    ScopedMallocChar pattern(AU::ToUtf8(m_view->m_fo.pattern,patlen));
    if (pattern.get()==NULL)
      return;
    int	  replen;
	// added by SeNS
	if (_Settings.GetNBSPChar().Compare(L"\u00A0") != 0)
		num_rep_nbsp = m_view->m_fo.replacement.Replace( L"\u00A0", _Settings.GetNBSPChar());
    ScopedMallocChar replacement(AU::ToUtf8(m_view->m_fo.replacement,replen));
    if (replacement.get()==NULL) {
      return;
    }

    // find first match
    int pos=m_source.SendMessage(SCI_SEARCHINTARGET,patlen,(LPARAM)pattern.get());

    int   num_repl=0;

    if (pos!=-1 && pos<=end) {
      int   last_match=pos;

      m_source.SendMessage(SCI_BEGINUNDOACTION);
      while (pos!=-1) {
	int matchlen=m_source.SendMessage(SCI_GETTARGETEND)-m_source.SendMessage(SCI_GETTARGETSTART);
	matchlen -= num_pat_nbsp*2;

	int mvp=0;
	if (matchlen<=0) {
	  char	ch=(char)m_source.SendMessage(SCI_GETCHARAT,m_source.SendMessage(SCI_GETTARGETEND));
	  if (ch=='\r' || ch=='\n')
	    mvp=1;
	}
	int rlen=matchlen;
	if (m_view->m_fo.fRegexp)
	  rlen=m_source.SendMessage(SCI_REPLACETARGETRE,replen,(LPARAM)replacement.get());
	else
	  m_source.SendMessage(SCI_REPLACETARGET,replen,(LPARAM)replacement.get());

	end += rlen-matchlen;
	last_match=pos+rlen+mvp+num_rep_nbsp*2;
	if (last_match>=end)
	  pos=-1;
	else {
	  m_source.SendMessage(SCI_SETTARGETSTART,last_match);
	  m_source.SendMessage(SCI_SETTARGETEND,end);
	  pos=m_source.SendMessage(SCI_SEARCHINTARGET,patlen,(LPARAM)pattern.get());
	}
	++num_repl;
      }
      m_source.SendMessage(SCI_ENDUNDOACTION);
    }


    if (num_repl>0) {
      SaveString();
      SaveHistory();
      U::MessageBox(MB_OK, IDS_REPL_ALL_CAPT, IDS_REPL_DONE_MSG, num_repl);
      MakeClose();
      m_selvalid=false;
    } else
	{
		U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_END_MSG, static_cast<LPCWSTR>(m_view->m_fo.pattern));
	}
  }
};

CMainFrame::~CMainFrame()
{
	ReleaseScriptResources();
	delete m_doc;
	delete m_sci_find_dlg;
}

LRESULT CMainFrame::OnUnhandledCommand(UINT /* unused: uMsg */, WPARAM wParam, LPARAM lParam, BOOL& /* unused: bHandled */)
{
	HWND hFocus = ::GetFocus();
	UINT idCtl = HIWORD(wParam);

	// only pass messages to the editors
	if (idCtl == 0 || idCtl == 1)
	{
		if (m_contextAttributeBars.ContainsFocus(hFocus))
				return ::SendMessage(hFocus, WM_COMMAND, wParam, lParam);

		// We need to check that the focused window is a web browser indeed
		if(hFocus == m_view.GetActiveWnd() || ::IsChild(m_view.GetActiveWnd(), hFocus))
		{
			if(IsSourceActive())
			{
				switch (LOWORD(wParam))
				{
					/*case ID_EDIT_UNDO:
						m_source.SendMessage(SCI_UNDO);
						break;*/
					case ID_EDIT_REDO:
						m_source.SendMessage(SCI_REDO);
						break;
					/*case ID_EDIT_CUT:
						m_source.SendMessage(SCI_CUT);
						break;
					case ID_EDIT_COPY:
						m_source.SendMessage(SCI_COPY);
						break;
					case ID_EDIT_PASTE:
						m_source.SendMessage(SCI_PASTE);
						break;*/
					case ID_EDIT_FIND:
						{
						if(!m_sci_find_dlg)
							m_sci_find_dlg = new CSciFindDlg(&m_doc->m_body, m_source);

						if(m_sci_find_dlg->IsValid())
							break;

						m_sci_find_dlg->UpdatePattern();

						m_sci_find_dlg->ShowDialog();
						break;
						}
					case ID_EDIT_FINDNEXT:
						m_doc->m_body.SciFindNext(m_source, false, true);
						break;
					case ID_EDIT_REPLACE:
						{
							if(!m_sci_replace_dlg)
								m_sci_replace_dlg = new CSciReplaceDlg(&m_doc->m_body, m_source);

							if(m_sci_replace_dlg->IsValid())
							break;

							m_sci_replace_dlg->UpdatePattern();

							m_sci_replace_dlg->ShowDialog();
							break;
						}
				}
			}
			else
				return ActiveView().SendMessage(WM_COMMAND, wParam, 0);
		}

		if(hFocus == m_document_tree.m_hWnd || ::IsChild(m_document_tree.m_hWnd, hFocus))
			return m_doc->m_body.SendMessage(WM_COMMAND,wParam,0);
	}

	// Last chance to send common commands to any focused window
	switch (LOWORD(wParam))
	{
	case ID_EDIT_UNDO:
		::SendMessage(hFocus, WM_UNDO, 0, 0);
		break;
	case ID_EDIT_REDO:
		::SendMessage(hFocus, EM_REDO, 0, 0);
		break;
	case ID_EDIT_CUT:
		::SendMessage(hFocus, WM_CUT, 0, 0);
		break;
	case ID_EDIT_COPY:
		::SendMessage(hFocus, WM_COPY, 0, 0);
		break;
	case ID_EDIT_PASTE:
		::SendMessage(hFocus, WM_PASTE, 0, 0);
		break;
	case ID_EDIT_INS_SYMBOL:
		::SendMessage(hFocus, WM_CHAR, wParam, 0);
		break;
	}

	return 0;
}

LRESULT CMainFrame::OnDropFiles(UINT /* unused: uMsg */, WPARAM wParam, LPARAM /* unused: lParam */, BOOL& /* unused: bHandled */)
{
  HDROP	  hDrop=(HDROP)wParam;
  UINT	  nf=::DragQueryFile(hDrop,0xFFFFFFFF,NULL,0);
  CString buf, ext;
  if (nf>0) {
    UINT    len=::DragQueryFile(hDrop,0,NULL,0);
    TCHAR   *cp=buf.GetBuffer(len+1);
    len=::DragQueryFile(hDrop,0,cp,len+1);
    buf.ReleaseBuffer(len);
  }
  ::DragFinish(hDrop);
  if (!buf.IsEmpty())
  {
	  ext.SetString(ATLPath::FindExtension(buf));
	  if (IsSupportedFictionBookFile(buf) || DetectDocumentContainerKind(buf) != DocumentContainerKind::None)
	  {
		if (LoadFile(buf)==OK)
		{
			if (m_document_session.Location().IsArchive()) FbeRecentDocuments::RememberArchiveMruRecord(m_recentDocuments.List(), m_document_session.Location());
			else FbeRecentDocuments::RememberNormalMruRecord(m_recentDocuments.List(), m_doc->m_filename);
		}
	  }
	  else if ((ext.CompareNoCase(L".JPG") == 0) || (ext.CompareNoCase(L".JPEG") == 0) || (ext.CompareNoCase(L".PNG") == 0))
	  {
		  m_doc->m_body.SetFocus();
		  m_doc->m_body.AddImage(buf, false);
	  }
  }
  return 0;
}

// drag & drop to the BODY window
LRESULT CMainFrame::OnNavigate(WORD, WORD, HWND, BOOL&)
{
  CString   url(m_doc->m_body.NavURL());
  if (!url.IsEmpty())
  {
	  CString ext(ATLPath::FindExtension(url));
	  if (IsSupportedFictionBookFile(url) || DetectDocumentContainerKind(url) != DocumentContainerKind::None)
	  {
		if (LoadFile(url)==OK)
		{
			if (m_document_session.Location().IsArchive()) FbeRecentDocuments::RememberArchiveMruRecord(m_recentDocuments.List(), m_document_session.Location());
			else FbeRecentDocuments::RememberNormalMruRecord(m_recentDocuments.List(), m_doc->m_filename);
		}
	  }
	  else if ((ext.CompareNoCase(L".JPG") == 0) || (ext.CompareNoCase(L".JPEG") == 0) || (ext.CompareNoCase(L".PNG") == 0))
	  {
		  m_doc->m_body.AddImage(url, false);
	  }
  }
  return 0;
}

// commands
LRESULT CMainFrame::OnFileNew(WORD, WORD, HWND, BOOL&)
{
  if (!DiscardChanges())
    return 0;

  DocumentLifecycleController lifecycle(*this, m_doc, m_document_session, m_view);
  if (lifecycle.NewDocument().Succeeded())
  {
	AttachDocument(m_doc);
	ResetStatusForDocument();
  }

  return 0;
}

LRESULT CMainFrame::OnFileOpen(WORD, WORD, HWND, BOOL& /* unused: bHandled */)
{
  if (LoadFile()==OK)
  {
	if (m_document_session.Location().IsArchive()) FbeRecentDocuments::RememberArchiveMruRecord(m_recentDocuments.List(), m_document_session.Location());
	else FbeRecentDocuments::RememberNormalMruRecord(m_recentDocuments.List(), m_doc->m_filename);
	if(_Settings.RestoreFilePosition())
	{
		int saved_pos = U::GetFileSelectedPos(m_doc->m_filename);
		GoTo(saved_pos);
	}
  }
  return 0;
}

LRESULT CMainFrame::OnFileOpenMRU(WORD /* unused: wNotifyCode */, WORD wID, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	CString filename;
	DocumentLocation archiveLocation;
	bool archiveMru = false;
	if (!m_recentDocuments.Resolve(wID, filename, archiveLocation, archiveMru)) return 0;

	const FILE_OP_STATUS result = archiveMru ? LoadFile(archiveLocation.storagePath, &archiveLocation) : LoadFile(filename);
	const DocumentLocation location = archiveMru ? archiveLocation : DocumentLocation();
	const DocumentLifecycleResult lifecycle = result == OK ? DocumentLifecycleController::Completed(location, archiveMru) :
		result == CANCELLED ? DocumentLifecycleController::Cancelled(location, archiveMru) :
		DocumentLifecycleController::Failed(location, archiveMru);
	switch(lifecycle.status)
	{
		case DocumentLifecycleStatus::Success:
			m_recentDocuments.OnOpened(wID, filename, archiveLocation, archiveMru);
			// added by SeNS
			if(_Settings.RestoreFilePosition())
			{
				int saved_pos = U::GetFileSelectedPos(m_doc->m_filename);
				GoTo(saved_pos);
			}
			break;
		case DocumentLifecycleStatus::Failed:
			m_recentDocuments.OnFailed(wID);
			break;
		case DocumentLifecycleStatus::Cancelled:
			if (archiveMru) m_recentDocuments.OnCancelledArchive(archiveLocation);
			break;
	}

	return 0;
}

LRESULT CMainFrame::OnFileSave(WORD, WORD, HWND, BOOL&)
{
  SaveFile(false);
  return 0;
}

LRESULT CMainFrame::OnFileSaveAs(WORD, WORD, HWND, BOOL&)
{
  SaveFile(true);
  return 0;
}

LRESULT CMainFrame::OnViewToolBar(WORD, WORD wID, HWND, BOOL&)
{
  int nBandIndex = m_rebar.IdToIndex(wID);
  BOOL bVisible = !IsBandVisible(wID);
  m_rebar.ShowBand(nBandIndex, bVisible);
  UISetCheck(wID, bVisible);

  if(wID == 60164 || wID == 60165)
  {
	if (wID == 60164) wID++; else wID--;
	nBandIndex = m_rebar.IdToIndex(wID);
	m_rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(wID, bVisible);
  }

  UpdateLayout();
  return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD, WORD, HWND, BOOL&)
{
  BOOL bVisible = !m_status.IsWindowVisible();
  ::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
  UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
  UpdateLayout();
  return 0;
}

LRESULT CMainFrame::OnViewFastMode(WORD, WORD, HWND, BOOL&)
{
	bool mode = m_doc->GetFastMode();
    mode = !mode;
	m_doc->SetFastMode(mode);
	_Settings.SetFastMode(m_doc->GetFastMode(), true);
	UISetCheck(ID_VIEW_FASTMODE, mode);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnViewTree(WORD, WORD, HWND, BOOL&)
{
	if(IsSourceActive())
		return 0;

	BOOL bVisible = !_Settings.ViewDocumentTree();
	m_document_tree.ShowWindow(bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	if(bVisible)
		m_document_tree.GetDocumentStructure(m_doc->m_body.Document());
	UISetCheck(ID_VIEW_TREE, bVisible);
	m_splitter.SetSinglePaneMode(bVisible ? SPLIT_PANE_NONE : SPLIT_PANE_RIGHT);
	_Settings.SetViewDocumentTree(bVisible != 0 , TRUE);

	return 0;
}

LRESULT CMainFrame::OnViewOptions(WORD, WORD, HWND, BOOL&)
{
	const DWORD previousInterfaceLanguage = _Settings.GetInterfaceLanguageID();
	const bool previousShowFullPathInWindowTitle = _Settings.GetShowFullPathInWindowTitle();
	const EditorConfigurationSnapshot previousConfiguration = CaptureEditorConfigurationSnapshot();
	bool bFind = m_doc->m_body.CloseFindDialog(m_doc->m_body.m_find_dlg);
	bool bReplace = m_doc->m_body.CloseFindDialog(m_doc->m_body.m_replace_dlg);

	bool bSciFind = m_doc->m_body.CloseFindDialog(m_sci_find_dlg);
	bool bSciRepl = m_doc->m_body.CloseFindDialog(m_sci_replace_dlg);

	int find_repl = (bFind || bSciFind) ? 1 : ((bReplace || bSciRepl) ? 2 : 0);

	if(ShowSettingsDialog(m_hWnd))
	{
		if(previousInterfaceLanguage != _Settings.GetInterfaceLanguageID())
		{
			FbeResetRuntimeLocalization();
			RefreshLocalizedMainFrameUi();
		}
		if (previousShowFullPathInWindowTitle != _Settings.GetShowFullPathInWindowTitle())
			m_need_title_update = true;

		const EditorConfigurationSnapshot currentConfiguration = CaptureEditorConfigurationSnapshot();
		if (!(previousConfiguration == currentConfiguration) || _Settings.NeedRestart())
		{
			if (HasOnlyEditorBackgroundConfigurationChanged(previousConfiguration, currentConfiguration))
			{
				ApplyEditorBackgroundChanges();
			}
			else if (HasOnlySourceEditorConfigurationChanged(previousConfiguration, currentConfiguration))
			{
				ApplyXmlSourceEditorChanges();
			}
			else
			{
				ApplyConfChanges(HasDocumentStyleConfigurationChanged(previousConfiguration, currentConfiguration));
			}
		}
		else
		{
			// Окно общих настроек не меняет hotkey- или word-коллекции.
			// Их XML-сериализация здесь не нужна и могла аварийно завершиться
			// на старых пользовательских настройках при смене только языка.
			_Settings.Save();
		}
	}

	switch(find_repl)
	{
	case 1:
		SendMessage(WM_COMMAND, ID_EDIT_FIND, NULL);
		break;
	case 2:
		SendMessage(WM_COMMAND, ID_EDIT_REPLACE, NULL);
		break;
	}

	return 0;
}

LRESULT CMainFrame::OnToolsImport(WORD, WORD wID, HWND, BOOL&) {
  wID-=ID_IMPORT_BASE;
  if (wID<m_plugins.ImportPlugins().GetSize()) {
    const CLSID& pluginClsid = m_plugins.ImportPlugins()[wID];
    const PluginImportResult execution = m_plugin_execution.Import(m_plugins.Manager(),
      pluginClsid, m_hWnd, _Settings.GetInterfaceLanguageName());
    if (!execution.Succeeded()) {
      if (execution.failure != PluginExecutionFailure::InterfaceUnavailable &&
        execution.failure != PluginExecutionFailure::ResultStream)
        U::ReportError(execution.hr);
      return 0;
    }
    m_plugins.SetLastCommand(wID + ID_IMPORT_BASE);
    try {
      const MSXML2::IXMLDOMDocument2Ptr& dom = execution.document;
      if (!(bool)dom)
	  {
		U::MessageBox(MB_OK|MB_ICONERROR, IDS_ERRMSGBOX_CAPTION, IDS_IMPORT_XML_ERR_MSG);
	  }
      else if (DiscardChanges())
	  {
		/*FB::Doc *doc=new FB::Doc(*this);
		FB::Doc::m_active_doc = doc;*/

		//if (doc->LoadFromDOM(m_view,dom)) {
		CComDispatchDriver	body(m_doc->m_body.Script());
		CComVariant		    args[2];
		CComVariant		    res;
		args[1]=dom.GetInterfacePtr();
		args[0] = _Settings.GetInterfaceLanguageName();
		CheckError(body.InvokeN(L"LoadFromDOM", args, 2, &res));
		if(res.boolVal)
		//if (doc->LoadFromHTML(m_view,(const wchar_t* )filename))
		{
			if (!execution.suggestedFilename.IsEmpty())
			{
				m_doc->m_filename=execution.suggestedFilename;
				U::SetCurrentDirectoryToFile(execution.suggestedFilename);
				if (m_doc->m_filename.GetLength()<4 || m_doc->m_filename.Right(4).CompareNoCase(_T(".fb2"))!=0)
				m_doc->m_filename+=_T(".fb2");
				m_doc->m_namevalid=true;
			}
			/*AttachDocument(doc);
			delete m_doc;
			m_doc=doc;*/
			m_doc->m_body.Init();
			m_doc->ResetSavePoint();
		}// else
			//FB::Doc::m_active_doc = m_doc;
		//delete doc;
	  }
    }
    catch (_com_error& e) {
      U::ReportError(e);
    }
  }
  return 0;
}

LRESULT CMainFrame::OnToolsExport(WORD, WORD wID, HWND, BOOL&)
{
	wID -= ID_EXPORT_BASE;
	if(wID<m_plugins.ExportPlugins().GetSize())
	{
		const CLSID& pluginClsid = m_plugins.ExportPlugins()[wID];
		try
		{
			PluginExportRequest request;
			request.clsid = pluginClsid; request.owner = m_hWnd;
			request.interfaceLanguage = _Settings.GetInterfaceLanguageName();
			request.document = m_doc->CreateDOM(m_doc->m_encoding, false);
			request.sourceFilename = m_doc->m_namevalid ? m_doc->m_filename.GetString() : L"";
			request.documentEncoding = m_doc->m_encoding;
			const PluginExecutionResult execution = m_plugin_execution.Export(m_plugins.Manager(), request);
			if (!execution.Succeeded()) {
				if (execution.failure != PluginExecutionFailure::InterfaceUnavailable)
					U::ReportError(execution.hr);
				return 0;
			}
			m_plugins.SetLastCommand(wID + ID_EXPORT_BASE);
			return 0;
		}
		catch(_com_error& e)
		{
			U::ReportError(e);
		}
	}
	return 0;
}

LRESULT CMainFrame::OnLastPlugin(WORD, WORD /* unused: wID */, HWND, BOOL&)
{
	if(m_plugins.LastCommand())
		::SendMessage(m_hWnd, WM_COMMAND, m_plugins.LastCommand(), NULL);
	return 0;
}

LRESULT CMainFrame::OnToolsWords(WORD, WORD, HWND, BOOL&)
{
	if(IsSourceActive())
		ShowView(BODY);

	if(m_Speller)
		m_Speller->EndDocumentCheck();

	bool bFind = m_doc->m_body.CloseFindDialog(m_doc->m_body.m_find_dlg);
	bool bReplace = m_doc->m_body.CloseFindDialog(m_doc->m_body.m_replace_dlg);

	int find_repl = bFind ? 1 : (bReplace ? 2 : 0);
	ShowWordsDialog(*m_doc, m_hWnd);

	switch(find_repl)
	{
	case 1:
		SendMessage(WM_COMMAND, ID_EDIT_FIND, NULL);
		break;
	case 2:
		SendMessage(WM_COMMAND, ID_EDIT_REPLACE, NULL);
		break;
	}

	return 0;
}

LRESULT CMainFrame::OnToolsOptions(WORD, WORD, HWND, BOOL&)
{
	const DWORD previousInterfaceLanguage = _Settings.GetInterfaceLanguageID();
	const bool previousShowFullPathInWindowTitle = _Settings.GetShowFullPathInWindowTitle();
	const EditorConfigurationSnapshot previousConfiguration = CaptureEditorConfigurationSnapshot();
	if(m_Speller)
		m_Speller->EndDocumentCheck();

	bool bFind = m_doc->m_body.CloseFindDialog(m_doc->m_body.m_find_dlg);
	bool bReplace = m_doc->m_body.CloseFindDialog(m_doc->m_body.m_replace_dlg);

	bool bSciFind = m_doc->m_body.CloseFindDialog(m_sci_find_dlg);
	bool bSciRepl = m_doc->m_body.CloseFindDialog(m_sci_replace_dlg);

	int find_repl = (bFind || bSciFind) ? 1 : ((bReplace || bSciRepl) ? 2 : 0);

	if(ShowSettingsDialog(m_hWnd))
	{
		if(previousInterfaceLanguage != _Settings.GetInterfaceLanguageID())
		{
			FbeResetRuntimeLocalization();
			RefreshLocalizedMainFrameUi();
		}
		if (previousShowFullPathInWindowTitle != _Settings.GetShowFullPathInWindowTitle())
			m_need_title_update = true;

		const EditorConfigurationSnapshot currentConfiguration = CaptureEditorConfigurationSnapshot();
		if (!(previousConfiguration == currentConfiguration) || _Settings.NeedRestart())
		{
			if (HasOnlyEditorBackgroundConfigurationChanged(previousConfiguration, currentConfiguration))
			{
				ApplyEditorBackgroundChanges();
			}
			else if (HasOnlySourceEditorConfigurationChanged(previousConfiguration, currentConfiguration))
			{
				ApplyXmlSourceEditorChanges();
			}
			else
			{
				ApplyConfChanges(HasDocumentStyleConfigurationChanged(previousConfiguration, currentConfiguration));
			}
		}
		else
		{
			// См. аналогичный путь OnViewOptions: сохраняем собственно
			// настройки, но не сериализуем не затронутые этим диалогом XML-коллекции.
			_Settings.Save();
		}
	}

	switch(find_repl)
	{
	case 1:
		SendMessage(WM_COMMAND, ID_EDIT_FIND, NULL);
		break;
	case 2:
		SendMessage(WM_COMMAND, ID_EDIT_REPLACE, NULL);
		break;
	}

	return 0;
}

LRESULT CMainFrame::OnToolsOpenDiagnosticLog(WORD, WORD, HWND, BOOL&)
{
	if(!m_diagnostic_commands.OpenCurrentLog())
	{
		::MessageBox(m_hWnd,
			GetDiagnosticTraceText(L"fbe.trace.open_failed", L"Не удалось открыть диагностический журнал."),
			GetDiagnosticTraceText(L"fbe.trace.caption", L"Диагностический журнал"), MB_OK | MB_ICONERROR);
	}
	return 0;
}

LRESULT CMainFrame::OnToolsOpenDiagnosticFolder(WORD, WORD, HWND, BOOL&)
{
	if (!m_diagnostic_commands.OpenLogFolder())
		::MessageBox(m_hWnd, GetDiagnosticTraceText(L"fbe.trace.open_folder_failed", L"Could not open the diagnostic log folder."),
			GetDiagnosticTraceText(L"fbe.trace.caption", L"Diagnostic trace"), MB_OK | MB_ICONERROR);
	return 0;
}

LRESULT CMainFrame::OnToolsCopyDiagnosticLogPath(WORD, WORD, HWND, BOOL&)
{
	const CString currentLogPath(m_diagnostic_commands.CurrentLogPath());
	bool copied = !currentLogPath.IsEmpty() && ::OpenClipboard(m_hWnd);
	if (copied)
	{
		::EmptyClipboard();
		const SIZE_T bytes = (static_cast<SIZE_T>(currentLogPath.GetLength()) + 1) * sizeof(wchar_t);
		HGLOBAL data = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
		void* target = data ? ::GlobalLock(data) : NULL;
		if (!target) { if (data) ::GlobalFree(data); copied = false; }
		else {
			memcpy(target, static_cast<LPCWSTR>(currentLogPath), bytes); ::GlobalUnlock(data);
			copied = ::SetClipboardData(CF_UNICODETEXT, data) != NULL;
			if (!copied) ::GlobalFree(data);
		}
		::CloseClipboard();
	}
	if (!copied)
		::MessageBox(m_hWnd, GetDiagnosticTraceText(L"fbe.trace.copy_path_failed", L"Could not copy the diagnostic log path."),
			GetDiagnosticTraceText(L"fbe.trace.caption", L"Diagnostic trace"), MB_OK | MB_ICONERROR);
	return 0;
}

LRESULT CMainFrame::OnToolsClearDiagnosticLogs(WORD, WORD, HWND, BOOL&)
{
	const CString caption(GetDiagnosticTraceText(L"fbe.trace.caption", L"Diagnostic trace"));
	if (::MessageBox(m_hWnd, GetDiagnosticTraceText(L"fbe.trace.clear_confirmation", L"Clear old diagnostic logs? The current log will be preserved."), caption, MB_YESNO | MB_ICONQUESTION) != IDYES)
		return 0;
	const StartupTrace::DiagnosticLogCleanupResult cleanup = m_diagnostic_commands.ClearOldLogSessions();
	if (cleanup.sessionsFound == 0 && cleanup.filesFailed == 0)
	{
		StartupTrace::Event(L"diagnostic", L"DG122", L"no old trace sessions found");
		::MessageBox(m_hWnd, GetDiagnosticTraceText(L"fbe.trace.clear_empty", L"No old diagnostic logs were found."), caption, MB_OK | MB_ICONINFORMATION);
	}
	else if (cleanup.filesFailed == 0 && cleanup.sessionsPartiallyDeleted == 0 && cleanup.sessionsFailed == 0)
	{
		CString details; details.Format(L"sessions-found=%u; sessions-fully-deleted=%u; sessions-partially-deleted=%u; sessions-failed=%u; files-deleted=%u", cleanup.sessionsFound, cleanup.sessionsFullyDeleted, cleanup.sessionsPartiallyDeleted, cleanup.sessionsFailed, cleanup.filesDeleted);
		StartupTrace::Event(L"diagnostic", L"DG120", details);
		CString message; message.Format(GetDiagnosticTraceText(L"fbe.trace.clear_completed_details", L"Deleted %u diagnostic sessions (%u files)."), cleanup.sessionsFullyDeleted, cleanup.filesDeleted);
		::MessageBox(m_hWnd, message, caption, MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		CString details; details.Format(L"sessions-fully-deleted=%u; sessions-partially-deleted=%u; sessions-failed=%u; files-deleted=%u; files-failed=%u; win32-error=%lu", cleanup.sessionsFullyDeleted, cleanup.sessionsPartiallyDeleted, cleanup.sessionsFailed, cleanup.filesDeleted, cleanup.filesFailed, static_cast<unsigned long>(cleanup.lastError));
		StartupTrace::Error(L"diagnostic", L"DG121", details);
		const bool partiallyDeleted = cleanup.sessionsFullyDeleted != 0 || cleanup.sessionsPartiallyDeleted != 0 || cleanup.filesDeleted != 0;
		CString message;
		if (partiallyDeleted)
			message.Format(GetDiagnosticTraceText(L"fbe.trace.clear_partial", L"Fully deleted sessions: %u\nPartially deleted sessions: %u\nFailed sessions: %u\nDeleted files: %u\nFailed files: %u."), cleanup.sessionsFullyDeleted, cleanup.sessionsPartiallyDeleted, cleanup.sessionsFailed, cleanup.filesDeleted, cleanup.filesFailed);
		else
			message.Format(GetDiagnosticTraceText(L"fbe.trace.clear_delete_failed", L"Could not delete %u diagnostic log files; Win32 error %lu."), cleanup.filesFailed, static_cast<unsigned long>(cleanup.lastError));
		::MessageBox(m_hWnd, message, caption, MB_OK | MB_ICONERROR);
	}
	return 0;
}
LRESULT CMainFrame::OnToolsCreateDiagnosticPackage(WORD, WORD, HWND, BOOL&)
{
	CString packagePath, error;
	const CString caption(GetDiagnosticTraceText(L"fbe.trace.caption", L"Diagnostic trace"));
	if (::MessageBox(m_hWnd, GetDiagnosticTraceText(L"fbe.trace.package_confirmation", L"Create a diagnostic package?\n\nIt includes selected diagnostic logs, environment and FBELib information, and a matching technical crash report when available.\n\nIt never includes books, book text, XML/HTML, settings, recovery files, user scripts, images, or Base64 data."), caption, MB_YESNO | MB_ICONQUESTION) != IDYES)
		return 0;
	if (!m_diagnostic_commands.CreatePackage(packagePath, error))
	{
		StartupTrace::Error(L"diagnostic", L"DG131", CString(L"diagnostic package creation failed: ") + StartupTrace::SanitizeLogText(error, 256));
		LPCWSTR key = L"fbe.trace.package_write_failed";
		LPCWSTR fallback = L"Could not write the diagnostic package.";
		if (error.Find(L"No diagnostic trace session") >= 0 || error.Find(L"trace session could not") >= 0) { key = L"fbe.trace.package_no_session"; fallback = L"No diagnostic trace session is available."; }
		else if (error.Find(L"Privacy scan rejected") >= 0) { key = L"fbe.trace.package_privacy_rejected"; fallback = L"The diagnostic package was not created because its privacy check rejected diagnostic content."; }
		::MessageBox(m_hWnd, GetDiagnosticTraceText(key, fallback), caption, MB_OK | MB_ICONERROR);
		return 0;
	}
	CString message; message.Format(GetDiagnosticTraceText(L"fbe.trace.package_created", L"Diagnostic package created:\n%s"), (LPCWSTR)packagePath);
	::MessageBox(m_hWnd, message, caption, MB_OK | MB_ICONINFORMATION);
	return 0;
}
LRESULT CMainFrame::OnToolsDiagnosticTrace(WORD, WORD, HWND, BOOL&)
{
	const bool enabled = m_diagnostic_commands.IsEnabledForNextLaunch();
	const CString caption(GetDiagnosticTraceText(L"fbe.trace.caption", L"Диагностический журнал"));
	if(enabled)
	{
		::MessageBox(m_hWnd,
			GetDiagnosticTraceText(L"fbe.trace.already_enabled",
				L"Диагностический журнал уже включён для следующих запусков FBE Next. После перезапуска программа предупредит о диагностическом режиме и предложит его отключить."),
			caption, MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	const CString question(GetDiagnosticTraceText(L"fbe.trace.enable.question",
			L"Диагностический журнал содержит технические сведения о запуске, командах и ошибках COM. Текст книги, XML, HTML, Base64 и содержимое пользовательских сценариев не записываются; пути обезличиваются.\n\n"
			L"Включить его для следующего запуска FBE Next? Для начала записи потребуется перезапустить программу."));
	if(::MessageBox(m_hWnd, question, caption, MB_YESNO | MB_ICONQUESTION) != IDYES)
		return 0;

	if(!m_diagnostic_commands.SetEnabledForNextLaunch(true))
	{
		::MessageBox(m_hWnd,
			GetDiagnosticTraceText(L"fbe.trace.change_failed",
				L"Не удалось изменить настройку диагностического журнала."),
			caption, MB_OK | MB_ICONERROR);
		return 0;
	}

	const CString result(GetDiagnosticTraceText(L"fbe.trace.enable.completed",
		L"Диагностический журнал включён. Перезапустите FBE Next, чтобы начать запись."));
	::MessageBox(m_hWnd, result, caption, MB_OK | MB_ICONINFORMATION);
	return 0;
}

LRESULT CMainFrame::OnToolsScript(WORD /* unused: wNotifyCode */, WORD wID, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	wID -= ID_SCRIPT_BASE;

	if(IsSourceActive())
		return 0;

  // ??????? ?? FBE ? ?? FBW ??????????? ?? ???????. ? FBE ??????? ??????????? ????? Active Scripting
  // ? ???????? ? ???? ??????????? ????? ?????????.
  // ? FBW ??????? ??????????? ? ????? HTML ?????????
	for(int i = 0; i < m_scripts.Menu().Count(); ++i)
	{
		if(m_scripts.Menu().Item(i).commandId == -1) continue;

		if(!m_scripts.Menu().Item(i).isFolder && m_scripts.Menu().Item(i).commandId == wID)
		{
			m_doc->RunScript(m_scripts.Menu().Item(i).path);
			m_scripts.SetLastScript(m_scripts.Menu().Item(i));
			break;
		}
	}

  return 0;
}

LRESULT CMainFrame::OnEditInsSymbol(WORD /* unused: wNotifyCode */, WORD wID, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	static int symHkGroup = -1;
	if(symHkGroup == -1)
	{
		for(unsigned int i = 0; i < _Settings.m_hotkey_groups.size(); ++i)
		{
			if(_Settings.m_hotkey_groups[i].m_reg_name == L"Symbols")
			{
				symHkGroup = i;
				break;
			}
		}
	}
	std::vector<CHotkey>& symHotkeys = _Settings.m_hotkey_groups[symHkGroup].m_hotkeys;

	wchar_t c = NULL;
	for(unsigned int i = 0; i < symHotkeys.size(); ++i)
	{
		if(symHotkeys[i].m_accel.cmd == wID)
		{
			c = symHotkeys[i].m_char_val;
			break;
		}
	}

	if(c)
	{
		::SendMessage(::GetFocus(), WM_CHAR, c, NULL);

		/*IServiceProviderPtr ServiceProvider;
		ServiceProvider = m_doc->m_body.Browser();
		if(ServiceProvider)
		{
			IOleWindowPtr Window = NULL;
			if(SUCCEEDED(ServiceProvider->QueryService(SID_SShellBrowser, IID_IOleWindow, (void**)&Window)))
			{
				HWND hwndBrowser = NULL;
				if (SUCCEEDED(Window->GetWindow(&hwndBrowser)))
				{
					while(::GetWindow(hwndBrowser, GW_CHILD))
						hwndBrowser = ::GetWindow(hwndBrowser, GW_CHILD);
					::SendMessage(hwndBrowser, WM_CHAR, c, 0);
				}
			}
		}*/
	}

	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD, WORD, HWND, BOOL&)
{
  CAboutDlg dlg;
  dlg.DoModal();
  return 0;
}

// Navigation
LRESULT CMainFrame::OnSelectCtl(WORD /* unused: wNotifyCode */, WORD wID, HWND /* unused: hWndCtl */, BOOL& bHandled)
{
	switch(wID)
	{
		case ID_SELECT_TREE:
			if(!m_document_tree.IsWindowVisible())
				OnViewTree(0, 0, 0, bHandled);
			m_document_tree.m_tree.m_tree.SetFocus();
			break;
		case ID_SELECT_ID:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 3))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 3, NULL, bHandled);
			m_contextAttributeBars.FocusLinkField(LinkAttributeField::Id);
			break;
			case ID_SELECT_HREF:
			{
				if(!IsBandVisible(ATL_IDW_BAND_FIRST + 3))
					OnViewToolBar(0,ATL_IDW_BAND_FIRST + 3, NULL, bHandled);
				m_contextAttributeBars.FocusLinkField(LinkAttributeField::Href, true);
				break;
			}
		case ID_SELECT_IMAGE:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 3))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 3, NULL, bHandled);
			m_contextAttributeBars.FocusLinkField(LinkAttributeField::ImageTitle);
			break;
		case ID_SELECT_TEXT:
			m_view.SetFocus();
			break;
		case ID_SELECT_SECTION:
			if (!IsBandVisible(ATL_IDW_BAND_FIRST + 3))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 3, NULL, bHandled);
			m_contextAttributeBars.FocusLinkField(LinkAttributeField::Section);
			break;
		case ID_SELECT_IDT:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_contextAttributeBars.FocusTableField(TableAttributeField::TableId);
			break;
		case ID_SELECT_STYLET:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_contextAttributeBars.FocusTableField(TableAttributeField::TableStyle);
			break;
		case ID_SELECT_STYLE:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_contextAttributeBars.FocusTableField(TableAttributeField::CellStyle);
			break;
		case ID_SELECT_COLSPAN:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_contextAttributeBars.FocusTableField(TableAttributeField::Colspan);
			break;
		case ID_SELECT_ROWSPAN:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_contextAttributeBars.FocusTableField(TableAttributeField::Rowspan);
			break;
		case ID_SELECT_ALIGNTR:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
			OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_contextAttributeBars.FocusTableField(TableAttributeField::RowAlign);
			break;
		case ID_SELECT_ALIGN:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_contextAttributeBars.FocusTableField(TableAttributeField::Align);
			break;
		case ID_SELECT_VALIGN:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_contextAttributeBars.FocusTableField(TableAttributeField::VAlign);
			break;
	}

	return 0;
}

LRESULT CMainFrame::OnNextItem(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
  ShowView(NextEditorView());
  return 1;
}

// editor notifications
LRESULT CMainFrame::OnCbEdChange(WORD /* unused: code */, WORD wID, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
  if (m_ignore_cb_changes)
    return 0;

  try {
	const LinkAttributeState linkState = m_contextAttributeBars.GetLinkState();
	const TableAttributeState tableState = m_contextAttributeBars.GetTableState();
    if (wID==IDC_HREF) {
      MSHTML::IHTMLElementPtr an(m_doc->m_body.SelectionAnchor());
      _variant_t    href;
      if (an)
		href=an->getAttribute(L"href",2);
      if ((bool)an && V_VT(&href)==VT_BSTR) {
		CString	    newhref(linkState.href);

		// changed by SeNS: href's fix - by default internal hrefs begins from '#'
		// otherwise set http protocol (if no other protocols specified)
		if (!newhref.IsEmpty() && (newhref[0] != L'#'))
		{
			if (newhref.Find (L"://") < 0)
				newhref = L"http://" + newhref;
		}

		if ( (U::scmp(an->tagName,L"DIV")==0) || (U::scmp(an->tagName,L"SPAN")==0)) // must be an image
		{
			U::ChangeAttribute(an, L"href", newhref);
			MSHTML::IHTMLElementPtr img = MSHTML::IHTMLDOMNodePtr(an)->firstChild;
			m_doc->m_body.ImgSetURL(img, newhref);
			IHTMLControlRangePtr r(((MSHTML::IHTMLElement2Ptr)(m_doc->m_body.Document()->body))->createControlRange());
			r->add((IHTMLControlElementPtr)img->parentElement);
			r->select();
		}
		else
		{
			U::ChangeAttribute(an, L"href", newhref);
			MSHTML::IHTMLTxtRangePtr r = m_doc->m_body.Document()->selection->createRange();
			r->moveToElementText(an);
			r->select();
		}
      } else {
		m_contextAttributeBars.ClearLinkState();
		m_contextAttributeBars.SetLinkAvailability(LinkAttributeAvailability{ false, false, false, false });
      }
    }
    if (wID==IDC_ID) {
      MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionStructCon());
      if (sc)
		sc->id=(const wchar_t *)linkState.id;
      else
		m_contextAttributeBars.SetLinkAvailability(LinkAttributeAvailability{ false, false, false, false });
    }
	if (wID==IDC_SECTION) {
		MSHTML::IHTMLElementPtr		scs(m_doc->m_body.SelectionStructSection());
		if (scs)
			scs->id=(const wchar_t *)linkState.section;
		else
			m_contextAttributeBars.SetLinkAvailability(LinkAttributeAvailability{ false, false, false, false });
	}

	if (wID==IDC_IMAGE_TITLE) {
		MSHTML::IHTMLElementPtr		scs(m_doc->m_body.SelectionStructImage());
		if (scs)
		{
			U::ChangeAttribute(scs, L"title", (const wchar_t *)linkState.imageTitle);

			IHTMLControlRangePtr r(((MSHTML::IHTMLElement2Ptr)(m_doc->m_body.Document()->body))->createControlRange());
			r->add((IHTMLControlElementPtr)scs);
			r->select();
		}
		else
			m_contextAttributeBars.SetLinkAvailability(LinkAttributeAvailability{ false, false, false, false });
	}

	if (wID==IDC_IDT) {
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionStructTable());
		if (sc)
			sc->id=(const wchar_t *)tableState.tableId;
		else
			m_contextAttributeBars.SetTableAvailability(TableAttributeAvailability{ false, false, false, false, false, false, false, false, false });
	}
	if (wID==IDC_ID) {
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionStructTableCon());
		if (sc)
			sc->id=(const wchar_t *)tableState.id;
		else
			m_contextAttributeBars.SetTableAvailability(TableAttributeAvailability{ false, false, false, false, false, false, false, false, false });
	}
	if (wID==IDC_STYLET) {
		_bstr_t style("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsStyleTB(style));
		if (sc){
			CString	    newsSyleT(tableState.tableStyle);
			sc->setAttribute(L"fbstyle",_variant_t((const wchar_t *)newsSyleT),0);
		}
		else
			m_contextAttributeBars.SetTableAvailability(TableAttributeAvailability{ false, false, false, false, false, false, false, false, false });
	}
	if (wID==IDC_STYLE) {
		_bstr_t style("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsStyleB(style));
		if (sc){
			CString	    newsSyle(tableState.style);
			sc->setAttribute(L"fbstyle",_variant_t((const wchar_t *)newsSyle),0);
		}
		else
			m_contextAttributeBars.SetTableAvailability(TableAttributeAvailability{ false, false, false, false, false, false, false, false, false });
	}
	if (wID==IDC_COLSPAN) {
		_bstr_t colspan("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsColspanB(colspan));
		if (sc){
			CString	    newsColspan(tableState.colspan);
			sc->setAttribute(L"fbcolspan",_variant_t((const wchar_t *)newsColspan),0);
		}
		else
			m_contextAttributeBars.SetTableAvailability(TableAttributeAvailability{ false, false, false, false, false, false, false, false, false });
	}
	if (wID==IDC_ROWSPAN) {
		_bstr_t rowspan("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsRowspanB(rowspan));
		if (sc){
			CString	    newsRowspan(tableState.rowspan);
			sc->setAttribute(L"fbrowspan",_variant_t((const wchar_t *)newsRowspan),0);
		}
		else
			m_contextAttributeBars.SetTableAvailability(TableAttributeAvailability{ false, false, false, false, false, false, false, false, false });
	}
	if (wID==IDC_ALIGNTR) {
		_bstr_t alignTR("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsAlignTRB(alignTR));
		if (sc){
			CString	    newsAlignTR(tableState.rowAlign);
			sc->setAttribute(L"fbalign",_variant_t((const wchar_t *)newsAlignTR),0);
		}
		else
			m_contextAttributeBars.SetTableAvailability(TableAttributeAvailability{ false, false, false, false, false, false, false, false, false });
	}
	if (wID==IDC_ALIGN) {
		_bstr_t align("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsAlignB(align));
		if (sc){
			CString	    newsAlign(tableState.align);
			sc->setAttribute(L"fbalign",_variant_t((const wchar_t *)newsAlign),0);
		}
		else
			m_contextAttributeBars.SetTableAvailability(TableAttributeAvailability{ false, false, false, false, false, false, false, false, false });
	}
	if (wID==IDC_VALIGN) {
		_bstr_t valign("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsVAlignB(valign));
		if (sc){
			CString	    newsVAlign(tableState.valign);
			sc->setAttribute(L"fbvalign",_variant_t((const wchar_t *)newsVAlign),0);
		}
		else
			m_contextAttributeBars.SetTableAvailability(TableAttributeAvailability{ false, false, false, false, false, false, false, false, false });
	}
  }
  catch (_com_error&) { }

  return 0;
}

// tree view notifications
LRESULT CMainFrame::OnTreeReturn(WORD, WORD, HWND, BOOL&)
{
  GoToSelectedTreeItem();
  return 0;
}

LRESULT CMainFrame::OnTreeUpdate(WORD, WORD, HWND, BOOL&)
{
  GetDocumentStructure();
  return 0;
}

LRESULT CMainFrame::OnTreeRestore(WORD, WORD, HWND, BOOL& /* unused: b */)
{
  m_document_tree.GetDocumentStructure(m_doc->m_body.Document());
  return 0;
}

LRESULT CMainFrame::OnTreeMoveElement(WORD, WORD, HWND, BOOL&)
{
	m_doc->m_body.BeginUndoUnit(L"structure editing");
	CTreeItem from = m_document_tree.m_tree.m_tree.GetMoveElementFrom();
	CTreeItem to = m_document_tree.m_tree.m_tree.GetMoveElementTo();

	MSHTML::IHTMLElementPtr elemFrom = (MSHTML::IHTMLElement *)from.GetData();
	MSHTML::IHTMLElementPtr elemTo;

	MSHTML::IHTMLDOMNodePtr nodeFrom =(MSHTML::IHTMLDOMNodePtr)elemFrom;
	MSHTML::IHTMLDOMNodePtr nodeTo;
	MSHTML::IHTMLDOMNodePtr nodeInsertBefore;

	switch(m_document_tree.m_tree.m_tree.m_insert_type)
	{
	case CTreeView::child:
		{
			elemTo = (MSHTML::IHTMLElement *)to.GetData();
			nodeTo =(MSHTML::IHTMLDOMNodePtr)elemTo;
			nodeInsertBefore = nodeTo->firstChild;
			break;
		}

	case CTreeView::sibling:
		{
			elemTo = (MSHTML::IHTMLElement *)to.GetData();
			nodeTo =(MSHTML::IHTMLDOMNodePtr)elemTo;
			nodeInsertBefore = nodeTo->nextSibling;
			nodeTo = nodeTo->parentNode;
			break;
		}

	case CTreeView::none:
		{
			m_doc->m_body.EndUndoUnit();
			return 0;
		}
	}

	if(!IsNodeSection(nodeFrom) || !IsNodeSection(nodeTo))
	{
		m_doc->m_body.EndUndoUnit();
		return 0;
	}

	if(!IsEmptySection(nodeTo))
	{
		MSHTML::IHTMLDOMNodePtr new_section = CreateNestedSection(nodeTo);
		if(!bool(new_section))
		{
			m_doc->m_body.EndUndoUnit();
			return 0;
		}

		nodeInsertBefore = new_section->nextSibling;
	}
	m_doc->MoveNode(nodeFrom, nodeTo, nodeInsertBefore);
	m_document_tree.UpdateDocumentStructure(m_doc->m_body.Document(), nodeTo);
	m_doc->m_body.EndUndoUnit();
	return 0;
}

LRESULT CMainFrame::OnTreeMoveElementOne(WORD, WORD, HWND, BOOL&)
{
	m_doc->m_body.BeginUndoUnit(L"structure editing");
	CTreeItem item = m_document_tree.m_tree.m_tree.GetFirstSelectedItem();
	MSHTML::IHTMLElementPtr elem = 0;
	MSHTML::IHTMLDOMNodePtr ret_node = 0;

	do
	{
		if(item.IsNull())
			break;


		if(!item.GetData() || !(bool)(elem = (IHTMLElement*) item.GetData()))
			continue;

		MSHTML::IHTMLDOMNodePtr node = (MSHTML::IHTMLDOMNodePtr)elem;
		if(!(bool)node)
			continue;

		ret_node = MoveRightElementWithoutChildren(node);
	}while(item = m_document_tree.m_tree.m_tree.GetNextSelectedItem(item));

	GetDocumentStructure();
	if((bool)ret_node)
	{
		MSHTML::IHTMLElementPtr movedElement(ret_node);
		m_document_tree.m_tree.m_tree.SelectElement(movedElement);
		GoTo(movedElement);
	}

	m_doc->m_body.EndUndoUnit();
	return 0;
}

LRESULT CMainFrame::OnTreeMoveLeftElement(WORD, WORD, HWND, BOOL&)
{
	m_doc->m_body.BeginUndoUnit(L"structure editing");
	CTreeItem item = m_document_tree.m_tree.m_tree.GetLastSelectedItem();
	MSHTML::IHTMLElementPtr elem = 0;
	MSHTML::IHTMLDOMNodePtr ret_node;

	do
	{
		if(item.IsNull())
			break;

		if(!item.GetData() || !(bool)(elem = (IHTMLElement*) item.GetData()))
			continue;

		MSHTML::IHTMLDOMNodePtr node = (MSHTML::IHTMLDOMNodePtr)elem;
		if(!(bool)node)
			continue;

		ret_node = MoveLeftElement(node);
	}while(item = m_document_tree.m_tree.m_tree.GetPrevSelectedItem(item));

	GetDocumentStructure();
	if((bool)ret_node)
	{
		MSHTML::IHTMLElementPtr movedElement(ret_node);
		m_document_tree.m_tree.m_tree.SelectElement(movedElement);
		GoTo(movedElement);
	}

	m_doc->m_body.EndUndoUnit();
	return 0;
}

LRESULT CMainFrame::OnTreeMoveElementSmart(WORD, WORD, HWND, BOOL&)
{
	// ???? ??????? ?????? ???? ???????, ?? ??????? ??? ??????
	// ???? ?????????, ?? ????????? ?????? ??? ??? ???
	// ???? ??????, ?? ?????? ????? ????
	// ----------
	// ----------
	//   ----------    ?????? ?????????? ???????
	//   ----------
	// ----------
	//   ----------    ?????? ?????????? ???????
	//   ----------

	// ??? ????????? ?????? ?? ?? ??? ? ??? ??????

	m_doc->m_body.BeginUndoUnit(L"structure editing");
	CTreeItem item = m_document_tree.m_tree.m_tree.GetFirstSelectedItem();

	MSHTML::IHTMLDOMNodePtr node = RecoursiveMoveRightElement(item);
	GetDocumentStructure();
	if((bool)node)
	{
		MSHTML::IHTMLElementPtr elem(node);
		m_document_tree.m_tree.m_tree.SelectElement(elem);
		GoTo(elem);
	}

	m_doc->m_body.EndUndoUnit();

	return 0;
}

MSHTML::IHTMLDOMNodePtr CMainFrame::RecoursiveMoveRightElement(CTreeItem item)
{
	MSHTML::IHTMLDOMNodePtr ret;
	if(item.IsNull() || !item.GetData())
		return false;

	CTreeItem next_selected_sibling = m_document_tree.m_tree.m_tree.GetNextSelectedSibling(item);
	bool smart_selection = (!next_selected_sibling.IsNull()) && (item.GetNextSibling() != next_selected_sibling);

	if(smart_selection)
	{
		CTreeItem next_sibling = item.GetNextSibling();
		CTreeItem cur_selected = next_selected_sibling;
		while(!item.IsNull())
		{
			if(!item.GetData())
				return 0;
			MSHTML::IHTMLElementPtr elem = (MSHTML::IHTMLElement*)item.GetData();
			if(!(bool)elem)
				return 0;

			MSHTML::IHTMLDOMNodePtr node =  MSHTML::IHTMLDOMNodePtr(elem);

			if(!(bool)node)
				return 0;

			MoveRightElement(node);
			if(next_sibling.IsNull())
				break;

			item = next_sibling;
			next_sibling = next_sibling.GetNextSibling();

			if(!next_selected_sibling.IsNull() && next_sibling == next_selected_sibling)
			{
				item = next_selected_sibling;
				next_sibling = next_selected_sibling.GetNextSibling();
				cur_selected = next_selected_sibling;
				next_selected_sibling = m_document_tree.m_tree.m_tree.GetNextSelectedSibling(next_selected_sibling);
				continue;
			}
		}
		RecoursiveMoveRightElement(m_document_tree.m_tree.m_tree.GetNextSelectedItem(cur_selected));
	}
	else
	{
		while(!item.IsNull())
		{
			MSHTML::IHTMLElementPtr elem = (MSHTML::IHTMLElement*)item.GetData();
			if(!(bool)elem)
				return 0;

			MSHTML::IHTMLDOMNodePtr node =  MSHTML::IHTMLDOMNodePtr(elem);
			if(!(bool)node)
				return 0;

			ret = MoveRightElement(node);

			item = m_document_tree.m_tree.m_tree.GetNextSelectedItem(item);
		}
	}
	return ret;
}


LRESULT CMainFrame::OnTreeViewElement(WORD, WORD, HWND, BOOL&)
{
	GoToSelectedTreeItem();
	return 0;
}

LRESULT CMainFrame::OnTreeViewElementSource(WORD, WORD, HWND, BOOL&)
{
	CTreeItem item = m_document_tree.GetSelectedItem();
	if(!item.IsNull() && item.GetData())
	{
		MSHTML::IHTMLBodyElementPtr body = (MSHTML::IHTMLBodyElementPtr)m_doc->m_body.Document()->body;
		MSHTML::IHTMLTxtRangePtr rng = body->createTextRange();
		MSHTML::IHTMLElement* elem = (MSHTML::IHTMLElement*)item.GetData();
		rng->moveToElementText(elem);
		rng->select();
		ShowView(SOURCE);
	}

	return 0;
}

LRESULT CMainFrame::OnTreeDeleteElement(WORD, WORD, HWND, BOOL&)
{
	wchar_t cpt[MAX_LOAD_STRING + 1];
	wchar_t msg[MAX_LOAD_STRING + 1];
	FbeLoadString(_Module.GetResourceInstance(), IDS_DOCUMENT_TREE_CAPTION, cpt, MAX_LOAD_STRING);
	FbeLoadString(_Module.GetResourceInstance(), ID_DT_DELETE, msg, MAX_LOAD_STRING);
	CString message(msg);
	message += L"?";

	if (MessageBox(message, cpt, MB_YESNO | MB_ICONINFORMATION) == IDYES)
	{
		CTreeItem item = m_document_tree.m_tree.m_tree.GetLastSelectedItem();
		m_doc->m_body.BeginUndoUnit(L"structure editing");
		do
		{
			if(!item.IsNull() && item.GetData())
			{
				MSHTML::IHTMLElement* elem = (MSHTML::IHTMLElement*)item.GetData();
				if(!elem)
					return 0;

				MSHTML::IHTMLDOMNodePtr node = (MSHTML::IHTMLDOMNodePtr)elem;
				node->removeNode(VARIANT_TRUE);
			}
			else break;

			item = m_document_tree.m_tree.m_tree.GetPrevSelectedItem(item);
		} while(!item.IsNull());
		m_doc->m_body.EndUndoUnit();
	}
	return 0;
}


LRESULT CMainFrame::OnTreeMerge(WORD, WORD, HWND, BOOL&)
{
	CTreeItem item = m_document_tree.GetSelectedItem();
	if(item.IsNull())
		return 0;

	MSHTML::IHTMLElement* elem = (MSHTML::IHTMLElement*)item.GetData();
	if(!elem)
		return 0;

	bool merged = m_doc->m_body.bCall(L"MergeContainers", elem);
	m_doc->m_body.Call(L"MergeContainers", elem);
	// Move cursor to selected element
	if(merged)
		GoTo(elem);

	return 0;
}

LRESULT CMainFrame::OnTreeClick(WORD, WORD, HWND /* unused: hWndCtl */, BOOL&)
{
  GoToSelectedTreeItem();
  return 0;
}

// binary objects
LRESULT CMainFrame::OnEditAddBinary(WORD, WORD, HWND, BOOL&) {
  if (!m_doc)
    return 0;

  // Modification by Pilgrim
	const std::vector<ImageImportFileType> imageTypes = ImageImportFileTypes();
	std::vector<COMDLG_FILTERSPEC> filters;
	filters.reserve(imageTypes.size());
	for (const ImageImportFileType& type : imageTypes)
		filters.push_back({ type.displayName.GetString(), type.wildcard.GetString() });
  wchar_t dlgTitle[MAX_LOAD_STRING + 1];
  FbeLoadString(_Module.GetResourceInstance(), IDS_ADD_BINARIES_FILEDLG, dlgTitle, MAX_LOAD_STRING);
	ModernFileDialog::Request request;
	request.allowMultiSelect = true;
	request.fileMustExist = true;
	request.pathMustExist = true;
	request.title = dlgTitle;
	request.filters = filters.data();
	request.filterCount = static_cast<UINT>(filters.size());
	request.filterIndex = 1;
	const ModernFileDialog::Result dialogResult = ModernFileDialog::Show(m_hWnd, request);
	if (dialogResult.outcome == ModernFileDialog::Outcome::Failed)
		StartupTrace::HResult(L"file-dialog", L"FD103", dialogResult.error, L"Add binary dialog");
	if (dialogResult.outcome == ModernFileDialog::Outcome::Accepted) {
	int added = 0, converted = 0;
	CString failures;
	for (const std::wstring& path : dialogResult.paths) {
		CString fileName(path.c_str());
		CString error;
		bool wasConverted = false;
		HRESULT importResult = m_doc->ImportBinary(fileName, error, &wasConverted);
		if (importResult == E_ABORT) {
			const CString question = FbeLoadRuntimeStringByKey(L"fbe.image_import.flatten_question", L"This image has transparency. Convert it to JPEG on a white background?");
			if (::MessageBox(m_hWnd, question, FbeLoadRuntimeStringByKey(L"fbe.image_import.batch_title", L"Image import"), MB_YESNO | MB_ICONWARNING) == IDYES)
				importResult = m_doc->ImportBinary(fileName, error, &wasConverted, true);
			else
				continue;
		}
		if (SUCCEEDED(importResult)) {
			++added;
			if (wasConverted) ++converted;
		} else {
			CString leaf = fileName.Mid(fileName.ReverseFind(L'\\') + 1);
			if (error.IsEmpty()) error = FbeLoadRuntimeStringByKey(L"fbe.image_import.add_failed", L"Could not add file.");
			failures += leaf + L" — " + error + L"\r\n";
		}
	}
	if (!failures.IsEmpty()) {
		CString summary = FbeLoadRuntimeStringByKey(L"fbe.image_import.batch_summary", L"Added: %d\r\nConverted: %d\r\nFailed:\r\n%s");
		CString message; message.Format(summary, added, converted, (LPCWSTR)failures);
		::MessageBox(m_hWnd, message, FbeLoadRuntimeStringByKey(L"fbe.image_import.batch_title", L"Image import"), MB_OK | MB_ICONWARNING);
	}
  }

  return 0;
}

// incremental search
LRESULT CMainFrame::OnEditIncSearch(WORD, WORD, HWND, BOOL&) {
  if (IsSourceActive())
    return 0;

  if (m_incsearch==0) {
    ShowView();
    m_doc->m_body.StartIncSearch();
    m_is_str.Empty();
    m_is_prev=m_doc->m_body.LastSearchPattern();
    m_incsearch=1;
    m_is_fail=false;
    SetIsText();
  } else if (m_incsearch==1 && m_is_str.IsEmpty() && !m_is_prev.IsEmpty()) {
    m_incsearch=2;
    m_is_str.Empty();
    for (int i=0;i<m_is_prev.GetLength();++i)
      PostMessage(WM_CHAR,m_is_prev[i],0x20000000);
  } else if (!m_is_fail)
    m_doc->m_body.DoIncSearch(m_is_str,true);
  return 0;
}

LRESULT CMainFrame::OnChar(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
  if (!m_incsearch)
    return 0;
  // only a few keys are supported
  if (wParam==8) { // backspace
    if (!m_is_str.IsEmpty())
      m_is_str.Delete(m_is_str.GetLength()-1);
    if (!m_doc->m_body.DoIncSearch(m_is_str,false)) {
      m_is_fail=true;
      ::MessageBeep(MB_ICONEXCLAMATION);
    } else
      m_is_fail=false;
  } else if (wParam==13) { // enter
    StopIncSearch(false);
    return 0;
  } else if (wParam>=32 && wParam!=127) { // printable char
    if (m_is_fail) {
      ::MessageBeep(MB_ICONEXCLAMATION);
      if (!(lParam&0x20000000))
	return 0;
    }
    m_is_str+=(TCHAR)wParam;
    if (!m_doc->m_body.DoIncSearch(m_is_str,false)) {
      if (!m_is_fail)
	::MessageBeep(MB_ICONEXCLAMATION);
      m_is_fail=true;
    } else
      m_is_fail=false;
  }
  SetIsText();
  return 0;
}

// Text refines the exact Source boundaries, while the DOM path supplies both
// the body scope and the expected structural position.  Without that position
// the helper refuses an ambiguous transfer instead of selecting another copy.
// DomPath may fail for a valid Source position (for example inside inline
// markup).  The body ordinal is still available from the source XML and is
// sufficient to constrain the fallback search to the matching visual body.
// Resolve the complete serialized range of one top-level FB2 body.  This is
// deliberately independent of DomPath: a native MSHTML text selection can be
// perfectly valid even when DomPath cannot represent one of its inline nodes.
// Находит диапазон в HTML по началу и концу видимого текста. Это покрывает
// Source-выделения, пересекающие абзацы: один вызов findText для всего такого
// диапазона не работает в MSHTML из-за разных представлений перевода строки.
// Границы выделения Source могут попасть в имя тега или его атрибут. В Body
// таких символов нет, поэтому отсекаем разметку и оставляем только видимый
// текст между тегами.
// Журнал не содержит текст книги или выделения: только длину диапазона.

static CString SelectionTraceSummary(const CString& text)
{
	CString result;
	result.Format(L"selection-chars=%d", text.GetLength());
	return result;
}
static void WriteSelectionTrace(const wchar_t* code, const CString& message)
{
	StartupTrace::Event(L"selection", code, message);
}

EditorSourceOperationResult CMainFrame::CommitSourceDocument()
{
	// A malformed document opened into Source has no DOM to update yet.  Keep
	// this validation before any view-state commit.
	if (m_bad_xml)
	{
		int col, line;
		if (!m_doc->SetXMLAndValidate(m_source, true, line, col))
		{
			U::MessageBox(MB_OK|MB_ICONERROR, IDR_MAINFRAME, IDS_BAD_XML_MSG);
			SourceGoTo(line, col);
			return EditorSourceOperationResult::InvalidSource;
		}
		AttachDocument(m_doc);
		m_doc->m_filename = m_bad_filename;
		if (m_bad_filename.CompareNoCase(L"Untitled.fb2") == 0)
		{
			m_document_session.NewDocument();
			m_doc->m_namevalid = false;
		}
		else
		{
			m_document_session.OpenNormal(m_doc->m_filename, m_doc->GetDocumentFileType());
			m_doc->m_namevalid = true;
		}
		m_bad_xml = false;
	}
	CString sourceEncoding = _Settings.KeepEncoding() ? m_doc->m_encoding : _Settings.GetDefaultEncoding();
	if (sourceEncoding.IsEmpty()) sourceEncoding = L"utf-8";
	m_source_view_session.SetInterfaceLanguage(_Settings.GetInterfaceLanguageName());
	m_source_view_session.SetSourceEncoding(sourceEncoding);
	m_source_view_session.SetMemoryProfilingEnabled(!AU::_ARGS.source_memory_benchmark_path.IsEmpty());
	const EditorSourceOperationResult sessionResult = m_source_view_session.CommitSourceDocument();
	if (sessionResult != EditorSourceOperationResult::Success)
	{
		const SourceDocumentApplyResult& applyResult = m_source_view_session.LastApplyResult();
		if (sessionResult == EditorSourceOperationResult::InvalidSource && !applyResult.errorMessage.IsEmpty())
		{
			::SendMessage(m_doc->m_frame, AU::WM_SETSTATUSTEXT, 0,
				(LPARAM)(const TCHAR*)applyResult.errorMessage);
			SourceGoTo(applyResult.errorLine, applyResult.errorColumn);
		}
		return sessionResult;
	}
	if (m_source_view_session.DocumentChanged()) ClearSelection();
	if (_Settings.ViewDocumentTree()) m_document_tree.GetDocumentStructure(m_doc->m_body.Document());
	return EditorSourceOperationResult::Success;

}

bool CMainFrame::SourceToHTML()
{
	return CommitSourceDocument() == EditorSourceOperationResult::Success;
}

EditorSourceOperationResult CMainFrame::PrepareSourceDocument(EditorView previous)
{
	CString sourceEncoding = _Settings.KeepEncoding() ? m_doc->m_encoding : _Settings.GetDefaultEncoding();
	if (sourceEncoding.IsEmpty()) sourceEncoding = L"utf-8";
	m_source_view_session.SetSourceEncoding(sourceEncoding);
	m_source_view_session.SetMemoryProfilingEnabled(!AU::_ARGS.source_memory_benchmark_path.IsEmpty());
	return m_source_view_session.PrepareSourceDocument(previous);
}


EditorView CMainFrame::NextEditorView()
{
	const EditorView current = m_editor_view_state.Current();
	const bool ctrlTabActive = m_editor_view_state.CtrlTabActive();
	const EditorView target = NextCtrlTabEditorView(current,
		m_editor_view_state.Previous(), m_editor_view_state.LastCtrlTabView(), ctrlTabActive);
	if(!ctrlTabActive)
	{
		m_editor_view_state.SetLastCtrlTabView(current);
		m_editor_view_state.SetCtrlTabActive(true);
	}
	return target;
}

void CMainFrame::PresentEditorViewChangeFailure(const EditorViewChangeResult& result)
{
	if (result.failure == EditorViewChangeFailure::HtmlUnavailable)
	{
		StartupTrace::Warning(L"selection", L"E281",
			L"view switch ignored: HTML document is unavailable");
	}
}

void CMainFrame::ShowView(EditorView vt)
{
	const EditorView previous = m_editor_view_state.Current();
	if (!m_editor_view_state.CtrlTabActive() && previous != vt)
		m_editor_view_state.SetLastCtrlTabView(previous);
	EditorViewPresentationContext presentationContext = {
		m_view, m_splitter, m_source, m_doc, m_editor_selection_state,
		_Settings.ViewDocumentTree() };
	EditorViewPresentationHost presentation(presentationContext);
	const EditorViewChangeResult result =
		m_editor_view_controller.ChangeView(m_editor_view_state, *this, presentation, vt);
	if (!result.Succeeded())
		PresentEditorViewChangeFailure(result);
	else
		ApplyEditorViewCommandUi(result.previous, result.current);
}

void CMainFrame::ApplyEditorViewCommandUi(EditorView prev, EditorView vt)
{
	if (vt != BODY && m_Speller) m_Speller->EndDocumentCheck();
	if (prev != vt) {
		m_doc->m_body.CloseFindDialog(m_doc->m_body.m_find_dlg);
		m_doc->m_body.CloseFindDialog(m_sci_find_dlg);
		m_doc->m_body.CloseFindDialog(m_doc->m_body.m_replace_dlg);
		m_doc->m_body.CloseFindDialog(m_sci_replace_dlg);
	}
	if (prev != vt && vt != SOURCE)
		UIEnable(ID_VIEW_TREE, 1);
	UISetCheck(ID_VIEW_BODY, vt == BODY); UISetCheck(ID_VIEW_DESC, vt == DESC); UISetCheck(ID_VIEW_SOURCE, vt == SOURCE);
	if (vt == BODY) { m_sel_changed = true; if (m_Speller) m_Speller->SetDocumentLanguage(); }
	if (vt == DESC) {
		m_contextAttributeBars.ClearLinkState(); m_contextAttributeBars.ClearTableState();
		m_contextAttributeBars.SetLinkAvailability(LinkAttributeAvailability{ false, false, false, false });
		m_contextAttributeBars.SetTableAvailability(TableAttributeAvailability{ false, false, false, false, false, false, false, false, false });
		SetStatusContext(L"");
	}
	if (vt == SOURCE) { SetStatusContext(L""); RefreshLocalizedToolbarButtonTexts(m_CmdToolbar); RefreshLocalizedToolbarButtonTexts(m_ScriptsToolbar); }
	UpdateStatusBar();
}

static SourceEditorConfig BuildSourceEditorConfig()
{
	SourceEditorConfig config;
	config.showEol = _Settings.XmlSrcShowEOL();
	config.showWhitespace = _Settings.XmlSrcShowSpace();
	config.wrap = _Settings.XmlSrcWrap();
	config.showLineNumbers = _Settings.XMLSrcShowLineNumbers();
	config.syntaxHighlight = _Settings.XmlSrcSyntaxHL();
	config.showSpecialCharacters = _Settings.XmlSrcShowSpecialChars();
	config.specialCharactersStyle = _Settings.XmlSrcSpecialCharsStyle();
	config.undoSelectionHistory = !AU::_ARGS.disable_undo_selection_history;
	config.tagHighlight = _Settings.XmlSrcTagHL();
	config.tagHighlightFullTag = _Settings.XmlSrcTagHighlightMode() != 0;
	config.tagHighlightAttributes = _Settings.XmlSrcTagHighlightAttributes();
	config.tagHighlightErrors = _Settings.XmlSrcTagHighlightErrors();
	config.fontName = _Settings.GetSrcFont();
	config.fontSize = static_cast<int>(_Settings.GetFontSize());
	for(int token = 0; token < XML_SRC_STYLE_TOKEN_COUNT; ++token)
		config.colors[token] = _Settings.GetXmlSrcStyleColor(static_cast<XmlSrcStyleToken>(token));
	return config;
}

LRESULT CMainFrame::OnFileValidate(WORD, WORD, HWND, BOOL&) {
  int col,line;
  bool fv;
  CString validationError;
  ClearSourceValidationAnnotations();
  if (IsSourceActive())
    fv=m_doc->SetXMLAndValidate(m_source,true,line,col,&validationError);// ?? ?????? Source
  else
    fv=m_doc->Validate(line,col);						// ?? ?????? Body
  if (fv) {
    ClearSourceValidationAnnotations();
		SetValidationStatus(FBEStatusBar::ValidationStatus::Valid);
    return 0;
  }
  if (!fv) {
		SetValidationStatus(FBEStatusBar::ValidationStatus::Invalid);
    ShowView(SOURCE);
    ShowSourceValidationAnnotation(line, col, validationError);
    // have to jump through the hoops to move to required column
    SourceGoTo(line, col);
  }
  return 0;
}

void  CMainFrame::SciModified(const SCNotification& scn) {
	m_source.HandleModified(scn);
}

void CMainFrame::ClearSourceValidationAnnotations()
{
	m_source.SendMessage(SCI_EOLANNOTATIONCLEARALL);
}

void CMainFrame::ShowSourceValidationAnnotation(int line, int column, const CString& message)
{
	if (line <= 0)
		return;

	CString annotation = message;
	annotation.Replace(L'\r', L' ');
	annotation.Replace(L'\n', L' ');
	if (annotation.IsEmpty())
		annotation.Format(L"XML validation error (line %d, column %d)", line, column);

	CW2A annotationUtf8(annotation, CP_UTF8);
	const int sourceLine = line - 1;
	m_source.SendMessage(SCI_EOLANNOTATIONSETTEXT, sourceLine, reinterpret_cast<LPARAM>(static_cast<LPCSTR>(annotationUtf8)));
	m_source.SendMessage(SCI_EOLANNOTATIONSETSTYLE, sourceLine, STYLE_LINENUMBER);
	m_source.SendMessage(SCI_EOLANNOTATIONSETVISIBLE, EOLANNOTATION_STANDARD);
}

bool CMainFrame::SciUpdateUI(bool gotoTag)
{
	UpdateStatusBar();
	const SourceEditorConfig config = BuildSourceEditorConfig();
	if (config.tagHighlight || gotoTag)
	{
		if (gotoTag) UIEnable(ID_GOTO_MATCHTAG, m_source.GotoMatchingTag());
		else UIEnable(ID_GOTO_MATCHTAG, m_source.UpdateTagHighlight({ config.tagHighlight, config.tagHighlightFullTag ? XmlTagHighlightMode::FullTag : XmlTagHighlightMode::NameOnly, config.tagHighlightAttributes, config.tagHighlightErrors }));
		return true;
	}
	return false;
}

void CMainFrame::SciGotoWrongTag()
{
	CWaitCursor hourglass;
	m_source.GotoWrongTag();

}

void CMainFrame::ShowFb2Autocomplete(int character)
{
	if (character != '<' && character != '/' && character != ' ' && character != ':' && character != '#')
		return;

	class ScintillaTextReader : public Fb2SourceTextReader
	{
	public:
		explicit ScintillaTextReader(HWND source) : m_source(source) {}
		std::size_t Length() const { return static_cast<std::size_t>(::SendMessage(m_source, SCI_GETLENGTH, 0, 0)); }
		void Read(std::size_t position, std::size_t length, std::string& text) const
		{
			text.assign(length, '\0');
			Sci_TextRange range = {};
			range.chrg.cpMin = static_cast<sptr_t>(position);
			range.chrg.cpMax = static_cast<sptr_t>(position + length);
			range.lpstrText = &text[0];
			::SendMessage(m_source, SCI_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&range));
			text.resize(strlen(text.c_str()));
		}
	private:
		HWND m_source;
	};

	const sptr_t caret = m_source.SendMessage(SCI_GETCURRENTPOS);
	ScintillaTextReader reader(m_source.m_hWnd);
	Fb2SourceStructuralContextResolver resolver;
	Fb2AutocompleteResult result = m_fb2_autocomplete.Complete(resolver.Resolve(reader, static_cast<std::size_t>(caret), character), character);
	if (result.needsDocumentIds)
	{
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> document(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(&document[0]));
		result.candidates = m_fb2_autocomplete.CompleteIds(&document[0]);
	}
	if (!result.candidates.empty())
		m_source.SendMessage(SCI_AUTOCSHOW, 0, reinterpret_cast<LPARAM>(result.candidates.c_str()));
}

void  CMainFrame::SciMarginClicked(const SCNotification& scn)
{
	m_source.HandleMarginClick(scn);
}


void CMainFrame::GoToSelectedTreeItem()
{
  CTreeItem ii(m_document_tree.GetSelectedItem());
  if (!ii.IsNull() && ii.GetData())
  {
    if(m_editor_view_state.Current() != BODY)
	{
		ShowView();
	}
    GoTo((MSHTML::IHTMLElement*) ii.GetData());
  }
}

void CMainFrame::SciCollapse(int level2Collapse, bool mode)
{
	m_source.SendMessage(SCI_COLOURISE, 0, -1);
	int maxLine = m_source.SendMessage(SCI_GETLINECOUNT);

	for (int line = 0; line < maxLine; line++)
	{
		int level = m_source.SendMessage(SCI_GETFOLDLEVEL, line);
		if (level & SC_FOLDLEVELHEADERFLAG)
		{
			level -= SC_FOLDLEVELBASE;
			if (level2Collapse == (level & SC_FOLDLEVELNUMBERMASK))
				if ((m_source.SendMessage(SCI_GETFOLDEXPANDED, line) != 0) != mode)
					m_source.SendMessage(SCI_TOGGLEFOLD, line);
		}
	}
}

MSHTML::IHTMLDOMNodePtr CMainFrame::MoveRightElementWithoutChildren(MSHTML::IHTMLDOMNodePtr node)
{
	MSHTML::IHTMLDOMNodePtr move_from;
	MSHTML::IHTMLDOMNodePtr move_to;
	MSHTML::IHTMLDOMNodePtr insert_before;
	MSHTML::IHTMLDOMNodePtr ret;
	// ?????? ???? ???????? ?????? ??????????? ?????
	// ????? ???? ????? ????? ?????? ?????? ????????

	if(!(bool)(ret = MoveRightElement(node)))
		return 0;

	MSHTML::IHTMLDOMNodePtr nextSibling = GetNextSiblingSection(node);

	MSHTML::IHTMLDOMNodePtr child = GetFirstChildSection(node);
	if((bool)child)
	{
		MSHTML::IHTMLDOMNodePtr parent = node->parentNode ;
		move_to = parent;
		insert_before = 0;
		MSHTML::IHTMLDOMNodePtr nextChild;
        do
		{
			move_from = child;
			nextChild = GetNextSiblingSection(child);
			m_doc->MoveNode(move_from, move_to, insert_before);
			child = nextChild;
		}while(nextChild);
	}

	return ret;
}

MSHTML::IHTMLDOMNodePtr CMainFrame::MoveRightElement(MSHTML::IHTMLDOMNodePtr node)
{
	MSHTML::IHTMLDOMNodePtr move_from;
	MSHTML::IHTMLDOMNodePtr move_to;
	MSHTML::IHTMLDOMNodePtr insert_before;
	// ?????? ???? ???????? ?????? ??????????? ?????

	if(!(bool)node)
		return 0;

	// ???? ????? ??????? ?????? ??????
	if(!IsNodeSection(node))
		return 0;

	// ???? ?? ????? ??????????? ????, ?? ?? ?????? ??????
	MSHTML::IHTMLDOMNodePtr prev_sibling = GetPrevSiblingSection(node);

	if(!(bool)prev_sibling)
		return 0;

	MSHTML::IHTMLDOMNodePtr child = GetLastChildSection(prev_sibling);

	// ?????? ???? ????????? ???????? ?????? ??????????? ?????
	move_to = prev_sibling;
	insert_before = 0;
	move_from = node;

	if(!IsEmptySection(move_to))
	{
		CreateNestedSection(move_to);
	}

	return m_doc->MoveNode(move_from, move_to, insert_before);
}

MSHTML::IHTMLDOMNodePtr CMainFrame::MoveLeftElement(MSHTML::IHTMLDOMNodePtr node)
{
	MSHTML::IHTMLDOMNodePtr ret;
	// ?????? ????  ????????? ?????? ?????? ????
	// ? ????? ????????? ??????? ?????? ??????

	if(!(bool)node)
		return 0;

	// ???? ????? ??????? ?????? ??????
	if(!IsNodeSection(node))
		return 0;

	// ???? ?? ????? ??????????? ????, ?? ?? ?????? ??????
	MSHTML::IHTMLDOMNodePtr parent = node->parentNode;
	if(!(bool)parent || !IsNodeSection(parent->parentNode))
		return 0;

	MSHTML::IHTMLDOMNodePtr sibling = node->nextSibling;

	while((bool)sibling)
	{
		MSHTML::IHTMLDOMNodePtr next_sibling = sibling->nextSibling;
		m_doc->MoveNode(sibling, node, 0);
		sibling = next_sibling;
	}
	// ?????? ????  ????????? ?????? ?????? ????
	ret = m_doc->MoveNode(node, parent->parentNode, parent->nextSibling);

	return ret;
}

bool CMainFrame::IsNodeSection(MSHTML::IHTMLDOMNodePtr node)
{
	if(!(bool)node)
	{
		return false;
	}

	MSHTML::IHTMLElementPtr elem = MSHTML::IHTMLElementPtr(node);
	if(!(bool)elem)
	{
		return false;
	}

	return (U::scmp(elem->tagName,L"DIV") == 0 && (U::scmp(elem->className,L"section") == 0 || U::scmp(elem->className,L"body")==0));
}

MSHTML::IHTMLDOMNodePtr CMainFrame::GetFirstChildSection(MSHTML::IHTMLDOMNodePtr node)
{
	if(!(bool)node)
		return 0;

	MSHTML::IHTMLDOMNodePtr child = node->firstChild;

	if(!(bool)child)
		return 0;

	if(IsNodeSection(child))
		return child;

	return GetNextSiblingSection(child);
}

MSHTML::IHTMLDOMNodePtr CMainFrame::GetNextSiblingSection(MSHTML::IHTMLDOMNodePtr node)
{
	if(!(bool)node)
		return 0;

	node = node->nextSibling;

	while(1)
	{
		if(!(bool)node)
			return 0;

		if(IsNodeSection(node))
			return node;

		node = node->nextSibling;
	}

	return 0;
}

MSHTML::IHTMLDOMNodePtr CMainFrame::GetPrevSiblingSection(MSHTML::IHTMLDOMNodePtr node)
{
	if(!(bool)node)
		return 0;

	node = node->previousSibling;

	while(1)
	{
		if(!(bool)node)
			return 0;

		if(IsNodeSection(node))
			return node;

		node = node->previousSibling;
	}

	return 0;
}

MSHTML::IHTMLDOMNodePtr CMainFrame::GetLastChildSection(MSHTML::IHTMLDOMNodePtr node)
{
	if(!(bool)node)
		return 0;

	MSHTML::IHTMLDOMNodePtr child = node->lastChild;

	if(!(bool)child)
		return 0;

	if(IsNodeSection(child))
		return child;

	return GetPrevSiblingSection(child);
}

LRESULT CMainFrame::OnSciCollapse(WORD /* unused: cose */, WORD wID, HWND, BOOL&)
{
	if(m_editor_view_state.Current() == SOURCE)
		SciCollapse(wID - ID_SCI_COLLAPSE_BASE, false);

	if(m_document_tree.IsWindowVisible())
		m_document_tree.m_tree.m_tree.Collapse(0, wID - ID_SCI_COLLAPSE_BASE, false);

	return 0;
}

LRESULT CMainFrame::OnSciExpand(WORD /* unused: cose */, WORD wID, HWND, BOOL&)
{
	if(m_editor_view_state.Current() == SOURCE)
		SciCollapse(wID - ID_SCI_EXPAND_BASE, true);

	if(m_document_tree.IsWindowVisible())
		m_document_tree.m_tree.m_tree.Collapse(0, wID - ID_SCI_EXPAND_BASE, true);

	return 0;
}

//////////////////////////////////////////////////////////////////////
/// @fn CMainFrame::IsEmptySection
///
/// ??????? ????????? ???? ?? ???????? ????? ?????? ???. ??????? ?????????
/// ????? ?????????????????? ????????, ?????????? ?????? ???? ??????, ????????
/// ?? ????????, ????????? ?????, ????????? ??????? ? ???????? ?????????
///	@param MSHTML::IHTMLDOMNodePtr section [in, out] ??????????? ??????
/// @return bool true - ???? ?????? ??????
/// @date 17.12.07 @author ????? ????
//////////////////////////////////////////////////////////////////////
bool CMainFrame::IsEmptySection(MSHTML::IHTMLDOMNodePtr section)
{
	section = section->firstChild;
	if(!(bool)section)
		return true;
	do
	{
		long node_type = section->nodeType;

		if(node_type == 3)//text node
		{
			variant_t vt = section->nodeValue;
			BSTR node_value = vt.bstrVal;
			if(!IsEmptyText(node_value))
			{
				return false;
			}
		}
		else
		{
			_bstr_t tag_name(section->nodeName);
			MSHTML::IHTMLElementPtr elem = (MSHTML::IHTMLElementPtr)section;
			_bstr_t class_name(elem->className);

			if((0 == U::scmp(tag_name, L"DIV")) &&
				((0 == U::scmp(class_name, L"section"))
				|| (0 == U::scmp(class_name, L"title"))
				|| (0 == U::scmp(class_name, L"epigraph"))
				|| (0 == U::scmp(class_name, L"annotation"))
				|| (0 == U::scmp(class_name, L"image"))
				))
			{
				continue;
			}

			if(!IsEmptyText(elem->outerText))
			{
				return false;
			}
		}
	}while((bool)(section = section->nextSibling));

	return true;
}

bool CMainFrame::IsEmptyText(BSTR text)
{
	wchar_t* ch = text;
	if(!ch)
		return true;

	while(*ch)
	{
		if(*ch != L' ' && *ch != L'\r' && *ch != L'\n' && *ch != L'\t')
			return false;

		++ch;
	}
	return true;
}

MSHTML::IHTMLDOMNodePtr CMainFrame::CreateNestedSection(MSHTML::IHTMLDOMNodePtr node)
{
	MSHTML::IHTMLDOMNodePtr section = node->firstChild;
	MSHTML::IHTMLDOMNodePtr new_node;
	if(!(bool)section)
		return 0;
	do
	{
		_bstr_t tag_name(section->nodeName);
		MSHTML::IHTMLElementPtr elem = (MSHTML::IHTMLElementPtr)section;
		_bstr_t class_name(elem->className);

		if((0 == U::scmp(tag_name, L"DIV")) &&
			((0 == U::scmp(class_name, L"section"))
			|| (0 == U::scmp(class_name, L"title"))
			|| (0 == U::scmp(class_name, L"epigraph"))
			|| (0 == U::scmp(class_name, L"annotation"))
			|| (0 == U::scmp(class_name, L"image"))
			))
		{
			continue;
		}

		MSHTML::IHTMLElementPtr new_elem = m_doc->m_body.Document()->createElement(L"DIV");
		new_elem->className = L"section";
		new_node = MSHTML::IHTMLDOMNodePtr(new_elem);
		MSHTML::IHTMLDOMNodePtr insert_before = section;
		m_doc->MoveNode(new_node, node, insert_before);
		do
		{
			MSHTML::IHTMLDOMNodePtr next_node = section->nextSibling;
			m_doc->MoveNode(section, new_node, 0);
			section = next_node;
		}while((bool)section);
		break;

	}while((bool)(section = section->nextSibling));

	return new_node;
}

void CMainFrame::ClearSelection()
{
	m_editor_selection_state.ClearHtmlRanges();
}

void CMainFrame::SourceGoTo(int line, int col)
{
	int	pos=m_source.SendMessage(SCI_POSITIONFROMLINE,line-1);
    while (col--)
      pos=m_source.SendMessage(SCI_POSITIONAFTER,pos);
    m_source.SendMessage(SCI_SETSELECTIONSTART,pos);
    m_source.SendMessage(SCI_SETSELECTIONEND,pos);
    m_source.SendMessage(SCI_SCROLLCARET);
}

bool CMainFrame::CheckFileTimeStamp()
{
	if (m_document_session.Location().storagePath.IsEmpty() || !IsDocumentLocationModified(m_document_session.Location())) return false;
	if(IDYES == U::MessageBox(MB_YESNO, IDS_FILE_CHANGED_CPT, IDS_FILE_CHANGED_MSG, static_cast<LPCWSTR>(m_doc->m_filename)))
		return ReloadFile();
	m_document_session.AcceptExternalVersion();
	return false;
}

bool CMainFrame::ReloadFile()
{
	if (m_document_session.Location().IsArchive())
		return LoadFile(m_document_session.Location().storagePath, &m_document_session.Location()) == OK;

	EnableWindow(FALSE);
	m_status.SetPaneText(ID_DEFAULT_PANE, FbeLoadRuntimeString(IDS_STATUS_LOADING));
	DocumentLifecycleController lifecycle(*this, m_doc, m_document_session, m_view);
	const DocumentLifecycleResult lifecycleResult = lifecycle.ReloadNormal(m_doc->m_filename);
	EnableWindow(TRUE);
	if (!lifecycleResult.Succeeded()) return false;
	AttachDocument(m_doc);
	return true;
}


void CMainFrame::GoTo(int selected_pos)
{
	MSHTML::IHTMLElementCollectionPtr children(m_doc->m_body.Document()->body->children);
	long			      c_len=children->length;

	MSHTML::IHTMLElementPtr fbw_body;

	for (long i=0;i<c_len;++i)
	{
		MSHTML::IHTMLElementPtr div(children->item(i));
		if (!(bool)div)
			continue;

		if (U::scmp(div->tagName,L"DIV")==0 && U::scmp(div->id,L"fbw_body")==0)
		{
			fbw_body = div;
			break;
		}
	}
	MSHTML::IHTMLTxtRangePtr rng(MSHTML::IHTMLBodyElementPtr(m_doc->m_body.Document()->body)->createTextRange());
	rng->moveToElementText(fbw_body);
	rng->collapse(VARIANT_TRUE);
	rng->move(L"character", selected_pos);
	rng->select();
}

bool CMainFrame::ShowSettingsDialog(HWND parent)
{
	CSettingsDlg dlg;
	return dlg.DoModal(parent) == IDOK;
}

LRESULT CMainFrame::OnApplyXmlSourceTheme(UINT, WPARAM, LPARAM, BOOL&)
{
	ApplyXmlSourceEditorChanges(false);
	return 0;
}
void CMainFrame::ApplyXmlSourceEditorChanges(bool saveSettings)
{
	const EditorView activeView = m_editor_view_state.Current();
	const SourceEditorConfig config = BuildSourceEditorConfig();
	m_source.ApplyConfiguration(config);
	m_source.UpdateTagHighlight({ config.tagHighlight, config.tagHighlightFullTag ? XmlTagHighlightMode::FullTag : XmlTagHighlightMode::NameOnly, config.tagHighlightAttributes, config.tagHighlightErrors });
	UIEnable(ID_GOTO_MATCHTAG, config.tagHighlight);
	// Перекраска XML-редактора не должна менять активный режим документа.
	if(activeView == BODY && m_doc)
		m_view.ActivateWnd(m_doc->m_body);
	if(saveSettings)
		_Settings.Save();
}
void CMainFrame::ApplyConfChanges(bool applyDocumentStyles)
{
	const EditorView activeView = m_editor_view_state.Current();
	CWaitCursor hourglass;
	LONG visible = false;

	wchar_t restartMsg[MAX_LOAD_STRING + 1];
	FbeLoadString(_Module.GetResourceInstance(), IDS_SETTINGS_NEED_RESTART, restartMsg, MAX_LOAD_STRING);

	if (applyDocumentStyles && m_doc)
		m_doc->ApplyConfChanges();
	const SourceEditorConfig config = BuildSourceEditorConfig();
	m_source.ApplyConfiguration(config);
	m_source.UpdateTagHighlight({ config.tagHighlight, config.tagHighlightFullTag ? XmlTagHighlightMode::FullTag : XmlTagHighlightMode::NameOnly, config.tagHighlightAttributes, config.tagHighlightErrors });
	UIEnable(ID_GOTO_MATCHTAG, config.tagHighlight);

	// added by SeNS
	if (_Settings.GetUseSpellChecker())
	{
		if (!m_Speller)
		{
			m_Speller = new CSpeller(U::GetProgDir()+L"dict\\");
			m_Speller->SetEnabled(false);
		}
		if (!m_Speller->Enabled())
		{
			m_Speller->SetFrame(m_hWnd);
			m_Speller->AttachDocument(m_doc->m_body.Document());
			m_Speller->SetEnabled(true);
		}
	}
	// don't use spellchecker
	else if (m_Speller) m_Speller->SetEnabled(false);

	if (m_Speller && m_Speller->Enabled())
	{
		m_Speller->SetHighlightMisspells(_Settings.GetHighlightMisspells());

		const CString custDictName = U::GetUserDataFile(_Settings.GetCustomDict(), m_doc->m_body.m_file_path);

		m_Speller->SetCustomDictionary(custDictName, _Settings.GetCustomDictCodepage());
	}

	// added by SeNS: issue 17: process nbsp change
	if (_Settings.GetOldNBSPChar().Compare (_Settings.GetNBSPChar()) != 0)
	{
		int numChanges = 0;
		// save caret position
		MSHTML::IDisplayServicesPtr ids (MSHTML::IDisplayServicesPtr(m_doc->m_body.Document()));
		MSHTML::IHTMLCaretPtr caret = 0;
		MSHTML::tagPOINT *point = new MSHTML::tagPOINT();
		if (ids)
		{
			ids->GetCaret(&caret);
			if (caret)
			{
				caret->IsVisible(&visible);
				if (visible) caret->GetLocation(point, true);
			}
		}

		MSHTML::IHTMLElementPtr fbwBody = MSHTML::IHTMLDocument3Ptr(m_doc->m_body.Document())->getElementById(L"fbw_body");
		MSHTML::IHTMLDOMNodePtr el = MSHTML::IHTMLDOMNodePtr(fbwBody)->firstChild;

		while (el && el!=fbwBody)
		{
			if (el->nodeType==3)
			{
				CString s = el->nodeValue;
				int n = s.Replace(_Settings.GetOldNBSPChar(), _Settings.GetNBSPChar());
				if (n)
				{
					numChanges += n;
					el->nodeValue = s.AllocSysString();
				}
			}
			if (el->firstChild)
				el=el->firstChild;
			else
			{
				while (el && el!=fbwBody && el->nextSibling==NULL) el=el->parentNode;
				if (el && el!=fbwBody) el=el->nextSibling;
			}
		}
		m_doc->AdvanceDocVersion(numChanges);

		// restore caret position
		if (caret && visible)
		{
			MSHTML::IDisplayPointerPtr disptr;
			ids->CreateDisplayPointer(&disptr);
			disptr->moveToPoint(*point, MSHTML::COORD_SYSTEM_GLOBAL, fbwBody, 0, 0);
			caret->MoveCaretToPointer(disptr, true, MSHTML::CARET_DIRECTION_SAME);
		}
	}

	_Settings.SaveHotkeyGroups();
	_Settings.Save();
	_Settings.SaveWords();
	// Rebuilding source-editor styles must not replace the active visual editor.
	if(activeView == BODY && m_doc)
		m_view.ActivateWnd(m_doc->m_body);



	if(_Settings.NeedRestart() && MessageBox(restartMsg, L"", MB_YESNO | MB_ICONINFORMATION) == IDYES)
	{
		return RestartProgram();
	}
}

void CMainFrame::ApplyEditorBackgroundChanges()
{
	// A background-only change is visual-editor CSS.  Avoid rebuilding
	// Scintilla styles, spell-check state and NBSP display for this path.
	if(m_doc)
		m_doc->ApplyConfChanges();
	_Settings.Save();
}

void CMainFrame::RestartProgram()
{
	BOOL b = false;
	if(OnClose(0, 0, 0, b))
	{
		const CString filename = U::GetModulePath(_Module.GetModuleInstance());
		CString ofn = m_doc->GetOpenFileName();
//		if(wcschr(filename, L' '))
		ofn.Format(L"\"%s\"", static_cast<LPCWSTR>(m_doc->GetOpenFileName()));
		ShellExecute(0, L"open", filename, ofn, 0, SW_SHOW);
	}
}

void CMainFrame::ReleaseScriptResources()
{
	// InitializeExtensionUi may be requested more than once.  Return the physical scripts
	// toolbar and its customization catalog to the resource baseline before the
	// next scan, otherwise every scan appends another copy of icon scripts.
	if(::IsWindow(m_ScriptsToolbar))
	{
		CImageList images = m_ScriptsToolbar.GetImageList();
		while(images && images.GetImageCount() > m_scriptsToolbarBaseImageCount)
			images.Remove(images.GetImageCount() - 1);
		const int defaultsIndex = m_aDefaultButtons.FindKey(m_ScriptsToolbar.m_hWnd);
		const int catalogIndex = m_aButtons.FindKey(m_ScriptsToolbar.m_hWnd);
		if(defaultsIndex >= 0 && catalogIndex >= 0)
		{
			TBBUTTONS defaults = m_aDefaultButtons.GetValueAt(defaultsIndex);
			while(m_ScriptsToolbar.GetButtonCount() > 0) m_ScriptsToolbar.DeleteButton(0);
			if(defaults.GetSize() > 0) m_ScriptsToolbar.AddButtons(defaults.GetSize(), defaults.GetData());
			m_aButtons.SetAt(m_ScriptsToolbar.m_hWnd, defaults);
			m_ScriptsToolbar.AutoSize();
		}
	}
	m_scripts.Menu().Clear();
	m_scripts.ClearLastScript();
	for(int index = m_BtnText.GetSize() - 1; index >= 0; --index)
	{
		const int command = m_BtnText.GetKeyAt(index);
		if(command >= ID_SCRIPT_BASE + 1 && command <= ID_SCRIPT_BASE + SCRIPT_COMMAND_COUNT)
			m_BtnText.RemoveAt(index);
	}

	for(size_t groupIndex = 0; groupIndex < _Settings.m_hotkey_groups.size(); ++groupIndex)
	{
		CHotkeysGroup& group = _Settings.m_hotkey_groups[groupIndex];
		if(group.m_reg_name != L"Scripts") continue;
		group.m_hotkeys.clear();
	}
}

void CMainFrame::InitScriptHotkey(ScriptDescriptor& script)
{
	if(script.commandId < 1 || script.commandId > SCRIPT_COMMAND_COUNT)
		return;
	const int commandId = ID_SCRIPT_BASE + script.commandId;
	std::vector<CHotkeysGroup>& hotkey_groups = _Settings.m_hotkey_groups;
	for(unsigned int i = 0; i < hotkey_groups.size(); ++i)
	{
		if(hotkey_groups.at(i).m_reg_name == L"Scripts")
		{
			// Script UID remains stable across a rename and has no machine path.
			const CString identity = L"script:" + script.uid;
			CHotkey ScriptsHotkey(identity,
				script.name,
				NULL,
				static_cast<WORD>(commandId),
				NULL,
				script.relativePath);
			hotkey_groups.at(i).m_hotkeys.push_back(ScriptsHotkey);
		}
	}
}

void CMainFrame::RegisterPluginHotkey(CString guid, UINT cmd, CString name)
{
	if(cmd > 0xffffu)
		return;
	std::vector<CHotkeysGroup>& hotkey_groups = _Settings.m_hotkey_groups;
	for(unsigned int i = 0; i < hotkey_groups.size(); ++i)
	{
		if(hotkey_groups.at(i).m_reg_name == L"Plugins")
		{
			CHotkey PluginsHotkey(guid,
				name,
				NULL,
				static_cast<WORD>(cmd),
				NULL);
			hotkey_groups.at(i).m_hotkeys.push_back(PluginsHotkey);
		}
	}
}

//
// Idea by Sclex
//
void CMainFrame::ChangeNBSP(MSHTML::IHTMLElementPtr elem)
{
	MSHTML::IHTMLElementPtr fbwBody = MSHTML::IHTMLDocument3Ptr(m_doc->m_body.Document())->getElementById(L"fbw_body");
	if (fbwBody	&& elem && fbwBody->contains(elem))
	{
		// save caret position
		MSHTML::IHTMLTxtRangePtr tr1;
		int offset = 0;
		MSHTML::IHTMLTxtRangePtr sel(m_doc->m_body.Document()->selection->createRange());
		if (sel)
		{
			tr1 = sel->duplicate();
			if (tr1)
			{
				tr1->moveToElementText(elem);
				tr1->setEndPoint(L"EndToStart",sel);
				CString s = tr1->text;
				offset = s.GetLength();
				// special fix for strange MSHTML bug (inline image present in html code)
				CString s2 = tr1->htmlText;
				int l = 0;
				int imagePos = 0;
				while ((imagePos = s2.Find(L"<IMG", imagePos)) != -1)
				{
					++l;
					imagePos += 4;
				}
				offset += (l * 3);
			}
		}

		MSHTML::IHTMLDOMNodePtr el = MSHTML::IHTMLDOMNodePtr(elem)->firstChild;

		CString s;
		int numChanges = 0;

		while (el && el!=elem)
		{
			if (el->nodeType==3)
			{
				try { s = el->nodeValue; } catch(...) { break; }
				int n = s.Replace( L"\u00A0", _Settings.GetNBSPChar());
				int k = s.Replace( L"<p>\u00A0<p>", L"<p><p>");
				if (n || k)
				{
					numChanges += n + k;
					el->nodeValue = s.AllocSysString();
				}
			}
			if (el->firstChild)
				el=el->firstChild;
			else
			{
				while (el && el!=elem && el->nextSibling==NULL) el=el->parentNode;
				if (el && el!=elem) el=el->nextSibling;
			}
		}

		if (numChanges)
		{
			m_doc->AdvanceDocVersion(numChanges);

			// restore caret position
			if (tr1)
			{
				tr1->moveToElementText(elem);
				tr1->collapse(VARIANT_TRUE);
				if (offset==0)
				{
					tr1->move(L"character",1);
					tr1->move(L"character",-1);
				}
				else tr1->move(L"character",offset);
				tr1->select();
			}
		}
	}
}

void CMainFrame::RemoveLastUndo()
{
	// remove last undo operation
	IServiceProviderPtr serviceProvider = IServiceProviderPtr(m_doc->m_body.Document());
	CComPtr<IOleUndoManager> undoManager;
	CComPtr<IOleUndoUnit> undoUnit[10];
	CComPtr<IEnumOleUndoUnits> undoUnits;
	if (SUCCEEDED(serviceProvider->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, (void **) &undoManager)))
	{
		undoManager->EnumUndoable(&undoUnits);
		if (undoUnits)
		{
			ULONG numUndos = 0;
			undoUnits->Next(10, &undoUnit[0], &numUndos);
			// delete whole stack
			undoManager->DiscardFrom(NULL);
			// restore all except previous
			if (numUndos)
				for (ULONG i=0; i<numUndos-1; i++)
					undoManager->Add(undoUnit[i]);
		}
	}
}

// added by SeNS: try to load incorrect XML directly to Scintilla
bool CMainFrame::LoadToScintilla(CString filename)
{
	bool result = false;
	bool isUTF8 = true;
	CString enc;
	ShowView(SOURCE);

	CString src(L"");
	std::ifstream load;
	load.open(filename);
	if (load.is_open())
	try
	{
		std::vector<char> buffer(65535);
		do
		{
			load.getline(buffer.data(), 65535, '\n');
			if (!strstr(buffer.data(), "<?xml version="))
			{
				src += CA2W(buffer.data(), 1251);
				src += L"\r\n";
			}
			// try to detect encoding
			else
			{
				enc = buffer.data();
				enc.MakeLower();
				int pos = enc.Find(L"encoding");
				if (pos >=0)
				{
					enc = enc.Mid(pos+10, enc.GetLength()-pos-13);
					if (enc != L"utf-8") isUTF8 = false;
				}
				else enc.SetString(L"utf-8");
			}
		}
		while (!load.eof());
		load.close();

		// send document to Scintilla
		m_source.SendMessage(SCI_CLEARALL);
		if (isUTF8)
		{
			CT2A s (src, 1251);
			m_source.SendMessage(SCI_APPENDTEXT, strlen(s),(LPARAM)(LPSTR)s);
		}
		else
		{
			CT2A s (src, CP_UTF8);
			m_source.SendMessage(SCI_APPENDTEXT, strlen(s),(LPARAM)(LPSTR)s);
		}
		m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
		m_source.SendMessage(SCI_SETSAVEPOINT);

		SciGotoWrongTag();

		m_bad_xml = true;
		m_bad_filename = filename;
		m_doc->m_encoding = enc;

		result = true;
	}
	catch(...) {};
	return result;
}

namespace {
class StatusBarScintillaTextReader : public Fb2SourceTextReader
{
public:
	explicit StatusBarScintillaTextReader(HWND source) : m_source(source) {}
	std::size_t Length() const { return static_cast<std::size_t>(::SendMessage(m_source, SCI_GETLENGTH, 0, 0)); }
	void Read(std::size_t position, std::size_t length, std::string& text) const
	{
		text.assign(length, '\0');
		Sci_TextRange range = {};
		range.chrg.cpMin = static_cast<sptr_t>(position);
		range.chrg.cpMax = static_cast<sptr_t>(position + length);
		range.lpstrText = &text[0];
		::SendMessage(m_source, SCI_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&range));
		text.resize(strlen(text.c_str()));
	}
private:
	HWND m_source;
};

CString CharacterInspectorText(unsigned int codePoint)
{
	CString text;
	text.Format(L"U+%04X  &#%u;", codePoint, codePoint);
	return text;
}

CString SourceBreadcrumb(HWND source, int caret)
{
	StatusBarScintillaTextReader reader(source);
	Fb2SourceStructuralContextResolver resolver;
	const Fb2SourceStructuralContext context = resolver.Resolve(reader, static_cast<std::size_t>(caret), 0);
	CString result;
	if(context.breadcrumbTruncated) result = L"…";
	for(std::vector<std::string>::const_iterator item = context.breadcrumb.begin(); item != context.breadcrumb.end(); ++item) {
		CA2W name(item->c_str(), CP_UTF8);
		result += L"/";
		result += static_cast<LPCWSTR>(name);
	}
	return result;
}

int SourceSelectionWordCount(HWND source, int start, int end)
{
	if(start >= end) return 0;
	std::vector<char> text(static_cast<std::size_t>(end - start) + 1, '\0');
	Sci_TextRange range = {};
	range.chrg.cpMin = start; range.chrg.cpMax = end; range.lpstrText = &text[0];
	::SendMessage(source, SCI_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&range));
	const sptr_t length = static_cast<sptr_t>(strlen(text.data()));
	return FBEStatusBar::CountUtf8Words(std::string(text.data(), static_cast<std::size_t>(length)));
}
}

bool CMainFrame::CurrentOverwriteMode() const
{
	return m_editor_view_state.Current() == SOURCE ? m_last_sci_ovr :
		m_editor_view_state.Current() == BODY ? m_last_ie_ovr : false;
}

void CMainFrame::RefreshStatusMainPane()
{
	if (!m_status.IsWindow()) return;
	m_status.SetPaneText(ID_DEFAULT_PANE,
		m_status_state.EffectiveMainText(m_incsearch != 0, m_is_fail, m_is_str));
}

LRESULT CMainFrame::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&)
{
	// Apply only resolved defaults; explicit BODY colours and background images
	// remain document-editor settings and are preserved by Doc::ApplyConfChanges.
	if(m_doc)
		m_doc->ApplyConfChanges();
	if(_Settings.GetXmlSrcColorPalette() == XML_SRC_COLOR_PALETTE_SYSTEM)
		ApplyXmlSourceEditorChanges(false);
	if(m_document_tree.IsWindow() && m_document_tree.m_tree.m_tree.IsWindow())
		m_document_tree.m_tree.m_tree.SetBkColor(ThemeManager::WindowColor());
	RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
	return 0;
}

void CMainFrame::SetValidationStatus(FBEStatusBar::ValidationStatus status)
{
	if (m_status_state.Validation() == status) return;
	m_status_state.SetValidation(status);
	if (m_status.IsWindow()) m_status.SetPaneText(ID_PANE_VALIDATION, m_doc ? GetStatusValidationText() : L"");
	UpdateStatusBarLayout();
}

void CMainFrame::ResetValidationStatus()
{
	SetValidationStatus(FBEStatusBar::ValidationStatus::Unknown);
}

void CMainFrame::ResetStatusForDocument()
{
	m_status_state.ResetForDocument();
	RefreshStatusMainPane();
	UpdateStatusBar();
}

void CMainFrame::SetStatusContext(const CString& text)
{
	m_status_state.SetContext(text);
	RefreshStatusMainPane();
}

void CMainFrame::SetTransientStatus(const CString& text)
{
	m_status_state.SetTransient(text, ::GetTickCount());
	RefreshStatusMainPane();
}

CString CMainFrame::GetStatusValidationText() const
{
	const bool fbd = m_doc && m_doc->GetDocumentFileType() == FictionBookFileType::Fbd;
	const wchar_t* type = fbd ? L"FBD" : L"FB2";
	const FBEStatusBar::ValidationStatus validation = m_status_state.Validation();
	const wchar_t* state = validation == FBEStatusBar::ValidationStatus::Valid ? L"OK" :
		validation == FBEStatusBar::ValidationStatus::Invalid ? L"!" : L"?";
	CString text;
	text.Format(L"%s: %s", type, state);
	return text;
}

UINT CMainFrame::StatusPaneAt(POINT point) const
{
	const UINT panes[] = { ID_PANE_POSITION, ID_PANE_SELECTION, ID_PANE_CHAR, ID_PANE_ENCODING, ID_PANE_VALIDATION, ID_PANE_INS };
	for(size_t i = 0; i < sizeof(panes) / sizeof(panes[0]); ++i) {
		CRect rect;
		if(m_status.GetPaneRect(panes[i], &rect) && rect.PtInRect(point)) return panes[i];
	}
	return 0;
}

void CMainFrame::ToggleStatusPaneVisibility(UINT command)
{
	if(command < ID_STATUS_PANE_POSITION || command > ID_STATUS_PANE_INSERT_MODE) return;
	const FBEStatusBar::Pane pane = static_cast<FBEStatusBar::Pane>(command - ID_STATUS_PANE_POSITION);
	DWORD panes = FBEStatusBar::TogglePaneVisibility(_Settings.StatusBarPanes(), pane);
	_Settings.SetStatusBarPanes(panes, true);
	UpdateStatusBarLayout();
}

LRESULT CMainFrame::OnStatusPaneVisibility(WORD, WORD command, HWND, BOOL&)
{
	ToggleStatusPaneVisibility(command);
	return 0;
}

LRESULT CMainFrame::OnStatusBarClick(int, LPNMHDR hdr, BOOL& bHandled)
{
	if(hdr->hwndFrom != m_status) { bHandled = FALSE; return 0; }
	const UINT pane = StatusPaneAt(reinterpret_cast<LPNMMOUSE>(hdr)->pt);
	if(FBEStatusBar::ClickAction(pane == ID_PANE_VALIDATION ? FBEStatusBar::Validation : FBEStatusBar::Position) == FBEStatusBar::Validate)
		OnFileValidate(0, ID_FILE_VALIDATE, NULL, bHandled);
	return 0;
}

LRESULT CMainFrame::OnStatusBarDoubleClick(int, LPNMHDR hdr, BOOL& bHandled)
{
	if(hdr->hwndFrom != m_status) { bHandled = FALSE; return 0; }
	const UINT pane = StatusPaneAt(reinterpret_cast<LPNMMOUSE>(hdr)->pt);
	const FBEStatusBar::Action action = FBEStatusBar::DoubleClickAction(
		pane == ID_PANE_INS ? FBEStatusBar::InsertMode : pane == ID_PANE_CHAR ? FBEStatusBar::Character : FBEStatusBar::Position,
		m_editor_view_state.Current() == SOURCE, m_editor_view_state.Current() == BODY && m_doc != NULL);
	if(action == FBEStatusBar::ToggleSourceOverwrite) {
			m_source.SendMessage(SCI_SETOVERTYPE, !CurrentOverwriteMode());
			m_last_sci_ovr = m_source.SendMessage(SCI_GETOVERTYPE) != 0;
		UpdateStatusBar();
	} else if(action == FBEStatusBar::ToggleBodyOverwrite) {
		m_doc->m_body.ExecCommand(IDM_OVERWRITE);
		UpdateStatusBar();
	} else if(action == FBEStatusBar::CopyUnicodeReference) {
		CString text; m_status.GetPaneText(ID_PANE_CHAR, text);
		const std::wstring reference = FBEStatusBar::DecimalXmlReference(static_cast<LPCWSTR>(text));
		if(!reference.empty() && ::OpenClipboard(m_hWnd)) {
			const SIZE_T bytes = static_cast<SIZE_T>(reference.length() + 1) * sizeof(wchar_t);
			HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
			if(memory) {
				memcpy(::GlobalLock(memory), reference.c_str(), bytes);
				::GlobalUnlock(memory); ::EmptyClipboard();
				if(!::SetClipboardData(CF_UNICODETEXT, memory)) ::GlobalFree(memory);
			}
			::CloseClipboard();
		}
	}
	return 0;
}

LRESULT CMainFrame::OnStatusBarRightClick(int, LPNMHDR hdr, BOOL& bHandled)
{
	if(hdr->hwndFrom != m_status) { bHandled = FALSE; return 0; }
	CMenu menu; menu.CreatePopupMenu();
	const UINT commands[] = { ID_STATUS_PANE_POSITION, ID_STATUS_PANE_SELECTION, ID_STATUS_PANE_CHARACTER, ID_STATUS_PANE_ENCODING, ID_STATUS_PANE_VALIDATION, ID_STATUS_PANE_INSERT_MODE };
	const UINT strings[] = { IDS_STATUS_PANE_POSITION, IDS_STATUS_PANE_SELECTION, IDS_STATUS_PANE_CHARACTER, IDS_STATUS_PANE_ENCODING, IDS_STATUS_PANE_VALIDATION, IDS_STATUS_PANE_INSERT_MODE };
	const DWORD panes = _Settings.StatusBarPanes();
	for(size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
		wchar_t label[MAX_LOAD_STRING + 1] = {};
		FbeLoadString(_Module.GetResourceInstance(), strings[i], label, MAX_LOAD_STRING);
		menu.AppendMenu(MF_STRING | (panes & (1 << i) ? MF_CHECKED : MF_UNCHECKED), commands[i], label);
	}
	POINT point = reinterpret_cast<LPNMMOUSE>(hdr)->pt; m_status.ClientToScreen(&point);
	const UINT command = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, m_hWnd);
	ToggleStatusPaneVisibility(command);
	return 0;
}

void CMainFrame::UpdateStatusBarLayout()
{
	if (!m_status.IsWindow())
		return;
	CClientDC dc(m_status);
	HFONT font = m_status.GetFont();
	HFONT oldFont = font ? dc.SelectFont(font) : NULL;
	const int padding = MulDiv(12, m_current_dpi ? m_current_dpi : 96, 96);
	auto measure = [&](UINT pane) -> int {
		CString text;
		m_status.GetPaneText(pane, text);
		if (text.IsEmpty()) return 0;
		SIZE size = {};
		dc.GetTextExtent(text, text.GetLength(), &size);
		return size.cx + padding;
	};
	int widths[FBEStatusBar::PaneCount] = {
		measure(ID_PANE_POSITION), measure(ID_PANE_SELECTION), measure(ID_PANE_CHAR),
		measure(ID_PANE_ENCODING), measure(ID_PANE_VALIDATION), measure(ID_PANE_INS)
	};
	CRect rc;
	m_status.GetClientRect(&rc);
	const int defaultMinimum = MulDiv(120, m_current_dpi ? m_current_dpi : 96, 96);
	FBEStatusBar::ApplyPaneVisibility(_Settings.StatusBarPanes(), rc.Width() - defaultMinimum, widths);
	m_status.SetPaneWidth(ID_PANE_POSITION, widths[FBEStatusBar::Position]);
	m_status.SetPaneWidth(ID_PANE_SELECTION, widths[FBEStatusBar::Selection]);
	m_status.SetPaneWidth(ID_PANE_CHAR, widths[FBEStatusBar::Character]);
	m_status.SetPaneWidth(ID_PANE_ENCODING, widths[FBEStatusBar::Encoding]);
	m_status.SetPaneWidth(ID_PANE_VALIDATION, widths[FBEStatusBar::Validation]);
	m_status.SetPaneWidth(ID_PANE_INS, widths[FBEStatusBar::InsertMode]);
	if (oldFont) dc.SelectFont(oldFont);
}

void CMainFrame::UpdateStatusBar()
{
	if (!m_status.IsWindow())
		return;
	CString position, selection, character, encoding;
	if (m_doc && !m_doc->m_encoding.IsEmpty())
		encoding = m_doc->m_encoding;
	if (m_editor_view_state.Current() == SOURCE)
	{
		const int caret = m_source.SendMessage(SCI_GETCURRENTPOS);
		const int line = m_source.SendMessage(SCI_LINEFROMPOSITION, caret);
		const int lineStart = m_source.SendMessage(SCI_POSITIONFROMLINE, line);
		const int column = m_source.SendMessage(SCI_COUNTCHARACTERS, lineStart, caret);
		wchar_t positionFormat[MAX_LOAD_STRING + 1] = {};
		FbeLoadString(_Module.GetResourceInstance(), IDS_STATUS_POSITION, positionFormat, MAX_LOAD_STRING);
		position.Format(positionFormat, line + 1, m_source.SendMessage(SCI_GETLINECOUNT), column + 1);
		SetStatusContext(SourceBreadcrumb(m_source.m_hWnd, caret));
		const int selectionStart = m_source.SendMessage(SCI_GETSELECTIONSTART);
		const int selectionEnd = m_source.SendMessage(SCI_GETSELECTIONEND);
		if (selectionStart != selectionEnd)
		{
			wchar_t selectionFormat[MAX_LOAD_STRING + 1] = {};
			FbeLoadString(_Module.GetResourceInstance(), IDS_STATUS_SELECTION, selectionFormat, MAX_LOAD_STRING);
			const int selectedChars = m_source.SendMessage(SCI_COUNTCHARACTERS, selectionStart, selectionEnd);
			const int startLine = m_source.SendMessage(SCI_LINEFROMPOSITION, selectionStart);
			const int endLine = m_source.SendMessage(SCI_LINEFROMPOSITION, selectionEnd);
			const bool endAtLineStart = selectionEnd == m_source.SendMessage(SCI_POSITIONFROMLINE, endLine);
			const int selectedLines = FBEStatusBar::SelectionLineCount(startLine, endLine, endAtLineStart);
			selection.Format(selectionFormat, selectedChars, SourceSelectionWordCount(m_source.m_hWnd, selectionStart, selectionEnd), selectedLines);
		}
		int inspectedPosition = selectionStart != selectionEnd ? selectionStart :
			(caret > 0 ? m_source.SendMessage(SCI_POSITIONBEFORE, caret) : -1);
		if (inspectedPosition >= 0 && inspectedPosition < m_source.SendMessage(SCI_GETLENGTH))
		{
			char bytes[5] = {};
			for (int i = 0; i < 4; ++i) bytes[i] = static_cast<char>(m_source.SendMessage(SCI_GETCHARAT, inspectedPosition + i));
			const int byteCount = UTF8_CHAR_LEN(bytes[0]);
			if (byteCount > 0 && byteCount <= 4)
			{
				CA2W wide(bytes, CP_UTF8);
				unsigned int codePoint = 0;
				if (FBEStatusBar::FirstCodePoint(wide, ::lstrlenW(wide), codePoint)) character = CharacterInspectorText(codePoint);
			}
		}
	}
	else if (m_editor_view_state.Current() == BODY && m_doc && m_doc->m_body.Document())
	{
		try
		{
			MSHTML::IHTMLTxtRangePtr range(m_doc->m_body.Document()->selection->createRange());
			if (!range) throw _com_error(E_NOINTERFACE);
			MSHTML::IHTMLTxtRangePtr copy(range->duplicate());
			if (!copy) throw _com_error(E_NOINTERFACE);
			CString text;
			text.SetString(copy->text);
			if (!text.IsEmpty())
			{
				wchar_t selectionFormat[MAX_LOAD_STRING + 1] = {};
				FbeLoadString(_Module.GetResourceInstance(), IDS_STATUS_SELECTION, selectionFormat, MAX_LOAD_STRING);
				int words = 0, lines = 1; bool inWord = false;
				for (int i = 0; i < text.GetLength(); ++i) {
					const wchar_t c = text[i];
					const bool word = iswalnum(c) || c == L'_';
					if (word && !inWord) ++words;
					inWord = word;
					if (c == L'\n') ++lines;
				}
				selection.Format(selectionFormat, text.GetLength(), words, lines);
			}
			if (text.IsEmpty())
			{
				if (copy->moveStart(L"character", -1) < 0)
					text.SetString(copy->text);
			}
			unsigned int codePoint = 0;
			if (FBEStatusBar::FirstCodePoint(text, text.GetLength(), codePoint)) character = CharacterInspectorText(codePoint);
		}
		catch (const _com_error&) { character.Empty(); }
	}
	m_status.SetPaneText(ID_PANE_POSITION, position);
	m_status.SetPaneText(ID_PANE_SELECTION, selection);
	m_status.SetPaneText(ID_PANE_CHAR, character);
	m_status.SetPaneText(ID_PANE_ENCODING, encoding);
	m_status.SetPaneText(ID_PANE_VALIDATION, m_doc ? GetStatusValidationText() : L"");
	m_status.SetPaneText(ID_PANE_INS, CurrentOverwriteMode() ? strOVR : strINS);
	UpdateStatusBarLayout();
}

void CMainFrame::DisplayCharCode()
{
	UpdateStatusBar();
}
