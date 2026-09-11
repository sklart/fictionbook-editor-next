// MainFrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "document\PendingDocument.h"
#include "document\DocumentLoader.h"
#include "document\DocumentOpenSource.h"
#include "document\DocumentSavePlan.h"
#include "document\ui\DocumentFileDialogs.h"
#include "archive\ui\ArchiveOpenCoordinator.h"

#include "MainFrm.h"
#include "AboutBox.h"
#include "..\\common\\ModernFileDialog.h"
#include "settings\\ui\\SettingsDlg.h"
#include "Settings.h"
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
#include "scripts\\ScriptCatalog.h"
#include "scripts\\ScriptCommandRegistry.h"
#include "source\\BodySourceSelectionTransfer.h"
#include "source\\SourceDocumentTransfer.h"
#include "XmlDeclaration.h"
#include "..\\common\\DeploymentContext.h"
#include "..\\common\\RuntimeLocalizationCommon.h"
#include "toolbars\\PortableToolbarStore.h"
#include "toolbars\\ToolbarLayoutAdapter.h"
#include "toolbars\\ToolbarFactory.h"
#include "toolbars\\TableToolbarCommands.h"
#include <string>
#include <vector>
#include <algorithm>
#include <psapi.h>



static const UINT_PTR RECOVERY_TIMER_ID = 0xFBE;
static const UINT_PTR IMAGE_IMPORT_TEST_TIMER_ID = 0xFBF;
static const UINT RECOVERY_INTERVAL_MS = 2 * 60 * 1000;
static bool IsFbeTestScenario(const wchar_t* expectedScenario);
static SourceEditorConfig BuildSourceEditorConfig();
typedef FbeArchive::ResolvedDocument ResolvedOpenDocument;


namespace
{
using ToolbarFactory::AutoSizeToolbar;
using ToolbarFactory::ImageListHasMaskPlane;
using ToolbarFactory::SetDialogFontForToolbarRow;
static PluginManager g_pluginManager;
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

static HRESULT CreateBundledPluginInstance(const CLSID& clsid, IUnknownPtr& instance)
{
	return g_pluginManager.CreateInstance(clsid, instance);
}
}

// The detailed ShowSource profile is intentionally diagnostic-only.  It is
// populated by the internal benchmark (-b) and has no work in the normal UI
// hot path beyond the disabled branch in Mark().
struct SourceProfileSample
{
	CStringA phase;
	double elapsedMilliseconds;
};

static std::vector<SourceProfileSample> g_show_source_profile;

class ShowSourcePhaseProfiler
{
public:
	ShowSourcePhaseProfiler() : m_enabled(!AU::_ARGS.source_memory_benchmark_path.IsEmpty()), m_frequency(0), m_start(0)
	{
		if (m_enabled)
		{
			LARGE_INTEGER frequency = {};
			LARGE_INTEGER start = {};
			::QueryPerformanceFrequency(&frequency);
			::QueryPerformanceCounter(&start);
			m_frequency = frequency.QuadPart;
			m_start = start.QuadPart;
			g_show_source_profile.clear();
		}
	}

	void Mark(const char* phase) const
	{
		if (!m_enabled)
			return;
		LARGE_INTEGER now = {};
		::QueryPerformanceCounter(&now);
		SourceProfileSample sample = {};
		sample.phase = phase;
		sample.elapsedMilliseconds = (now.QuadPart - m_start) * 1000.0 / m_frequency;
		g_show_source_profile.push_back(sample);
	}

private:
	bool m_enabled;
	LONGLONG m_frequency;
	LONGLONG m_start;
};

struct ProcessMemorySnapshot
{
	SIZE_T privateBytes;
	SIZE_T workingSetBytes;
	SIZE_T committedBytes;
	SIZE_T reservedBytes;
};

static ProcessMemorySnapshot GetProcessMemorySnapshot()
{
	ProcessMemorySnapshot snapshot = {};
	PROCESS_MEMORY_COUNTERS_EX counters = {};
	counters.cb = sizeof(counters);
	if (::GetProcessMemoryInfo(::GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
	{
		snapshot.privateBytes = counters.PrivateUsage;
		snapshot.workingSetBytes = counters.WorkingSetSize;
	}

	SYSTEM_INFO systemInfo = {};
	::GetSystemInfo(&systemInfo);
	for (BYTE* address = NULL; address < systemInfo.lpMaximumApplicationAddress; )
	{
		MEMORY_BASIC_INFORMATION memory = {};
		const SIZE_T result = ::VirtualQuery(address, &memory, sizeof(memory));
		if (result == 0)
			break;
		if (memory.State == MEM_COMMIT)
			snapshot.committedBytes += memory.RegionSize;
		else if (memory.State == MEM_RESERVE)
			snapshot.reservedBytes += memory.RegionSize;
		BYTE* const nextAddress = static_cast<BYTE*>(memory.BaseAddress) + memory.RegionSize;
		if (nextAddress <= address)
			break;
		address = nextAddress;
	}
	return snapshot;
}

extern CSettings _Settings;

static void TracePluginDiagnostic(const wchar_t* type, const CLSID& clsid, const wchar_t* operation, HRESULT result, int domReturned)
{
	wchar_t clsidText[64] = {};
	::StringFromGUID2(clsid, clsidText, _countof(clsidText));
	CString details;
	details.Format(L"type=%s; clsid=%s; operation=%s; dom-returned=%d", type, clsidText, operation, domReturned);
	if (FAILED(result)) StartupTrace::HResult(L"plugin", L"P210", result, details);
	else StartupTrace::Event(L"plugin", L"P210", details);
}

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

static bool IsDiagnosticTraceEnabledForNextLaunch()
{
	return StartupTrace::IsEnabledForNextLaunch();
}

static bool SetDiagnosticTraceEnabledForNextLaunch(bool enabled)
{
	return StartupTrace::SetEnabledForNextLaunch(enabled);
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
static void RefreshBundledPluginMenuTexts(HMENU menu, const TCHAR* type, UINT commandBase)
{
	if(menu == NULL)
		return;
	int commandOffset = 0;
	const int commandCapacity = static_cast<int>((commandBase == ID_IMPORT_BASE ? ID_PLUGIN_IMPORT_LAST : ID_PLUGIN_EXPORT_LAST) - commandBase + 1);
	const std::vector<PluginDescriptor>& plugins = g_pluginManager.GetPlugins();
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
	// Runtime integration uses an explicitly supplied output only in this
	// narrowly scoped test mode; normal Save As always shows the native dialog.
	if (IsFbeTestScenario(L"archive-rar-save-runtime"))
	{
		wchar_t testPath[MAX_PATH] = {};
		const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SAVE_PATH", testPath, _countof(testPath));
		if (length && length < _countof(testPath))
		{
			encoding = _Settings.KeepEncoding() ? m_doc->m_encoding : _Settings.GetDefaultEncoding();
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
  if ((IsSourceActive() && !SourceToHTML()) || m_bad_xml) // added by SeNS: do not save bad xml!
    return FAIL;

  const DocumentSavePlan savePlan = DocumentSavePlan::Create(askname, m_doc->m_namevalid, m_document_session.Location());

  if (savePlan.target == DocumentSaveTarget::CurrentArchive)
  {
	std::vector<unsigned char> serialized;
	if (!m_doc->SerializeToMemory(serialized, m_document_session.Location().documentType)) return FAIL;
	if (IsFbeTestScenario(L"archive-runtime") || IsFbeTestScenario(L"archive-rar-save-runtime"))
	{
		const std::vector<unsigned char>::const_iterator marker = std::search(serialized.begin(), serialized.end(),
			"ARCHIVE_RUNTIME_AFTER", "ARCHIVE_RUNTIME_AFTER" + strlen("ARCHIVE_RUNTIME_AFTER"));
		::SetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SERIALIZED_CHANGED", marker != serialized.end() ? L"1" : L"0");
	}
	FbeArchive::Error error;
	DocumentLocation savedArchiveLocation;
	if (!FbeArchive::SaveDocument(m_document_session.Location(), serialized, savedArchiveLocation, error))
	{
		if (IsFbeTestScenario(L"archive-recovery-external-verify"))
		{
			wchar_t diagnostic[16] = {};
			swprintf_s(diagnostic, _countof(diagnostic), L"%d", static_cast<int>(error.code));
			::SetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SAVE_ERROR", diagnostic);
		}
		if (IsFbeTestScenario(L"archive-runtime"))
		{
			wchar_t diagnostic[64] = {};
			swprintf_s(diagnostic, _countof(diagnostic), L"%d/%lu", static_cast<int>(error.code), error.systemError);
			::SetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_WRITE_ERROR", diagnostic);
		}
		FbeArchiveUi::ShowError(m_hWnd, error); return FAIL;
	}
	m_document_session.SavedArchive(savedArchiveLocation);
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
    m_doc->m_encoding=encoding;
    if (m_doc->Save(filename)) {
      m_doc->m_filename=filename;
	  m_document_session.SaveAsNormal(filename, m_doc->GetDocumentFileType());
	  if (wasFbd != IsFbdFile(filename)) ResetValidationStatus();
	  U::SetCurrentDirectoryToFile(filename);
      m_doc->m_namevalid=true;
	  FbeRecentDocuments::RememberNormalMruRecord(m_mru, filename);
	  CommitSuccessfulSave();
	  UpdateStatusBar();
      return OK;
    }
    return FAIL;
  }
  bool saved = m_doc->Save();

  if(saved)
  {
	  m_document_session.Saved();
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

	if (!IsFbeTestScenario(L"archive-mru-runtime") && !DiscardChanges())
	    return CANCELLED;

	PendingDocument pending(*this, m_doc);
	FB::Doc* doc = &pending.Document();
	if((filename.ReverseFind(L'\\') + 1) != -1 && (filename.ReverseFind(L'\\') + 1) < filename.GetLength() - 1)
	{
		doc->m_body.m_file_path = filename.Mid(0, filename.ReverseFind(L'\\') + 1);
		doc->m_body.m_file_name = filename.Mid(filename.ReverseFind(L'\\') + 1, filename.GetLength() - 1);
	}
  EnableWindow(FALSE);
  m_status.SetPaneText(ID_DEFAULT_PANE, FbeLoadRuntimeString(IDS_STATUS_LOADING));
	DocumentOpenSource source = archive ? DocumentOpenSource() : DocumentOpenSource::Normal(filename);
	if (archive) { source.location = resolved.location; source.rawBytes = resolved.rawBytes; }
	bool fLoaded = DocumentLoader::Load(*doc, m_view, source);
  EnableWindow(TRUE);
  if (!fLoaded)
  {
	  pending.Rollback();
	  if (LoadToScintilla(filename)) return OK;
	  return FAIL;
  }

  AttachDocument(doc);
  delete m_doc;
	m_doc=pending.Commit();
	 if (archive) m_document_session.OpenArchive(resolved.location); else m_document_session.OpenNormal(filename, m_doc->GetDocumentFileType());
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
		for(int i = 0; i < m_script_menu.Count(); ++i)
		{
			if(!m_script_menu.Item(i).isFolder)
			{
				::EnableMenuItem(scripts, ID_SCRIPT_BASE + m_script_menu.Item(i).commandId, MF_BYCOMMAND | MF_GRAYED);
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
		for (int i = 0; i < m_script_menu.Count(); ++i)
		{
			if(!m_script_menu.Item(i).isFolder)
			{
				::EnableMenuItem(scripts, ID_SCRIPT_BASE + m_script_menu.Item(i).commandId, MF_BYCOMMAND | MF_ENABLED);
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
	if(!m_status_msg.IsEmpty())
	{
		SetTransientStatus(m_status_msg);
		m_status_msg.Empty();
	}
	if (!m_status_transient.IsEmpty() && static_cast<LONG>(::GetTickCount() - m_status_transient_expiration) >= 0)
	{
		m_status_transient.Empty();
		RefreshStatusMainPane();
	}

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
	for(int index = 0; index < m_script_menu.Count(); ++index) {
		const ScriptDescriptor& script = m_script_menu.Item(index);
		if(!script.isFolder && script.commandId > 0)
			addCommand(ID_SCRIPT_BASE + script.commandId, script.name, script.relativePath);
	}
	std::sort(commands.begin(), commands.end(), [](const ScriptsToolbarCommand& left, const ScriptsToolbarCommand& right) {
		return left.name.CompareNoCase(right.name) < 0;
	});
	CScriptsToolbarCustomizeDlg dialog(m_ScriptsToolbar, commands, defaults, _Settings);
	dialog.DoModal(m_hWnd);
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
	if(DeploymentContext::RegistryPersistenceAllowed()) return;
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
		for(int scriptIndex = 0; scriptIndex < m_script_menu.Count(); ++scriptIndex)
		{
			const ScriptDescriptor& script = m_script_menu.Item(scriptIndex);
			if(script.isFolder || script.commandId < 1) continue;
			const int command = ID_SCRIPT_BASE + script.commandId;
			bool found = false;
			for(int buttonIndex = 0; buttonIndex < available.GetSize(); ++buttonIndex)
				if(available[buttonIndex].idCommand == command) { found = true; break; }
			if(found) continue;
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
		if(!item.relativePath.IsEmpty())
		{
			command = 0;
			for(int scriptIndex = 0; scriptIndex < m_script_menu.Count(); ++scriptIndex)
				if(!m_script_menu.Item(scriptIndex).isFolder && m_script_menu.Item(scriptIndex).relativePath == item.relativePath && m_script_menu.Item(scriptIndex).commandId > 0)
				{
					command = ID_SCRIPT_BASE + m_script_menu.Item(scriptIndex).commandId;
					break;
				}
		}
		item.command = command; // deleted scripts remain unresolved and are ignored by the adapter.
	}
	std::vector<TBBUTTON> catalogButtons(catalog.GetSize());
	for(int index = 0; index < catalog.GetSize(); ++index) catalogButtons[index] = catalog[index];
	ToolbarLayoutAdapter::Apply(target, saved, catalogButtons);

	if(scriptsToolbar && !layout.lastScript.IsEmpty())
		for(int scriptIndex = 0; scriptIndex < m_script_menu.Count(); ++scriptIndex)
			if(!m_script_menu.Item(scriptIndex).isFolder && m_script_menu.Item(scriptIndex).relativePath == layout.lastScript)
			{
				m_last_script = &m_script_menu.Item(scriptIndex);
				break;
			}
}

void CMainFrame::SavePortableToolbarLayout()
{
	if(DeploymentContext::RegistryPersistenceAllowed()) return;
	PortableToolbarLayout layout;
	layout.commandToolbarPresent = true; layout.scriptsToolbarPresent = true;
	ToolbarLayoutAdapter::Capture(m_CmdToolbar, layout.commands);
	ToolbarLayoutAdapter::Capture(m_ScriptsToolbar, layout.scripts);
	for(size_t index = 0; index < layout.scripts.size(); ++index)
	{
		PortableToolbarItem& item = layout.scripts[index];
		if(item.separator || item.command < ID_SCRIPT_BASE + 1 || item.command > ID_SCRIPT_BASE + SCRIPT_COMMAND_COUNT) continue;
		const int scriptId = item.command - ID_SCRIPT_BASE;
		for(int scriptIndex = 0; scriptIndex < m_script_menu.Count(); ++scriptIndex)
			if(!m_script_menu.Item(scriptIndex).isFolder && m_script_menu.Item(scriptIndex).commandId == scriptId) { item.command = 0; item.relativePath = m_script_menu.Item(scriptIndex).relativePath; break; }
	}
	if(m_last_script != NULL) layout.lastScript = m_last_script->relativePath;
	PortableToolbarStore::Save(layout);
}

void CMainFrame::InitPluginsType(HMENU hMenu, const TCHAR* type, UINT cmdbase, CSimpleArray<CLSID>& plist)
{
	const int commandCapacity = static_cast<int>((cmdbase == ID_IMPORT_BASE ? ID_PLUGIN_IMPORT_LAST : ID_PLUGIN_EXPORT_LAST) - cmdbase + 1);
	const std::vector<PluginDescriptor>& plugins = g_pluginManager.GetPlugins();
	for(size_t index = 0; index < plugins.size() && plist.GetSize() < commandCapacity; ++index)
	{
		const PluginDescriptor& plugin = plugins[index];
		if(plugin.type != type) continue;
		const int command = cmdbase + plist.GetSize();
		const CString menu = FbeLoadRuntimeStringByKey(plugin.menuKey, plugin.menu);
		plist.Add(plugin.clsid);
		::AppendMenu(hMenu, MF_STRING, command, menu);
		CString hs = menu;
		hs.Remove(L'&');
		const CString pluginType = FbeLoadRuntimeStringByKey(
			plugin.type == L"Import" ? L"fbe.hotkey.plugins.import" : L"fbe.hotkey.plugins.export",
			plugin.type);
		InitPluginHotkey(plugin.clsidText, command, pluginType + CString(L" | ") + hs);
		// check if an icon is available
		CString icon(plugin.icon);
		if(!icon.IsEmpty())
		{
			int cp = icon.ReverseFind(L',');
			int iconID;
			if(cp > 0 && _stscanf((const TCHAR *)icon + cp, L",%d", &iconID) == 1)
				icon.Delete(cp, icon.GetLength() - cp);
			else
				iconID = 0;

			// try load from file first
			HICON hIcon;
			if(::ExtractIconEx(icon, iconID, NULL, &hIcon, 1) > 0 && hIcon)
			{
				m_MenuBar.AddIcon(hIcon, command);
				::DestroyIcon(hIcon);
			}
		}
	}
	if(plist.GetSize() > 0) // delete placeholder from menu
	::RemoveMenu(hMenu, 0, MF_BYPOSITION);
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

void CMainFrame::InitPlugins()
{
	g_pluginManager.DiscoverBundledPlugins();
	ReleaseScriptResources();
	if (StartupTrace::Enabled())
	{
		StartupTrace::Event(L"plugin", L"P100", L"script directory resolved");
	}
	FbeScripts::Catalog scriptCatalog;
	scriptCatalog.Discover(_Settings.GetScriptsFolder(), L"*.js");
	const std::vector<ScriptDescriptor>& scriptCandidates = scriptCatalog.Items();
	for (size_t index = 0; index < scriptCandidates.size(); ++index)
	{
		const ScriptDescriptor& candidate = scriptCandidates[index];
		if (!candidate.isFolder)
		{
			ScriptDiscoveryRuntime runtime(this);
			if (!runtime.Started() || FAILED(ScriptLoad(candidate.path)) || !ScriptFindFunc(L"Run"))
				continue;
		}

		ScriptDescriptor script(candidate);
		const CString directory = candidate.isFolder ? candidate.path : candidate.path.Left(candidate.path.ReverseFind(L'\\') + 1);
		CString pictureName(candidate.path.Mid(candidate.path.ReverseFind(L'\\') + 1));
		if (!candidate.isFolder && pictureName.GetLength() >= 3) pictureName.Delete(pictureName.GetLength() - 3, 3);
		FbeScripts::VisualResource visual = m_script_visuals.Load(directory, pictureName);
		m_script_menu.Add(script, static_cast<FbeScripts::VisualResource&&>(visual));
	}
	if (StartupTrace::Enabled())
	{
		CString trace;
		trace.Format(L"script-count=%d", m_script_menu.Count());
		StartupTrace::Event(L"plugin", L"P110", trace);
	}
	StartupTrace::Event(L"plugin", L"P120", L"scripts collected");
	CString serializedCommandIds;
	if (m_script_menu.AssignCommandIds(SCRIPT_COMMAND_COUNT, _Settings.GetScriptCommandIds(), serializedCommandIds))
		_Settings.SetScriptCommandIds(serializedCommandIds);
	StartupTrace::Event(L"plugin", L"P130", L"scripts sorted");

	HMENU file = ::GetSubMenu(m_MenuBar.GetMenu(), 0);
	HMENU sub = ::GetSubMenu(file, 6);
	InitPluginsType(sub, L"Import", ID_IMPORT_BASE, m_import_plugins);
	StartupTrace::Event(L"plugin", L"P140", L"import plugins initialized");

	sub = ::GetSubMenu(file, 7);
	InitPluginsType(sub, L"Export", ID_EXPORT_BASE, m_export_plugins);
	StartupTrace::Event(L"plugin", L"P150", L"export plugins initialized");

	sub = ::GetSubMenu(file, 9);
	m_mru.SetMenuHandle(sub);
	RefreshMruEmptyStateText(m_mru);
	m_mru.SetMaxEntries(m_mru.m_nMaxEntries_Max - 1);
	if (DeploymentContext::RegistryPersistenceAllowed())
		m_mru.ReadFromRegistry(_Settings.GetKeyPath());
	else
		FbeRecentDocuments::ReadPortableMru(m_mru);
	m_mru.SetMaxEntries(m_mru.m_nMaxEntries_Max - 1);
	FbeRecentDocuments::RemoveLegacyArchiveMruEntries(m_mru);
	FbeRecentDocuments::AddArchiveMruRecordsToList(m_mru);
	StartupTrace::Event(L"plugin", L"P160", L"MRU initialized");

	// Scripts
	HMENU ManMenu = m_MenuBar.GetMenu();
	HMENU scripts = GetSubMenu(ManMenu, 6);

	while(::GetMenuItemCount(scripts) > 0)
	::RemoveMenu(scripts, 0, MF_BYPOSITION);

	if(m_script_menu.Count())
	{
		m_script_menu.Build(scripts,
			[](ScriptDescriptor&) {},
			[this](const ScriptDescriptor& script, const FbeScripts::VisualResource& visual, UINT command) {
				if (!script.isFolder && visual.icon != NULL)
					AddTbButton(m_ScriptsToolbar, script.name, command, TBSTATE_ENABLED, visual.icon);
				if (!script.isFolder)
				{
					TBBUTTONS catalog;
					bool available = GetAvailableButtons(m_ScriptsToolbar, catalog);
					for(int index = 0; available && index < catalog.GetSize(); ++index)
						if(catalog[index].idCommand == static_cast<int>(command)) { available = false; break; }
					if(available)
					{
						// Keep customization's available catalog aligned with the script
						// menu even when toolbar artwork could not be added.
						TBBUTTON button = {};
						button.iBitmap = I_IMAGENONE;
						button.idCommand = command;
						button.fsState = TBSTATE_ENABLED;
						button.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
						AddToolbarButton(m_ScriptsToolbar, button, script.name);
					}
				}
				if (visual.bitmap != NULL) m_MenuBar.AddBitmap(visual.bitmap, command);
				else if (visual.icon != NULL) m_MenuBar.AddIcon(visual.icon, command);
			});
		// Hotkey registration is catalog lifecycle, not menu rendering.  Keeping
		// it outside MenuBuilder's recursive traversal makes every discovered
		// script available to portable hotkey migration, including nested items.
		for(int index = 0; index < m_script_menu.Count(); ++index)
			if(!m_script_menu.Item(index).isFolder)
				InitScriptHotkey(m_script_menu.Item(index));
	}
	else
	{
		wchar_t buf[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(), IDS_NO_SCRIPTS, buf, MAX_LOAD_STRING);
		AppendMenu(scripts, MF_STRING | MF_DISABLED | MF_GRAYED, IDCANCEL, buf);
	}
	ApplyRuntimeMainFrameMenuLocalization(ManMenu);
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
  if (IsFbeTestScenario(L"editor-background-runtime"))
  {
    StartupTrace::AppendTestStartupBreadcrumb("plugins-init-skipped-runtime-test");
  }
  else
  {
    InitPlugins();
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
	  if (m_document_session.Location().IsArchive()) FbeRecentDocuments::RememberArchiveMruRecord(m_mru, m_document_session.Location());
	  else FbeRecentDocuments::RememberNormalMruRecord(m_mru, startupFileName);
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
	if (DeploymentContext::RegistryPersistenceAllowed())
		m_ScriptsToolbar.RestoreState(HKEY_CURRENT_USER, _Settings.GetKeyPath() + L"\\Toolbars", L"ScriptsToolbar");
	else
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
		  if (SetDiagnosticTraceEnabledForNextLaunch(false))
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
  if (DiscardChanges())
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
		FbeRecentDocuments::WriteRegistryMruWithoutArchive(m_mru, _Settings.GetKeyPath());
	else
		FbeRecentDocuments::WritePortableMru(m_mru);
    // save toolbars state
    CString tbs;
    REBARBANDINFO  rbi;
    memset(&rbi,0,sizeof(rbi));
    rbi.cbSize=sizeof(rbi);
    rbi.fMask=RBBIM_ID|RBBIM_SIZE|RBBIM_STYLE;
    int	  num_bands=m_rebar.GetBandCount();
    for (int i=0;i<num_bands;++i) {
      m_rebar.GetBandInfo(i,&rbi);
      CString	bi;
      bi.Format(_T("%d,%d,%d;"), static_cast<int>(rbi.wID), static_cast<int>(rbi.fStyle), static_cast<int>(rbi.cx));
      tbs+=bi;
    }

	// Save toolbar layout
	if (DeploymentContext::RegistryPersistenceAllowed())
	{
		m_CmdToolbar.SaveState(HKEY_CURRENT_USER, _Settings.GetKeyPath() + L"\\Toolbars", L"CommandToolbar");
		m_ScriptsToolbar.SaveState(HKEY_CURRENT_USER, _Settings.GetKeyPath() + L"\\Toolbars", L"ScriptsToolbar");
	}
	else
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
	return m_recovery.Save(m_doc, DocChanged(), sourceActive, m_bad_xml,
		sourceText.empty() ? NULL : sourceText.data(), sourceText.empty() ? 0 : sourceText.size() - 1, m_document_session.Location());
}
void CMainFrame::TryRestoreRecovery()
{
	FbeRecovery::RestoreCandidate candidate;
	if (!m_recovery.GetRestoreCandidate(_ARGV.GetSize() > 0, candidate)) return;

	if (!IsFbeTestScenario(L"archive-recovery-verify") && !IsFbeTestScenario(L"archive-recovery-external-verify") && U::MessageBox(MB_YESNO | MB_ICONQUESTION, IDS_RECOVERY_CAPTION, IDS_RECOVERY_MSG) != IDYES)
		return;

	if (LoadFile(candidate.snapshotPath) == OK)
	{
		if (candidate.archiveBacked)
		{
			m_document_session.RestoreArchive(candidate.archiveLocation);
			m_doc->m_filename = candidate.archiveLocation.storagePath;
			m_doc->m_namevalid = true;
			m_doc->SetDocumentFileType(candidate.archiveLocation.documentType);
		}
		else
		{
			m_doc->m_filename = L"Untitled.fb2";
			m_doc->m_namevalid = false;
		}
		m_doc->ResetSavePoint();
		if (m_bad_xml)
			m_bad_filename = L"Untitled.fb2";
		m_recovery.CompleteRestore();
	}
}

LRESULT CMainFrame::OnSettingChange(UINT, WPARAM, LPARAM, BOOL&)
{
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
			m_document_tree.m_tree.m_tree.SetBkColor(::GetSysColor(COLOR_WINDOW));
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

static bool IsFbeTestScenario(const wchar_t* expectedScenario)
{
	wchar_t testMode[4] = {}, scenario[64] = {};
	const DWORD testModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode));
	const DWORD scenarioLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SCENARIO", scenario, _countof(scenario));
	return testModeLength == 1 && testMode[0] == L'1' &&
		scenarioLength == wcslen(expectedScenario) && wcscmp(scenario, expectedScenario) == 0;
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

void CMainFrame::RunPortableStateTestScenario()
{
	const bool ordinaryWrite = IsFbeTestScenario(L"portable-state-write");
	const bool ordinaryRead = IsFbeTestScenario(L"portable-state-read");
	const bool emptyToolbarWrite = IsFbeTestScenario(L"portable-toolbar-empty-write");
	const bool emptyToolbarRead = IsFbeTestScenario(L"portable-toolbar-empty-read");
	const bool toolbarLayoutWrite = IsFbeTestScenario(L"portable-toolbar-layout-write");
	const bool toolbarLayoutRead = IsFbeTestScenario(L"portable-toolbar-layout-read");
	const bool missingScriptRead = IsFbeTestScenario(L"portable-toolbar-missing-script-read");
	const bool malformedToolbarRead = IsFbeTestScenario(L"portable-toolbar-malformed-read");
	const bool scriptsReload = IsFbeTestScenario(L"portable-scripts-reload");
	const bool legacyHotkeyRead = IsFbeTestScenario(L"portable-legacy-hotkey-read");
	const bool diagnosticCleanup = IsFbeTestScenario(L"portable-diagnostic-cleanup");
	if (!ordinaryWrite && !ordinaryRead && !emptyToolbarWrite && !emptyToolbarRead && !toolbarLayoutWrite && !toolbarLayoutRead && !missingScriptRead && !malformedToolbarRead && !scriptsReload && !legacyHotkeyRead && !diagnosticCleanup)
		return;

	const CString diagnosticsDirectory(DeploymentContext::DiagnosticsDirectory().c_str());
	const CString scriptsDirectory(DeploymentContext::UserScriptsDirectory().c_str());
	const CString diagnosticsMarker(diagnosticsDirectory + L"portable-state-sentinel.txt");
	const CString recoveryMarker(m_recovery.SnapshotPath());
	const CString reportPath(diagnosticsDirectory + L"portable-state-report.txt");
	const WORD portableStateHotkeyFlags = FVIRTKEY | FCONTROL | FSHIFT;
	const WORD portableStateHotkeyKey = VK_F24;
	const int portableStateToolbarWidth = 731;
	const UINT portableStateToolbarBandId = ATL_IDW_BAND_FIRST;
	if (DeploymentContext::CurrentMode() != DeploymentContext::Mode::Portable)
	{
		WritePortableStateTestText(reportPath, "phase=failed\nreason=not-portable\n");
		PostMessage(WM_CLOSE);
		return;
	}
	if (diagnosticCleanup)
	{
		// This narrow test hook exercises the public cleanup operation after the
		// portable startup trace has been opened.  The PowerShell regression seeds
		// more than ten completed sessions and checks a separate user directory.
		const StartupTrace::DiagnosticLogCleanupResult cleanup = StartupTrace::ClearOldLogSessions();
		CStringA report;
		report.Format("phase=diagnostic-cleanup\nportable=1\nsessions-found=%u\nsessions-deleted=%u\nfiles-deleted=%u\nfiles-failed=%u\nresult=%s\n",
			cleanup.sessionsFound, cleanup.sessionsFullyDeleted, cleanup.filesDeleted, cleanup.filesFailed,
			cleanup.filesFailed == 0 && cleanup.sessionsFailed == 0 && cleanup.sessionsPartiallyDeleted == 0 ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
		PostMessage(WM_CLOSE);
		return;
	}

	::CreateDirectory(scriptsDirectory, NULL);
	auto catalogButton = [&](HWND toolbar, int ordinal, TBBUTTON& button) -> bool
	{
		const int catalogIndex = m_aButtons.FindKey(toolbar);
		if(catalogIndex < 0) return false;
		TBBUTTONS catalog = m_aButtons.GetValueAt(catalogIndex);
		int found = 0;
		for(int index = 0; index < catalog.GetSize(); ++index)
			if((catalog[index].fsStyle & TBSTYLE_SEP) == 0 && catalog[index].idCommand != 0)
				if(found++ == ordinal) { button = catalog[index]; return true; }
		return false;
	};
	auto catalogButtonByCommand = [&](HWND toolbar, int command, TBBUTTON& button) -> bool
	{
		const int catalogIndex = m_aButtons.FindKey(toolbar);
		if(catalogIndex < 0) return false;
		TBBUTTONS catalog = m_aButtons.GetValueAt(catalogIndex);
		for(int index = 0; index < catalog.GetSize(); ++index)
			if(catalog[index].idCommand == command) { button = catalog[index]; return true; }
		return false;
	};
	auto hasButtons = [](CToolBarCtrl& toolbar, const TBBUTTON& first, const TBBUTTON& second, const TBBUTTON& third) -> bool
	{
		TBBUTTON current = {};
		return toolbar.GetButtonCount() == 3 &&
			toolbar.GetButton(0, &current) && current.idCommand == first.idCommand &&
			toolbar.GetButton(1, &current) && (current.fsStyle & TBSTYLE_SEP) != 0 && current.iBitmap == second.iBitmap &&
			toolbar.GetButton(2, &current) && current.idCommand == third.idCommand;
	};
	if (emptyToolbarWrite)
	{
		while(m_CmdToolbar.GetButtonCount() > 0) m_CmdToolbar.DeleteButton(0);
		while(m_ScriptsToolbar.GetButtonCount() > 0) m_ScriptsToolbar.DeleteButton(0);
		m_CmdToolbar.AutoSize();
		m_ScriptsToolbar.AutoSize();
		WritePortableStateTestText(reportPath, "phase=toolbar-empty-write\nresult=pass\n");
	}
	else if (emptyToolbarRead)
	{
		const bool empty = m_CmdToolbar.GetButtonCount() == 0 && m_ScriptsToolbar.GetButtonCount() == 0;
		CStringA report;
		report.Format("phase=toolbar-empty-read\nempty-toolbar=%d\nresult=%s\n", empty, empty ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (malformedToolbarRead)
	{
		const bool defaultsKept = m_CmdToolbar.GetButtonCount() > 0 && m_ScriptsToolbar.GetButtonCount() > 0;
		CStringA report;
		report.Format("phase=toolbar-malformed-read\ndefaults-kept=%d\nresult=%s\n", defaultsKept, defaultsKept ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (toolbarLayoutWrite || toolbarLayoutRead || missingScriptRead)
	{
		TBBUTTON commandFirst = {}, commandAdded = {}, alphaButton = {}, betaButton = {}, separator = {};
		separator.iBitmap = 13;
		separator.fsStyle = TBSTYLE_SEP;
		const bool commandCatalogReady = catalogButton(m_CmdToolbar, 0, commandFirst) &&
			catalogButton(m_CmdToolbar, 2, commandAdded);
		int alphaCommand = 0, betaCommand = 0;
		CStringA discoveredScripts;
		for(int index = 0; index < m_script_menu.Count(); ++index)
			if(!m_script_menu.Item(index).isFolder)
			{
				const ScriptDescriptor& script = m_script_menu.Item(index);
				CStringA entry;
				entry.Format("%ls:%d;", static_cast<LPCWSTR>(script.relativePath), script.commandId);
				discoveredScripts += entry;
				if(script.relativePath == L"test/alpha.js") alphaCommand = ID_SCRIPT_BASE + script.commandId;
				if(script.relativePath == L"test/beta.js") betaCommand = ID_SCRIPT_BASE + script.commandId;
		}
		const bool alphaInCatalog = alphaCommand != 0 && catalogButtonByCommand(m_ScriptsToolbar, alphaCommand, alphaButton);
		const bool betaInCatalog = betaCommand != 0 && catalogButtonByCommand(m_ScriptsToolbar, betaCommand, betaButton);
		const bool scriptsCatalogReady = alphaInCatalog && betaInCatalog;
		const bool catalogReady = commandCatalogReady && scriptsCatalogReady;
		if(toolbarLayoutWrite && catalogReady)
		{
			// This mirrors a real customization: remove a default command, add a
			// different one, and retain the resulting order on both toolbar rows.
			while(m_CmdToolbar.GetButtonCount() > 0) m_CmdToolbar.DeleteButton(0);
			m_CmdToolbar.AddButton(&commandAdded);
			m_CmdToolbar.AddButton(&separator);
			m_CmdToolbar.AddButton(&commandFirst);
			while(m_ScriptsToolbar.GetButtonCount() > 0) m_ScriptsToolbar.DeleteButton(0);
			m_ScriptsToolbar.AddButton(&alphaButton);
			m_ScriptsToolbar.AddButton(&separator);
			m_ScriptsToolbar.AddButton(&betaButton);
			for(int index = 0; index < m_script_menu.Count(); ++index)
				if(!m_script_menu.Item(index).isFolder && m_script_menu.Item(index).relativePath == L"test/beta.js")
					m_last_script = &m_script_menu.Item(index);
			m_CmdToolbar.AutoSize();
			m_ScriptsToolbar.AutoSize();
		}
		const bool commandLayout = catalogReady && hasButtons(m_CmdToolbar, commandAdded, separator, commandFirst);
		const bool scriptsLayout = catalogReady && hasButtons(m_ScriptsToolbar, alphaButton, separator, betaButton);
		const bool lastScriptIsBeta = m_last_script != NULL && m_last_script->relativePath == L"test/beta.js";
		TBBUTTON missingSeparator = {};
		const bool missingScriptSafe = alphaInCatalog && m_ScriptsToolbar.GetButtonCount() == 2 &&
			m_ScriptsToolbar.GetButton(0, &alphaButton) && alphaButton.idCommand == alphaCommand &&
			m_ScriptsToolbar.GetButton(1, &missingSeparator) && (missingSeparator.fsStyle & TBSTYLE_SEP) != 0 && m_last_script == NULL;
		CStringA report;
		report.Format("phase=toolbar-layout-%s\nscript-count=%d\ndiscovered-scripts=%s\nalpha-command=%d\nbeta-command=%d\ncommand-catalog=%d\nalpha-catalog=%d\nbeta-catalog=%d\nnonempty-command=%d\nnonempty-scripts=%d\nlast-script-beta=%d\nmissing-script-safe=%d\nresult=%s\n",
			missingScriptRead ? "missing-script-read" : toolbarLayoutWrite ? "write" : "read", m_script_menu.Count(), static_cast<LPCSTR>(discoveredScripts), alphaCommand, betaCommand, commandCatalogReady, alphaInCatalog, betaInCatalog, commandLayout, scriptsLayout, lastScriptIsBeta, missingScriptSafe,
			missingScriptRead ? (missingScriptSafe ? "pass" : "fail") : commandLayout && scriptsLayout && lastScriptIsBeta ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (scriptsReload)
	{
		const DWORD before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
		InitPlugins();
		InitPlugins();
		InitPlugins();
		const DWORD after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
		bool validRun = false, invalidRejected = true, noRunRejected = true;
		for (int index = 0; index < m_script_menu.Count(); ++index)
		{
			const ScriptDescriptor& script = m_script_menu.Item(index);
			if (script.isFolder) continue;
			if (script.relativePath == L"foo.js") validRun = true;
			if (script.relativePath == L"invalid.js") invalidRejected = false;
			if (script.relativePath == L"no-run.js") noRunRejected = false;
		}
		CStringA report;
		const bool passed = after <= before && validRun && invalidRejected && noRunRejected;
		report.Format("phase=scripts-reload\ngdi-before=%lu\ngdi-after=%lu\ngdi-stable=%d\nvalid-run=%d\ninvalid-js-rejected=%d\nno-run-rejected=%d\nresult=%s\n",
			before, after, after <= before, validRun, invalidRejected, noRunRejected, passed ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (legacyHotkeyRead)
	{
		CHotkeysGroup* scripts = _Settings.GetGroupByName(L"Scripts");
		CHotkey* foo = scripts ? _Settings.GetHotkeyByName(L"tools/foo.js", *scripts) : NULL;
		const bool migrated = foo != NULL && foo->m_accel.fVirt == (FVIRTKEY | FCONTROL) && foo->m_accel.key == VK_F9;
		CStringA report;
		report.Format("phase=legacy-hotkey-read\nlegacy-hotkey=%d\nresult=%s\n", migrated, migrated ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (ordinaryWrite)
	{
		// These deterministic mutations go through the same settings, MRU,
		// toolbar and recovery code that normal UI actions persist on close.
		_Settings.SetSplitterPos(271);
		_Settings.SetShowFullPathInWindowTitle(true);
		_Settings.SetInterfaceLanguage(FBE_INTERFACE_LANGUAGE_RUSSIAN);
		_Settings.SetScriptsFolder(scriptsDirectory, true);
		_Settings.m_words.push_back(WordsItem(L"portable-state-sentinel", 17));
		m_mru.AddToList(U::GetProgDirFile(L"portable-state-sentinel.fb2"));
		if (CHotkeysGroup* tools = _Settings.GetGroupByName(L"Tools"))
			if (CHotkey* hotkey = _Settings.GetHotkeyByName(L"Words", *tools))
			{
				hotkey->m_accel.fVirt = portableStateHotkeyFlags;
				hotkey->m_accel.key = portableStateHotkeyKey;
				_Settings.SaveHotkeyGroups();
			}
		if (m_rebar.GetBandCount() > 0)
		{
			REBARBANDINFO band = {}; band.cbSize = sizeof(band); band.fMask = RBBIM_ID | RBBIM_SIZE | RBBIM_STYLE;
			if (m_rebar.GetBandInfo(0, &band) && band.wID == portableStateToolbarBandId)
			{
				band.cx = portableStateToolbarWidth;
				m_rebar.SetBandInfo(0, &band);
			}
		}
		WritePortableStateTestText(diagnosticsMarker, "portable-state-diagnostics\n");
		WritePortableStateTestText(recoveryMarker, "portable-state-recovery\n");
		WritePortableStateTestText(reportPath, "phase=write\nresult=pass\n");
	}
	else if (ordinaryRead)
	{
		bool wordFound = false, mruFound = false;
		for (size_t index = 0; index < _Settings.m_words.size(); ++index)
			if (_Settings.m_words[index].m_word == L"portable-state-sentinel" && _Settings.m_words[index].m_count == 17) wordFound = true;
		CHotkeysGroup* tools = _Settings.GetGroupByName(L"Tools");
		CHotkey* hotkey = tools ? _Settings.GetHotkeyByName(L"Words", *tools) : NULL;
		const bool hotkeys = hotkey != NULL && hotkey->m_accel.fVirt == portableStateHotkeyFlags &&
			hotkey->m_accel.key == portableStateHotkeyKey;
		for (int index = 0; index < m_mru.m_arrDocs.GetSize(); ++index)
			if (CString(m_mru.m_arrDocs[index].szDocName).Find(L"portable-state-sentinel.fb2") >= 0) mruFound = true;
		const bool settings = _Settings.GetShowFullPathInWindowTitle();
		const bool locale = _Settings.GetInterfaceLocaleName() == L"ru-RU";
		const bool scripts = _Settings.GetScriptsFolder().CompareNoCase(scriptsDirectory) == 0;
		const bool toolbar = HasPortableStateToolbarWidth(_Settings.GetToolbarsSettings(),
			portableStateToolbarBandId, portableStateToolbarWidth);
		const bool diagnostics = ::GetFileAttributes(diagnosticsMarker) != INVALID_FILE_ATTRIBUTES;
		const bool recovery = ::GetFileAttributes(recoveryMarker) != INVALID_FILE_ATTRIBUTES;
		CStringA report;
		report.Format("phase=read\nsettings=%d\nhotkeys=%d\nwords=%d\nlocale=%d\nmru=%d\ntoolbar=%d\nscripts=%d\ndiagnostics=%d\nrecovery=%d\nresult=%s\n",
			settings, hotkeys, wordFound, locale, mruFound, toolbar, scripts, diagnostics, recovery,
			settings && hotkeys && wordFound && locale && mruFound && toolbar && scripts && diagnostics && recovery ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	PostMessage(WM_CLOSE);
}

LRESULT CMainFrame::OnSourceMemoryBenchmark(UINT, WPARAM, LPARAM, BOOL&)
{
	CAtlFile output;
	if (FAILED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS)))
		return 0;
	if (IsFbeTestScenario(L"archive-open-runtime"))
	{
		const bool archiveSource = m_document_session.Location().IsArchive();
		const bool fb2 = m_doc->GetDocumentFileType() == FictionBookFileType::Fb2;
		const bool fbd = m_doc->GetDocumentFileType() == FictionBookFileType::Fbd;
		const bool htmlReady = m_doc->m_body.Document() != NULL;
		const bool rar = m_document_session.Location().containerKind == DocumentContainerKind::Rar;
		CStringA report;
		report.Format("archive=%d\nfb2=%d\nfbd=%d\nmshtml=%d\nrar=%d\nentry=%S\n", archiveSource, fb2, fbd, htmlReady, rar,
			static_cast<LPCWSTR>(m_document_session.Location().entryPath));
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(archiveSource && (fb2 || fbd) && htmlReady ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-runtime") || IsFbeTestScenario(L"archive-rar-save-runtime"))
	{
		const bool archiveSource = m_document_session.Location().IsArchive();
		const bool fb2 = m_doc->GetDocumentFileType() == FictionBookFileType::Fb2;
		const bool htmlReady = m_doc->m_body.Document() != NULL;
		ShowView(SOURCE);
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		const char* markerText = "ARCHIVE_RUNTIME_BEFORE";
		char* marker = strstr(source.data(), markerText);
		if (!marker)
		{
			// The checked-in RAR5 fixture is an existing valid FB2 regression
			// document.  Its stable author field is the edit target for Save As.
			markerText = "FBE Test";
			marker = strstr(source.data(), markerText);
		}
		const bool markerFound = marker != NULL;
		if (markerFound) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen(markerText)); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_AFTER")); }
		int sourceLine = 0, sourceColumn = 0;
		const bool sourceCommitted = markerFound && m_doc->SetXMLAndValidate(m_source, false, sourceLine, sourceColumn);
		ShowView(BODY);
		const bool bodyActive = !IsSourceActive();
		const bool readOnlyArchive = m_document_session.Location().containerKind == DocumentContainerKind::Rar;
		const bool saveAs = IsFbeTestScenario(L"archive-rar-save-runtime");
		const bool shouldSave = !readOnlyArchive || saveAs;
		const bool saved = sourceCommitted && bodyActive && markerFound && (!shouldSave || SaveFile(false) == OK);
		wchar_t serializedChanged[4] = {};
		const bool archiveSaveSerializedChanged = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SERIALIZED_CHANGED", serializedChanged, _countof(serializedChanged)) == 1 && serializedChanged[0] == L'1';
		wchar_t archiveWriteError[64] = {};
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_WRITE_ERROR", archiveWriteError, _countof(archiveWriteError));
		CStringA report;
		report.Format("archive=%d\nfb2=%d\nfbd=%d\nmshtml=%d\nrar=%d\nsave_as=%d\nsource_committed=%d\nbody_active=%d\narchive_save_serialized_changed=%d\narchive_write_error=%S\nentry=%S\nsaved=%d\n", archiveSource, fb2,
			m_doc->GetDocumentFileType() == FictionBookFileType::Fbd, htmlReady, readOnlyArchive, saveAs,
			sourceCommitted, bodyActive, archiveSaveSerializedChanged, archiveWriteError, static_cast<LPCWSTR>(m_document_session.Location().entryPath), saved);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(saved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"body-source-transition-runtime"))
	{
		FB::Doc* const originalDocument = m_doc;
		ShowView(SOURCE);
		const bool sourceActive = IsSourceActive();
		const sptr_t initialLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> initialSource(static_cast<size_t>(initialLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, initialLength + 1, reinterpret_cast<LPARAM>(initialSource.data()));
		const char* const originalMarker = "BODY_SOURCE_ORIGINAL";
		const bool sourceCurrent = strstr(initialSource.data(), originalMarker) != NULL;
		ShowView(BODY);
		const bool bodyWithoutChange = !IsSourceActive() && m_doc == originalDocument && m_doc->m_body.Document() != NULL;
		ShowView(SOURCE);
		char* marker = strstr(initialSource.data(), originalMarker);
		const bool markerFound = marker != NULL;
		if(markerFound)
		{
			const sptr_t position = static_cast<sptr_t>(marker - initialSource.data());
			m_source.SendMessage(SCI_SETSEL, position, position + strlen(originalMarker));
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("BODY_SOURCE_EDITED"));
		}
		ShowView(BODY);
		const bool validEditApplied = markerFound && !IsSourceActive() && m_doc == originalDocument && m_doc->m_body.Document() != NULL;
		ShowView(SOURCE);
		const sptr_t editedLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> editedSource(static_cast<size_t>(editedLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, editedLength + 1, reinterpret_cast<LPARAM>(editedSource.data()));
		const bool editedSourceCurrent = strstr(editedSource.data(), "BODY_SOURCE_EDITED") != NULL;
		bool cycles = true;
		for(int cycle = 0; cycle < 3; ++cycle) { ShowView(BODY); cycles = cycles && !IsSourceActive(); ShowView(SOURCE); cycles = cycles && IsSourceActive(); }
		const sptr_t preservedSelectionStart = m_source.SendMessage(SCI_GETSELECTIONSTART);
		const sptr_t preservedSelectionEnd = m_source.SendMessage(SCI_GETSELECTIONEND);
		m_source.SendMessage(SCI_SELECTALL);
		m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("<FictionBook><broken>"));
		const bool invalidRejected = !SourceToHTML();
		const bool invalidPreserved = invalidRejected && IsSourceActive() && m_doc == originalDocument &&
			m_source.SendMessage(SCI_GETLENGTH) > 0 && m_source.SendMessage(SCI_GETSELECTIONSTART) >= 0 && m_source.SendMessage(SCI_GETSELECTIONEND) >= 0;
		CStringA report;
		report.Format("source_active=%d\nsource_current=%d\nbody_without_change=%d\nvalid_edit=%d\nedited_source=%d\ncycles=%d\ninvalid_rejected=%d\ninvalid_preserved=%d\nselection_saved=%d\n", sourceActive, sourceCurrent, bodyWithoutChange, validEditApplied, editedSourceCurrent, cycles, invalidRejected, invalidPreserved, preservedSelectionStart >= 0 && preservedSelectionEnd >= 0);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(sourceActive && sourceCurrent && bodyWithoutChange && validEditApplied && editedSourceCurrent && cycles && invalidPreserved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"editor-view-lifecycle-runtime"))
	{
		FB::Doc* const originalDocument = m_doc;
		const auto isBodyHostActive = [&]() { return m_view.GetActiveWnd() == m_doc->m_body; };
		const auto descriptionModeEnabled = [&]()
		{
			MSHTML::IHTMLDocument3Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr description = document ? document->getElementById(L"fbw_desc") : MSHTML::IHTMLElementPtr();
			return description && description->style && U::scmp(description->style->display, L"block") == 0;
		};
		ShowView(DESC);
		const bool bodyToDescription = m_editor_view_state.Current() == DESC && m_editor_view_state.Previous() == BODY && isBodyHostActive() && descriptionModeEnabled();
		ShowView(BODY);
		const bool descriptionToBody = m_editor_view_state.Current() == BODY && m_editor_view_state.Previous() == DESC && isBodyHostActive() && !descriptionModeEnabled();
		ShowView(SOURCE);
		const bool bodyToSource = m_editor_view_state.Current() == SOURCE && IsSourceActive() && m_editor_view_state.Previous() == BODY;
		ShowView(BODY);
		const bool sourceToBody = m_editor_view_state.Current() == BODY && !IsSourceActive() && m_editor_view_state.Previous() == SOURCE && isBodyHostActive();
		ShowView(DESC);
		ShowView(SOURCE);
		const bool descriptionToSource = m_editor_view_state.Current() == SOURCE && IsSourceActive() && m_editor_view_state.Previous() == DESC;
		ShowView(DESC);
		const bool sourceToDescription = m_editor_view_state.Current() == DESC && !IsSourceActive() && m_editor_view_state.Previous() == SOURCE && isBodyHostActive();
		ShowView(SOURCE);
		ShowView(BODY);
		ShowView(DESC);
		const bool sourceBodyDescription = m_editor_view_state.Current() == DESC && m_editor_view_state.Previous() == BODY && isBodyHostActive();
		ShowView(SOURCE);
		ShowView(DESC);
		ShowView(BODY);
		const bool sourceDescriptionBody = m_editor_view_state.Current() == BODY && m_editor_view_state.Previous() == DESC && isBodyHostActive();
		bool cycles = true;
		for(int cycle = 0; cycle < 3; ++cycle)
		{
			ShowView(DESC); cycles = cycles && m_editor_view_state.Current() == DESC && isBodyHostActive();
			ShowView(SOURCE); cycles = cycles && IsSourceActive();
			ShowView(BODY); cycles = cycles && m_editor_view_state.Current() == BODY && isBodyHostActive();
		}
		const bool documentPreserved = m_doc == originalDocument && m_doc->m_body.Document() != NULL;
		CStringA report;
		report.Format("body_desc=%d\ndesc_body=%d\nbody_source=%d\nsource_body=%d\ndesc_source=%d\nsource_desc=%d\nsource_body_desc=%d\nsource_desc_body=%d\ncycles=%d\ndocument=%d\n", bodyToDescription, descriptionToBody, bodyToSource, sourceToBody, descriptionToSource, sourceToDescription, sourceBodyDescription, sourceDescriptionBody, cycles, documentPreserved);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(bodyToDescription && descriptionToBody && bodyToSource && sourceToBody && descriptionToSource && sourceToDescription && sourceBodyDescription && sourceDescriptionBody && cycles && documentPreserved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-two-phase-runtime"))
	{
		wchar_t failedArchive[MAX_PATH] = {};
		const DWORD failedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_FAILURE_PATH", failedArchive, _countof(failedArchive));
		ShowView(SOURCE);
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "ARCHIVE_RUNTIME_BEFORE");
		if (marker)
		{
			const size_t offset = static_cast<size_t>(marker - source.data());
			m_source.SendMessage(SCI_SETSEL, static_cast<WPARAM>(offset), static_cast<LPARAM>(offset + strlen("ARCHIVE_RUNTIME_BEFORE")));
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_UNSAVED"));
		}
		const CString filenameBefore(m_doc->m_filename);
		CString mruBefore;
		for (int index = 0; index < m_mru.m_arrDocs.GetSize(); ++index)
			mruBefore.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_mru.m_arrDocs[index].szDocName));
		const FILE_OP_STATUS result = failedLength && failedLength < _countof(failedArchive) ? LoadFile(failedArchive) : FAIL;
		const bool sourceStillModified = m_source.SendMessage(SCI_GETMODIFY) != 0;
		const bool sameDocument = CString(m_doc->m_filename) == filenameBefore && !m_document_session.Location().IsArchive();
		CString mruAfter;
		for (int index = 0; index < m_mru.m_arrDocs.GetSize(); ++index)
			mruAfter.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_mru.m_arrDocs[index].szDocName));
		const bool mruUnchanged = mruAfter == mruBefore;
		CStringA report;
		report.Format("open_cancelled=%d\nmodified=%d\nsame_document=%d\nmru_unchanged=%d\n", result == CANCELLED, sourceStillModified, sameDocument, mruUnchanged);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(result == CANCELLED && sourceStillModified && sameDocument && mruUnchanged ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"failed-open-runtime"))
	{
		wchar_t failedPath[MAX_PATH] = {};
		const DWORD failedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_FAILED_OPEN_PATH", failedPath, _countof(failedPath));
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		CString mruBefore;
		for (int index = 0; index < m_mru.m_arrDocs.GetSize(); ++index) mruBefore.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_mru.m_arrDocs[index].szDocName));
		const FILE_OP_STATUS result = failedLength && failedLength < _countof(failedPath) ? LoadFile(failedPath) : FAIL;
		CString mruAfter;
		for (int index = 0; index < m_mru.m_arrDocs.GetSize(); ++index) mruAfter.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_mru.m_arrDocs[index].szDocName));
		const bool preserved = result == FAIL && m_doc == original && FB::Doc::m_active_doc == m_doc &&
			m_document_session.Location().storagePath == originalLocation.storagePath && mruBefore == mruAfter;
		CStringA report; report.Format("failed=%d\nidentity=%d\nactive=%d\nsession=%d\nmru_unchanged=%d\n", result == FAIL, m_doc == original, FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath == originalLocation.storagePath, mruBefore == mruAfter);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(preserved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"malformed-source-fallback-runtime"))
	{
		wchar_t malformedPath[MAX_PATH] = {};
		const DWORD malformedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MALFORMED_SOURCE_PATH", malformedPath, _countof(malformedPath));
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		const FILE_OP_STATUS result = malformedLength && malformedLength < _countof(malformedPath) ? LoadFile(malformedPath) : FAIL;
		const bool sourceFallback = result == OK && m_doc == original && FB::Doc::m_active_doc == m_doc &&
			m_bad_xml && m_bad_filename == malformedPath && m_editor_view_state.Current() == SOURCE &&
			m_document_session.Location().storagePath == originalLocation.storagePath;
		CStringA report; report.Format("fallback=%d\nidentity=%d\nactive=%d\nsession=%d\nsource=%d\n", result == OK, m_doc == original,
			FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath == originalLocation.storagePath, m_bad_xml && m_bad_filename == malformedPath && m_editor_view_state.Current() == SOURCE);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(sourceFallback ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"successful-open-runtime"))
	{
		wchar_t openedPath[MAX_PATH] = {};
		const DWORD openedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SUCCESSFUL_OPEN_PATH", openedPath, _countof(openedPath));
		FB::Doc* const original = m_doc;
		const FILE_OP_STATUS result = openedLength && openedLength < _countof(openedPath) ? LoadFile(openedPath) : FAIL;
		const bool opened = result == OK && m_doc != original && FB::Doc::m_active_doc == m_doc &&
			m_document_session.Location().storagePath == openedPath && m_doc->m_filename == openedPath && m_doc->m_body.Document() != NULL;
		CStringA report; report.Format("opened=%d\nidentity_changed=%d\nactive=%d\nsession=%d\nfilename=%d\nvalid=%d\n", result == OK,
			m_doc != original, FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath == openedPath,
			m_doc->m_filename == openedPath, m_doc->m_body.Document() != NULL);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(opened ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"new-document-runtime"))
	{
		FB::Doc* const original = m_doc;
		BOOL handled = FALSE;
		OnFileNew(0, ID_FILE_NEW, NULL, handled);
		const bool created = m_doc != original && FB::Doc::m_active_doc == m_doc &&
			m_document_session.Location().storagePath.IsEmpty() && !m_document_session.Location().IsArchive() && m_doc->m_body.Document() != NULL;
		CStringA report; report.Format("created=%d\nidentity_changed=%d\nactive=%d\nsession_new=%d\nvalid=%d\n", created,
			m_doc != original, FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath.IsEmpty() && !m_document_session.Location().IsArchive(), m_doc->m_body.Document() != NULL);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(created ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"reload-success-runtime") || IsFbeTestScenario(L"reload-failure-runtime"))
	{
		const bool expectSuccess = IsFbeTestScenario(L"reload-success-runtime");
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		if (!expectSuccess) ::SetEnvironmentVariable(L"FBE_NEXT_FAULT_INJECT", L"api-load-return-false");
		const bool result = ReloadFile();
		if (!expectSuccess) ::SetEnvironmentVariable(L"FBE_NEXT_FAULT_INJECT", NULL);
		const bool reloadMatches = expectSuccess
			? result && m_doc != original && FB::Doc::m_active_doc == m_doc && m_doc->m_body.Document() != NULL &&
				m_document_session.Location().storagePath == originalLocation.storagePath
			: !result && m_doc == original && FB::Doc::m_active_doc == m_doc &&
				m_document_session.Location().storagePath == originalLocation.storagePath;
		CStringA report; report.Format("reloaded=%d\nidentity=%d\nactive=%d\nsession=%d\nvalid=%d\n", result,
			expectSuccess ? m_doc != original : m_doc == original, FB::Doc::m_active_doc == m_doc,
			m_document_session.Location().storagePath == originalLocation.storagePath, m_doc->m_body.Document() != NULL);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(reloadMatches ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-mru-runtime"))
	{
		wchar_t secondEntry[MAX_PATH] = {}, occurrenceText[16] = {}, normalEntries[8192] = {};
		const DWORD secondLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_ENTRY", secondEntry, _countof(secondEntry));
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_OCCURRENCE", occurrenceText, _countof(occurrenceText));
		const DWORD normalLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_NORMAL_ENTRIES", normalEntries, _countof(normalEntries));
		DocumentLocation first = m_document_session.Location(), second = first;
		unsigned int occurrence = 0;
		const bool secondValid = secondLength > 0 && secondLength < _countof(secondEntry) && FbeRecentDocuments::ParseArchiveMruUnsigned(occurrenceText, occurrence);
		second.entryPath = secondEntry; second.entryOccurrence = occurrence; second.documentType = DetectFictionBookFileType(second.entryPath);
		// This scenario exercises MRU routing, not the unsaved-changes prompt.
		// A freshly loaded MSHTML document can carry a transient form-change bit.
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		ResolvedOpenDocument secondResolved; FbeArchive::Error secondError;
		const bool secondFound = secondValid && FbeArchiveUi::ResolveOpenRequest(second.storagePath, secondResolved, &second, &secondError);
		const FILE_OP_STATUS secondOpen = secondFound ? LoadFile(second.storagePath, &second) : CANCELLED;
		if (secondOpen == OK) FbeRecentDocuments::RememberArchiveMruRecord(m_mru, m_document_session.Location());
		if (secondOpen == OK) { m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT); }
		std::vector<FbeRecentDocuments::ArchiveMruRecord> records; FbeRecentDocuments::ReadArchiveMruRecords(records);
		const CString firstKey = FbeRecentDocuments::ArchiveMruKey(first); WORD firstCommand = 0;
		for (int offset = 0; offset < m_mru.m_arrDocs.GetSize() && offset <= ID_FILE_MRU_LAST - ID_FILE_MRU_FIRST; ++offset) { CString key; const WORD candidate = MruCommandId(offset); if (m_mru.GetFromList(candidate, key) && key == firstKey) { firstCommand = candidate; break; } }
		DocumentLocation menuFirst;
		const bool menuLookup = firstCommand != 0 && FbeRecentDocuments::FindArchiveMruRecord(firstKey, menuFirst) && FbeRecentDocuments::SameArchiveMruIdentity(menuFirst, first);
		BOOL handled = FALSE;
		const LRESULT handlerResult = secondOpen == OK && menuLookup ? OnFileOpenMRU(0, firstCommand, NULL, handled) : 1;
		const FILE_OP_STATUS firstOpen = handlerResult == 0 && FbeRecentDocuments::SameArchiveMruIdentity(m_document_session.Location(), first) ? OK : FAIL;
		const bool reopenedFirst = firstOpen == OK && FbeRecentDocuments::SameArchiveMruIdentity(m_document_session.Location(), first);
		bool normalEntriesOpened = normalLength == 0;
		if (normalLength > 0 && normalLength < _countof(normalEntries))
		{
			int position = 0;
			while (position >= 0)
			{
				const CString normal = CString(normalEntries).Tokenize(L"|", position);
				if (normal.IsEmpty()) continue;
				m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
				if (LoadFile(normal) != OK) { normalEntriesOpened = false; break; }
				FbeRecentDocuments::RememberNormalMruRecord(m_mru, normal);
				normalEntriesOpened = true;
			}
		}
		FbeRecentDocuments::ReadArchiveMruRecords(records);
		bool hasFirst = false, hasSecond = false;
		for (size_t index = 0; index < records.size(); ++index) { hasFirst = hasFirst || FbeRecentDocuments::SameArchiveMruIdentity(records[index].location, first); hasSecond = hasSecond || FbeRecentDocuments::SameArchiveMruIdentity(records[index].location, second); }
		DocumentLocation missing = first; missing.entryPath = L"missing.fb2"; missing.entryOccurrence = 0;
		ResolvedOpenDocument ignored; FbeArchive::Error missingError;
		const bool missingRejected = !FbeArchiveUi::ResolveOpenRequest(first.storagePath, ignored, &missing, &missingError) && missingError.code == FbeArchive::ErrorCode::EntryNotFound;
		int menuCount = 0, visibleArchiveCount = 0; bool menuClean = true;
		const HMENU mruMenu = m_mru.GetMenuHandle();
		if (mruMenu != NULL) for (int index = 0; index < ::GetMenuItemCount(mruMenu); ++index)
		{
			MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID;
			if (!::GetMenuItemInfo(mruMenu, index, TRUE, &item) || item.wID < ID_FILE_MRU_FIRST || item.wID > ID_FILE_MRU_LAST) continue;
			++menuCount; wchar_t caption[512] = {};
			::GetMenuString(mruMenu, item.wID, caption, _countof(caption), MF_BYCOMMAND);
			DocumentLocation captionLocation;
			if (FbeRecentDocuments::ParseArchiveMruKey(caption, captionLocation) || CString(caption).Left(2) == L"&1" || CString(caption).Left(2) == L"&2") menuClean = false;
			CString key; DocumentLocation keyLocation;
			if (m_mru.GetFromList(item.wID, key) && FbeRecentDocuments::ParseArchiveMruKey(key, keyLocation)) ++visibleArchiveCount;
		}
		const CString firstCaption = FbeRecentDocuments::ArchiveMruCaption(firstKey), secondCaption = FbeRecentDocuments::ArchiveMruCaption(FbeRecentDocuments::ArchiveMruKey(second));
		const bool captionsDifferent = firstCaption.Compare(secondCaption) != 0;
		const bool captionsDistinct = visibleArchiveCount < 2 || captionsDifferent;
		FbeRecentDocuments::WritePortableMru(m_mru);
		CStringA report; report.Format("first=%d\nsecond=%d\nmenu_lookup=%d\nsecond_found=%d\nsecond_error=%d\nsecond_open=%d\nfirst_open=%d\nreopened_first=%d\nmissing_entry=%d\narchive_records=%u\nnormal_entries=%d\nmenu_count=%d\nmenu_clean=%d\ncaption_diff=%d\ncaptions_distinct=%d\n", hasFirst, hasSecond, menuLookup, secondFound, static_cast<int>(secondError.code), secondOpen, firstOpen, reopenedFirst, missingRejected, static_cast<unsigned int>(records.size()), normalEntriesOpened, menuCount, menuClean, captionsDifferent, captionsDistinct);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(hasFirst && hasSecond && menuLookup && reopenedFirst && missingRejected && normalEntriesOpened && menuCount > 0 && menuCount <= 10 && menuClean && captionsDistinct ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-mru-restart-runtime"))
	{
		std::vector<CString> order; FbeRecentDocuments::ReadMruOrder(order);
		const int count = m_mru.m_arrDocs.GetSize(); bool exactOrder = count == 10 && order.size() == 10, cleanMenu = true; int menuCount = 0;
		for (int index = 0; index < count && exactOrder; ++index) exactOrder = CString(m_mru.m_arrDocs[index].szDocName) == order[order.size() - 1 - index];
		for (int index = 0; index < count; ++index) { DocumentLocation location; if (FbeRecentDocuments::ParseArchiveMruKey(CString(m_mru.m_arrDocs[index].szDocName), location) && !FbeRecentDocuments::FindArchiveMruRecord(CString(m_mru.m_arrDocs[index].szDocName), location)) cleanMenu = false; }
		std::vector<CString> captions; const HMENU menu = m_mru.GetMenuHandle(); int emptyCaption = 0, rawCaption = 0, numberedCaption = 0, duplicateCaption = 0, disabledCaption = 0; bool minimalFolderContexts = false;
		if (menu == NULL) cleanMenu = false; else for (int index = 0; index < ::GetMenuItemCount(menu); ++index)
		{
			MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID;
			if (!::GetMenuItemInfo(menu, index, TRUE, &item) || item.wID < ID_FILE_MRU_FIRST || item.wID > ID_FILE_MRU_LAST) continue;
			++menuCount; wchar_t text[512] = {}; ::GetMenuString(menu, index, text, _countof(text), MF_BYPOSITION); CString caption(text), parsedKey;
			item.fMask = MIIM_STATE; ::GetMenuItemInfo(menu, index, TRUE, &item); DocumentLocation parsed; if (caption == m_mru.m_szNoEntries) { ++emptyCaption; } if ((item.fState & (MFS_DISABLED | MFS_GRAYED)) != 0) { cleanMenu = false; ++disabledCaption; } if (FbeRecentDocuments::ParseArchiveMruKey(caption, parsed)) { cleanMenu = false; ++rawCaption; } if (caption.GetLength() > 1 && caption[0] == L'&' && caption[1] >= L'0' && caption[1] <= L'9') { cleanMenu = false; ++numberedCaption; }
			for (size_t previous = 0; previous < captions.size(); ++previous) if (captions[previous].CompareNoCase(caption) == 0) { cleanMenu = false; ++duplicateCaption; }
			captions.push_back(caption);
		}
		for (size_t i = 0; i < captions.size(); ++i) for (size_t j = i + 1; j < captions.size(); ++j)
			if ((captions[i].Find(L"A\\Books\\archive.zip") >= 0 && captions[j].Find(L"B\\Books\\archive.zip") >= 0) || (captions[i].Find(L"B\\Books\\archive.zip") >= 0 && captions[j].Find(L"A\\Books\\archive.zip") >= 0)) minimalFolderContexts = true;
		wchar_t path[MAX_PATH] = {}, entry[MAX_PATH] = {}, occurrenceText[16] = {}; ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_REOPEN_PATH", path, _countof(path)); ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_ENTRY", entry, _countof(entry)); ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_OCCURRENCE", occurrenceText, _countof(occurrenceText));
		unsigned int occurrence = 0; DocumentLocation target; target.containerKind = DetectDocumentContainerKind(path); target.storagePath = path; target.entryPath = entry; target.entryOccurrence = FbeRecentDocuments::ParseArchiveMruUnsigned(occurrenceText, occurrence) ? occurrence : 0; target.documentType = DetectFictionBookFileType(target.entryPath);
		WORD command = 0; const CString key = FbeRecentDocuments::ArchiveMruKey(target); for (int offset = 0; offset < count && offset <= ID_FILE_MRU_LAST - ID_FILE_MRU_FIRST; ++offset) { CString value; const WORD candidate = MruCommandId(offset); if (m_mru.GetFromList(candidate, value) && value == key) { command = candidate; break; } }
		BOOL handled = FALSE; const bool reopened = command != 0 && OnFileOpenMRU(0, command, NULL, handled) == 0 && FbeRecentDocuments::SameArchiveMruIdentity(m_document_session.Location(), target);
		auto getMruItem = [&](UINT id, CString& caption, UINT& state) -> bool
		{
			const int position = FindMenuPositionByCommand(menu, id);
			if (position < 0) return false;
			wchar_t text[512] = {}; if (::GetMenuString(menu, position, text, _countof(text), MF_BYPOSITION) <= 0) return false;
			MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_STATE;
			if (!::GetMenuItemInfo(menu, position, TRUE, &item)) return false;
			caption = text; state = item.fState; return true;
		};
		CString firstBefore, firstRussian; UINT firstBeforeState = 0, firstRussianState = 0;
		CString firstKeyBefore; const bool firstMappedBefore = m_mru.GetFromList(ID_FILE_MRU_FIRST, firstKeyBefore);
		const bool firstVisibleBefore = getMruItem(ID_FILE_MRU_FIRST, firstBefore, firstBeforeState);
		_Settings.SetInterfaceLanguage(FBE_INTERFACE_LANGUAGE_RUSSIAN);
		FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName()); FbeResetRuntimeLocalization(); RefreshLocalizedMainFrameUi();
		const bool russianLocaleSelected = _Settings.GetInterfaceLocaleName() == L"ru-RU";
		const CString russianLocalizedEmpty = FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.recent.empty", L"No Recent Files");
		CString firstKeyRussian; const bool firstMappedRussian = m_mru.GetFromList(ID_FILE_MRU_FIRST, firstKeyRussian);
		const bool firstVisibleRussian = getMruItem(ID_FILE_MRU_FIRST, firstRussian, firstRussianState);
		const bool nonEmptyMruLocalized = firstMappedBefore && firstMappedRussian && firstKeyBefore == firstKeyRussian && firstVisibleBefore && firstVisibleRussian && firstBefore == firstRussian && firstRussian != m_mru.m_szNoEntries && (firstRussianState & (MFS_DISABLED | MFS_GRAYED)) == 0;
		m_mru.m_arrDocs.RemoveAll();
		RefreshMruEmptyStateText(m_mru); FbeRecentDocuments::RebuildMruMenu(m_mru);
		CString russianEmpty; UINT russianEmptyState = 0; int russianEmptyCount = 0;
		for (int index = 0; index < ::GetMenuItemCount(menu); ++index) { MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID; if (::GetMenuItemInfo(menu, index, TRUE, &item) && item.wID >= ID_FILE_MRU_FIRST && item.wID <= ID_FILE_MRU_LAST) ++russianEmptyCount; }
		const bool russianEmptyOk = russianEmptyCount == 1 && !russianLocalizedEmpty.IsEmpty() && getMruItem(ID_FILE_MRU_FIRST, russianEmpty, russianEmptyState) && russianEmpty == russianLocalizedEmpty && (russianEmptyState & (MFS_DISABLED | MFS_GRAYED)) != 0;
		_Settings.SetInterfaceLanguage(FBE_INTERFACE_LANGUAGE_ENGLISH);
		FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName()); FbeResetRuntimeLocalization(); RefreshLocalizedMainFrameUi();
		CString englishEmpty; UINT englishEmptyState = 0; int englishEmptyCount = 0;
		for (int index = 0; index < ::GetMenuItemCount(menu); ++index) { MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID; if (::GetMenuItemInfo(menu, index, TRUE, &item) && item.wID >= ID_FILE_MRU_FIRST && item.wID <= ID_FILE_MRU_LAST) ++englishEmptyCount; }
		const bool englishEmptyOk = englishEmptyCount == 1 && getMruItem(ID_FILE_MRU_FIRST, englishEmpty, englishEmptyState) && englishEmpty == L"No Recent Files" && (englishEmptyState & (MFS_DISABLED | MFS_GRAYED)) != 0;
		CStringA report; report.Format("count=%d\nmenu_count=%d\norder=%d\nclean=%d\nempty=%d\ndisabled=%d\nraw=%d\nnumbered=%d\nduplicates=%d\nfolders=%d\nreopened=%d\nlocalized_nonempty=%d\nrussian_empty=%d\nrussian_locale=%d\nenglish_empty=%d\n", count, menuCount, exactOrder, cleanMenu, emptyCaption, disabledCaption, rawCaption, numberedCaption, duplicateCaption, minimalFolderContexts, reopened, nonEmptyMruLocalized, russianEmptyOk, russianLocaleSelected, englishEmptyOk); DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(count == 10 && menuCount == 10 && exactOrder && cleanMenu && reopened && nonEmptyMruLocalized && russianEmptyOk && englishEmptyOk ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"archive-recovery-create"))
	{
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "ARCHIVE_RUNTIME_BEFORE");
		if (marker) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen("ARCHIVE_RUNTIME_BEFORE")); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_RECOVERY")); }
		else
		{
			char* titleEnd = strstr(source.data(), "</book-title>");
			const size_t offset = titleEnd ? static_cast<size_t>(titleEnd - source.data()) : static_cast<size_t>(length);
			m_source.SendMessage(SCI_SETSEL, offset, offset);
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(" ARCHIVE_RUNTIME_RECOVERY"));
		}
		const bool saved = SourceToHTML() && SaveRecoveryNow(); CStringA report; report.Format("recovery_created=%d\n", saved); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(saved ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"archive-recovery-verify"))
	{
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		const bool archive = m_document_session.Location().IsArchive(); const bool fbd = m_doc->GetDocumentFileType() == FictionBookFileType::Fbd;
		const bool payload = strstr(source.data(), "ARCHIVE_RUNTIME_RECOVERY") != NULL;
		CStringA report; report.Format("archive=%d\nfbd=%d\nrecovery_payload=%d\n", archive, fbd, payload); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(archive && payload ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"archive-recovery-external-verify"))
	{
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "ARCHIVE_RUNTIME_RECOVERY");
		if (marker) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen("ARCHIVE_RUNTIME_RECOVERY")); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_EXTERNAL")); }
		::SetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SAVE_ERROR", NULL);
		const bool blocked = marker != NULL && SourceToHTML() && SaveFile(false) == FAIL;
		wchar_t errorCode[16] = {};
		const bool modifiedExternally = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SAVE_ERROR", errorCode, _countof(errorCode)) > 0 &&
			_wtoi(errorCode) == static_cast<int>(FbeArchive::ErrorCode::ModifiedExternally);
		CStringA report; report.Format("archive=%d\nblocked=%d\nmodified_externally=%d\nerror_code=%S\n", m_document_session.Location().IsArchive(), blocked, modifiedExternally, errorCode); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(blocked && modifiedExternally ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"table-roundtrip"))
	{
		const ULONGLONG start = ::GetTickCount64();
		auto appendTablePhase = [&](const char* phase)
		{
			const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
			std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
			m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
			auto countTag = [&](const char* tag) -> long { long count = 0; for (const char* position = source.data(); (position = strstr(position, tag)) != NULL; ++position) ++count; return count; };
			const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
			CStringA row;
			row.Format("%s\t%I64u\t%I64u\t%I64u\t%ld\t%ld\t%ld\t%ld\r\n", phase,
				::GetTickCount64() - start, static_cast<unsigned __int64>(memory.privateBytes), static_cast<unsigned __int64>(memory.workingSetBytes),
				countTag("<table"), countTag("<tr"), countTag("<td"), countTag("<th"));
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		CStringA header("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\ttable_count\ttr_count\ttd_count\tth_count\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();
		appendTablePhase("open-complete");
		for (int cycle = 1; cycle <= 5; ++cycle)
		{
			CStringA phase; phase.Format("source-%d-start", cycle); appendTablePhase(phase);
			ShowView(SOURCE); phase.Format("source-%d-complete", cycle); appendTablePhase(phase);
			phase.Format("body-%d-start", cycle); appendTablePhase(phase);
			ShowView(BODY); phase.Format("body-%d-complete", cycle); appendTablePhase(phase);
		}
		appendTablePhase("save-1-start");
		if (!m_doc->Save())
		{
			appendTablePhase("save-1-failed;phase=save-1;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		appendTablePhase("save-1-complete");
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"editor-background-runtime"))
	{
		StartupTrace::AppendTestStartupBreadcrumb("scenario-enter");
		CStringA header("phase\timage\tcss_url\trepeat\tposition\tsize\tattachment\tmodified\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		auto backgroundPathFromEnvironment = [](const wchar_t* name) -> CString
		{
			wchar_t value[1024] = {};
			const DWORD length = ::GetEnvironmentVariable(name, value, _countof(value));
			return length && length < _countof(value) ? CString(value) : CString();
		};
		auto appendBackgroundPhase = [&](const char* phase)
		{
			MSHTML::IHTMLStylePtr style(m_doc->m_body.Document() ? m_doc->m_body.Document()->body->style : MSHTML::IHTMLStylePtr());
			CString image, repeat, position, attachment, cssText, size(L"auto");
			CString cssUrl;
			if(style) {
				image = static_cast<LPCWSTR>(style->backgroundImage); repeat = static_cast<LPCWSTR>(style->backgroundRepeat);
				position = static_cast<LPCWSTR>(style->backgroundPosition); attachment = static_cast<LPCWSTR>(style->backgroundAttachment);
				cssText = static_cast<LPCWSTR>(style->cssText);
				if(cssText.Find(L"background-size: contain") >= 0) size = L"contain";
				else if(cssText.Find(L"background-size: cover") >= 0) size = L"cover";
				else { _variant_t sizeAttribute(style->getAttribute(L"background-size", 0)); if(sizeAttribute.vt == VT_BSTR && sizeAttribute.bstrVal) size = sizeAttribute.bstrVal; }
			}
			CString path;
			if(_Settings.GetEditorBackgroundKind() == L"builtin") EditorBackgrounds::ResolveBuiltIn(_Settings.GetEditorBackgroundId(), path);
			else if(_Settings.GetEditorBackgroundKind() == L"custom") path = _Settings.GetEditorBackgroundCustomPath();
			if(!path.IsEmpty()) { const CString uri = U::UrlFromPath(path); if(!uri.IsEmpty()) cssUrl.Format(L"url(\"%s\")", static_cast<LPCWSTR>(uri)); }
			CStringA imageA(CW2A(image, CP_UTF8)), repeatA(CW2A(repeat, CP_UTF8)), positionA(CW2A(position, CP_UTF8));
			CStringA cssUrlA(CW2A(cssUrl, CP_UTF8)), sizeA(CW2A(size, CP_UTF8)), attachmentA(CW2A(attachment, CP_UTF8));
			CStringA row; row.Format("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\r\n", phase,
				imageA.GetString(), cssUrlA.GetString(), repeatA.GetString(), positionA.GetString(), sizeA.GetString(), attachmentA.GetString(), m_doc->DocChanged() ? 1 : 0);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		_Settings.SetEditorBackgroundKind(L"none"); _Settings.SetEditorBackgroundId(CString()); _Settings.SetEditorBackgroundCustomPath(CString()); _Settings.SetEditorBackgroundLayout(L"tile");
		StartupTrace::AppendTestStartupBreadcrumb("none-start"); m_doc->ApplyConfChanges(); appendBackgroundPhase("none"); StartupTrace::AppendTestStartupBreadcrumb("none-complete");
		_Settings.SetEditorBackgroundKind(L"builtin"); _Settings.SetEditorBackgroundId(L"01_clean_white"); _Settings.SetEditorBackgroundLayout(L"tile");
		StartupTrace::AppendTestStartupBreadcrumb("builtin-tile-start"); m_doc->ApplyConfChanges(); appendBackgroundPhase("builtin-tile"); StartupTrace::AppendTestStartupBreadcrumb("builtin-tile-complete");
		for(const wchar_t* layout : { L"center", L"contain", L"cover" }) {
			_Settings.SetEditorBackgroundLayout(layout); m_doc->ApplyConfChanges();
			if(wcscmp(layout, L"center") == 0) { appendBackgroundPhase("builtin-center"); StartupTrace::AppendTestStartupBreadcrumb("builtin-center-complete"); }
			else if(wcscmp(layout, L"contain") == 0) { appendBackgroundPhase("builtin-contain"); StartupTrace::AppendTestStartupBreadcrumb("builtin-contain-complete"); }
			else { appendBackgroundPhase("builtin-cover"); StartupTrace::AppendTestStartupBreadcrumb("builtin-cover-complete"); }
		}
		StartupTrace::AppendTestStartupBreadcrumb("source-view-start"); ShowView(SOURCE); StartupTrace::AppendTestStartupBreadcrumb("source-view-complete"); StartupTrace::AppendTestStartupBreadcrumb("body-view-start"); ShowView(BODY); StartupTrace::AppendTestStartupBreadcrumb("body-view-complete"); m_doc->ApplyConfChanges(); appendBackgroundPhase("builtin-after-view-recreate"); StartupTrace::AppendTestStartupBreadcrumb("view-recreate-complete");
		_Settings.SetEditorBackgroundId(L"unknown-background"); m_doc->ApplyConfChanges(); appendBackgroundPhase("unknown-builtin"); StartupTrace::AppendTestStartupBreadcrumb("unknown-builtin-complete");
		_Settings.SetEditorBackgroundKind(L"custom"); _Settings.SetEditorBackgroundCustomPath(backgroundPathFromEnvironment(L"FBE_NEXT_TEST_BACKGROUND_MISSING_PATH")); m_doc->ApplyConfChanges(); appendBackgroundPhase("missing-custom"); StartupTrace::AppendTestStartupBreadcrumb("missing-custom-complete");
		_Settings.SetEditorBackgroundCustomPath(backgroundPathFromEnvironment(L"FBE_NEXT_TEST_BACKGROUND_PATH")); _Settings.SetEditorBackgroundLayout(L"contain"); m_doc->ApplyConfChanges(); appendBackgroundPhase("custom"); StartupTrace::AppendTestStartupBreadcrumb("custom-complete");
		_Settings.SetEditorBackgroundKind(L"builtin"); _Settings.SetEditorBackgroundId(L"01_clean_white"); _Settings.SetEditorBackgroundLayout(L"tile"); m_doc->ApplyConfChanges(); appendBackgroundPhase("before-save"); StartupTrace::AppendTestStartupBreadcrumb("before-save");
		StartupTrace::AppendTestStartupBreadcrumb("save-start");
		if(!m_doc->Save()) {
			CString saveFailure;
			saveFailure.Format(L"editor background runtime save failed; name-valid=%d", m_doc->m_namevalid ? 1 : 0);
			StartupTrace::HResult(L"test", L"TST202", m_doc->GetLastSaveError(), saveFailure);
			CStringA savePhase;
			savePhase.Format("save-failed-hr-0x%08lX", static_cast<unsigned long>(m_doc->GetLastSaveError()));
			appendBackgroundPhase(savePhase);
			StartupTrace::AppendTestStartupBreadcrumb(m_doc->m_namevalid ? "save-failed" : "save-failed-name-invalid");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		StartupTrace::AppendTestStartupBreadcrumb("save-complete");
		appendBackgroundPhase("after-save"); output.Flush(); StartupTrace::AppendTestStartupBreadcrumb("after-save"); StartupTrace::AppendTestStartupBreadcrumb("report-flush"); output.Close(); StartupTrace::AppendTestStartupBreadcrumb("report-closed"); StartupTrace::AppendTestStartupBreadcrumb("shutdown-requested"); ::PostQuitMessage(0); StartupTrace::AppendTestStartupBreadcrumb("shutdown-quit-posted"); return 0;
	}
	if (IsFbeTestScenario(L"cite-poem-undo"))
	{
		wchar_t operation[16] = {};
		wchar_t target[16] = {};
		wchar_t selectionMode[16] = {};
		const DWORD operationLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_OPERATION", operation, _countof(operation));
		const DWORD targetLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_TARGET", target, _countof(target));
		const DWORD selectionModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_SELECTION_MODE", selectionMode, _countof(selectionMode));
		const bool cite = operationLength == 4 && wcscmp(operation, L"cite") == 0;
		const bool poem = operationLength == 4 && wcscmp(operation, L"poem") == 0;
		const wchar_t* targetClass = targetLength ? target : L"section";
		const CStringA targetName((CW2A(targetClass)));
		const bool selectCaret = selectionModeLength == 5 && wcscmp(selectionMode, L"caret") == 0;
		const CStringA selectionName(selectCaret ? "caret" : "selected");
		const bool repeat = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_REPEAT", nullptr, 0) != 0;
		CStringA header("operation\ttarget\tselection_mode\tselection_collapsed\tselection_text_utf16\tselection_html_utf16\tselection_parent_utf16\tselection_start_to_first_start\tselection_end_to_first_end\tcheck_allowed\tbefore_equals_undo\tafter_equals_redo\tsequential_cycle\tbefore_paragraphs\tafter_cites\tafter_poems\tafter_stanzas\tpoem_text_utf16\tempty_divs\tempty_paragraphs\tempty_stanzas\tsaved\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		auto writeFailure = [&](const char* reason)
		{
			CStringA row; row.Format("%s\t%s\t%s\t0\t-\t-\t-\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t-\t0\t0\t0\t0\t%s\r\n", cite ? "cite" : poem ? "poem" : "unknown", (LPCSTR)targetName, (LPCSTR)selectionName, reason);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1);
		};
		if (!cite && !poem) { writeFailure("invalid-operation"); return 0; }
		MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr divs(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr container;
		for (long index = 0; divs && index < divs->length; ++index) {
			MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
			if (div && U::scmp(div->className, targetClass) == 0) { container = div; break; }
		}
		MSHTML::IHTMLElementCollectionPtr paragraphs(container ? MSHTML::IHTMLElement2Ptr(container)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		if (!body || !container || !paragraphs || paragraphs->length == 0) { writeFailure("missing-paragraph"); return 0; }
		MSHTML::IHTMLElementPtr first(paragraphs->item(_variant_t(0L), _variant_t()));
		MSHTML::IHTMLElementPtr last(paragraphs->item(_variant_t(paragraphs->length - 1), _variant_t()));
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		MSHTML::IHTMLTxtRangePtr rangeEnd(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		if (!first || !last || !range || !rangeEnd) { writeFailure("selection-create"); return 0; }
		m_doc->m_body.SetFocus();
		range->moveToElementText(first); range->collapse(VARIANT_TRUE);
		// Keep the caret inside an empty P.  Moving one character from its start
		// makes MSHTML place the range after the P, in an editor-owned DIV.
		if (!CString((const wchar_t*)first->innerText).IsEmpty())
			range->move(L"character", 1);
		if (!selectCaret) {
			rangeEnd->moveToElementText(last);
			rangeEnd->collapse(VARIANT_FALSE); rangeEnd->move(L"character", -1);
			// Both boundaries must be inside their P elements. MSHTML otherwise
			// reports the enclosing DIV as parentElement(), and
			// ExpandTxtRangeToParagraphs rejects the structural selection.
			range->setEndPoint(L"EndToEnd", rangeEnd);
		}
		auto utf16Summary = [](const CString& value) -> CStringA
		{
			if (value.IsEmpty()) return CStringA("-");
			CStringA summary;
			for (int index = 0; index < value.GetLength(); ++index) {
				if (index) summary += ',';
				CStringA codeUnit; codeUnit.Format("%04X", static_cast<unsigned int>(static_cast<unsigned short>(value[index])));
				summary += codeUnit;
			}
			return summary;
		};
		const CString selectionText((const wchar_t*)range->text), selectionHtml((const wchar_t*)range->htmlText);
		const bool selectionCollapsed = range->compareEndPoints(L"StartToEnd", range) == 0;
		MSHTML::IHTMLTxtRangePtr firstRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		firstRange->moveToElementText(first);
		MSHTML::IHTMLElementPtr selectionParent(range->parentElement());
		CString selectionParentText = selectionParent ? CString((const wchar_t*)selectionParent->tagName) + L":" + CString((const wchar_t*)selectionParent->className) : CString(L"-");
		const long selectionStartToFirstStart = range->compareEndPoints(L"StartToStart", firstRange);
		const long selectionEndToFirstEnd = range->compareEndPoints(L"EndToEnd", firstRange);
		const CStringA selectionTextSummary(utf16Summary(selectionText)), selectionHtmlSummary(utf16Summary(selectionHtml));
		const CStringA selectionParentSummary(utf16Summary(selectionParentText));
		range->select();
		auto countEmpty = [&](const wchar_t* tagName, const wchar_t* className = nullptr) -> long
		{
			long count = 0;
			MSHTML::IHTMLElementCollectionPtr elements(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(tagName));
			for (long index = 0; elements && index < elements->length; ++index) {
				MSHTML::IHTMLElementPtr element(elements->item(_variant_t(index), _variant_t()));
				MSHTML::IHTMLElementCollectionPtr children(element ? element->children : MSHTML::IHTMLElementCollectionPtr());
				if (element && (!className || U::scmp(element->className, className) == 0) && (!children || children->length == 0) && CString((const wchar_t*)element->innerHTML).Trim().IsEmpty()) ++count;
			}
			return count;
		};
		const long beforeEmptyDivs = countEmpty(L"DIV"), beforeEmptyParagraphs = countEmpty(L"P"), beforeEmptyStanzas = countEmpty(L"DIV", L"stanza");
		const CString before((const wchar_t*)body->innerHTML);
		const long beforeParagraphs = paragraphs->length;
		const bool checkAllowed = cite ? m_doc->m_body.InsertCite(true) : m_doc->m_body.InsertPoem(true);
		const bool applied = cite ? m_doc->m_body.InsertCite(false) : m_doc->m_body.InsertPoem(false);
		const CString after((const wchar_t*)body->innerHTML);
		if (!applied || before == after) {
			CStringA row;
			row.Format("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%ld\t%ld\t%d\t0\t0\t0\t%ld\t0\t0\t0\t-\t0\t0\t0\t0\toperation-failed\r\n", cite ? "cite" : "poem", (LPCSTR)targetName, (LPCSTR)selectionName, selectionCollapsed, (LPCSTR)selectionTextSummary, (LPCSTR)selectionHtmlSummary, (LPCSTR)selectionParentSummary, selectionStartToFirstStart, selectionEndToFirstEnd, checkAllowed, beforeParagraphs);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0;
		}
		BOOL handled = FALSE;
		m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
		const CString undo((const wchar_t*)body->innerHTML);
		m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
		const CString redo((const wchar_t*)body->innerHTML);
		auto countClass = [&](const wchar_t* className) -> long
		{
			long count = 0;
			MSHTML::IHTMLElementCollectionPtr divs(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV"));
			for (long index = 0; divs && index < divs->length; ++index) {
				MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
				if (div && U::scmp(div->className, className) == 0) ++count;
			}
			return count;
		};
		const long citeCount = countClass(L"cite"), poemCount = countClass(L"poem"), stanzaCount = countClass(L"stanza");
		CString poemText;
		MSHTML::IHTMLElementCollectionPtr poemElements(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV"));
		for (long index = 0; poemElements && index < poemElements->length; ++index) {
			MSHTML::IHTMLElementPtr poemElement(poemElements->item(_variant_t(index), _variant_t()));
			if (poemElement && U::scmp(poemElement->className, L"poem") == 0) { poemText = (const wchar_t*)poemElement->innerText; break; }
		}
		const CStringA poemTextSummary(utf16Summary(poemText));
		const long emptyDivsAfterRedo = countEmpty(L"DIV"), emptyParagraphsAfterRedo = countEmpty(L"P"), emptyStanzasAfterRedo = countEmpty(L"DIV", L"stanza");
		m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
		const CString restored((const wchar_t*)body->innerHTML);
		const long undoEmptyDivs = countEmpty(L"DIV"), undoEmptyParagraphs = countEmpty(L"P"), undoEmptyStanzas = countEmpty(L"DIV", L"stanza");
		const long emptyDivs = (emptyDivsAfterRedo > undoEmptyDivs ? emptyDivsAfterRedo : undoEmptyDivs) - beforeEmptyDivs;
		const long emptyParagraphs = (emptyParagraphsAfterRedo > undoEmptyParagraphs ? emptyParagraphsAfterRedo : undoEmptyParagraphs) - beforeEmptyParagraphs;
		const long emptyStanzas = (emptyStanzasAfterRedo > undoEmptyStanzas ? emptyStanzasAfterRedo : undoEmptyStanzas) - beforeEmptyStanzas;
		const bool undone = before == undo && before == restored;
		const bool redone = after == redo;
		bool sequential = true;
		if (repeat) {
			range->select();
			const bool secondApplied = cite ? m_doc->m_body.InsertCite(false) : m_doc->m_body.InsertPoem(false);
			const CString secondAfter((const wchar_t*)body->innerHTML);
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			const CString secondUndo((const wchar_t*)body->innerHTML);
			m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
			const CString secondRedo((const wchar_t*)body->innerHTML);
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			const CString secondRestored((const wchar_t*)body->innerHTML);
			sequential = secondApplied && before == secondUndo && secondAfter == secondRedo && before == secondRestored;
		}
		const bool structure = cite ? citeCount == 1 && poemCount == 0 : poemCount == 1 && stanzaCount >= 1;
		const bool saved = m_doc->Save();
		const bool passed = undone && redone && sequential && structure && emptyDivs == 0 && emptyParagraphs == 0 && emptyStanzas == 0 && saved;
		CStringA row;
		row.Format("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%ld\t%ld\t%d\t%d\t%d\t%d\t%ld\t%ld\t%ld\t%ld\t%s\t%ld\t%ld\t%ld\t%d\t%s\r\n", cite ? "cite" : "poem", (LPCSTR)targetName, (LPCSTR)selectionName, selectionCollapsed, (LPCSTR)selectionTextSummary, (LPCSTR)selectionHtmlSummary, (LPCSTR)selectionParentSummary, selectionStartToFirstStart, selectionEndToFirstEnd, checkAllowed, undone, redone, sequential,
			beforeParagraphs, citeCount, poemCount, stanzaCount, (LPCSTR)poemTextSummary, emptyDivs, emptyParagraphs, emptyStanzas, saved, passed ? "pass" : "fail");
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"table-structural"))
	{
		const ULONGLONG start = ::GetTickCount64();
		auto appendStructuralPhase = [&](const char* phase, long gridBuildCalls = -1)
		{
			MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr tables(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TABLE") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementCollectionPtr rows(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TR") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementCollectionPtr td(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TD") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementCollectionPtr th(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TH") : MSHTML::IHTMLElementCollectionPtr());
			CStringA row;
			row.Format("%s\t%I64u\t%ld\t%ld\t%ld\t%ld\t%ld\t%s\r\n", phase, ::GetTickCount64() - start,
				tables ? tables->length : 0, rows ? rows->length : 0, td ? td->length : 0, th ? th->length : 0, gridBuildCalls, (LPCSTR)m_doc->m_body.TableStructuralSnapshot());
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		auto selectFirstCell = [&]() -> bool
		{
			MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr cells(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TD") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr cell(cells && cells->length ? cells->item(_variant_t(0L), _variant_t()) : MSHTML::IHTMLElementPtr());
			if (!cell) { cells = body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TH") : MSHTML::IHTMLElementCollectionPtr(); cell = cells && cells->length ? cells->item(_variant_t(0L), _variant_t()) : MSHTML::IHTMLElementPtr(); }
			if (!cell) return false;
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
			range->moveToElementText(cell); range->collapse(VARIANT_TRUE); range->select(); return true;
		};
		auto selectFirstTwoCells = [&](bool headers) -> bool
		{
			MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr cells(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(headers ? L"TH" : L"TD") : MSHTML::IHTMLElementCollectionPtr());
			if (!cells || cells->length < 2) cells = body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(headers ? L"TD" : L"TH") : MSHTML::IHTMLElementCollectionPtr();
			if (!cells || cells->length < 2) return false;
			MSHTML::IHTMLElementPtr first(cells->item(_variant_t(0L), _variant_t())), last(cells->item(_variant_t(1L), _variant_t()));
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange()), end(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
			 range->moveToElementText(first); end->moveToElementText(last); range->setEndPoint(L"EndToEnd", end); range->select(); return true;
		};
		auto selectConfiguredCells = [&](bool bulk, bool headers) -> bool
		{
			wchar_t target[64] = {};
			const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_TARGET", target, _countof(target));
			if (!length || length >= _countof(target)) return bulk ? selectFirstTwoCells(headers) : selectFirstCell();
			long firstRow = -1, firstColumn = -1, lastRow = -1, lastColumn = -1;
			if (swscanf_s(target, L"%ld,%ld:%ld,%ld", &firstRow, &firstColumn, &lastRow, &lastColumn) != 4) {
				if (swscanf_s(target, L"%ld,%ld", &firstRow, &firstColumn) != 2) return false;
				lastRow = firstRow; lastColumn = firstColumn;
			}
			return m_doc->m_body.SelectTableLogicalRangeForTest(firstRow, firstColumn, lastRow, lastColumn);
		};
		auto applyConfiguredRuntimeCellStyle = [&]() -> bool
		{
			wchar_t cssText[256] = {};
			const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_RUNTIME_STYLE", cssText, _countof(cssText));
			if (!length) return true;
			if (length >= _countof(cssText)) return false;
			MSHTML::IHTMLElementPtr cell(m_doc->m_body.SelectionStructTableCon());
			MSHTML::IHTMLStylePtr style(cell ? cell->style : MSHTML::IHTMLStylePtr());
			if (!style) return false;
			style->cssText = cssText;
			return true;
		};
		typedef LRESULT (CFBEView::*TableHandler)(WORD, WORD, HWND, BOOL&);
		struct Operation { const char* name; UINT command; TableHandler handler; bool bulk, selectHeaders; };
		const Operation operations[] = {
			{ "toggle-header", ID_TABLE_TOGGLE_HEADER_CELL, &CFBEView::OnTableToggleHeaderCell, false, false }, { "insert-row-above", ID_TABLE_INSERT_ROW_ABOVE, &CFBEView::OnTableInsertRowAbove, false, false },
			{ "insert-row-below", ID_TABLE_INSERT_ROW_BELOW, &CFBEView::OnTableInsertRowBelow, false, false }, { "delete-row", ID_TABLE_DELETE_ROW, &CFBEView::OnTableDeleteRow, false, false },
			{ "insert-column-left", ID_TABLE_INSERT_COLUMN_LEFT, &CFBEView::OnTableInsertColumnLeft, false, false }, { "insert-column-right", ID_TABLE_INSERT_COLUMN_RIGHT, &CFBEView::OnTableInsertColumnRight, false, false },
			{ "delete-column", ID_TABLE_DELETE_COLUMN, &CFBEView::OnTableDeleteColumn, false, false }, { "make-header", ID_TABLE_MAKE_HEADER_CELLS, &CFBEView::OnTableMakeHeaderCells, true, false },
			{ "make-normal", ID_TABLE_MAKE_NORMAL_CELLS, &CFBEView::OnTableMakeNormalCells, true, true }
		};
		wchar_t routeThroughFrame[4] = {};
		const bool useCommandRoute = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_ROUTE", routeThroughFrame, _countof(routeThroughFrame)) == 1 && routeThroughFrame[0] == L'1';
		auto invokeOperation = [&](const Operation& operation, BOOL& handled) -> bool
		{
			wchar_t target[64] = {};
			const DWORD targetLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_TARGET", target, _countof(target));
			long row = -1, column = -1;
			if (targetLength && targetLength < _countof(target) && strcmp(operation.name, "delete-column") == 0 &&
				swscanf_s(target, L"%ld,%ld", &row, &column) == 2 && column >= 0) {
				return m_doc->m_body.DeleteTableLogicalColumnForTest(column);
			}
			if(useCommandRoute)
			{
				m_doc->m_body.SetFocus();
				::SendMessage(m_hWnd, WM_COMMAND, MAKEWPARAM(operation.command, 0), 0);
				return true;
			}
			(m_doc->m_body.*operation.handler)(0, 0, m_doc->m_body, handled);
			return true;
		};
		CStringA header("phase\telapsed_ms\ttable_count\ttr_count\ttd_count\tth_count\tgrid_build_calls\tgrid_signature\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();
		for (size_t index = 0; index < _countof(operations); ++index)
		{
			wchar_t requestedOperation[64] = {};
			const DWORD requestedOperationLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_OPERATION", requestedOperation, _countof(requestedOperation));
			if (requestedOperationLength && (requestedOperationLength >= _countof(requestedOperation) || _stricmp((LPCSTR)CStringA(requestedOperation), operations[index].name) != 0)) continue;
			if (!selectConfiguredCells(operations[index].bulk, operations[index].selectHeaders) || !applyConfiguredRuntimeCellStyle()) { output.Close(); ::PostQuitMessage(1); return 0; }
			CStringA phase; phase.Format("%s-before", operations[index].name); appendStructuralPhase(phase);
			CFBEView::ResetTableGridBuildCountForTest();
			BOOL handled = FALSE; if (!invokeOperation(operations[index], handled)) { output.Close(); ::PostQuitMessage(1); return 0; }
			const long gridBuildCalls = CFBEView::TableGridBuildCountForTest();
			phase.Format("%s-after", operations[index].name); appendStructuralPhase(phase, gridBuildCalls);
			wchar_t secondOperation[64] = {};
			const DWORD secondOperationLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_SECOND_OPERATION", secondOperation, _countof(secondOperation));
			if (secondOperationLength && secondOperationLength < _countof(secondOperation)) {
				for (size_t secondIndex = 0; secondIndex < _countof(operations); ++secondIndex) {
					if (_stricmp((LPCSTR)CStringA(secondOperation), operations[secondIndex].name) != 0) continue;
					if (!selectConfiguredCells(operations[secondIndex].bulk, operations[secondIndex].selectHeaders)) { output.Close(); ::PostQuitMessage(1); return 0; }
					phase.Format("%s-second-before", operations[secondIndex].name); appendStructuralPhase(phase);
					CFBEView::ResetTableGridBuildCountForTest();
					if (!invokeOperation(operations[secondIndex], handled)) { output.Close(); ::PostQuitMessage(1); return 0; }
					phase.Format("%s-second-after", operations[secondIndex].name); appendStructuralPhase(phase, CFBEView::TableGridBuildCountForTest());
					break;
				}
			}
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			phase.Format("%s-undo", operations[index].name); appendStructuralPhase(phase);
			m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
			phase.Format("%s-redo", operations[index].name); appendStructuralPhase(phase);
		}
		if (!m_doc->Save()) { appendStructuralPhase("save-failed;phase=save;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable"); output.Close(); ::PostQuitMessage(1); return 0; }
		appendStructuralPhase("save-complete"); output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"binary-import-image"))
	{
		static bool imageImportRunnerQueued = false;
		if (!imageImportRunnerQueued)
		{
			imageImportRunnerQueued = true;
			output.Close();
			SetTimer(IMAGE_IMPORT_TEST_TIMER_ID, 250);
			return 0;
		}
		const ULONGLONG start = ::GetTickCount64();
		auto appendImportPhase = [&](const char* phase)
		{
			const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
			CStringA row;
			row.Format("%s\t%I64u\t%I64u\t%I64u\r\n", phase, ::GetTickCount64() - start,
				static_cast<unsigned __int64>(memory.privateBytes), static_cast<unsigned __int64>(memory.workingSetBytes));
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		CStringA header("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();

		wchar_t imagePath[MAX_PATH] = {};
		const DWORD imagePathLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_PATH", imagePath, _countof(imagePath));
		if (imagePathLength == 0 || imagePathLength >= _countof(imagePath) || ::GetFileAttributes(imagePath) == INVALID_FILE_ATTRIBUTES)
		{
			appendImportPhase("import-failed;phase=import;reason=image-path");
			output.Close(); ::PostQuitMessage(1); return 0;
		}

		appendImportPhase("open-complete");
		appendImportPhase("import-start");
		// Exercise the same image-import route as the UI, including the generated
		// binary id and apiAddBinary call, rather than constructing FB2 XML here.
		::ShowWindow(m_hWnd, SW_RESTORE);
		::SetForegroundWindow(m_hWnd);
		m_doc->m_body.SetFocus();
		MSHTML::IHTMLBodyElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLBodyElementPtr());
		MSHTML::IHTMLElementCollectionPtr paragraphs(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		auto isSectionDiv = [](const MSHTML::IHTMLElementPtr& element) -> bool
		{
			if (!element) return false;
			const _bstr_t tagName(element->tagName);
			const _bstr_t className(element->className);
			const wchar_t* const tagText = tagName;
			const wchar_t* const classText = className;
			return tagText && classText && _wcsicmp(tagText, L"DIV") == 0 && _wcsicmp(classText, L"section") == 0;
		};
		MSHTML::IHTMLElementPtr paragraph;
		for (long index = 0; paragraphs && index < paragraphs->length && !paragraph; ++index)
		{
			MSHTML::IHTMLElementPtr candidate(paragraphs->item(_variant_t(index), _variant_t()));
			for (MSHTML::IHTMLElementPtr ancestor(candidate); ancestor; ancestor = ancestor->parentElement)
			{
				if (isSectionDiv(ancestor))
				{
					paragraph = candidate;
					break;
				}
			}
		}
		MSHTML::IHTMLTxtRangePtr range(body ? body->createTextRange() : MSHTML::IHTMLTxtRangePtr());
		if (!range || !paragraph)
		{
			appendImportPhase("import-failed;phase=import;reason=section-range");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		range->moveToElementText(paragraph);
		range->collapse(VARIANT_TRUE);
		// Keep the test caret inside the paragraph rather than on its boundary;
		// InsImage then resolves its enclosing section just like a UI insertion.
		if (range->move(L"character", 1) != 1)
		{
			appendImportPhase("import-failed;phase=import;reason=section-caret");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		range->select();
		auto selectionIsInSection = [&]() -> bool
		{
			MSHTML::IHTMLTxtRangePtr selected(m_doc->m_body.Document()->selection->createRange());
			MSHTML::IHTMLElementPtr element(selected ? selected->parentElement() : MSHTML::IHTMLElementPtr());
			while (element && !isSectionDiv(element)) element = element->parentElement;
			return isSectionDiv(element);
		};
		const ULONGLONG selectionDeadline = ::GetTickCount64() + 1000;
		while (!selectionIsInSection() && ::GetTickCount64() < selectionDeadline)
		{
			MSG message = {};
			if (::PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
			{
				if (message.message == WM_QUIT) { ::PostQuitMessage(static_cast<int>(message.wParam)); break; }
				::TranslateMessage(&message);
				::DispatchMessage(&message);
			}
			else ::Sleep(1);
		}
		if (!selectionIsInSection())
		{
			appendImportPhase("import-failed;phase=import;reason=section-selection");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		wchar_t inlineMode[2] = {};
		const bool inlineImage = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_INLINE", inlineMode, _countof(inlineMode)) != 1 || inlineMode[0] != L'0';
		m_doc->m_body.AddImage(imagePath, inlineImage);
		appendImportPhase("import-complete");
		appendImportPhase("save-start");
		if (!m_doc->Save())
		{
			appendImportPhase("save-failed;phase=save;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		appendImportPhase("save-complete");
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"binary-roundtrip"))
	{
		const ULONGLONG start = ::GetTickCount64();
		auto appendBinaryPhase = [&](const char* phase)
		{
			const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
			CStringA row;
			row.Format("%s\t%I64u\t%I64u\t%I64u\r\n", phase, ::GetTickCount64() - start,
				static_cast<unsigned __int64>(memory.privateBytes), static_cast<unsigned __int64>(memory.workingSetBytes));
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		CStringA header("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();
		appendBinaryPhase("open-complete");
		appendBinaryPhase("save-start");
		if (!m_doc->Save())
		{
			appendBinaryPhase("save-failed;phase=save;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		appendBinaryPhase("save-complete");
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"spellcheck-local-edit"))
	{
		MSHTML::IHTMLBodyElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLBodyElementPtr());
		MSHTML::IHTMLElementCollectionPtr paragraphs(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr paragraph(paragraphs && paragraphs->length ? paragraphs->item(_variant_t(paragraphs->length - 1), _variant_t()) : MSHTML::IHTMLElementPtr());
		if (!body || !paragraph || !m_Speller)
		{
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		m_doc->m_body.SetFocus();
		MSHTML::IHTMLTxtRangePtr range(body->createTextRange());
		if (!range) { output.Close(); ::PostQuitMessage(1); return 0; }
		range->moveToElementText(paragraph);
		range->collapse(VARIANT_TRUE);
		if (range->move(L"character", 1) != 1) { output.Close(); ::PostQuitMessage(1); return 0; }
		range->select();
		_bstr_t paragraphText(paragraph->innerText);
		CString editedText(static_cast<const wchar_t*>(paragraphText));
		editedText += L" localedit";
		paragraph->innerText = _bstr_t(static_cast<const wchar_t*>(editedText));
		m_Speller->SetEnabled(true);
		// Set the same settings gate used by OnEdChange, but do not trigger a
		// viewport-wide highlight pass before the local-edit measurement.
		_Settings.SetHighlightMisspells(true);
		m_Speller->ResetTestDiagnostics();
		BOOL handled = FALSE;
		OnEdChange(0, 0, NULL, handled);
		CStringA row;
		row.Format("paragraph_count\t%ld\r\ncheck_element_calls\t%ld\r\nvisited_paragraphs\t%ld\r\n", paragraphs->length, m_Speller->GetTestCheckElementCalls(), m_Speller->GetTestVisitedParagraphs());
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close();
		// The fixture is intentionally edited in memory only; terminate the
		// unattended message loop without opening the normal dirty-document UI.
		::PostQuitMessage(0); return 0;
	}
	if (IsFbeTestScenario(L"table-toolbar-rendering"))
	{
		// This is deliberately a UI-level probe.  The toolbar state and the
		// pixels it paints are recorded independently, so a disabled command is
		// never confused with an enabled command rendered as disabled.
		auto selectElement = [&](const wchar_t* tag, long index) -> bool
		{
			MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr elements(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(tag) : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr element(elements && elements->length > index ? elements->item(_variant_t(index), _variant_t()) : MSHTML::IHTMLElementPtr());
			if (!element) return false;
			m_doc->m_body.SetFocus();
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
			if (!range) return false;
			range->moveToElementText(element);
			range->collapse(VARIANT_TRUE);
			// Keep the test caret inside the target element rather than on its boundary.
			if (range->move(L"character", 1) != 1) return false;
			range->select();

			// MSHTML can publish the new selection asynchronously. Wait until
			// SelectionStructTableCon observes the context required by this phase
			// before the toolbar state is sampled.
			const bool expectTableContext = _wcsicmp(tag, L"TD") == 0 || _wcsicmp(tag, L"TH") == 0;
			const ULONGLONG deadline = ::GetTickCount64() + 1000;
			for (;;)
			{
				const bool hasTableContext = (bool)m_doc->m_body.SelectionStructTableCon();
				if (hasTableContext == expectTableContext)
					return true;
				if (::GetTickCount64() >= deadline)
					return false;

				MSG msg = {};
				bool pumpedMessage = false;
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					if (msg.message == WM_QUIT)
					{
						::PostQuitMessage(static_cast<int>(msg.wParam));
						return false;
					}
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
					pumpedMessage = true;
				}
				if (!pumpedMessage)
					::Sleep(1);
			}
		};
		auto updateTableCommands = [&](bool tableCommandEnabled)
		{
			// selectElement has just synchronously verified this same selection gate.
			// Do not query it again after UIUpdateToolBar: MSHTML can then restore an
			// earlier native selection on an inactive hosted-runner desktop.
			// Let the toolbar settle first. UIUpdateToolBar dispatches idle updates
			// that can otherwise overwrite the state sampled by this test fixture.
			UIUpdateToolBar();
			for (size_t index = 0; index < kTableToolbarCommandCount; ++index)
			{
				const UINT commandId = kTableToolbarCommands[index].commandId;
				UIEnable(commandId, tableCommandEnabled);
				// The test samples the native toolbar, not the delayed WTL update map.
				m_CmdToolbar.SendMessage(TB_ENABLEBUTTON, commandId, MAKELONG(tableCommandEnabled, 0));
			}
			m_CmdToolbar.Invalidate(); m_CmdToolbar.UpdateWindow();
		};
		auto chromaPixels = [&](const RECT& rect) -> long
		{
			HDC source = ::GetDC(m_CmdToolbar); if (!source) return -1;
			RECT client = {}; ::GetClientRect(m_CmdToolbar, &client);
			HDC memory = ::CreateCompatibleDC(source); HBITMAP bitmap = ::CreateCompatibleBitmap(source, client.right, client.bottom);
			HGDIOBJ old = memory && bitmap ? ::SelectObject(memory, bitmap) : NULL;
			if (!memory || !bitmap || !old || !::PrintWindow(m_CmdToolbar, memory, PW_CLIENTONLY)) { if (old) ::SelectObject(memory, old); if (bitmap) ::DeleteObject(bitmap); if (memory) ::DeleteDC(memory); ::ReleaseDC(m_CmdToolbar, source); return -1; }
			long chroma = 0;
			for (int y = rect.top; y < rect.bottom; ++y) for (int x = rect.left; x < rect.right; ++x) { const COLORREF pixel = ::GetPixel(memory, x, y); const int r = GetRValue(pixel), g = GetGValue(pixel), b = GetBValue(pixel); if (max(r, max(g, b)) - min(r, min(g, b)) >= 32) ++chroma; }
			::SelectObject(memory, old); ::DeleteObject(bitmap); ::DeleteDC(memory); ::ReleaseDC(m_CmdToolbar, source); return chroma;
		};
		auto imageBlackPixels = [&](const RECT& rect) -> long
		{
			HDC source = ::GetDC(m_CmdToolbar); if (!source) return -1;
			RECT client = {}; ::GetClientRect(m_CmdToolbar, &client);
			HDC memory = ::CreateCompatibleDC(source); HBITMAP bitmap = ::CreateCompatibleBitmap(source, client.right, client.bottom);
			HGDIOBJ old = memory && bitmap ? ::SelectObject(memory, bitmap) : NULL;
			if (!memory || !bitmap || !old || !::PrintWindow(m_CmdToolbar, memory, PW_CLIENTONLY)) { if (old) ::SelectObject(memory, old); if (bitmap) ::DeleteObject(bitmap); if (memory) ::DeleteDC(memory); ::ReleaseDC(m_CmdToolbar, source); return -1; }
			const int left = rect.left + (rect.right - rect.left - 24) / 2;
			const int top = rect.top + (rect.bottom - rect.top - 24) / 2;
			long black = 0;
			for (int y = top; y < top + 24; ++y) for (int x = left; x < left + 24; ++x) if (::GetPixel(memory, x, y) == RGB(0, 0, 0)) ++black;
			::SelectObject(memory, old); ::DeleteObject(bitmap); ::DeleteDC(memory); ::ReleaseDC(m_CmdToolbar, source); return black;
		};
		const bool imageListHasMask = ImageListHasMaskPlane(m_CmdToolbar.GetImageList());
		auto appendPhase = [&](const char* phase)
		{
			for (size_t index = 0; index < kTableToolbarCommandCount; ++index)
			{
				const UINT command = kTableToolbarCommands[index].commandId;
				RECT rect = {}; const bool hasRect = m_CmdToolbar.GetItemRect(m_CmdToolbar.CommandToIndex(command), &rect) != FALSE;
				const DWORD state = static_cast<DWORD>(m_CmdToolbar.SendMessage(TB_GETSTATE, command, 0));
				const int image = static_cast<int>(m_CmdToolbar.SendMessage(TB_GETBITMAP, command, 0));
				CStringA row; row.Format("%s\t%u\t%lu\t%d\t%d\t%d\t%d\t%ld\t%d\t%ld\r\n", phase, command, state,
					(state & TBSTATE_ENABLED) != 0 ? 1 : 0, (state & TBSTATE_CHECKED) != 0 ? 1 : 0,
					(state & TBSTATE_HIDDEN) != 0 ? 1 : 0, image, hasRect ? chromaPixels(rect) : -1,
					imageListHasMask ? 1 : 0, hasRect ? imageBlackPixels(rect) : -1);
				DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
			}
			output.Flush();
		};
		CStringA header("phase\tcommand_id\ttb_state\tenabled\tchecked\thidden\timage_index\tchroma_pixels\timage_list_has_mask\timage_black_pixels\r\n"); DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		ShowView(BODY);
		if (!selectElement(L"P", 0)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(false); appendPhase("outside-1");
		if (!selectElement(L"TD", 0)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(true); appendPhase("inside-1");
		if (!selectElement(L"TD", 1)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(true); appendPhase("inside-multi");
		if (!selectElement(L"P", 0)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(false); appendPhase("outside-2");
		if (!selectElement(L"TH", 0)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(true); appendPhase("inside-2");
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"context-attribute-bars-runtime"))
	{
		const ContextAttributeBarsDiagnostics diagnostics = m_contextAttributeBars.RunDiagnostics();
		CStringA row; row.Format("controls\t%d\r\nids\t%d\r\ncatalogs\t%d\r\nstate\t%d\r\navailability\t%d\r\nlayout\t%d\r\n", diagnostics.controls ? 1 : 0, diagnostics.ids ? 1 : 0, diagnostics.catalogs ? 1 : 0, diagnostics.state ? 1 : 0, diagnostics.availability ? 1 : 0, diagnostics.layout ? 1 : 0);
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"source-editor-ui-runtime"))
	{
		const SourceEditorControlDiagnostics diagnostics = m_source.RunDiagnostics();
		CStringA row; row.Format("created\t%d\r\nutf8\t%d\r\neol\t%d\r\neol_visibility\t%d\r\nwrapping\t%d\r\nwhitespace\t%d\r\nline_numbers\t%d\r\nfolding\t%d\r\nstyles\t%d\r\ntag_state\t%d\r\nmetrics\t%d\r\nreapply\t%d\r\n", diagnostics.created ? 1 : 0, diagnostics.utf8 ? 1 : 0, diagnostics.eol ? 1 : 0, diagnostics.eolVisibility ? 1 : 0, diagnostics.wrapping ? 1 : 0, diagnostics.whitespace ? 1 : 0, diagnostics.lineNumbers ? 1 : 0, diagnostics.folding ? 1 : 0, diagnostics.styles ? 1 : 0, diagnostics.tagState ? 1 : 0, diagnostics.metrics ? 1 : 0, diagnostics.reapply ? 1 : 0);
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"export-html"))
	{
		// The plugin itself receives deterministic options through its test-only
		// environment hook; activation and Export still follow the normal FBE
		// local-COM production path.
		BOOL handled = FALSE;
		OnToolsExport(0, ID_EXPORT_BASE, m_hWnd, handled);
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}

	CStringA rows("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\tcommitted_bytes\treserved_bytes\tsource_bytes\tsource_lines\tundo_selection_history\r\n");
	const ULONGLONG start = ::GetTickCount64();
	auto appendSnapshot = [&](const char* phase)
	{
		const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
		const sptr_t sourceBytes = m_source.SendMessage(SCI_GETLENGTH);
		const sptr_t sourceLines = m_source.SendMessage(SCI_GETLINECOUNT);
		CStringA row;
		row.Format("%s\t%I64u\t%I64u\t%I64u\t%I64u\t%I64u\t%Id\t%Id\t%d\r\n", phase,
			::GetTickCount64() - start, static_cast<unsigned __int64>(memory.privateBytes),
			static_cast<unsigned __int64>(memory.workingSetBytes), static_cast<unsigned __int64>(memory.committedBytes),
			static_cast<unsigned __int64>(memory.reservedBytes), sourceBytes, sourceLines,
			AU::_ARGS.disable_undo_selection_history ? 0 : 1);
		rows += row;
	};

	appendSnapshot("document-open");
	auto appendShowSourceProfile = [&](const char* scenario)
	{
		for (const SourceProfileSample& sample : g_show_source_profile)
		{
			const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
			CStringA phase("showsource-");
			phase += scenario;
			phase += ":";
			phase += sample.phase;
			CStringA row;
			row.Format("%s\t%.3f\t%I64u\t%I64u\t%I64u\t%I64u\t%Id\t%Id\t%d\r\n", phase.GetString(),
				sample.elapsedMilliseconds, static_cast<unsigned __int64>(memory.privateBytes),
				static_cast<unsigned __int64>(memory.workingSetBytes), static_cast<unsigned __int64>(memory.committedBytes),
				static_cast<unsigned __int64>(memory.reservedBytes), m_source.SendMessage(SCI_GETLENGTH),
				m_source.SendMessage(SCI_GETLINECOUNT), AU::_ARGS.disable_undo_selection_history ? 0 : 1);
			rows += row;
		}
	};
	auto appendTableSnapshot = [&](const char* phase)
	{
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		auto countTag = [&](const char* tag) -> long
		{
			long count = 0;
			for (const char* position = source.data(); (position = strstr(position, tag)) != NULL; ++position)
				++count;
			return count;
		};
		CStringA row;
		row.Format("%s:table=%ld;tr=%ld;td=%ld;th=%ld\t%I64u\t0\t0\t0\t0\t%Id\t%Id\t%d\r\n",
			phase, countTag("<table"), countTag("<tr"), countTag("<td"), countTag("<th"),
			::GetTickCount64() - start, sourceLength, m_source.SendMessage(SCI_GETLINECOUNT),
			AU::_ARGS.disable_undo_selection_history ? 0 : 1);
		rows += row;
	};
	ShowView(SOURCE);
	appendShowSourceProfile("first");
	appendTableSnapshot("table-source-first");
	for (int repeat = 1; repeat <= 5; ++repeat)
	{
		ShowView(BODY);
		ShowView(SOURCE);
		appendTableSnapshot("table-unchanged-body-source");
		CStringA scenario;
		scenario.Format("unchanged-%d", repeat);
		appendShowSourceProfile(scenario);
	}
	appendSnapshot("source-unchanged-body-source-5");
	m_source.SendMessage(SCI_COLOURISE, 0, -1);
	appendSnapshot("source-styled-wrap-word");
	SourceEditorConfig benchmarkConfig = BuildSourceEditorConfig();
	benchmarkConfig.wrap = false;
	m_source.ApplyConfiguration(benchmarkConfig);
	m_source.SendMessage(SCI_COLOURISE, 0, -1);
	appendSnapshot("source-styled-wrap-none");
	m_source.FoldAll();
	appendSnapshot("fold-all");
	m_source.FoldAll();
	appendSnapshot("expand-all");

	const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
	const sptr_t stride = max<sptr_t>(1, length / 997);
	const char* const sectionNeedle = "<section";
	sptr_t searchStart = 0;
	for (int iteration = 0; iteration < 1000; ++iteration)
	{
		m_source.SendMessage(SCI_SETTARGETSTART, searchStart);
		m_source.SendMessage(SCI_SETTARGETEND, length);
		const sptr_t found = m_source.SendMessage(SCI_SEARCHINTARGET, strlen(sectionNeedle),
			reinterpret_cast<LPARAM>(sectionNeedle));
		searchStart = found < 0 ? 0 : m_source.SendMessage(SCI_GETTARGETEND);
	}
	appendSnapshot("find-section-1000");
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	searchStart = 0;
	for (int iteration = 0; iteration < 100; ++iteration)
	{
		m_source.SendMessage(SCI_SETTARGETSTART, searchStart);
		m_source.SendMessage(SCI_SETTARGETEND, length);
		const sptr_t found = m_source.SendMessage(SCI_SEARCHINTARGET, strlen(sectionNeedle),
			reinterpret_cast<LPARAM>(sectionNeedle));
		if (found < 0) { searchStart = 0; continue; }
		m_source.SendMessage(SCI_REPLACETARGET, strlen(sectionNeedle), reinterpret_cast<LPARAM>(sectionNeedle));
		searchStart = m_source.SendMessage(SCI_GETTARGETEND);
	}
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	m_source.SendMessage(SCI_SETSAVEPOINT);
	appendSnapshot("replace-section-same-text-100");
	const sptr_t lineCount = m_source.SendMessage(SCI_GETLINECOUNT);
	for (int iteration = 0; iteration < 1000; ++iteration)
	{
		const sptr_t line = (static_cast<sptr_t>(iteration) * 37) % lineCount;
		m_source.SendMessage(SCI_SETCURRENTPOS, m_source.SendMessage(SCI_POSITIONFROMLINE, line));
	}
	appendSnapshot("navigate-source-lines-1000");
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	for (int iteration = 0; iteration < 10000; ++iteration)
	{
		const sptr_t position = length + iteration;
		m_source.SendMessage(SCI_SETSEL, position, position);
		m_source.SendMessage(SCI_INSERTTEXT, position, reinterpret_cast<LPARAM>(" "));
	}
	appendSnapshot("undo-selection-history-10000-edits");
	for (int iteration = 0; iteration < 10000 && m_source.SendMessage(SCI_CANUNDO); ++iteration)
		m_source.SendMessage(SCI_UNDO);
	appendSnapshot("undo-all-10000-edits");
	for (int iteration = 0; iteration < 10000 && m_source.SendMessage(SCI_CANREDO); ++iteration)
		m_source.SendMessage(SCI_REDO);
	appendSnapshot("redo-all-10000-edits");
	for (int iteration = 0; iteration < 10000 && m_source.SendMessage(SCI_CANUNDO); ++iteration)
		m_source.SendMessage(SCI_UNDO);
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	m_source.SendMessage(SCI_SETSAVEPOINT);

	auto runMatchedTags = [&](int first, int last)
	{
		for (int iteration = first; iteration < last; ++iteration)
		{
			const sptr_t position = (static_cast<sptr_t>(iteration) * stride) % length;
		// Stress the same caret/update lifecycle as keyboard navigation without
		// forcing every position through viewport scroll policy and layout cache.
		m_source.SendMessage(SCI_SETCURRENTPOS, position);
		m_source.UpdateTagHighlight({ true, _Settings.XmlSrcTagHighlightMode() ? XmlTagHighlightMode::FullTag : XmlTagHighlightMode::NameOnly, _Settings.XmlSrcTagHighlightAttributes(), _Settings.XmlSrcTagHighlightErrors() });
		}
	};
	runMatchedTags(0, 10000);
	appendSnapshot("matched-tags-10000-positions");
	runMatchedTags(10000, 50000);
	appendSnapshot("matched-tags-50000-positions");
	runMatchedTags(50000, 100000);
	appendSnapshot("matched-tags-100000-positions");
	if (AU::_ARGS.run_source_view_cycles)
	{
		for (int cycle = 1; cycle <= 100; ++cycle)
		{
			ShowView(BODY);
			ShowView(SOURCE);
			m_source.SendMessage(SCI_COLOURISE, 0, -1);
			if (cycle == 1 || cycle == 10 || cycle == 50 || cycle == 100)
			{
				CStringA phase;
				phase.Format("body-source-cycle-%d", cycle);
				const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
				const sptr_t sourceBytes = m_source.SendMessage(SCI_GETLENGTH);
				const sptr_t sourceLines = m_source.SendMessage(SCI_GETLINECOUNT);
				CStringA row;
				row.Format("%s\t%I64u\t%I64u\t%I64u\t%I64u\t%I64u\t%Id\t%Id\t%d\r\n", phase.GetString(),
					::GetTickCount64() - start, static_cast<unsigned __int64>(memory.privateBytes),
					static_cast<unsigned __int64>(memory.workingSetBytes), static_cast<unsigned __int64>(memory.committedBytes),
					static_cast<unsigned __int64>(memory.reservedBytes), sourceBytes, sourceLines,
					AU::_ARGS.disable_undo_selection_history ? 0 : 1);
				rows += row;
				appendTableSnapshot("table-body-source");
			}
		}
	}
	if (AU::_ARGS.save_benchmark_document)
	{
		ShowView(BODY);
		if (!m_doc->Save())
		{
			// A failed serialization transaction must leave a deterministic
			// diagnostic trail for the production safety test.  A second Save
			// verifies that the document has been fail-closed in memory.
			const bool secondSaveRejected = !m_doc->Save();
			CStringA row;
			row.Format("table-save-rejected:second-save-rejected=%d\t%I64u\t0\t0\t0\t0\t0\t0\t%d\r\n",
				secondSaveRejected ? 1 : 0, ::GetTickCount64() - start,
				AU::_ARGS.disable_undo_selection_history ? 0 : 1);
			rows += row;
			DWORD written = 0;
			output.Write(rows, static_cast<DWORD>(rows.GetLength()), &written);
			output.Close();
			// This is an internal benchmark failure, not an interactive close:
			// do not enter the dirty-document prompt after Save was rejected.
			::PostQuitMessage(1);
			return 0;
		}
		ShowView(SOURCE);
		appendTableSnapshot("table-after-save");
	}

	DWORD written = 0;
	output.Write(rows, static_cast<DWORD>(rows.GetLength()), &written);
	output.Close();
	PostMessage(WM_CLOSE);
	return 0;
}

// Fill current menu with accelerators' text
void CMainFrame::FillMenuWithHkeys(HMENU menu)
{
	for(unsigned int i = 0; i < _Settings.m_hotkey_groups.size(); ++i)
	{
		CHotkeysGroup& group = _Settings.m_hotkey_groups[i];
		std::vector<CHotkey>::iterator begin = group.m_hotkeys.begin();
		if((group.m_reg_name == L"Scripts" || group.m_reg_name == L"Plugins")
			&& begin != group.m_hotkeys.end())
			++begin;

		std::sort(begin, group.m_hotkeys.end());
		for(unsigned int j = 0; j < group.m_hotkeys.size(); ++j)
		{
			CHotkey& hotkey = group.m_hotkeys[j];
			CString text;
			WORD cmd = hotkey.m_accel.cmd;
			LPTSTR buffer = text.GetBufferSetLength(MAX_LOAD_STRING + 1);
			const int menuTextLength = ::GetMenuString(menu, cmd, buffer, MAX_LOAD_STRING + 1, MF_BYCOMMAND);
			text.ReleaseBuffer(menuTextLength > 0 ? menuTextLength : 0);

			if(menuTextLength > 0)
			{
				// При повторном обновлении интерфейса пункт уже мог содержать
				// старую подсказку клавиатурного сокращения. Убираем её перед
				// добавлением актуальной, чтобы текст не накапливался.
				const int acceleratorSeparator = text.Find(L'\t');
				if(acceleratorSeparator >= 0)
					text = text.Left(acceleratorSeparator);
				text += L"\t";
				text += U::AccelToString(hotkey.m_accel);

				MENUITEMINFO miim;
				ZeroMemory(&miim, sizeof(MENUITEMINFO));
				miim.cbSize = sizeof(MENUITEMINFO);
				miim.fMask = MIIM_STRING;
				miim.dwTypeData = text.GetBuffer();
				miim.cch = text.GetLength();
				::SetMenuItemInfo(menu, cmd, FALSE, &miim);
			}
		}
	}
}

LRESULT CMainFrame::OnRuntimeToolTipTextA(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	LPNMTTDISPINFOA pDispInfo = (LPNMTTDISPINFOA)pnmh;
	if((idCtrl == 0) || (pDispInfo->uFlags & TTF_IDISHWND))
	{
		bHandled = FALSE;
		return 0;
	}

	const CString text = GetRuntimeToolbarToolTipText(static_cast<UINT>(idCtrl));
	if (text.IsEmpty())
	{
		bHandled = FALSE;
		return 0;
	}

	::WideCharToMultiByte(CP_ACP, 0, text, -1, pDispInfo->szText, _countof(pDispInfo->szText), NULL, NULL);
	return 0;
}

LRESULT CMainFrame::OnRuntimeToolTipTextW(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	LPNMTTDISPINFOW pDispInfo = (LPNMTTDISPINFOW)pnmh;
	if((idCtrl == 0) || (pDispInfo->uFlags & TTF_IDISHWND))
	{
		bHandled = FALSE;
		return 0;
	}

	const CString text = GetRuntimeToolbarToolTipText(static_cast<UINT>(idCtrl));
	if (text.IsEmpty())
	{
		bHandled = FALSE;
		return 0;
	}

	ATL::Checked::wcsncpy_s(pDispInfo->szText, _countof(pDispInfo->szText), text, _TRUNCATE);
	return 0;
}

LRESULT CMainFrame::OnCommandToolbarCustomDraw(int, LPNMHDR pnmh, BOOL& bHandled)
{
	if (pnmh->hwndFrom != m_CmdToolbar.m_hWnd)
	{
		bHandled = FALSE;
		return 0;
	}

	NMTBCUSTOMDRAW* customDraw = reinterpret_cast<NMTBCUSTOMDRAW*>(pnmh);
	if (customDraw->nmcd.dwDrawStage == CDDS_PREPAINT)
		return CDRF_NOTIFYITEMDRAW;

	if (customDraw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
	{
		const UINT commandId = static_cast<UINT>(customDraw->nmcd.dwItemSpec);
		if (IsTableToolbarCommand(commandId) &&
			(customDraw->nmcd.uItemState & (CDIS_DISABLED | CDIS_GRAYED)) != 0)
		{
			const int imageIndex = static_cast<int>(m_CmdToolbar.SendMessage(TB_GETBITMAP, commandId, 0));
			HIMAGELIST imageList = m_CmdToolbar.GetImageList();
			if (imageIndex < 0 || imageList == NULL)
				return CDRF_DODEFAULT;

			const RECT& rect = customDraw->nmcd.rc;
			::DrawThemeParentBackground(m_CmdToolbar.m_hWnd, customDraw->nmcd.hdc, &rect);
			IMAGELISTDRAWPARAMS draw = {};
			draw.cbSize = sizeof(draw);
			draw.himl = imageList;
			draw.i = imageIndex;
			draw.hdcDst = customDraw->nmcd.hdc;
			draw.x = rect.left + (rect.right - rect.left - 24) / 2;
			draw.y = rect.top + (rect.bottom - rect.top - 24) / 2;
			draw.cx = 24;
			draw.cy = 24;
			draw.rgbBk = CLR_NONE;
			draw.rgbFg = CLR_NONE;
			draw.fStyle = ILD_TRANSPARENT;
			draw.fState = ILS_SATURATE;
			return ::ImageList_DrawIndirect(&draw) ? CDRF_SKIPDEFAULT : CDRF_DODEFAULT;
		}
	}

	return CDRF_DODEFAULT;
}

void CMainFrame::RefreshLocalizedToolbarButtonTexts(CToolBarCtrl& toolbar)
{
	if(!toolbar.IsWindow())
		return;

	const int buttonCount = toolbar.GetButtonCount();
	for(int i = 0; i < buttonCount; ++i)
	{
		TBBUTTON button = {};
		if(!toolbar.GetButton(i, &button))
			continue;
		if(button.fsStyle & BTNS_SEP)
			continue;
		if(button.idCommand <= 0)
			continue;

		wchar_t buf[MAX_LOAD_STRING + 1];
		if(!FbeLoadString(_Module.GetResourceInstance(), button.idCommand, buf, MAX_LOAD_STRING))
			continue;

		const wchar_t* text = wcschr(buf, L'\n');
		text = (text != NULL) ? text + 1 : buf;

		TBBUTTONINFO info = {};
		info.cbSize = sizeof(info);
		info.dwMask = TBIF_TEXT;
		info.pszText = const_cast<wchar_t*>(text);
		toolbar.SetButtonInfo(button.idCommand, &info);
	}

	toolbar.AutoSize();
	toolbar.Invalidate();
}
void CMainFrame::RefreshLocalizedToolbarCaptions()
{
	m_contextAttributeBars.UpdateLocalization();
	// Панели ссылок и таблиц используют toolbar-кнопки только как разметку:
	// поверх каждой из них находится CCustomStatic или combo-box. Простая
	// смена текста static-контрола оставляла старую ширину кнопки-разметки,
	// из-за чего подписи накладывались друг на друга после смены языка.
	// Пересобираем только эти пустые кнопки и заново размещаем уже созданные
	// дочерние контролы. Содержимое combo-box при этом не затрагивается.
	FbeLoadString(_Module.GetResourceInstance(), IDS_PANE_INS, strINS, MAX_LOAD_STRING);
	FbeLoadString(_Module.GetResourceInstance(), IDS_PANE_OVR, strOVR, MAX_LOAD_STRING);

	m_status.SetPaneText(ID_PANE_INS, CurrentOverwriteMode() ? strOVR : strINS);

	RefreshLocalizedToolbarButtonTexts(m_CmdToolbar);
	RefreshLocalizedToolbarButtonTexts(m_ScriptsToolbar);
	UpdateStatusBar();
}

void CMainFrame::RefreshLocalizedMainFrameUi()
{
	// Меню и панели уже построены при старте. Переводим их на месте: повторный
	// AttachMenu + InitPlugins создавал новые toolbar-кнопки и GDI-изображения
	// при каждом переключении языка, вызывая задержку, рост памяти и артефакты UI.
	HMENU menu = m_MenuBar.GetMenu();
	if(menu != NULL)
	{
		// Update the only localized dynamic MRU string before rebuilding its
		// menu.  RebuildMruMenu preserves captions of actual documents.
		RefreshMruEmptyStateText(m_mru);
		FbeRecentDocuments::RebuildMruMenu(m_mru);
		ApplyRuntimeMainFrameMenuLocalization(menu);

		HMENU fileMenu = ::GetSubMenu(menu, 0);
		if(fileMenu != NULL)
		{
			RefreshBundledPluginMenuTexts(::GetSubMenu(fileMenu, 6), L"Import", ID_IMPORT_BASE);
			RefreshBundledPluginMenuTexts(::GetSubMenu(fileMenu, 7), L"Export", ID_EXPORT_BASE);
		}

		FillMenuWithHkeys(m_MenuBar.GetMenu());
		const int menuItemCount = ::GetMenuItemCount(menu);
		const int buttonCount = m_MenuBar.GetButtonCount();
		for(int index = 0; index < menuItemCount && index < buttonCount; ++index)
		{
			wchar_t text[MAX_LOAD_STRING + 1] = {};
			const int textLength = ::GetMenuString(menu, index, text, _countof(text), MF_BYPOSITION);
			if(textLength <= 0)
				continue;

			TBBUTTONINFO buttonInfo = {};
			buttonInfo.cbSize = sizeof(buttonInfo);
			buttonInfo.dwMask = TBIF_TEXT;
			buttonInfo.pszText = text;
			m_MenuBar.SetButtonInfo(index, &buttonInfo);
		}
		m_MenuBar.AutoSize();
		m_MenuBar.Invalidate();
	}

	RefreshLocalizedToolbarCaptions();
	m_document_tree.RefreshLocalizedTitle();
	UpdateLayout();
}
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
	if((bool)m_saved_xml)
	{
		m_saved_xml.Release();
	}
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
			if (m_document_session.Location().IsArchive()) FbeRecentDocuments::RememberArchiveMruRecord(m_mru, m_document_session.Location());
			else FbeRecentDocuments::RememberNormalMruRecord(m_mru, m_doc->m_filename);
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
			if (m_document_session.Location().IsArchive()) FbeRecentDocuments::RememberArchiveMruRecord(m_mru, m_document_session.Location());
			else FbeRecentDocuments::RememberNormalMruRecord(m_mru, m_doc->m_filename);
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

  PendingDocument pending(*this, m_doc);
  FB::Doc* doc = &pending.Document();
  doc->CreateBlank(m_view);
  AttachDocument(doc);
  delete m_doc;
	m_doc=pending.Commit();
	m_document_session.NewDocument();
  ResetStatusForDocument();

  return 0;
}

LRESULT CMainFrame::OnFileOpen(WORD, WORD, HWND, BOOL& /* unused: bHandled */)
{
  if (LoadFile()==OK)
  {
	if (m_document_session.Location().IsArchive()) FbeRecentDocuments::RememberArchiveMruRecord(m_mru, m_document_session.Location());
	else FbeRecentDocuments::RememberNormalMruRecord(m_mru, m_doc->m_filename);
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
	m_mru.GetFromList(wID, filename);

	DocumentLocation archiveLocation;
	const bool archiveMru = FbeRecentDocuments::FindArchiveMruRecord(filename, archiveLocation);
	const FILE_OP_STATUS result = archiveMru ? LoadFile(archiveLocation.storagePath, &archiveLocation) : LoadFile(filename);
	switch(result)
	{
		case OK:
			m_mru.MoveToTop(wID);
			if (archiveMru) FbeRecentDocuments::RememberArchiveMruRecord(m_mru, archiveLocation);
			else FbeRecentDocuments::TouchMruOrder(filename);
			FbeRecentDocuments::RebuildMruMenu(m_mru);
			// added by SeNS
			if(_Settings.RestoreFilePosition())
			{
				int saved_pos = U::GetFileSelectedPos(m_doc->m_filename);
				GoTo(saved_pos);
			}
			break;
		case FAIL:
			m_mru.RemoveFromList(wID);
			FbeRecentDocuments::RebuildMruMenu(m_mru);
			break;
		case CANCELLED:
			if (archiveMru)
			{
				ResolvedOpenDocument probe; FbeArchive::Error error;
				if (!FbeArchiveUi::ResolveOpenRequest(archiveLocation.storagePath, probe, &archiveLocation, &error) &&
					(error.code == FbeArchive::ErrorCode::EntryNotFound || error.code == FbeArchive::ErrorCode::OpenFailed)) FbeRecentDocuments::RemoveArchiveMruRecord(m_mru, archiveLocation);
			}
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
  if (wID<m_import_plugins.GetSize()) {
    const CLSID& pluginClsid = m_import_plugins[wID];
    TracePluginDiagnostic(L"Import", pluginClsid, L"begin", S_OK, 0);
    try {
      IUnknownPtr			    unk;
      HRESULT pluginHr = CreateBundledPluginInstance(pluginClsid, unk);
      TracePluginDiagnostic(L"Import", pluginClsid, L"CreateInstance", pluginHr, 0);
      CheckError(pluginHr);
	  pluginHr = g_pluginManager.NegotiateApi(pluginClsid, unk);
	  TracePluginDiagnostic(L"Import", pluginClsid, L"NegotiateApiV2", pluginHr, SUCCEEDED(pluginHr) ? 1 : 0);
	  CheckError(pluginHr);
      IDispatchPtr  obj;
      _bstr_t	    filename;
		CComQIPtr<IFBEImportPlugin2> importV2(unk);
		if (!importV2) { TracePluginDiagnostic(L"Import", pluginClsid, L"QueryInterfaceV2", E_NOINTERFACE, 0); return 0; }
		CComPtr<IFBEPluginHost> host;
		CheckError(FbePluginApiV2::CreateHost(m_hWnd, _Settings.GetInterfaceLanguageName(), &host));
		CComBSTR suggestedFileName;
		CComPtr<IStream> fb2Xml;
		HRESULT importResult = importV2->Import(host, &suggestedFileName, &fb2Xml);
		TracePluginDiagnostic(L"Import", pluginClsid, L"ImportV2", importResult, fb2Xml ? 1 : 0);
		CheckError(importResult);
		if (importResult != S_OK || !fb2Xml) return 0;
		CComPtr<MSXML2::IXMLDOMDocument2> v2Dom;
		CheckError(v2Dom.CoCreateInstance(L"Msxml2.DOMDocument.6.0"));
		CComQIPtr<IPersistStreamInit> streamLoader(v2Dom);
		if (!streamLoader) { TracePluginDiagnostic(L"Import", pluginClsid, L"ImportV2StreamLoader", E_NOINTERFACE, 0); return 0; }
		CheckError(streamLoader->Load(fb2Xml));
		CheckError(v2Dom.QueryInterface(&obj));
		filename.Assign(suggestedFileName.Detach());
		m_last_plugin = wID + ID_IMPORT_BASE;

      MSXML2::IXMLDOMDocument2Ptr dom(obj);
      TracePluginDiagnostic(L"Import", pluginClsid, L"DOM result", dom ? S_OK : E_NOINTERFACE, dom ? 1 : 0);
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
			if (filename.length()>0)
			{
				m_doc->m_filename=(const TCHAR *)filename;
				U::SetCurrentDirectoryToFile((const wchar_t*)filename);
				if (m_doc->m_filename.GetLength()<4 || m_doc->m_filename.Right(4).CompareNoCase(_T(".fb2"))!=0)
				m_doc->m_filename+=_T(".fb2");
				m_doc->m_namevalid=true;
			}
			/*AttachDocument(doc);
			delete m_doc;
			m_doc=doc;*/
			m_doc->m_body.Init();
			m_doc->ResetSavePoint();
			TracePluginDiagnostic(L"Import", pluginClsid, L"completed", S_OK, 1);
		}// else
			//FB::Doc::m_active_doc = m_doc;
		//delete doc;
	  }
	}
    catch (_com_error& e) {
      TracePluginDiagnostic(L"Import", pluginClsid, L"exception", e.Error(), 0);
      U::ReportError(e);
    }
  }
  return 0;
}

LRESULT CMainFrame::OnToolsExport(WORD, WORD wID, HWND, BOOL&)
{
	wID -= ID_EXPORT_BASE;
	if(wID<m_export_plugins.GetSize())
	{
		const CLSID& pluginClsid = m_export_plugins[wID];
		TracePluginDiagnostic(L"Export", pluginClsid, L"begin", S_OK, 0);
		try
		{
			IUnknownPtr unk;
			HRESULT pluginHr = CreateBundledPluginInstance(pluginClsid, unk);
			TracePluginDiagnostic(L"Export", pluginClsid, L"CreateInstance", pluginHr, 0);
			CheckError(pluginHr);
			pluginHr = g_pluginManager.NegotiateApi(pluginClsid, unk);
			TracePluginDiagnostic(L"Export", pluginClsid, L"NegotiateApiV2", pluginHr, SUCCEEDED(pluginHr) ? 1 : 0);
			CheckError(pluginHr);

				CComQIPtr<IFBEExportPlugin2> exportV2(unk);
				if (!exportV2) { TracePluginDiagnostic(L"Export", pluginClsid, L"QueryInterfaceV2", E_NOINTERFACE, 0); return 0; }
				m_last_plugin = wID + ID_EXPORT_BASE;
				MSXML2::IXMLDOMDocument2Ptr dom(m_doc->CreateDOM(m_doc->m_encoding, false));
				CComPtr<IFBEPluginHost> host; CComPtr<IFBEDocumentSnapshot> snapshot;
				CheckError(FbePluginApiV2::CreateHost(m_hWnd, _Settings.GetInterfaceLanguageName(), &host));
				CheckError(FbePluginApiV2::CreateSnapshot(dom, m_doc->m_namevalid ? m_doc->m_filename.GetString() : L"", m_doc->m_encoding, &snapshot));
				_bstr_t filename = m_doc->m_namevalid ? static_cast<LPCWSTR>(m_doc->m_filename) : L"";
				HRESULT exportResult = exportV2->Export(host, filename, snapshot);
				TracePluginDiagnostic(L"Export", pluginClsid, L"ExportV2", exportResult, 0);
				CheckError(exportResult);
				TracePluginDiagnostic(L"Export", pluginClsid, L"completed", S_OK, 0);
				return 0;
		}
		catch(_com_error& e)
		{
			TracePluginDiagnostic(L"Export", pluginClsid, L"exception", e.Error(), 0);
			U::ReportError(e);
		}
	}
	return 0;
}

LRESULT CMainFrame::OnLastPlugin(WORD, WORD /* unused: wID */, HWND, BOOL&)
{
	if(m_last_plugin)
		::SendMessage(m_hWnd, WM_COMMAND, m_last_plugin, NULL);
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

static bool OpenDiagnosticLog()
{
	const CString currentLogPath(StartupTrace::CurrentLogPath());
	if(!currentLogPath.IsEmpty() && ::GetFileAttributes(currentLogPath) != INVALID_FILE_ATTRIBUTES)
	{
		return reinterpret_cast<INT_PTR>(::ShellExecute(NULL, L"open", currentLogPath,
			NULL, NULL, SW_SHOWNORMAL)) > 32;
	}

	const CString currentLogDirectory(StartupTrace::CurrentLogDirectory());
	if(!currentLogDirectory.IsEmpty() && ::GetFileAttributes(currentLogDirectory) != INVALID_FILE_ATTRIBUTES)
	{
		return reinterpret_cast<INT_PTR>(::ShellExecute(NULL, L"open", currentLogDirectory,
			NULL, NULL, SW_SHOWNORMAL)) > 32;
	}
	return false;
}

static bool OpenDiagnosticLogFolder()
{
	const CString currentLogDirectory(StartupTrace::CurrentLogDirectory());
	return !currentLogDirectory.IsEmpty() && ::GetFileAttributes(currentLogDirectory) != INVALID_FILE_ATTRIBUTES &&
		reinterpret_cast<INT_PTR>(::ShellExecute(NULL, L"open", currentLogDirectory, NULL, NULL, SW_SHOWNORMAL)) > 32;
}

static bool CopyDiagnosticLogPathToClipboard()
{
	const CString currentLogPath(StartupTrace::CurrentLogPath());
	if (currentLogPath.IsEmpty() || !::OpenClipboard(NULL)) return false;
	::EmptyClipboard();
	const SIZE_T bytes = (static_cast<SIZE_T>(currentLogPath.GetLength()) + 1) * sizeof(wchar_t);
	HGLOBAL data = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
	if (!data) { ::CloseClipboard(); return false; }
	void* target = ::GlobalLock(data);
	if (!target) { ::GlobalFree(data); ::CloseClipboard(); return false; }
	memcpy(target, static_cast<LPCWSTR>(currentLogPath), bytes);
	::GlobalUnlock(data);
	const bool copied = ::SetClipboardData(CF_UNICODETEXT, data) != NULL;
	if (!copied) ::GlobalFree(data);
	::CloseClipboard();
	return copied;
}

LRESULT CMainFrame::OnToolsOpenDiagnosticLog(WORD, WORD, HWND, BOOL&)
{
	if(!OpenDiagnosticLog())
	{
		::MessageBox(m_hWnd,
			GetDiagnosticTraceText(L"fbe.trace.open_failed", L"Не удалось открыть диагностический журнал."),
			GetDiagnosticTraceText(L"fbe.trace.caption", L"Диагностический журнал"), MB_OK | MB_ICONERROR);
	}
	return 0;
}

LRESULT CMainFrame::OnToolsOpenDiagnosticFolder(WORD, WORD, HWND, BOOL&)
{
	if (!OpenDiagnosticLogFolder())
		::MessageBox(m_hWnd, GetDiagnosticTraceText(L"fbe.trace.open_folder_failed", L"Could not open the diagnostic log folder."),
			GetDiagnosticTraceText(L"fbe.trace.caption", L"Diagnostic trace"), MB_OK | MB_ICONERROR);
	return 0;
}

LRESULT CMainFrame::OnToolsCopyDiagnosticLogPath(WORD, WORD, HWND, BOOL&)
{
	if (!CopyDiagnosticLogPathToClipboard())
		::MessageBox(m_hWnd, GetDiagnosticTraceText(L"fbe.trace.copy_path_failed", L"Could not copy the diagnostic log path."),
			GetDiagnosticTraceText(L"fbe.trace.caption", L"Diagnostic trace"), MB_OK | MB_ICONERROR);
	return 0;
}

LRESULT CMainFrame::OnToolsClearDiagnosticLogs(WORD, WORD, HWND, BOOL&)
{
	const CString caption(GetDiagnosticTraceText(L"fbe.trace.caption", L"Diagnostic trace"));
	if (::MessageBox(m_hWnd, GetDiagnosticTraceText(L"fbe.trace.clear_confirmation", L"Clear old diagnostic logs? The current log will be preserved."), caption, MB_YESNO | MB_ICONQUESTION) != IDYES)
		return 0;
	const StartupTrace::DiagnosticLogCleanupResult cleanup = StartupTrace::ClearOldLogSessions();
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
	if (!StartupTrace::CreateDiagnosticPackage(packagePath, error))
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
	const bool enabled = IsDiagnosticTraceEnabledForNextLaunch();
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

	if(!SetDiagnosticTraceEnabledForNextLaunch(true))
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
	for(int i = 0; i < m_script_menu.Count(); ++i)
	{
		if(m_script_menu.Item(i).commandId == -1) continue;

		if(!m_script_menu.Item(i).isFolder && m_script_menu.Item(i).commandId == wID)
		{
			m_doc->RunScript(m_script_menu.Item(i).path);
			m_last_script = &m_script_menu.Item(i);
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

bool  CMainFrame::SourceToHTML()
{
	m_editor_selection_state.BodySource().sourceToBodyTransferred = false;
	LRESULT changed = m_source.SendMessage(SCI_GETMODIFY);
	SourceDocumentText sourceDocument;
	if(SourceDocumentTransfer::ReadSourceText(m_source, sourceDocument) != SourceTransitionResult::Success)
		return false;
	const int textlen = static_cast<int>(sourceDocument.utf8.size()) - 1;

	int begin_char = 0;
	int end_char = 0;
	int bodies_count = 0;
	int selected_body_index = -1;

	BSTR ustr = ::SysAllocStringLen(sourceDocument.text, sourceDocument.text.GetLength());
	if(!ustr) return false;

	//	??????? ?????????? ???????
	int selectedPosBegin = sourceDocument.selectionStart;
	int selectedPosEnd = sourceDocument.selectionEnd;
	bool one_pos = sourceDocument.caret;
	if (StartupTrace::Enabled())
	{
		CString trace;
		trace.Format(L"SourceToHTML: source bytes=[%d,%d], text bytes=%d, caret=%d",
			sourceDocument.selectionStartByte, sourceDocument.selectionEndByte, textlen, one_pos ? 1 : 0);
		WriteSelectionTrace(L"E210", trace);
	}
	if(one_pos)
	{
		selectedPosEnd = selectedPosBegin = sourceDocument.selectionStart;
	}
	else
	{
		selectedPosBegin = sourceDocument.selectionStart;
		selectedPosEnd = sourceDocument.selectionEnd;
	}
	CString sourceText(ustr);
	selected_body_index = SourceDocumentTransfer::FindXmlBodyIndexAtPosition(sourceText, selectedPosBegin);
	if (!one_pos)
	{
		selectedPosBegin = SourceDocumentTransfer::SkipXmlMarkupForward(sourceText, selectedPosBegin);
		selectedPosEnd = SourceDocumentTransfer::SkipXmlMarkupBackward(sourceText, selectedPosEnd);
		if (selectedPosEnd < selectedPosBegin)
			selectedPosEnd = selectedPosBegin;
	}
	CString selectedSourceText;
	bool selectionCrossesParagraph = false;
	if (selectedPosEnd > selectedPosBegin)
	{
		const CString selectedSourceXml = sourceText.Mid(selectedPosBegin,
			selectedPosEnd - selectedPosBegin);
		selectedSourceText = SourceDocumentTransfer::ExtractVisibleXmlText(selectedSourceXml);
		selectionCrossesParagraph = selectedSourceXml.Find(L"</p") >= 0 ||
			selectedSourceXml.Find(L"<p") >= 0;
	}
	if (StartupTrace::Enabled())
	{
		CString trace;
		trace.Format(L"SourceToHTML: XML chars=[%d,%d], visible chars=%d, crosses-p=%d, text=\"%s\"",
			selectedPosBegin, selectedPosEnd, selectedSourceText.GetLength(),
			selectionCrossesParagraph ? 1 : 0,
			(const wchar_t*)SelectionTraceSummary(selectedSourceText));
		WriteSelectionTrace(L"E220", trace);
	}

	//	?????????? ? XML
	U::DomPath path_begin;
	U::DomPath path_end;

	bool selection_path_available = path_begin.CreatePathFromText(ustr, selectedPosBegin, &begin_char);

	if(one_pos)
	{
		path_end = path_begin;
		end_char = begin_char;
	}
	else
	{
		selection_path_available = path_end.CreatePathFromText(ustr, selectedPosEnd, &end_char) && selection_path_available;
	}
	if (StartupTrace::Enabled())
	{
		CString trace;
		trace.Format(L"SourceToHTML: DOM path available=%d, chars=[%d,%d]",
			selection_path_available ? 1 : 0, begin_char, end_char);
		WriteSelectionTrace(L"E230", trace);
	}

	const SourceDocumentApplyResult applyResult =
		SourceDocumentTransfer::ApplySourceDocument(*m_doc, sourceDocument,
			changed != 0, m_saved_xml, _Settings.GetInterfaceLanguageName());
	SysFreeString(ustr);
	if(applyResult.result != SourceTransitionResult::Success)
	{
		if(applyResult.result == SourceTransitionResult::InvalidSource &&
			!applyResult.errorMessage.IsEmpty())
		{
			::SendMessage(m_doc->m_frame, AU::WM_SETSTATUSTEXT, 0,
				(LPARAM)(const TCHAR*)applyResult.errorMessage);
			SourceGoTo(applyResult.errorLine, applyResult.errorColumn);
		}
		return false;
	}
	if(applyResult.documentChanged)
		ClearSelection();


	MSXML2::IXMLDOMNodeListPtr ChildNodes = m_saved_xml->documentElement->childNodes;
	MSXML2::IXMLDOMNodePtr body;

	MSXML2::IXMLDOMElementPtr selectedElementBegin;
	MSXML2::IXMLDOMElementPtr selectedElementEnd;
	U::DomPath fallback_begin_path;
	U::DomPath fallback_scope_path;
	bool fallback_path_available = false;
	if(selection_path_available)
	{
		selectedElementBegin = path_begin.GetNodeFromXMLDOM(m_saved_xml);
		for(int i = 0; (bool)selectedElementBegin && i < ChildNodes->length; i++)
		{
			bstr_t name = ChildNodes->item[i]->nodeName;
			if(U::scmp(ChildNodes->item[i]->nodeName, L"body") == 0)
			{
				if(U::IsParentElement(selectedElementBegin, ChildNodes->item[i]))
				{
					body = ChildNodes->item[i];
					selected_body_index = bodies_count;
					break;
				}
				else
				{
					++bodies_count;
				}
			}
		}

		selection_path_available = (bool)body;
		if(selection_path_available)
		{
			MSXML2::IXMLDOMNodePtr scopeNode = selectedElementBegin;
			for(MSXML2::IXMLDOMNodePtr parent = scopeNode->parentNode;
				(bool)parent && parent != body; parent = parent->parentNode)
			{
				if(U::scmp(parent->nodeName, L"section") == 0)
				{
					scopeNode = parent;
					break;
				}
			}
			fallback_path_available =
				fallback_begin_path.CreatePathFromXMLDOM(body, selectedElementBegin) &&
				fallback_scope_path.CreatePathFromXMLDOM(body, scopeNode);
		}
		if(selection_path_available && !selectionCrossesParagraph)
		{
			selection_path_available = path_begin.CreatePathFromXMLDOM(body, selectedElementBegin);
			if(one_pos)
			{
				path_end = path_begin;
			}
			else
			{
				selectedElementEnd = path_end.GetNodeFromXMLDOM(m_saved_xml);
				selection_path_available = (bool)selectedElementEnd &&
					path_end.CreatePathFromXMLDOM(body, selectedElementEnd) && selection_path_available;
			}
		}
	}


	// ???? ???????? ??? ???????, ?? ?????????? ??? ? HTML
	if(selection_path_available && !selectionCrossesParagraph)
	{
		// Выделение из Source переносится только в отображаемый текстовый body.
		MSHTML::IHTMLElementPtr selectedHTMLElementBegin;
		MSHTML::IHTMLElementPtr selectedHTMLElementEnd;
		MSHTML::IHTMLDOMNodePtr root = m_doc->m_body.Document()->body;
		if(root)
			root = root->firstChild; // <DIV id = fbw_desc>
		if(root)
			root = root->nextSibling; // <DIV id = fbw_body>
		if(root)
			root = root->firstChild; // <DIV class = ...>

		int htmlBodyIndex = selected_body_index;
		while(root)
		{
			if(U::scmp(MSHTML::IHTMLElementPtr(root)->className, L"body") == 0)
			{
				if(htmlBodyIndex > 0)
				{
					--htmlBodyIndex;
				}
				else
				{
					selectedHTMLElementBegin = path_begin.GetNodeFromHTMLDOM(root);
					selectedHTMLElementEnd = one_pos
						? selectedHTMLElementBegin
						: path_end.GetNodeFromHTMLDOM(root);
					break;
				}
			}
			root = root->nextSibling;
		}

		if((bool)selectedHTMLElementBegin && (bool)selectedHTMLElementEnd)
		{
			m_doc->m_body.GoTo(selectedHTMLElementBegin);
			m_editor_selection_state.BodyRange() = m_doc->m_body.SetSelection(
				selectedHTMLElementBegin, selectedHTMLElementEnd, begin_char, end_char);
			m_editor_selection_state.BodySource().sourceToBodyTransferred = (bool)m_editor_selection_state.BodyRange();
		}
	}

	if(!m_editor_selection_state.BodySource().sourceToBodyTransferred && !selectedSourceText.IsEmpty())
	{
		MSHTML::IHTMLElementPtr htmlScope;
		MSHTML::IHTMLElementPtr expectedStartElement;
		MSHTML::IHTMLDOMNodePtr root = m_doc->m_body.Document()->body;
		if(root) root = root->firstChild; // <DIV id = fbw_desc>
		if(root) root = root->nextSibling; // <DIV id = fbw_body>
		if(root) root = root->firstChild;
		int htmlBodyIndex = selected_body_index >= 0 ? selected_body_index : 0;
		while(root)
		{
			MSHTML::IHTMLElementPtr element(root);
			if((bool)element && U::scmp(element->className, L"body") == 0)
			{
				if(htmlBodyIndex == 0)
				{
					// The corresponding HTML body is always the base fallback
					// scope.  A DomPath, when available, may only narrow it.
					htmlScope = element;
					if(fallback_path_available)
					{
						MSHTML::IHTMLElementPtr refinedScope =
							fallback_scope_path.GetNodeFromHTMLDOM(root);
						if((bool)refinedScope) htmlScope = refinedScope;
						expectedStartElement = fallback_begin_path.GetNodeFromHTMLDOM(root);
					}
					break;
				}
				if(htmlBodyIndex > 0) --htmlBodyIndex;
			}
			root = root->nextSibling;
		}
		MSHTML::IHTMLBodyElementPtr htmlBody(m_doc->m_body.Document()->body);
		MSHTML::IHTMLTxtRangePtr range = SourceDocumentTransfer::FindBodyTextRange(htmlBody, htmlScope,
			expectedStartElement, selectedSourceText);
		if((bool)range)
		{
			m_editor_selection_state.BodyRange() = range;
			m_editor_selection_state.BodySource().sourceToBodyTransferred = true;
		}
	}
	if (StartupTrace::Enabled())
	{
		CString trace;
		trace.Format(L"SourceToHTML: transfer result=%d, DOM-path=%d, crosses-p=%d",
			m_editor_selection_state.BodySource().sourceToBodyTransferred ? 1 : 0,
			selection_path_available ? 1 : 0, selectionCrossesParagraph ? 1 : 0);
		WriteSelectionTrace(L"E240", trace);
	}

	m_doc->MarkDocCP(); // document is in sync with source
	if(_Settings.ViewDocumentTree())
	{
		m_document_tree.GetDocumentStructure(m_doc->m_body.Document());
	}
	return true;
	//m_document_tree.HighlightItemAtPos(m_doc->m_body.SelectionContainer());
}

bool CMainFrame::ShowSource(bool saveSelection)
{
	ShowSourcePhaseProfiler phaseProfiler;
	m_editor_selection_state.BodySource().bodyToSourceTransferred = false;
	U::DomPath selection_begin_path;
	U::DomPath selection_end_path;

	int selection_begin_char = 0;
	int selection_end_char = 0;
	bstr_t path;
	bool one_element = false;
	bool selection_path_available = false;

	int bodies_count = 0;
	int selected_body_index = -1;
	// ????? HTML
	// ?????????? ???? ?? ??????????? ????????
	if(saveSelection)
	{
		MSHTML::IHTMLElementPtr selectedBeginElement;
		MSHTML::IHTMLElementPtr selectedEndElement;

		m_doc->m_body.GetSelectionInfo((MSHTML::IHTMLElementPtr*)(&selectedBeginElement), (MSHTML::IHTMLElementPtr*)(&selectedEndElement), &selection_begin_char, &selection_end_char, 0);
		phaseProfiler.Mark("Body selection extraction");
		if(selectedBeginElement == selectedEndElement && (bool)m_editor_selection_state.BodyRange())
		{
			const CString selectedText((const wchar_t*)m_editor_selection_state.BodyRange()->text);
			if(!selectedText.IsEmpty())
				selection_end_char = selection_begin_char + selectedText.GetLength();
		}
		if (StartupTrace::Enabled())
		{
			CString selectedText;
			if ((bool)m_editor_selection_state.BodyRange())
				selectedText = (const wchar_t*)m_editor_selection_state.BodyRange()->text;
			CString trace;
			trace.Format(L"ShowSource: Body chars=[%d,%d], same-element=%d, text chars=%d, text=\"%s\"",
				selection_begin_char, selection_end_char,
				selectedBeginElement == selectedEndElement ? 1 : 0,
				selectedText.GetLength(),
				(const wchar_t*)SelectionTraceSummary(selectedText));
			WriteSelectionTrace(L"E250", trace);
		}


		// <body>
		MSHTML::IHTMLDOMNodePtr root = m_doc->m_body.Document()->body;
		root = root->firstChild; // <DIV id = fbw_desc>
		root = root->nextSibling; // <DIV id = fbw_body>
		root = root->firstChild;// <DIV clss = ...>
		if (root && (bool)selectedBeginElement && (bool)selectedEndElement) do
		{
			if(U::scmp(MSHTML::IHTMLElementPtr(root)->className, L"body") == 0)
			{
				if(!U::IsParentElement(selectedEndElement, root))
				{
					++bodies_count;
				}
				else
				{
					selected_body_index = bodies_count;
					selection_path_available = selection_begin_path.CreatePathFromHTMLDOM(root, selectedBeginElement);
					one_element = selectedBeginElement == selectedEndElement;
					if(one_element)
					{
						selection_end_path = selection_begin_path;
					}
				else
					{
						selection_path_available = selection_end_path.CreatePathFromHTMLDOM(root, selectedEndElement) && selection_path_available;
					}
					if(selection_path_available)
						path = selection_begin_path;

					break;
				}
			}
		}while(root = root->nextSibling);
	}
	phaseProfiler.Mark("DomPath construction");

	// Preserve the XML declaration encoding when switching to Source view.
	CString sourceEncoding = _Settings.KeepEncoding()
		? m_doc->m_encoding
		: _Settings.GetDefaultEncoding();

	if (sourceEncoding.IsEmpty())
		sourceEncoding = L"utf-8";

	CString srcText;
	if(SourceDocumentTransfer::PrepareSerializedSource(*m_doc, m_saved_xml,
		sourceEncoding, srcText) != SourceTransitionResult::Success)
		return false;
	phaseProfiler.Mark("serialized source preparation");

/*	std::ofstream save;
	CString s = m_saved_xml->xml;
	CT2A str (s, 1251);
	save.open(L"1.xml", std::ios_base::out | std::ios_base::trunc);
	if (save.is_open())
		save << str << '\n';
	save.close();

	MSHTML::IHTMLElementPtr body = (MSHTML::IHTMLElementPtr)m_doc->m_body.Document()->body;
	s.SetString(body->innerHTML);
	CT2A str2 (s, 1251);
	save.open(L"1.htm", std::ios_base::out | std::ios_base::trunc);
	if (save.is_open())
		save << str2 << '\n';
	save.close(); */

	MSXML2::IXMLDOMNodePtr xml_selected_begin;
	MSXML2::IXMLDOMNodePtr xml_selected_end;
	if(selection_path_available)
	{
		MSXML2::IXMLDOMElementPtr xml_root = m_saved_xml->documentElement;
		if (!(bool)xml_root)
			return false;

		MSXML2::IXMLDOMNodePtr xml_body = xml_root->firstChild;
		while (xml_body)
	{
		if(U::scmp(xml_body->nodeName, L"body") == 0)
		{
			if(bodies_count)
			{
				--bodies_count;
				xml_body = xml_body->nextSibling;
				continue;
			}
			xml_selected_begin = selection_begin_path.GetNodeFromXMLDOM(xml_body);
			if(!(bool)xml_selected_begin ||
				!selection_begin_path.CreatePathFromXMLDOM(m_saved_xml, xml_selected_begin))
			{
				selection_path_available = false;
				break;
			}
			path = selection_begin_path;

			if(one_element)
			{
				selection_end_path = selection_begin_path;
				xml_selected_end = xml_selected_begin;
			}
			else
			{
				xml_selected_end = selection_end_path.GetNodeFromXMLDOM(xml_body);
				if(!(bool)xml_selected_end ||
					!selection_end_path.CreatePathFromXMLDOM(m_saved_xml, xml_selected_end))
					selection_path_available = false;
			}
			break;
		}
		xml_body = xml_body->nextSibling;
	}
	}
	phaseProfiler.Mark("selection DOM lookup");

	phaseProfiler.Mark("source serialization and XML declaration normalization");
	_bstr_t src((const wchar_t*)srcText);

	int savedPosBegin = 0;
	int savedPosEnd = 0;
	bool selection_mapped_to_source = false;
	if(saveSelection)
	{
		int beginPosition = -1;
		int endPosition = -1;
		bool hasBodySelectionText = false;
		int bodyStart = -1;
		int bodyEnd = -1;
		int caretScopeStart = -1;
		int caretScopeEnd = -1;
		if((bool)m_editor_selection_state.BodyRange())
		{
			const CString selectedText((const wchar_t*)m_editor_selection_state.BodyRange()->text);
			hasBodySelectionText = !selectedText.IsEmpty();
			if(hasBodySelectionText && selection_path_available &&
				(bool)xml_selected_begin && (bool)xml_selected_end)
			{
				const int expectedBegin = selection_begin_path.GetNodeFromText(src,
					selection_begin_char);
				const int expectedEnd = selection_end_path.GetNodeFromText(src,
					selection_end_char);
				SourceDocumentTransfer::TextRange bodyRange;
				if(expectedBegin >= 0 && expectedEnd >= expectedBegin &&
					SourceDocumentTransfer::FindEnclosingXmlElementRange(srcText, expectedBegin, L"body", bodyRange))
				{
					bodyStart = bodyRange.start; bodyEnd = bodyRange.end;
					SourceDocumentTransfer::TextRange visibleRange;
					if(SourceDocumentTransfer::FindVisibleXmlTextRange(srcText, selectedText, bodyStart, bodyEnd, expectedBegin, visibleRange)) { beginPosition = visibleRange.start; endPosition = visibleRange.end; }
				}
			}
			// DomPath is positional refinement, not a prerequisite for a native
			// Body selection.  The selected FB2 body remains a safe base scope;
			// FindVisibleXmlTextRange refuses ambiguous repeated text within it.
			SourceDocumentTransfer::TextRange fallbackBodyRange;
			if(hasBodySelectionText && (beginPosition < 0 || endPosition < 0) &&
				SourceDocumentTransfer::FindXmlBodyRangeByIndex(srcText, selected_body_index, fallbackBodyRange))
			{
				bodyStart = fallbackBodyRange.start; bodyEnd = fallbackBodyRange.end;
				SourceDocumentTransfer::TextRange visibleRange;
				if(SourceDocumentTransfer::FindVisibleXmlTextRange(srcText, selectedText, bodyStart, bodyEnd, -1, visibleRange)) { beginPosition = visibleRange.start; endPosition = visibleRange.end; }
			}
		}

		// Для реального выделения DOM-path задаёт позиционный контекст, а текст
		// выше уточняет точные границы. Если этот контекст недоступен, перенос
		// намеренно не производится: глобальный поиск одинакового текста опасен.
		if(!hasBodySelectionText && selection_path_available && (bool)xml_selected_begin &&
			(beginPosition < 0 || endPosition < 0))
		{
			beginPosition = selection_begin_path.GetNodeFromText(src, selection_begin_char);
			endPosition = selection_end_path.GetNodeFromText(src, selection_end_char);
			int bodyAnchor = beginPosition;
			if(bodyAnchor < 0)
				bodyAnchor = selection_begin_path.GetNodeFromText(src, 0);
			if(bodyAnchor >= 0)
			{
				SourceDocumentTransfer::TextRange bodyRange;
				if(SourceDocumentTransfer::FindEnclosingXmlElementRange(srcText, bodyAnchor, L"body", bodyRange)) { bodyStart = bodyRange.start; bodyEnd = bodyRange.end; }
				SourceDocumentTransfer::TextRange sectionRange;
				if(!SourceDocumentTransfer::FindEnclosingXmlElementRange(srcText, bodyAnchor, L"section", sectionRange))
				{
					caretScopeStart = bodyStart;
					caretScopeEnd = bodyEnd;
				}
				else { caretScopeStart = sectionRange.start; caretScopeEnd = sectionRange.end; }
			}
		}

		// Старый DomPath не всегда умеет пройти от XML-документа к узлу
		// отображаемого текста. Сам узел уже получен из DOM, поэтому при
		// таком отказе сопоставляем позицию по его XML-представлению.
		if(!hasBodySelectionText && selection_path_available && (bool)xml_selected_begin &&
			beginPosition < 0 && caretScopeStart >= 0)
			beginPosition = SourceDocumentTransfer::FindXmlNodeTextPosition(srcText, xml_selected_begin,
				selection_begin_char, caretScopeStart, caretScopeEnd);
		if(!hasBodySelectionText && selection_path_available && (bool)xml_selected_end &&
			endPosition < 0 && caretScopeStart >= 0)
			endPosition = SourceDocumentTransfer::FindXmlNodeTextPosition(srcText, xml_selected_end,
				selection_end_char, caretScopeStart, caretScopeEnd);

		if(beginPosition >= 0 && endPosition >= 0)
		{
			savedPosBegin = ::WideCharToMultiByte(CP_UTF8, 0, src,
				beginPosition, NULL, 0, NULL, NULL);
			savedPosEnd = ::WideCharToMultiByte(CP_UTF8, 0, src,
				endPosition, NULL, 0, NULL, NULL);
			selection_mapped_to_source = true;
		}
		if (StartupTrace::Enabled())
		{
			CString trace;
			trace.Format(L"ShowSource: mapping-by-text=%d, DOM-path=%d, XML chars=[%d,%d], source bytes=[%d,%d], mapped=%d",
				hasBodySelectionText ? 1 : 0, selection_path_available ? 1 : 0,
				beginPosition, endPosition, savedPosBegin, savedPosEnd,
				selection_mapped_to_source ? 1 : 0);
			WriteSelectionTrace(L"E260", trace);
		}
	}
	phaseProfiler.Mark("selection lookup and mapping");

	//	???????? ????? ? ?????????
	if(m_doc->DocRelChanged())
	{
		const DWORD nch=::WideCharToMultiByte(CP_UTF8,0,src,src.length(), NULL,0,NULL,NULL);
		phaseProfiler.Mark("UTF-8 size calculation");
		m_source.SendMessage(SCI_CLEARALL);
		phaseProfiler.Mark("SCI_CLEARALL");
		// Source is filled by one bulk append, so reserve its line-index table once.
		m_source.SendMessage(SCI_ALLOCATELINES, EstimateSourceLineCount(srcText));
		phaseProfiler.Mark("line count estimation and SCI_ALLOCATELINES");
		std::vector<char> buffer(nch);
		if (!buffer.empty())
		{
			::WideCharToMultiByte(CP_UTF8,0,src,src.length(),
									buffer.data(),nch,NULL,NULL);
			phaseProfiler.Mark("UTF-16 to UTF-8 conversion");
			m_source.SendMessage(SCI_APPENDTEXT,nch,(LPARAM)buffer.data());
			phaseProfiler.Mark("SCI_APPENDTEXT");
		}
	}

	//	????????? ?? ???????
	m_source.SendMessage(SCI_SETSELECTIONSTART,savedPosBegin);
	m_source.SendMessage(SCI_SETSELECTIONEND,savedPosEnd);
	phaseProfiler.Mark("selection restoration");
	m_source.SendMessage(SCI_SCROLLCARET);
	phaseProfiler.Mark("scroll restoration");
	m_editor_selection_state.BodySource().bodyToSourceTransferred = selection_mapped_to_source;
	m_editor_selection_state.BodySource().sourceStart = savedPosBegin;
	m_editor_selection_state.BodySource().sourceEnd = savedPosEnd;
	if (StartupTrace::Enabled())
	{
		const int sourceLine = m_source.SendMessage(SCI_LINEFROMPOSITION, savedPosBegin);
		CString trace;
		trace.Format(L"ShowSource: applied bytes=[%d,%d], line=%d, first-visible=%d",
			savedPosBegin, savedPosEnd, sourceLine,
			(int)m_source.SendMessage(SCI_GETFIRSTVISIBLELINE));
		WriteSelectionTrace(L"E270", trace);
	}

	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	phaseProfiler.Mark("SCI_EMPTYUNDOBUFFER");
	m_doc->MarkDocCP();
	return true;
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

void CMainFrame::SetDescriptionMode(bool enabled)
{
	CComDispatchDriver body(m_doc->m_body.Script());
	CComVariant argument;
	argument = enabled;
	CheckError(body.Invoke1(L"apiShowDesc", &argument));
}

void  CMainFrame::ShowView(EditorView vt)
{
	EditorView prev = m_editor_view_state.Current();
	const EditorViewTransitionPlan transition = MakeEditorViewTransitionPlan(prev, vt);
	if (StartupTrace::Enabled())
	{
		const wchar_t* const viewNames[] = { L"Body", L"Description", L"Source" };
		CString trace;
		trace.Format(L"ShowView: requested %s -> %s", viewNames[static_cast<int>(prev)], viewNames[static_cast<int>(vt)]);
		WriteSelectionTrace(L"E280", trace);
	}
	if(transition.saveCurrentSelection)
		SaveSelection(m_editor_view_state.Current());

  // added by SeNS
  if (vt != BODY)
	if (m_Speller)
		m_Speller->EndDocumentCheck();

  if(prev != vt)
  {
	  m_doc->m_body.CloseFindDialog(m_doc->m_body.m_find_dlg);
	  m_doc->m_body.CloseFindDialog(m_sci_find_dlg);
	  m_doc->m_body.CloseFindDialog(m_doc->m_body.m_replace_dlg);
	  m_doc->m_body.CloseFindDialog(m_sci_replace_dlg);
  }

	if(!m_editor_view_state.CtrlTabActive() && prev != vt)
	{
		m_editor_view_state.SetLastCtrlTabView(m_editor_view_state.Current());
	}


	if (transition.commitSourceToDocument) {
	  // added by SeNS: special trick for incorrect XML
	  if (m_bad_xml)
	  {
			int col,line;
			bool fv;
			fv=m_doc->SetXMLAndValidate(m_source,true,line,col);// ?? ?????? Source
			if (!fv)
			{
				U::MessageBox(MB_OK|MB_ICONERROR, IDR_MAINFRAME, IDS_BAD_XML_MSG);
				SourceGoTo(line, col);
				return;
			}
			else
			{
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
				m_bad_xml=false;
			}
	  }

    /*if (!SourceToHTML())
      return;*/
		if (!SourceToHTML()) return;
		m_source.SendMessage(SCI_SETSAVEPOINT);
  }

  if ((vt == BODY || vt == DESC) && (!m_doc || !m_doc->m_body.HasDoc()))
  {
    StartupTrace::Warning(L"selection", L"E281", L"view switch ignored: HTML document is unavailable");
    return;
  }
	if (transition.prepareDocumentSource)
  {
	  if(!this->ShowSource(prev == BODY))
	  {
		  return;
	  }
	  // turn off doctree
	  /*m_save_sp_mode=m_document_tree.IsWindowVisible()!=0;
	  UISetCheck(ID_VIEW_TREE,0);*/
  }

  if (prev!=vt && vt!=SOURCE) {
    UIEnable(ID_VIEW_TREE,1);
	/*m_save_sp_mode=true;// Modification by Pilgrim - ????? ?????? ?? ??(!)??? ?????? DESC ??????? ID_VIEW_TREE ? ??????? ?? BODY ?? ???????????????. ??, ???? ????? ??????? ????? ??????? ?? SOURCE, ?? ???????? ?? DESC ? BODY ?? ?????? ID_VIEW_TREE. ???? ???????????, ? ????? ??????? m_save_sp_mode=true;
    UISetCheck(ID_VIEW_TREE, m_save_sp_mode);*/
    m_splitter.SetSinglePaneMode(_Settings.ViewDocumentTree() ? SPLIT_PANE_NONE : SPLIT_PANE_RIGHT);
  }

  UISetCheck(ID_VIEW_BODY, 0);
  UISetCheck(ID_VIEW_DESC, 0);
  UISetCheck(ID_VIEW_SOURCE, 0);
	if(transition.leaveDescriptionMode) SetDescriptionMode(false);
	if(transition.enterDescriptionMode) SetDescriptionMode(true);

  switch (vt) {
  case BODY:
	  {
	        UISetCheck(ID_VIEW_BODY, 1);
			m_view.ActivateWnd(m_doc->m_body);
			m_sel_changed=true;
			m_status.SetPaneText(ID_PANE_INS, CurrentOverwriteMode() ? strOVR : strINS);

			if (m_Speller)
				m_Speller->SetDocumentLanguage();
	  }
    break;
  case DESC:
    UISetCheck(ID_VIEW_DESC, 1);
    m_view.ActivateWnd(m_doc->m_body);
	m_contextAttributeBars.ClearLinkState();
	m_contextAttributeBars.ClearTableState();
	m_contextAttributeBars.SetLinkAvailability(LinkAttributeAvailability{ false, false, false, false });
	m_contextAttributeBars.SetTableAvailability(TableAttributeAvailability{ false, false, false, false, false, false, false, false, false });

	SetStatusContext(_T(""));
    break;
  case SOURCE:
	m_source.UpdateLineNumberMargin(false);

    UISetCheck(ID_VIEW_SOURCE, 1);
    m_view.HideActiveWnd();
    m_splitter.SetSinglePaneMode(SPLIT_PANE_RIGHT);
    m_view.ActivateWnd(m_source);
	if(m_editor_selection_state.BodySource().bodyToSourceTransferred)
	{
		m_source.SendMessage(SCI_SETSELECTIONSTART, m_editor_selection_state.BodySource().sourceStart);
		m_source.SendMessage(SCI_SETSELECTIONEND, m_editor_selection_state.BodySource().sourceEnd);
		m_source.SendMessage(SCI_SCROLLCARET);
	}
	{
		if(prev == BODY)
		{
			CComDispatchDriver	body(m_doc->m_body.Script());
			// Эта вспомогательная функция не должна отменять переход в Source.
			// На части систем MSHTML возвращает E_INVALIDARG, хотя сохранение
			// прокрутки не влияет на содержимое документа.
			body.Invoke0(L"SaveBodyScroll");
		}
	}
	SetStatusContext(L"");
	m_status.SetPaneText(ID_PANE_INS, CurrentOverwriteMode() ? strOVR : strINS);

	RefreshLocalizedToolbarButtonTexts(m_CmdToolbar);
	RefreshLocalizedToolbarButtonTexts(m_ScriptsToolbar);
    break;
  }
	m_editor_view_state.CommitTransition(vt);
	UpdateStatusBar();
	if(transition.restoreTargetSelection)
		RestoreSelection();
  m_view.SetFocus();
	if(vt == BODY && prev == SOURCE && m_editor_selection_state.BodySource().sourceToBodyTransferred &&
		(bool)m_editor_selection_state.BodyRange())
	{
		// Activating the MSHTML host can clear its visual highlight.  Apply the
		// already mapped range once, after the final focus assignment.  MSHTML
		// can stop extending a new drag-selection when the same IHTMLTxtRange is
		// selected both before and after the host gains focus.
		m_editor_selection_state.BodyRange()->select();
	}
	if(vt == SOURCE && m_editor_selection_state.BodySource().bodyToSourceTransferred)
	{
		// Source получает фокус и окончательный размер только в конце смены
		// режима. Повторная установка здесь делает прокрутку устойчивой.
		m_source.SendMessage(SCI_SETSEL, m_editor_selection_state.BodySource().sourceStart,
			m_editor_selection_state.BodySource().sourceEnd);
		const int sourceLine = m_source.SendMessage(SCI_LINEFROMPOSITION,
			m_editor_selection_state.BodySource().sourceStart);
		m_source.SendMessage(SCI_ENSUREVISIBLEENFORCEPOLICY, sourceLine);
		m_source.SendMessage(SCI_GOTOPOS, m_editor_selection_state.BodySource().sourceStart);
		m_source.SendMessage(SCI_SETSEL, m_editor_selection_state.BodySource().sourceStart,
			m_editor_selection_state.BodySource().sourceEnd);
		m_source.SendMessage(SCI_SCROLLCARET);
		// После отображения панели Scintilla может сбросить положение каретки.
		// Повторяем диапазон в очереди сообщений уже после завершения layout.
		::PostMessage(m_source, SCI_ENSUREVISIBLEENFORCEPOLICY, sourceLine, 0);
		::PostMessage(m_source, SCI_GOTOPOS, m_editor_selection_state.BodySource().sourceStart, 0);
		::PostMessage(m_source, SCI_SETSEL, m_editor_selection_state.BodySource().sourceStart,
			m_editor_selection_state.BodySource().sourceEnd);
		::PostMessage(m_source, SCI_SCROLLCARET, 0, 0);
		if (StartupTrace::Enabled())
		{
			CString trace;
			trace.Format(L"ShowView: Source final bytes=[%d,%d], line=%d, first-visible=%d",
				m_editor_selection_state.BodySource().sourceStart, m_editor_selection_state.BodySource().sourceEnd, sourceLine,
				(int)m_source.SendMessage(SCI_GETFIRSTVISIBLELINE));
			WriteSelectionTrace(L"E290", trace);
		}
	}
	else if (StartupTrace::Enabled())
	{
		CString trace;
		trace.Format(L"ShowView: completed current=%d, body-transfer=%d, source-transfer=%d",
			m_editor_view_state.Current(), m_editor_selection_state.BodySource().bodyToSourceTransferred ? 1 : 0,
			m_editor_selection_state.BodySource().sourceToBodyTransferred ? 1 : 0);
		WriteSelectionTrace(L"E299", trace);
	}
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
    SetValidationStatus(VALIDATION_VALID);
    return 0;
  }
  if (!fv) {
    SetValidationStatus(VALIDATION_INVALID);
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

void CMainFrame::RestoreSelection()
{
	if(m_editor_view_state.Current() == BODY && (bool)m_editor_selection_state.BodyRange())
	{
		m_editor_selection_state.BodyRange()->select();
	}
	if(m_editor_view_state.Current() == DESC && (bool)m_editor_selection_state.DescriptionRange())
	{
		m_editor_selection_state.DescriptionRange()->select();
	}
}


void CMainFrame::SaveSelection(EditorView vt)
{
	if ((vt == BODY || vt == DESC) && (!m_doc || !m_doc->m_body.HasDoc()))
	{
		StartupTrace::Warning(L"selection", L"E301", L"SaveSelection ignored: HTML document is unavailable");
		return;
	}
	if(vt == BODY)
	{
		m_editor_selection_state.BodyRange() = m_doc->m_body.Document()->selection->createRange();
		if (StartupTrace::Enabled() && (bool)m_editor_selection_state.BodyRange())
		{
			const CString selectedText((const wchar_t*)m_editor_selection_state.BodyRange()->text);
			CString trace;
			trace.Format(L"SaveSelection: Body; selection-chars=%d", selectedText.GetLength());
			WriteSelectionTrace(L"E300", trace);
		}
	}
	if(vt == DESC)
	{
		m_editor_selection_state.DescriptionRange() = m_doc->m_body.Document()->selection->createRange();
	}
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

	PendingDocument pending(*this, m_doc);
	FB::Doc* doc = &pending.Document();

	EnableWindow(FALSE);
	m_status.SetPaneText(ID_DEFAULT_PANE, FbeLoadRuntimeString(IDS_STATUS_LOADING));
	bool fLoaded = DocumentLoader::Load(*doc, m_view, DocumentOpenSource::Normal(m_doc->m_filename));
	EnableWindow(TRUE);
	if (!fLoaded)
	{
		pending.Rollback();
		return false;
	}

	AttachDocument(doc);
	delete m_doc;
	m_doc=pending.Commit();
	m_document_session.ReloadedNormal(m_doc->m_filename, m_doc->GetDocumentFileType());
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
	// InitPlugins may be requested more than once.  Return the physical scripts
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
	m_script_menu.Clear();
	m_last_script = NULL;
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
			// relativePath is the persistent script identity.  An absolute path
			// breaks portable hotkeys as soon as the package is moved.
			CHotkey ScriptsHotkey(script.relativePath,
				script.name,
				NULL,
				static_cast<WORD>(commandId),
				NULL,
				script.relativePath);
			hotkey_groups.at(i).m_hotkeys.push_back(ScriptsHotkey);
		}
	}
}

void CMainFrame::InitPluginHotkey(CString guid, UINT cmd, CString name)
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
	if (m_incsearch)
		m_status.SetPaneText(ID_DEFAULT_PANE, m_is_fail ? L"Failing Incremental Search: " + m_is_str : L"Incremental Search: " + m_is_str);
	else if (!m_status_transient.IsEmpty() && static_cast<LONG>(::GetTickCount() - m_status_transient_expiration) < 0)
		m_status.SetPaneText(ID_DEFAULT_PANE, m_status_transient);
	else
		m_status.SetPaneText(ID_DEFAULT_PANE, m_status_context);
}

void CMainFrame::SetValidationStatus(ValidationStatus status)
{
	if (m_validation_status == status) return;
	m_validation_status = status;
	if (m_status.IsWindow()) m_status.SetPaneText(ID_PANE_VALIDATION, m_doc ? GetStatusValidationText() : L"");
	UpdateStatusBarLayout();
}

void CMainFrame::ResetValidationStatus()
{
	SetValidationStatus(VALIDATION_UNKNOWN);
}

void CMainFrame::ResetStatusForDocument()
{
	m_status_context.Empty();
	m_status_transient.Empty();
	m_status_transient_expiration = 0;
	ResetValidationStatus();
	RefreshStatusMainPane();
	UpdateStatusBar();
}

void CMainFrame::SetStatusContext(const CString& text)
{
	m_status_context = text;
	RefreshStatusMainPane();
}

void CMainFrame::SetTransientStatus(const CString& text)
{
	m_status_transient = text;
	m_status_transient_expiration = ::GetTickCount() + 5000;
	RefreshStatusMainPane();
}

CString CMainFrame::GetStatusValidationText() const
{
	const bool fbd = m_doc && m_doc->GetDocumentFileType() == FictionBookFileType::Fbd;
	const wchar_t* type = fbd ? L"FBD" : L"FB2";
	const wchar_t* state = m_validation_status == VALIDATION_VALID ? L"OK" :
		m_validation_status == VALIDATION_INVALID ? L"!" : L"?";
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
