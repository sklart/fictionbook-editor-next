// MainFrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "MainFrm.h"
#include "AboutBox.h"
#include "..\\common\\ModernFileDialog.h"
#include "SettingsDlg.h"
#include "KeyboardLayoutSelection.h"
#include "RuntimeLocalization.h"
#include "ImageImport.h"
#include "FictionBookFileType.h"
#include "xmlMatchedTagsHighlighter.h"
#include "StartupTrace.h"
#include "UiMetrics.h"
#include "BodySourceSelectionTransfer.h"
#include "..\\common\\DeploymentContext.h"
#include "..\\common\\RuntimeLocalizationCommon.h"
#include <string>
#include <vector>
#include <algorithm>
#include <psapi.h>

static const UINT_PTR RECOVERY_TIMER_ID = 0xFBE;
static const UINT_PTR IMAGE_IMPORT_TEST_TIMER_ID = 0xFBF;
static const UINT RECOVERY_INTERVAL_MS = 2 * 60 * 1000;
static bool IsFbeTestScenario(const wchar_t* expectedScenario);

namespace
{
const int SCRIPT_COMMAND_COUNT = 999;
const int SCRIPT_FOLDER_MENU_ID_BASE = ID_EDIT_INS_SYMBOL + 101;
const int SCRIPT_FOLDER_MENU_ID_COUNT = 999;
static_assert(ID_SCRIPT_BASE + SCRIPT_COMMAND_COUNT < SCRIPT_FOLDER_MENU_ID_BASE, "Script and folder menu IDs overlap");
static_assert(ID_LAST_PLUGIN < ID_SPELL_REPLACE_FIRST, "Plug-in and spell suggestion command IDs overlap");
static_assert(ID_SCRIPT_BASE + 999 < ID_SPELL_REPLACE_FIRST, "Script and spell suggestion command IDs overlap");
static_assert(ID_SPELL_REPLACE_LAST < ID_SCI_COLLAPSE_BASE, "Scintilla and spell suggestion command IDs overlap");
static_assert(ID_SPELL_REPLACE_LAST < 0xffff, "Spell suggestion command IDs must fit in WM_COMMAND");
static_assert(SCRIPT_FOLDER_MENU_ID_BASE > ID_EDIT_INS_SYMBOL + 100, "Folder menu IDs overlap symbol commands");
static_assert(SCRIPT_FOLDER_MENU_ID_BASE + SCRIPT_FOLDER_MENU_ID_COUNT < ID_NEXT_ITEM, "Folder menu IDs overlap regular commands");

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

struct ToolbarResourceData
{
	WORD version;
	WORD width;
	WORD height;
	WORD itemCount;
	WORD* Items() { return reinterpret_cast<WORD*>(this + 1); }
};

// Kept solely for the unattended rendering probe: creation itself never
// reconstructs an image list after the toolbar is populated.
static bool ImageListHasMaskPlane(HIMAGELIST imageList)
{
	IMAGEINFO imageInfo = {};
	return imageList != NULL && ::ImageList_GetImageInfo(imageList, 0, &imageInfo) != FALSE && imageInfo.hbmMask != NULL;
}

static bool CopyToolbarImages(HIMAGELIST destination, HIMAGELIST source, int imageCount)
{
	for (int index = 0; index < imageCount; ++index)
	{
		HICON icon = ::ImageList_GetIcon(source, index, ILD_NORMAL);
		const int copiedIndex = icon != NULL ? ::ImageList_AddIcon(destination, icon) : -1;
		if (icon != NULL) ::DestroyIcon(icon);
		if (copiedIndex != index) return false;
	}
	return true;
}

static BOOL CALLBACK SetDialogFontForToolbarChild(HWND window, LPARAM)
{
	::SendMessage(window, WM_SETFONT, reinterpret_cast<WPARAM>(UiMetrics::DialogFont()), TRUE);
	return TRUE;
}

static void SetDialogFontForToolbarRow(HWND window, bool includeChildren = false)
{
	if(window == NULL) return;
	::SendMessage(window, WM_SETFONT, reinterpret_cast<WPARAM>(UiMetrics::DialogFont()), TRUE);
	if(includeChildren) ::EnumChildWindows(window, SetDialogFontForToolbarChild, 0);
}

static void AutoSizeToolbar(HWND window)
{
	if(window != NULL) ::SendMessage(window, TB_AUTOSIZE, 0, 0);
}

static HWND CreateCommandToolbarCtrl(HWND parent, CImageList& ownedImages, UINT toolbarResourceId,
	DWORD style = ATL_SIMPLE_TOOLBAR_STYLE, UINT controlId = ATL_IDW_TOOLBAR)
{
	HINSTANCE module = _Module.GetResourceInstance();
	HRSRC resource = ::FindResource(module, MAKEINTRESOURCE(toolbarResourceId), RT_TOOLBAR);
	HGLOBAL resourceData = resource != NULL ? ::LoadResource(module, resource) : NULL;
	ToolbarResourceData* toolbarData = resourceData != NULL ? static_cast<ToolbarResourceData*>(::LockResource(resourceData)) : NULL;
	if (toolbarData == NULL || toolbarData->version != 1 || toolbarData->width != 24 || toolbarData->height != 24)
		return NULL;

	ATL::CTempBuffer<TBBUTTON, _WTL_STACK_ALLOC_THRESHOLD> buttonsBuffer;
	TBBUTTON* buttons = buttonsBuffer.Allocate(toolbarData->itemCount);
	if (buttons == NULL) return NULL;

	int standardImageCount = 0;
	for (int index = 0; index < toolbarData->itemCount; ++index)
	{
		TBBUTTON& button = buttons[index];
		::ZeroMemory(&button, sizeof(button));
		const WORD commandId = toolbarData->Items()[index];
		if (commandId != 0)
		{
			button.iBitmap = standardImageCount++;
			button.idCommand = commandId;
			button.fsState = TBSTATE_ENABLED;
			button.fsStyle = BTNS_BUTTON;
		}
		else
		{
			button.iBitmap = 8;
			button.fsStyle = BTNS_SEP;
		}
	}

	HWND window = ::CreateWindowEx(0, TOOLBARCLASSNAME, NULL, style, 0, 0, 100, 100, parent,
		(HMENU)LongToHandle(controlId), module, NULL);
	if (window == NULL) return NULL;
	::SendMessage(window, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

	if (!ownedImages.Create(24, 24, ILC_COLOR32 | ILC_MASK, standardImageCount + 8, 8))
	{
		::DestroyWindow(window);
		return NULL;
	}
	HIMAGELIST sourceImages = ::ImageList_LoadImage(module, MAKEINTRESOURCE(toolbarResourceId), 24, 1, CLR_DEFAULT,
		IMAGE_BITMAP, LR_CREATEDIBSECTION | LR_DEFAULTSIZE);
	const bool copied = sourceImages != NULL && ::ImageList_GetImageCount(sourceImages) >= standardImageCount &&
		CopyToolbarImages(ownedImages, sourceImages, standardImageCount);
	if (sourceImages != NULL) ::ImageList_Destroy(sourceImages);
	if (!copied)
	{
		ownedImages.Destroy();
		::DestroyWindow(window);
		return NULL;
	}

	const HIMAGELIST previousImages = reinterpret_cast<HIMAGELIST>(::SendMessage(window, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(static_cast<HIMAGELIST>(ownedImages))));
	if (previousImages != NULL || ::SendMessage(window, TB_ADDBUTTONS, toolbarData->itemCount, reinterpret_cast<LPARAM>(buttons)) == FALSE)
	{
		::SendMessage(window, TB_SETIMAGELIST, 0, 0);
		ownedImages.Destroy();
		::DestroyWindow(window);
		return NULL;
	}

	SetDialogFontForToolbarRow(window);
	// The image list is 24x24. Keep bitmap geometry fixed until the artwork
	// itself is DPI-aware, otherwise comctl32 reserves blank space below icons.
	::SendMessage(window, TB_SETBITMAPSIZE, 0, MAKELONG(24, 24));
	::SendMessage(window, TB_SETBUTTONSIZE, 0, MAKELONG(toolbarData->width + 7, toolbarData->height + 7));
	AutoSizeToolbar(window);
	StartupTrace::Event(L"toolbar", L"TB210", L"command-toolbar image list created; 24x24; ILC_COLOR32|ILC_MASK");
	return window;
}

static int AddToolbarBitmapFromModule(CToolBarCtrl& toolbar, HINSTANCE module, UINT bitmapResourceId)
{
	HBITMAP colorBitmap = static_cast<HBITMAP>(::LoadImage(module, MAKEINTRESOURCE(bitmapResourceId),
		IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
	if (colorBitmap == NULL) return -1;

	DIBSECTION bitmapSection = {};
	if (::GetObject(colorBitmap, sizeof(bitmapSection), &bitmapSection) != sizeof(bitmapSection) ||
		bitmapSection.dsBm.bmWidth != 24 || bitmapSection.dsBmih.biHeight == 0 ||
		(bitmapSection.dsBmih.biHeight < 0 ? -bitmapSection.dsBmih.biHeight : bitmapSection.dsBmih.biHeight) != 24 ||
		bitmapSection.dsBm.bmBitsPixel != 24 || bitmapSection.dsBm.bmBits == NULL ||
		bitmapSection.dsBm.bmWidthBytes < 24 * 3)
	{
		::DeleteObject(colorBitmap);
		return -1;
	}

	const int imageIndex = ::ImageList_AddMasked(toolbar.GetImageList(), colorBitmap, RGB(192, 192, 192));
	::DeleteObject(colorBitmap);
	return imageIndex;
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

struct TableToolbarCommand
{
	UINT bitmapResourceId;
	UINT commandId;
	LPCWSTR localizationKey;
	LPCWSTR fallbackText;
};

static const TableToolbarCommand kTableToolbarCommands[] =
{
	{ IDB_TABLE_TOOLBAR_INSERT_ROW_ABOVE, ID_TABLE_INSERT_ROW_ABOVE, L"fbe.menu.idr_mainframe.table.insert_row_above", L"Insert row above" },
	{ IDB_TABLE_TOOLBAR_INSERT_ROW_BELOW, ID_TABLE_INSERT_ROW_BELOW, L"fbe.menu.idr_mainframe.table.insert_row_below", L"Insert row below" },
	{ IDB_TABLE_TOOLBAR_DELETE_ROW, ID_TABLE_DELETE_ROW, L"fbe.menu.idr_mainframe.table.delete_row", L"Delete row" },
	{ IDB_TABLE_TOOLBAR_INSERT_COLUMN_LEFT, ID_TABLE_INSERT_COLUMN_LEFT, L"fbe.menu.idr_mainframe.table.insert_column_left", L"Insert column left" },
	{ IDB_TABLE_TOOLBAR_INSERT_COLUMN_RIGHT, ID_TABLE_INSERT_COLUMN_RIGHT, L"fbe.menu.idr_mainframe.table.insert_column_right", L"Insert column right" },
	{ IDB_TABLE_TOOLBAR_DELETE_COLUMN, ID_TABLE_DELETE_COLUMN, L"fbe.menu.idr_mainframe.table.delete_column", L"Delete column" },
	{ IDB_TABLE_TOOLBAR_MAKE_HEADER_CELLS, ID_TABLE_MAKE_HEADER_CELLS, L"fbe.menu.idr_mainframe.table.make_header_cells", L"Make header cells" },
	{ IDB_TABLE_TOOLBAR_MAKE_NORMAL_CELLS, ID_TABLE_MAKE_NORMAL_CELLS, L"fbe.menu.idr_mainframe.table.make_normal_cells", L"Make normal cells" },
};

static bool IsTableToolbarCommand(UINT commandId)
{
	for (size_t index = 0; index < _countof(kTableToolbarCommands); ++index)
	{
		if (kTableToolbarCommands[index].commandId == commandId)
			return true;
	}
	return false;
}

// A process launched elevated (for example from an administrator Visual
// Studio) does not use per-user COM registrations.  The bundled export DLLs
// live next to plugins.json, so fall back to their class factory directly when
// CoCreateInstance cannot see the HKCU registration.
struct BundledPluginMetadata
{
	CString type, module, modulePath, clsidText, menuText, menuKey;
	CLSID clsid;
};

static bool ReadBundledPluginString(const std::wstring& json, size_t objectStart, const wchar_t* name, CString& value)
{
	size_t valueStart = 0; std::wstring result;
	if(!FbeRuntimeLocalization::JsonFindObjectMember(json, objectStart, name, valueStart) || !FbeRuntimeLocalization::JsonParseString(json, valueStart, result) || result.empty()) return false;
	value = result.c_str(); return true;
}

static bool IsBundledPluginModuleName(const CString& value)
{
	// "module" is intentionally a file name relative to plugins.json.  Do not
	// accept paths that could escape the shipped Plugins directory.
	return !value.IsEmpty() && value.Find(L'\\') < 0 && value.Find(L'/') < 0 && value.Find(L':') < 0;
}

static const std::vector<BundledPluginMetadata>& BundledPluginCatalog()
{
	static std::vector<BundledPluginMetadata> entries; static bool initialized = false;
	if(initialized) return entries;
	initialized = true;
	std::wstring json; const CString path = U::GetProgDirFile(L"Plugins\\plugins.json");
	const int directoryEnd = path.ReverseFind(L'\\');
	if(directoryEnd < 0) return entries;
	const CString directory = path.Left(directoryEnd + 1);
	if(!FbeRuntimeLocalization::ReadUtf8TextFile(path, json)) return entries;
	size_t array = 0;
	if(!FbeRuntimeLocalization::JsonFindObjectMember(json, 0, L"plugins", array)) return entries;
	FbeRuntimeLocalization::JsonSkipWhitespace(json, array);
	if(array >= json.size() || json[array++] != L'[') return entries;
	for(;;) {
		FbeRuntimeLocalization::JsonSkipWhitespace(json, array);
		if(array >= json.size() || json[array] == L']') break;
		const size_t object = array;
		if(!FbeRuntimeLocalization::JsonSkipValue(json, array)) { entries.clear(); return entries; }
		BundledPluginMetadata entry = {};
		if(ReadBundledPluginString(json, object, L"type", entry.type) && ReadBundledPluginString(json, object, L"module", entry.module) && IsBundledPluginModuleName(entry.module) && ReadBundledPluginString(json, object, L"clsid", entry.clsidText) && ReadBundledPluginString(json, object, L"menu", entry.menuText) && ReadBundledPluginString(json, object, L"menuKey", entry.menuKey) && ::CLSIDFromString(const_cast<LPOLESTR>(static_cast<LPCWSTR>(entry.clsidText)), &entry.clsid) == S_OK) { entry.modulePath = directory + entry.module; entries.push_back(entry); }
		FbeRuntimeLocalization::JsonSkipWhitespace(json, array);
		if(array < json.size() && json[array] == L',') { ++array; continue; }
		if(array < json.size() && json[array] == L']') break;
		entries.clear(); return entries;
	}
	return entries;
}

static const BundledPluginMetadata* FindBundledPlugin(const CLSID& clsid)
{
	const std::vector<BundledPluginMetadata>& entries = BundledPluginCatalog();
	for(size_t index = 0; index < entries.size(); ++index) if(::InlineIsEqualGUID(entries[index].clsid, clsid)) return &entries[index];
	return NULL;
}

static const BundledPluginMetadata* FindBundledPlugin(const CString& clsidText)
{
	CLSID clsid = {};
	return ::CLSIDFromString(const_cast<LPOLESTR>(static_cast<LPCWSTR>(clsidText)), &clsid) == S_OK ? FindBundledPlugin(clsid) : NULL;
}

static HRESULT CreateBundledPluginInstance(const CLSID& clsid, IUnknownPtr& instance)
{
	// Bundled entries are trusted only from the package beside FBE.exe.  Never
	// let a stale/foreign CLSID registration intercept their activation.
	HRESULT result = E_NOINTERFACE;

	const BundledPluginMetadata* plugin = FindBundledPlugin(clsid);
	if(plugin == NULL)
		return instance.CreateInstance(clsid); // external legacy plug-in

	const CString& path = plugin->modulePath;
	HMODULE module = ::LoadLibraryEx(path, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
	if(module == NULL && ::GetLastError() == ERROR_INVALID_PARAMETER)
		module = ::LoadLibraryEx(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	if(module == NULL)
		return HRESULT_FROM_WIN32(::GetLastError());

	typedef HRESULT (STDAPICALLTYPE* DllGetClassObjectProc)(REFCLSID, REFIID, LPVOID*);
	DllGetClassObjectProc getClassObject = reinterpret_cast<DllGetClassObjectProc>(
		::GetProcAddress(module, "DllGetClassObject"));
	if(getClassObject == NULL)
		return E_NOINTERFACE;

	CComPtr<IClassFactory> factory;
	result = getClassObject(clsid, IID_IClassFactory, reinterpret_cast<void**>(&factory));
	if(FAILED(result))
		return result;
	IUnknown* localInstance = NULL;
	result = factory->CreateInstance(NULL, IID_IUnknown, reinterpret_cast<void**>(&localInstance));
	if(SUCCEEDED(result))
		instance.Attach(localInstance);
	return result;
}

static void AddBundledPluginCatalog(HMENU menu, const TCHAR* type, UINT commandBase, CSimpleArray<CLSID>& plugins)
{
	const std::vector<BundledPluginMetadata>& entries = BundledPluginCatalog();
	for(size_t index = 0; index < entries.size(); ++index)
	{
		if(::lstrcmpi(entries[index].type, type) != 0) continue;
		plugins.Add(entries[index].clsid);
		const CString text = FbeLoadRuntimeStringByKey(entries[index].menuKey, entries[index].menuText);
		::AppendMenu(menu, MF_STRING, commandBase + plugins.GetSize() - 1, text);
	}
}

static CString PortableMruPath()
{
	return CString(DeploymentContext::SettingsDirectory().c_str()) + L"MRU.xml";
}

static void ReadPortableMru(CRecentDocumentList& list)
{
	const CString path = PortableMruPath();
	HANDLE file = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) return;
	const DWORD length = ::GetFileSize(file, NULL);
	if(length == INVALID_FILE_SIZE || length > 64 * 1024) { ::CloseHandle(file); return; }
	std::vector<wchar_t> text(length / sizeof(wchar_t) + 1, 0); DWORD read = 0;
	const BOOL ok = ::ReadFile(file, &text[0], length, &read, NULL); ::CloseHandle(file);
	if(!ok || (read % sizeof(wchar_t)) != 0) return;
	int position = 0; CString line;
	while(position >= 0) { line = CString(&text[0]).Tokenize(L"\n", position); line.Trim(); if(!line.IsEmpty()) list.AddToList(line); }
}

static void WritePortableMru(const CRecentDocumentList& list)
{
	const CString directory(DeploymentContext::SettingsDirectory().c_str());
	if(!::CreateDirectory(directory, NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS) return;
	const CString path = PortableMruPath(), temporary = path + L".tmp";
	HANDLE file = ::CreateFile(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) return;
	for(int index = 0; index < list.m_arrDocs.GetSize(); ++index) { const CString line = CString(list.m_arrDocs[index].szDocName) + L"\r\n"; DWORD written = 0; if(!::WriteFile(file, line.GetString(), line.GetLength() * sizeof(wchar_t), &written, NULL) || written != static_cast<DWORD>(line.GetLength() * sizeof(wchar_t))) { ::CloseHandle(file); ::DeleteFile(temporary); return; } }
	::FlushFileBuffers(file); ::CloseHandle(file);
	if(!::MoveFileEx(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) ::DeleteFile(temporary);
}

struct PortableToolbarItem
{
	bool separator;
	int command;
	int width;
	CString relativePath;
};

static CString PortableToolbarsPath()
{
	return CString(DeploymentContext::SettingsDirectory().c_str()) + L"Toolbars.xml";
}

static CString XmlEscape(const CString& value)
{
	CString escaped(value);
	escaped.Replace(L"&", L"&amp;");
	escaped.Replace(L"\"", L"&quot;");
	escaped.Replace(L"<", L"&lt;");
	escaped.Replace(L">", L"&gt;");
	return escaped;
}

static CString XmlUnescape(const CString& value)
{
	CString unescaped(value);
	unescaped.Replace(L"&quot;", L"\"");
	unescaped.Replace(L"&lt;", L"<");
	unescaped.Replace(L"&gt;", L">");
	unescaped.Replace(L"&amp;", L"&");
	return unescaped;
}

static bool ReadPortableToolbars(std::vector<PortableToolbarItem>& commands,
	std::vector<PortableToolbarItem>& scripts, CString& lastScript,
	bool& commandToolbarPresent, bool& scriptsToolbarPresent)
{
	commandToolbarPresent = false;
	scriptsToolbarPresent = false;
	const CString path = PortableToolbarsPath();
	HANDLE file = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) return false;
	const DWORD length = ::GetFileSize(file, NULL);
	if(length == INVALID_FILE_SIZE || length > 256 * 1024 || (length % sizeof(wchar_t)) != 0) { ::CloseHandle(file); return false; }
	std::vector<wchar_t> text(length / sizeof(wchar_t) + 1, 0); DWORD read = 0;
	const BOOL ok = ::ReadFile(file, &text[0], length, &read, NULL); ::CloseHandle(file);
	if(!ok || read != length) return false;

	CString content(&text[0]);
	if(content.Find(L"<Toolbars version=\"1\">") < 0 || content.Find(L"</Toolbars>") < 0)
		return false;
	int cursor = 0;
	CString active;
	while(cursor >= 0)
	{
		const int start = content.Find(L'<', cursor);
		if(start < 0) break;
		const int end = content.Find(L'>', start + 1);
		if(end < 0) break;
		CString tag = content.Mid(start + 1, end - start - 1);
		cursor = end + 1;
		if(tag.Left(8) == L"Toolbar ")
		{
			active = tag.Find(L"name=\"Command\"") >= 0 ? L"Command" :
				tag.Find(L"name=\"Scripts\"") >= 0 ? L"Scripts" : CString();
			if(active == L"Command") commandToolbarPresent = true;
			if(active == L"Scripts") scriptsToolbarPresent = true;
			continue;
		}
		if(tag.Left(8) == L"/Toolbar") { active.Empty(); continue; }
		if(active.IsEmpty())
		{
			if(tag.Left(10) == L"LastScript")
			{
				const int value = tag.Find(L"path=\"");
				if(value >= 0) { const int tail = tag.Find(L'\"', value + 6); if(tail > value) lastScript = XmlUnescape(tag.Mid(value + 6, tail - value - 6)); }
			}
			continue;
		}
		PortableToolbarItem item = {};
		if(tag.Left(9) == L"Separator")
		{
			item.separator = true;
			const int width = tag.Find(L"width=\"");
			if(width >= 0) item.width = _wtoi(tag.Mid(width + 7));
		}
		else if(tag.Left(7) == L"Command")
		{
			item.command = _wtoi(tag.Mid(tag.Find(L"id=\"") + 4));
		}
		else if(tag.Left(6) == L"Script")
		{
			const int value = tag.Find(L"path=\"");
			if(value < 0) continue;
			const int tail = tag.Find(L'\"', value + 6);
			if(tail < 0) continue;
			item.relativePath = XmlUnescape(tag.Mid(value + 6, tail - value - 6));
		}
		else continue;
		(active == L"Command" ? commands : scripts).push_back(item);
	}
	return commandToolbarPresent || scriptsToolbarPresent;
}

static bool WritePortableToolbarsText(const CString& text)
{
	const CString directory(DeploymentContext::SettingsDirectory().c_str());
	if(!::CreateDirectory(directory, NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS) return false;
	const CString path = PortableToolbarsPath(), temporary = path + L".tmp";
	HANDLE file = ::CreateFile(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	const DWORD bytes = static_cast<DWORD>(text.GetLength() * sizeof(wchar_t));
	const bool ok = ::WriteFile(file, text, bytes, &written, NULL) != FALSE && written == bytes;
	if(ok) ::FlushFileBuffers(file);
	::CloseHandle(file);
	if(!ok || !::MoveFileEx(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { ::DeleteFile(temporary); return false; }
	return true;
}

struct ScriptCommandId
{
	CString relativePath;
	int value;
};

static CString NormalizeFullPath(const CString& path)
{
	DWORD length = ::GetFullPathName(path, 0, NULL, NULL);
	if(length == 0)
		return CString();

	std::vector<wchar_t> buffer(length + 1);
	if(::GetFullPathName(path, static_cast<DWORD>(buffer.size()), &buffer[0], NULL) == 0)
		return CString();

	CString normalized(&buffer[0]);
	while(normalized.GetLength() > 3 && (normalized[normalized.GetLength() - 1] == L'\\' || normalized[normalized.GetLength() - 1] == L'/'))
		normalized.Delete(normalized.GetLength() - 1);
	return normalized;
}

static CString NormalizeScriptRelativePath(const CString& scriptsRoot, const CString& scriptPath)
{
	const CString root = NormalizeFullPath(scriptsRoot);
	const CString fullPath = NormalizeFullPath(scriptPath);
	if(root.IsEmpty() || fullPath.GetLength() <= root.GetLength() || fullPath.Left(root.GetLength()).CompareNoCase(root) != 0)
		return CString();

	const wchar_t separator = fullPath[root.GetLength()];
	if(separator != L'\\' && separator != L'/')
		return CString();

	CString relativePath = fullPath.Mid(root.GetLength() + 1);
	relativePath.Replace(L'\\', L'/');
	relativePath.MakeLower();
	return relativePath;
}

static DWORD HashScriptRelativePath(const CString& relativePath)
{
	DWORD hash = 2166136261u;
	for(int i = 0; i < relativePath.GetLength(); ++i)
	{
		hash ^= static_cast<DWORD>(relativePath[i]);
		hash *= 16777619u;
	}
	return hash;
}

static CString EncodeScriptPath(const CString& path)
{
	CString encoded;
	for(int i = 0; i < path.GetLength(); ++i)
	{
		CString codeUnit;
		codeUnit.Format(L"%04X", static_cast<unsigned int>(path[i]));
		encoded += codeUnit;
	}
	return encoded;
}

static bool DecodeScriptPath(const CString& encoded, CString& path)
{
	if(encoded.IsEmpty() || encoded.GetLength() % 4 != 0)
		return false;

	path.Empty();
	for(int i = 0; i < encoded.GetLength(); i += 4)
	{
		const CString codeUnit = encoded.Mid(i, 4);
		if(codeUnit.SpanIncluding(L"0123456789ABCDEFabcdef").GetLength() != 4)
			return false;
		path.AppendChar(static_cast<wchar_t>(wcstoul(codeUnit, NULL, 16)));
	}
	return true;
}

static bool ContainsScriptPath(const std::vector<ScriptCommandId>& ids, const CString& path, int* value = NULL)
{
	for(size_t i = 0; i < ids.size(); ++i)
	{
		if(ids[i].relativePath == path)
		{
			if(value != NULL)
				*value = ids[i].value;
			return true;
		}
	}
	return false;
}

static bool IsScriptCommandIdUsed(const std::vector<ScriptCommandId>& ids, int value)
{
	for(size_t i = 0; i < ids.size(); ++i)
		if(ids[i].value == value)
			return true;
	return false;
}

static std::vector<ScriptCommandId> ParseScriptCommandIds(const CString& serialized)
{
	std::vector<ScriptCommandId> ids;
	int cursor = 0;
	CString entry;
	while(!(entry = serialized.Tokenize(L";", cursor)).IsEmpty())
	{
		const int delimiter = entry.Find(L':');
		if(delimiter <= 0)
			continue;

		const int value = _wtoi(entry.Left(delimiter));
		CString relativePath;
		if(value < 1 || value > SCRIPT_COMMAND_COUNT || !DecodeScriptPath(entry.Mid(delimiter + 1), relativePath) ||
			ContainsScriptPath(ids, relativePath) || IsScriptCommandIdUsed(ids, value))
			continue;

		ScriptCommandId id = { relativePath, value };
		ids.push_back(id);
	}
	return ids;
}

static CString SerializeScriptCommandIds(const std::vector<ScriptCommandId>& ids)
{
	CString serialized;
	for(size_t i = 0; i < ids.size(); ++i)
	{
		CString entry;
		entry.Format(L"%d:%s", ids[i].value, static_cast<const wchar_t*>(EncodeScriptPath(ids[i].relativePath)));
		if(!serialized.IsEmpty())
			serialized += L";";
		serialized += entry;
	}
	return serialized;
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
	{ ID_FILE_MRU_FIRST, L"fbe.menu.idr_mainframe.recent.empty" },
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
	for (size_t index = 0; index < _countof(kTableToolbarCommands); ++index)
	{
		const TableToolbarCommand& command = kTableToolbarCommands[index];
		if (command.commandId == commandId)
			return FbeLoadRuntimeStringByKey(command.localizationKey, command.fallbackText);
	}

	wchar_t resourceText[MAX_LOAD_STRING + 1] = {};
	if (!FbeLoadString(_Module.GetResourceInstance(), commandId, resourceText, MAX_LOAD_STRING))
		return CString();

	const wchar_t* fallback = wcschr(resourceText, L'\n');
	fallback = (fallback != NULL) ? fallback + 1 : resourceText;
	const LPCWSTR key = FindRuntimeMainFrameMenuCommandKey(commandId);
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

static CString LocalizeBundledPluginMenuText(const CString& clsidText, const CString& fallback)
{
	const BundledPluginMetadata* plugin = FindBundledPlugin(clsidText);
	return plugin == NULL ? fallback : FbeLoadRuntimeStringByKey(plugin->menuKey, fallback);
}

// Обновляет уже существующие пункты встроенных плагинов. При смене языка не
// нужно заново создавать плагины, скрипты, значки меню и кнопки toolbar: такой
// путь накапливал GDI-ресурсы и добавлял повторные элементы интерфейса.
static void RefreshBundledPluginMenuTexts(HMENU menu, const TCHAR* type, UINT commandBase)
{
	if(menu == NULL)
		return;

	CRegKey pluginsKey;
	if(pluginsKey.Open(HKEY_CURRENT_USER, _Settings.GetKeyPath() + L"\\Plugins") != ERROR_SUCCESS)
		return;

	int commandOffset = 0;
	for(int registryIndex = 0; commandOffset < 20; ++registryIndex)
	{
		CString clsidText;
		DWORD size = 128;
		TCHAR* buffer = clsidText.GetBuffer(size);
		FILETIME writeTime = {};
		if(::RegEnumKeyEx(pluginsKey, registryIndex, buffer, &size, 0, 0, 0, &writeTime) != ERROR_SUCCESS)
			break;
		clsidText.ReleaseBuffer(size);

		CRegKey pluginKey;
		if(pluginKey.Open(pluginsKey, clsidText) != ERROR_SUCCESS)
			continue;

		const CString pluginType(U::QuerySV(pluginKey, L"Type"));
		const CString fallback(U::QuerySV(pluginKey, L"Menu"));
		if(pluginType.IsEmpty() || fallback.IsEmpty() || pluginType != type)
			continue;

		const CString text = LocalizeBundledPluginMenuText(clsidText, fallback);
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
		HMENU recentMenu = ::GetSubMenu(fileMenu, 9);
		if(importMenu != NULL && ::GetMenuItemID(importMenu, 0) == IDCANCEL)
			SetRuntimePlainMenuItemTextByPosition(importMenu, 0, L"fbe.menu.idr_mainframe.plugins.none.import");
		if(exportMenu != NULL && ::GetMenuItemID(exportMenu, 0) == IDCANCEL)
			SetRuntimePlainMenuItemTextByPosition(exportMenu, 0, L"fbe.menu.idr_mainframe.plugins.none.export");
		if(recentMenu != NULL && ::GetMenuItemID(recentMenu, 0) == IDCANCEL)
			SetRuntimePlainMenuItemTextByPosition(recentMenu, 0, L"fbe.menu.idr_mainframe.recent.empty");
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
			background == other.background && sourceColorPalette == other.sourceColorPalette &&
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
		before.fastMode != after.fastMode;
}
static bool HasOnlySourceEditorConfigurationChanged(const EditorConfigurationSnapshot& before,
	const EditorConfigurationSnapshot& after)
{
	return before.font == after.font && before.foreground == after.foreground &&
		before.background == after.background && before.fontSize == after.fontSize &&
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
		if (activatedWnd != (HWND)wParam)
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
	m_current_view = BODY;
	m_last_view = DESC;
	m_last_ctrl_tab_view= DESC;
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

CString	CMainFrame::GetOpenFileName() 
{
	const COMDLG_FILTERSPEC filters[] = {
		{ L"FictionBook files (*.fb2;*.fbd)", L"*.fb2;*.fbd" },
		{ L"All files (*.*)", L"*.*" }
	};
	ModernFileDialog::Request request;
	request.fileMustExist = true;
	request.pathMustExist = true;
	request.defaultExtension = L"fb2";
	request.filters = filters;
	request.filterCount = _countof(filters);
	request.filterIndex = 1;
	const ModernFileDialog::Result result = ModernFileDialog::Show(m_hWnd, request);
	if (result.outcome == ModernFileDialog::Outcome::Failed)
		StartupTrace::HResult(L"file-dialog", L"FD101", result.error, L"Open FictionBook dialog");
	if (result.outcome == ModernFileDialog::Outcome::Accepted) return result.paths.front().c_str();
	return CString();
}

CString	CMainFrame::GetSaveFileName(CString& encoding) {
	bstr_t filename = m_doc->m_filename;
	if (!filename || (filename == bstr_t(L"Untitled.fb2")))
		filename = L"";
	const bool saveAsFbd = IsFbdFile((const wchar_t*)filename);
	const COMDLG_FILTERSPEC filters[] = {
		{ L"FictionBook (*.fb2)", L"*.fb2" },
		{ L"FictionBook Description (*.fbd)", L"*.fbd" },
		{ L"All files (*.*)", L"*.*" }
	};
	CString selectedEncoding = _Settings.KeepEncoding() ? m_doc->m_encoding : _Settings.GetDefaultEncoding();
	wchar_t encodingBuffer[1024] = {};
	FbeLoadString(_Module.GetResourceInstance(), IDS_ENCODINGS, encodingBuffer, _countof(encodingBuffer));
	CString encodingList(encodingBuffer);
	ModernFileDialog::Request request;
	request.save = true;
	request.pathMustExist = true;
	request.overwritePrompt = true;
	request.defaultExtension = L"fb2";
	request.initialFileName = static_cast<const wchar_t*>(filename);
	request.filters = filters;
	request.filterCount = _countof(filters);
	request.filterIndex = saveAsFbd ? 2 : 1;
	request.customize = [&encodingList, &selectedEncoding](IFileDialogCustomize* customize) -> HRESULT {
		const DWORD labelId = 1000;
		const DWORD controlId = 1001;
		HRESULT hr = customize->AddText(labelId, FbeLoadRuntimeStringByKey(L"fbe.save_as.encoding", L"Encoding:"));
		if (FAILED(hr)) return hr;
		hr = customize->AddComboBox(controlId);
		if (FAILED(hr)) return hr;
		int index = 0, selectedIndex = 0;
		CString remaining(encodingList);
		while (!remaining.IsEmpty()) {
			const int comma = remaining.Find(L',');
			const CString item = comma >= 0 ? remaining.Left(comma) : remaining;
			remaining = comma >= 0 ? remaining.Mid(comma + 1) : CString();
			if (!item.IsEmpty()) {
				customize->AddControlItem(controlId, ++index, item);
				if (item == selectedEncoding) selectedIndex = index;
			}
		}
		return customize->SetSelectedControlItem(controlId, selectedIndex ? selectedIndex : 1);
	};
	request.readCustomization = [&encodingList, &selectedEncoding](IFileDialogCustomize* customize) {
		DWORD selected = 0;
		if (!customize || FAILED(customize->GetSelectedControlItem(1001, &selected))) return;
		int index = 0;
		CString remaining(encodingList);
		while (!remaining.IsEmpty()) {
			const int comma = remaining.Find(L',');
			const CString item = comma >= 0 ? remaining.Left(comma) : remaining;
			remaining = comma >= 0 ? remaining.Mid(comma + 1) : CString();
			if (!item.IsEmpty() && ++index == static_cast<int>(selected)) { selectedEncoding = item; return; }
		}
	};
	const ModernFileDialog::Result dialogResult = ModernFileDialog::Show(m_hWnd, request);
	if (dialogResult.outcome == ModernFileDialog::Outcome::Failed)
		StartupTrace::HResult(L"file-dialog", L"FD102", dialogResult.error, L"Save FictionBook dialog");
	if (dialogResult.outcome == ModernFileDialog::Outcome::Accepted) {
		encoding = selectedEncoding;
		CString result(dialogResult.paths.front().c_str());
		FictionBookFileType targetType = dialogResult.filterIndex == 2 ? FictionBookFileType::Fbd :
			dialogResult.filterIndex == 1 ? FictionBookFileType::Fb2 :
		ResolveFictionBookTargetType(CString(), m_doc->m_filename);
	return AddFictionBookExtensionIfMissing(result, targetType);
  }
  return CString();
}

bool	CMainFrame::DocChanged() {
	return m_doc && m_doc->DocChanged() || IsSourceActive() && m_source.SendMessage(SCI_GETMODIFY);
}

bool	CMainFrame::DiscardChanges() {	
  U::SaveFileSelectedPos(m_doc->m_filename, m_doc->GetSelectedPos());

  if (DocChanged())
  {
    switch (U::MessageBox(MB_YESNOCANCEL|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SAVE_DLG_MSG, m_doc->m_filename))
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

  if (!askname && m_doc->m_namevalid) {
    const DWORD attributes = ::GetFileAttributes(m_doc->m_filename);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY)) {
      if (U::MessageBox(MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON1,
            IDR_MAINFRAME, IDS_READONLY_SAVE_MSG, m_doc->m_filename) == IDYES)
        return SaveFile(true);
      return CANCELLED;
    }
  }

  if (askname || !m_doc->m_namevalid) { // ask user about save file name
    CString encoding;
    CString filename(GetSaveFileName(encoding));
    if (filename.IsEmpty())
      return CANCELLED;
    const bool wasFbd = IsFbdFile(m_doc->m_filename);
    m_doc->m_encoding=encoding;
    if (m_doc->Save(filename)) {
      m_doc->m_filename=filename;
	  if (wasFbd != IsFbdFile(filename)) ResetValidationStatus();
	  U::SetCurrentDirectoryToFile(filename);
      m_doc->m_namevalid=true;
      m_mru.AddToList(filename);
	  m_file_age = FileAge(m_doc->m_filename);
	  if(IsSourceActive())
		  m_source.SendMessage(SCI_SETSAVEPOINT);
      DeleteRecoveryFile();
	  UpdateStatusBar();
      return OK;
    }
    return FAIL;
  }
  bool saved = m_doc->Save();

  if(saved)
  {
	  m_file_age = FileAge(m_doc->m_filename);
	  if(IsSourceActive())
		  m_source.SendMessage(SCI_SETSAVEPOINT);
      DeleteRecoveryFile();
	  return OK;
  }
  else
  {
	const HRESULT saveError = m_doc->GetLastSaveError();
	const bool accessDenied = saveError == E_ACCESSDENIED || HRESULT_CODE(saveError) == ERROR_ACCESS_DENIED;
	if (accessDenied && U::MessageBox(MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON1,
		IDR_MAINFRAME, IDS_SAVE_ACCESS_DENIED_MSG, m_doc->m_filename) == IDYES)
		return SaveFile(true);
	return FAIL;
  }
}

CMainFrame::FILE_OP_STATUS  CMainFrame::LoadFile(const wchar_t *initfilename)
{
  if (!DiscardChanges())
    return CANCELLED; 
  
  CString filename(initfilename);
  if (filename.IsEmpty())
    filename = GetOpenFileName();
  if (filename.IsEmpty())
    return CANCELLED;
  
	FB::Doc *doc = new FB::Doc(*this);
	FB::Doc::m_active_doc = doc;
	if((filename.ReverseFind(L'\\') + 1) != -1 && (filename.ReverseFind(L'\\') + 1) < filename.GetLength() - 1)
	{
		doc->m_body.m_file_path = filename.Mid(0, filename.ReverseFind(L'\\') + 1);
		doc->m_body.m_file_name = filename.Mid(filename.ReverseFind(L'\\') + 1, filename.GetLength() - 1);
	}
  EnableWindow(FALSE);
  m_status.SetPaneText(ID_DEFAULT_PANE,L"Loading...");
  bool fLoaded = doc->Load(m_view, filename);
  EnableWindow(TRUE);
  if (!fLoaded) 
  {
	  if (LoadToScintilla(filename)) return OK;
	  else return FAIL;
/*  delete doc;
	FB::Doc::m_active_doc = m_doc;
    return FAIL; */
  }

  AttachDocument(doc);
  m_file_age = FileAge(filename);
  delete m_doc;
  m_doc=doc;
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
		m_ctrl_tab = false;
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
		for(int i = 0; i < m_scripts.GetSize(); ++i)
		{
			if(!m_scripts[i].isFolder)
			{
				::EnableMenuItem(scripts, ID_SCRIPT_BASE + m_scripts[i].wID, MF_BYCOMMAND | MF_GRAYED);
			}
		}

		m_id_box.EnableWindow(FALSE);
		m_href_box.EnableWindow(FALSE);
		m_image_title_box.EnableWindow(FALSE);
		m_section_box.EnableWindow(FALSE);
		m_id_table_id_box.EnableWindow(FALSE);
		m_id_table_box.EnableWindow(FALSE);
		m_styleT_table_box.EnableWindow(FALSE);
		m_style_table_box.EnableWindow(FALSE);
		m_colspan_table_box.EnableWindow(FALSE);
		m_rowspan_table_box.EnableWindow(FALSE);
		m_alignTR_table_box.EnableWindow(FALSE);
		m_align_table_box.EnableWindow(FALSE);
		m_valign_table_box.EnableWindow(FALSE);

		m_id_caption.SetEnabled(false);
		m_href_caption.SetEnabled(false);
		m_section_id_caption.SetEnabled(false);
		m_image_title_caption.SetEnabled(false);
		m_table_id_caption.SetEnabled(false);
		m_table_style_caption.SetEnabled(false);
		m_id_table_caption.SetEnabled(false);
		m_style_caption.SetEnabled(false);
		m_colspan_caption.SetEnabled(false);
		m_rowspan_caption.SetEnabled(false);
		m_tr_allign_caption.SetEnabled(false);
		m_th_allign_caption.SetEnabled(false);
		m_valign_caption.SetEnabled(false);	

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
		for (int i = 0; i < m_scripts.GetSize(); ++i)
		{
			if(!m_scripts[i].isFolder)
			{
				::EnableMenuItem(scripts, ID_SCRIPT_BASE + m_scripts[i].wID, MF_BYCOMMAND | MF_ENABLED);
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

		if (m_sel_changed && /*GetCurView()*/m_current_view != DESC)
		{
			SetStatusContext(m_doc->m_body.SelPath());
			UpdateStatusBar();

			// update links and IDs
			try
			{
				MSHTML::IHTMLElementPtr an(m_doc->m_body.SelectionAnchor());
				_variant_t    href;

				if(an)
					href.Attach(an->getAttribute(L"href", 2));

				if((bool)an && V_VT(&href)==VT_BSTR)
				{
					m_href_box.EnableWindow();
					m_href_caption.SetEnabled();
					m_ignore_cb_changes = true;

					if(!(m_href == ::GetFocus()))
					{
						// changed by SeNS: fix hrefs
						CString tmp(V_BSTR(&href));
						if (tmp.Find(L"file") == 0)
							tmp = tmp.Mid(tmp.ReverseFind (L'#'),1024);
						m_href.SetWindowText(tmp);
						m_href.SetSel(tmp.GetLength(), tmp.GetLength(), FALSE);
					}

					m_ignore_cb_changes = false;

					// SeNS - inline images
					bool img = (U::scmp(an->tagName, L"DIV") == 0) || (U::scmp(an->tagName, L"SPAN") == 0);
					if(img != m_cb_last_images)
						m_cb_updated = false;
					m_cb_last_images = img;
				}
				else
				{
					m_href_box.SetWindowText(L"");
					m_href_box.EnableWindow(FALSE);
					m_href_caption.SetEnabled(false);
				}

				MSHTML::IHTMLElementPtr	sc(m_doc->m_body.SelectionStructCon());
				if(sc)
				{
					m_id_box.EnableWindow();
					m_id_caption.SetEnabled(true);
					m_ignore_cb_changes = true;

					if(U::scmp(sc->id, L"fbw_body"))
						m_id.SetWindowText(sc->id);		  
					else
						m_id.SetWindowText(L"");	

					m_ignore_cb_changes = false;
				}
				else
				{
					m_id_box.EnableWindow(FALSE);
					m_id_caption.SetEnabled(false);
				}

				MSHTML::IHTMLElementPtr	  im(m_doc->m_body.SelectionStructImage());
				if(im)
				{
					m_image_title_box.EnableWindow();
					m_image_title_caption.SetEnabled();
					m_ignore_cb_changes = true;
					m_image_title.SetWindowText(im->title);
					_bstr_t title = im->title;
					const int titleLength = title.length();
					if(titleLength)
						m_image_title.SetSel(titleLength, titleLength, FALSE);
					m_ignore_cb_changes = false;
				}
				else
				{
					m_image_title_box.SetWindowText(L"");
					m_image_title_box.EnableWindow(FALSE);
					m_image_title_caption.SetEnabled(false);
				}
		
				// ??????????? ID ??? ????? <section>
				MSHTML::IHTMLElementPtr scstn(m_doc->m_body.SelectionStructSection());
				if(scstn)
				{
					m_section_box.EnableWindow(TRUE);
					m_section_id_caption.SetEnabled();
					m_ignore_cb_changes = true;	  
					m_section.SetWindowText(scstn->id);
					m_ignore_cb_changes = false;
				}
				else
				{
					m_section_box.SetWindowText(L"");
					m_section_box.EnableWindow(FALSE);
					m_section_id_caption.SetEnabled(false);
				}	
				// ??????????? ID ??? ????? <table>
				MSHTML::IHTMLElementPtr sct(m_doc->m_body.SelectionStructTable());
				if(sct)
				{
					m_id_table_id_box.EnableWindow(TRUE);
					m_table_id_caption.SetEnabled();
					m_ignore_cb_changes = true;	  
					m_id_table_id.SetWindowText(sct->id);
					m_ignore_cb_changes = false;
				}
				else
				{
					m_id_table_id_box.SetWindowText(L"");
					m_id_table_id_box.EnableWindow(FALSE);
					m_table_id_caption.SetEnabled(false);
				}

				// ??????????? ID ??? ????? <tr>, <th>, <td>
				MSHTML::IHTMLElementPtr sctc(m_doc->m_body.SelectionStructTableCon());
				if (sctc) {
					m_id_table_box.EnableWindow(TRUE);
					m_id_table_caption.SetEnabled();
					m_ignore_cb_changes = true;	  
					m_id_table.SetWindowText(sctc->id);
					m_ignore_cb_changes = false;
				}
				else
				{
					m_id_table_box.SetWindowText(L"");
					m_id_table_box.EnableWindow(FALSE);
					m_id_table_caption.SetEnabled(false);
				}

				// ??????????? style ??? ????? <table>
				_bstr_t	styleT("");
				MSHTML::IHTMLElementPtr scsT(m_doc->m_body.SelectionsStyleTB(styleT));
				if(scsT)
				{
					m_styleT_table_box.EnableWindow(TRUE);
					m_table_style_caption.SetEnabled();
					if(U::scmp(styleT,L"") != 0)
					{
						m_styleT_table_box.EnableWindow(TRUE);
						m_table_style_caption.SetEnabled();
						m_ignore_cb_changes = true;	  
						m_styleT_table.SetWindowText(styleT);
						m_ignore_cb_changes = false;
					}
					else
					{
						m_styleT_table_box.SetWindowText(L"");
					}
				}
				else
				{
					m_styleT_table_box.SetWindowText(L"");
					m_table_style_caption.SetEnabled(false);
					m_styleT_table_box.EnableWindow(FALSE);
				}

				// ??????????? style ??? ????? <th>, <td>
				_bstr_t	style("");
				MSHTML::IHTMLElementPtr scs(m_doc->m_body.SelectionsStyleB(style));
				if(scs)
				{
					m_style_table_box.EnableWindow(TRUE);
					m_style_caption.SetEnabled();
					if (U::scmp(style,L"") != 0)
					{
						m_style_table_box.EnableWindow(TRUE);
						m_style_caption.SetEnabled();
						m_ignore_cb_changes = true;	  
						m_style_table.SetWindowText(style);
						m_ignore_cb_changes = false;
					}
					else
					{
						m_style_table_box.SetWindowText(L"");
					}
				}
				else
				{
					m_style_table_box.SetWindowText(L"");
					m_style_table_box.EnableWindow(FALSE);
					m_style_caption.SetEnabled(false);
				}

				// ??????????? colspan ??? ????? <th>, <td>
				_bstr_t colspan("");
				MSHTML::IHTMLElementPtr scc(m_doc->m_body.SelectionsColspanB(colspan));
				if(scc)
				{
					m_colspan_table_box.EnableWindow(TRUE);
					m_colspan_caption.SetEnabled();
					if(U::scmp(colspan, L"") != 0)
					{
						m_colspan_table_box.EnableWindow(TRUE);
						m_colspan_caption.SetEnabled();
						m_ignore_cb_changes = true;	  
						m_colspan_table.SetWindowText(colspan);
						m_ignore_cb_changes = false;
					}
					else
					{
						m_colspan_table_box.SetWindowText(L"");
					}
				}
				else
				{
					m_colspan_table_box.SetWindowText(_T(""));
					m_colspan_table_box.EnableWindow(FALSE);
					m_colspan_caption.SetEnabled(false);
				}

				// ??????????? rowspan ??? ????? <th>, <td>
				_bstr_t rowspan("");
				MSHTML::IHTMLElementPtr scr(m_doc->m_body.SelectionsRowspanB(rowspan));
				if(scr)
				{
					m_rowspan_table_box.EnableWindow(TRUE);
					m_rowspan_caption.SetEnabled();
					if (U::scmp(rowspan,L"") != 0)
					{
						m_rowspan_table_box.EnableWindow(TRUE);
						m_rowspan_caption.SetEnabled();
						m_ignore_cb_changes = true;	  
						m_rowspan_table.SetWindowText(rowspan);
						m_ignore_cb_changes = false;
					}
					else
					{
						m_rowspan_table_box.SetWindowText(L"");
					}
				}
				else
				{
					m_rowspan_table_box.SetWindowText(L"");
					m_rowspan_table_box.EnableWindow(FALSE);
					m_rowspan_caption.SetEnabled(false);
				}

				// ??????????? align ??? ????? <tr>
				_bstr_t alignTR("");
				MSHTML::IHTMLElementPtr scaTR(m_doc->m_body.SelectionsAlignTRB(alignTR));
				if(scaTR)
				{
					m_alignTR_table_box.EnableWindow(TRUE);
					m_tr_allign_caption.SetEnabled();
					if(U::scmp(alignTR,L"") != 0)
					{
						m_alignTR_table_box.EnableWindow(TRUE);
						m_tr_allign_caption.SetEnabled();
						m_ignore_cb_changes = true;	  
						m_alignTR_table_box.SetCurSel(m_alignTR_table_box.FindString(0,alignTR));
						m_ignore_cb_changes = false;
					}
					else
					{
						m_alignTR_table_box.SetCurSel(m_alignTR_table_box.FindString( 0, L""));
					}
				}
				else
				{
					m_alignTR_table_box.SetCurSel(m_alignTR_table_box.FindString(0, L""));
					m_alignTR_table_box.EnableWindow(FALSE);
					m_tr_allign_caption.SetEnabled(false);
				}

				// ??????????? align ??? ????? <th>, <td>
				_bstr_t align("");
				MSHTML::IHTMLElementPtr sca(m_doc->m_body.SelectionsAlignB(align));
				if(sca)
				{
					m_align_table_box.EnableWindow(TRUE);
					m_th_allign_caption.SetEnabled();
					if(U::scmp(align,L"") != 0)
					{
						m_align_table_box.EnableWindow(TRUE);
						m_th_allign_caption.SetEnabled();
						m_ignore_cb_changes = true;	  
						m_align_table_box.SetCurSel(m_align_table_box.FindString(0, align));
						m_ignore_cb_changes = false;
					}
					else
					{
						m_align_table_box.SetCurSel(m_align_table_box.FindString(0, L""));
					}
				}
				else
				{
					m_align_table_box.SetCurSel(m_align_table_box.FindString(0, L""));
					m_align_table_box.EnableWindow(FALSE);
					m_th_allign_caption.SetEnabled(false);
				}

				// ??????????? valign ??? ????? <th>, <td>
				_bstr_t valign("");
				MSHTML::IHTMLElementPtr scva(m_doc->m_body.SelectionsVAlignB(valign));
				if(scva)
				{
					m_valign_table_box.EnableWindow(TRUE);
					m_valign_caption.SetEnabled();	
					if (U::scmp(valign,L"") != 0)
					{
						m_valign_table_box.EnableWindow(TRUE);
						m_valign_caption.SetEnabled();	
						m_ignore_cb_changes = true;	  
						m_valign_table_box.SetCurSel(m_valign_table_box.FindString(0, valign));
						m_ignore_cb_changes = false;
					}
					else
					{
						m_valign_table_box.SetCurSel(m_valign_table_box.FindString(0, L""));
					}
				}
				else
				{
					m_valign_table_box.SetCurSel(m_valign_table_box.FindString(0, L""));
					m_valign_table_box.EnableWindow(FALSE);
					m_valign_caption.SetEnabled(false);	
				}
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
	if (m_Speller && m_Speller->Enabled() && m_current_view == BODY) 
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

	const bool tableCommandEnabled = m_current_view == BODY && m_doc && m_doc->m_body.SelectionStructTableCon();
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

void CMainFrame::RestorePortableToolbarLayout(HWND toolbar, bool scriptsToolbar)
{
	if(DeploymentContext::RegistryPersistenceAllowed()) return;
	std::vector<PortableToolbarItem> commands, scripts;
	CString lastScript; bool commandToolbarPresent = false, scriptsToolbarPresent = false;
	if(!ReadPortableToolbars(commands, scripts, lastScript, commandToolbarPresent, scriptsToolbarPresent)) return;
	const bool toolbarPresent = scriptsToolbar ? scriptsToolbarPresent : commandToolbarPresent;
	if(!toolbarPresent) return;
	const std::vector<PortableToolbarItem>& saved = scriptsToolbar ? scripts : commands;

	CToolBarCtrl target = toolbar;
	const int catalogIndex = m_aButtons.FindKey(static_cast<int>(reinterpret_cast<INT_PTR>(toolbar)));
	if(catalogIndex < 0) return;
	TBBUTTONS catalog = m_aButtons.GetValueAt(catalogIndex);
	std::vector<TBBUTTON> restored;
	for(size_t index = 0; index < saved.size(); ++index)
	{
		const PortableToolbarItem& item = saved[index];
		if(item.separator)
		{
			TBBUTTON separator = {};
			separator.iBitmap = item.width > 0 ? item.width : 8;
			separator.fsStyle = TBSTYLE_SEP;
			restored.push_back(separator);
			continue;
		}

		int command = item.command;
		if(!item.relativePath.IsEmpty())
		{
			command = 0;
			for(int scriptIndex = 0; scriptIndex < m_scripts.GetSize(); ++scriptIndex)
				if(!m_scripts[scriptIndex].isFolder && m_scripts[scriptIndex].relativePath == item.relativePath && m_scripts[scriptIndex].wID > 0)
				{
					command = ID_SCRIPT_BASE + m_scripts[scriptIndex].wID;
					break;
				}
		}
		if(command == 0) continue; // deleted script or obsolete command
		for(int buttonIndex = 0; buttonIndex < catalog.GetSize(); ++buttonIndex)
			if(catalog[buttonIndex].idCommand == command)
			{
				restored.push_back(catalog[buttonIndex]);
				break;
			}
	}
	while(target.GetButtonCount() > 0) target.DeleteButton(0);
	if(!restored.empty()) target.AddButtons(static_cast<int>(restored.size()), &restored[0]);
	target.AutoSize();

	if(scriptsToolbar && !lastScript.IsEmpty())
		for(int scriptIndex = 0; scriptIndex < m_scripts.GetSize(); ++scriptIndex)
			if(!m_scripts[scriptIndex].isFolder && m_scripts[scriptIndex].relativePath == lastScript)
			{
				m_last_script = &m_scripts[scriptIndex];
				break;
			}
}

void CMainFrame::SavePortableToolbarLayout()
{
	if(DeploymentContext::RegistryPersistenceAllowed()) return;
	CString xml(L"<Toolbars version=\"1\">\r\n");
	auto appendToolbar = [&](const wchar_t* name, HWND toolbar, bool scriptsToolbar)
	{
		xml.AppendFormat(L"  <Toolbar name=\"%s\">\r\n", name);
		CToolBarCtrl source = toolbar;
		for(int index = 0; index < source.GetButtonCount(); ++index)
		{
			TBBUTTON button = {};
			if(!source.GetButton(index, &button)) continue;
			if((button.fsStyle & TBSTYLE_SEP) != 0)
			{
				xml.AppendFormat(L"    <Separator width=\"%d\" />\r\n", button.iBitmap);
				continue;
			}
			if(scriptsToolbar && button.idCommand >= ID_SCRIPT_BASE + 1 && button.idCommand <= ID_SCRIPT_BASE + SCRIPT_COMMAND_COUNT)
			{
				const int scriptId = button.idCommand - ID_SCRIPT_BASE;
				for(int scriptIndex = 0; scriptIndex < m_scripts.GetSize(); ++scriptIndex)
					if(!m_scripts[scriptIndex].isFolder && m_scripts[scriptIndex].wID == scriptId)
					{
						xml.AppendFormat(L"    <Script path=\"%s\" />\r\n", XmlEscape(m_scripts[scriptIndex].relativePath));
						break;
					}
			}
			else if(button.idCommand != 0)
				xml.AppendFormat(L"    <Command id=\"%d\" />\r\n", button.idCommand);
		}
		xml.Append(L"  </Toolbar>\r\n");
	};
	appendToolbar(L"Command", m_CmdToolbar, false);
	appendToolbar(L"Scripts", m_ScriptsToolbar, true);
	if(m_last_script != NULL && !m_last_script->relativePath.IsEmpty())
		xml.AppendFormat(L"  <LastScript path=\"%s\" />\r\n", XmlEscape(m_last_script->relativePath));
	xml.Append(L"</Toolbars>\r\n");
	WritePortableToolbarsText(xml);
}

static void SubclassBox(HWND hWnd, RECT& rc, const int pos, CComboBox& box, DWORD dwStyle, CCustomEdit& custedit, const int resID, HFONT& hFont)
{
	  ::SendMessage(hWnd, TB_GETITEMRECT, pos, (LPARAM)&rc);
	  rc.bottom--;
	  box.Create(hWnd, rc, NULL, dwStyle, WS_EX_CLIENTEDGE, resID);
	  box.SetFont(hFont);
	  custedit.SubclassWindow(box.ChildWindowFromPoint(CPoint(3,3)));
}

void CMainFrame::AddStaticText(CCustomStatic &st, HWND toolbarHwnd, int id, const TCHAR *text, HFONT hFont)
{
	RECT rect;
	SendMessage(toolbarHwnd, TB_GETITEMRECT, id, (LPARAM)&rect);  
	rect.bottom--; 

	st.Create(toolbarHwnd, rect, NULL, WS_CHILD|WS_VISIBLE, WS_EX_TRANSPARENT, IDC_ID);
	st.SetFont(hFont);
	st.SetWindowText(text);
	st.SetEnabled(true);
}

void CMainFrame::InitPluginsType(HMENU hMenu, const TCHAR* type, UINT cmdbase, CSimpleArray<CLSID>& plist)
{
	CRegKey rk;
	AddBundledPluginCatalog(hMenu, type, cmdbase, plist);
	int ncmd = plist.GetSize();
	// A portable copy must be self-contained: bundled entries above are loaded
	// from Plugins\\plugins.json, but legacy per-user registrations belong only
	// to an installed copy on this Windows profile.
	if(!DeploymentContext::RegistryPersistenceAllowed())
	{
		if(ncmd > 0) ::RemoveMenu(hMenu, 0, MF_BYPOSITION);
		return;
	}
	if(rk.Open(HKEY_CURRENT_USER, _Settings.GetKeyPath() + L"\\Plugins") != ERROR_SUCCESS)
	{
		if(ncmd > 0) ::RemoveMenu(hMenu, 0, MF_BYPOSITION);
		return;
	}
	for(int i = 0; ncmd < 20; ++i)
	{
		CString name;
		DWORD size = 128; // enough for GUIDs
		TCHAR* cp = name.GetBuffer(size);
		FILETIME ft;
		if(::RegEnumKeyEx(rk, i, cp, &size, 0, 0, 0, &ft) != ERROR_SUCCESS)
			break;
		name.ReleaseBuffer(size);
		CRegKey pk;
		if(pk.Open(rk, name) != ERROR_SUCCESS)
			continue;
		CString pt(U::QuerySV(pk, L"Type"));
		CString ms(U::QuerySV(pk, L"Menu"));
		if(pt.IsEmpty() || ms.IsEmpty() || pt != type)
			continue;
		ms = LocalizeBundledPluginMenuText(name, ms);
		CLSID clsid;
		if(::CLSIDFromString((TCHAR*)(const TCHAR *)name, &clsid) != NOERROR)
			continue;
		bool alreadyBundled = false;
		for(int existing = 0; existing < plist.GetSize(); ++existing)
			if(::InlineIsEqualGUID(plist[existing], clsid)) { alreadyBundled = true; break; }
		if(alreadyBundled) continue;

		// all checks pass, add to menu and remember clsid
		plist.Add(clsid);
		::AppendMenu(hMenu, MF_STRING, cmdbase + ncmd, ms);
		CString hs = ms;
		hs.Remove(L'&');
		InitPluginHotkey(name, cmdbase + ncmd, pt + CString(L" | ") + hs);
		// check if an icon is available
		CString icon(U::QuerySV(pk, L"Icon"));
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
				m_MenuBar.AddIcon(hIcon, cmdbase + ncmd);
				::DestroyIcon(hIcon);
			}
		}
		++ncmd;
	}

	// Не подхватываем legacy Haali/FBE plugins из HKLM: Next использует только
	// собственную per-user ветку, чтобы не смешивать две установленные версии.
	if(ncmd > 0) // delete placeholder from menu
	::RemoveMenu(hMenu, 0, MF_BYPOSITION);
}

void CMainFrame::InitPlugins()
{
	ReleaseScriptResources();
	if (StartupTrace::Enabled())
	{
		StartupTrace::Event(L"plugin", L"P100", L"script directory resolved");
	}
	CollectScripts(_Settings.GetScriptsFolder(), L"*.js", 1, L"0");	
	if (StartupTrace::Enabled())
	{
		CString trace;
		trace.Format(L"script-count=%d", m_scripts.GetSize());
		StartupTrace::Event(L"plugin", L"P110", trace);
	}
	StartupTrace::Event(L"plugin", L"P120", L"scripts collected");
	SortScripts();
	AssignScriptCommandIds();
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
	if (DeploymentContext::RegistryPersistenceAllowed())
		m_mru.ReadFromRegistry(_Settings.GetKeyPath());
	else
		ReadPortableMru(m_mru);
	m_mru.SetMaxEntries(m_mru.m_nMaxEntries_Max - 1);
	StartupTrace::Event(L"plugin", L"P160", L"MRU initialized");

	// Scripts
	HMENU ManMenu = m_MenuBar.GetMenu();
	HMENU scripts = GetSubMenu(ManMenu, 6);

	while(::GetMenuItemCount(scripts) > 0)
	::RemoveMenu(scripts, 0, MF_BYPOSITION);

	if(m_scripts.GetSize())
	{
		int nextFolderMenuId = 0;
		AddScriptsSubMenu(scripts, L"0", m_scripts, nextFolderMenuId);
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
  StartupTrace::Event(L"mainframe", L"M100", L"OnCreate started");
  StartupTrace::Event(L"settings", L"G100", L"application settings applied");
	UiMetrics::UpdateForWindow(m_hWnd);
  m_ctrl_tab = false;

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

	m_CmdToolbar = CreateCommandToolbarCtrl(m_hWnd, m_commandToolbarImages, IDR_MAINFRAME,
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
	for (size_t index = 0; index < _countof(kTableToolbarCommands); ++index)
	{
    m_table_toolbar_image_indices[index] = -1;
  }
  for (size_t index = 0; index < _countof(kTableToolbarCommands); ++index)
  {
    const TableToolbarCommand& command = kTableToolbarCommands[index];
    const int imageIndex = AddToolbarBitmapFromModule(m_CmdToolbar, applicationModule, command.bitmapResourceId);
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
  for (size_t index = 0; index < _countof(kTableToolbarCommands); ++index)
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
  UIAddToolBar(m_ScriptsToolbar);

	m_hWndLinksBar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST, 0, 0, 100, 100,
	  m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
   
	m_hWndTableBar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST , 0, 0, 100, 100,
	  m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
	m_hWndTableBar2 = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST, 0, 0, 100, 100,
	  m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
	SetDialogFontForToolbarRow(m_hWndLinksBar);
	SetDialogFontForToolbarRow(m_hWndTableBar);
	SetDialogFontForToolbarRow(m_hWndTableBar2);
	HWND hWndLinksBar = m_hWndLinksBar;
	HWND hWndTableBar = m_hWndTableBar;
	HWND hWndTableBar2 = m_hWndTableBar2;
  
  wchar_t buf[MAX_LOAD_STRING + 1];
  HFONT hFont = (HFONT)::SendMessage(hWndLinksBar, WM_GETFONT, 0, 0);

  // Links toolbar preparation
  ::SendMessage(hWndLinksBar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
  // Next line provides empty drawing of text
  ::SendMessage(hWndLinksBar,TB_SETDRAWTEXTFLAGS, (WPARAM)DT_CALCRECT, (LPARAM)DT_CALCRECT);

  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_ID, buf, MAX_LOAD_STRING);
  AddTbButton(hWndLinksBar, buf);
  AddStaticText(m_id_caption, hWndLinksBar, 0, buf, hFont);
  AddTbButton(hWndLinksBar, L"123456789012345678901234567890");

  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_HREF, buf, MAX_LOAD_STRING);
  AddTbButton(hWndLinksBar, buf);
  AddStaticText(m_href_caption,	hWndLinksBar, 2, buf, hFont);
  AddTbButton(hWndLinksBar, L"123456789012345678901234567890");

  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_SECTION_ID, buf, MAX_LOAD_STRING);
  AddTbButton(hWndLinksBar, buf);
  AddStaticText(m_section_id_caption, hWndLinksBar, 4, buf, hFont);
  AddTbButton(hWndLinksBar, L"123456789012345678901234567890");

  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_IMAGE_TITLE, buf, MAX_LOAD_STRING);
  AddTbButton(hWndLinksBar, buf);
  AddStaticText(m_image_title_caption, hWndLinksBar, 6, buf, hFont);
  AddTbButton(hWndLinksBar, L"123456789012345678901234567890");

  // Table's first toolbar preparation
  ::SendMessage(hWndTableBar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
  ::SendMessage(hWndTableBar, TB_SETDRAWTEXTFLAGS, (WPARAM)DT_CALCRECT, (LPARAM)DT_CALCRECT);

  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_TABLE_ID, buf, MAX_LOAD_STRING);
  AddTbButton(hWndTableBar, buf);
  AddStaticText(m_table_id_caption,	hWndTableBar, 0, buf, hFont);
  AddTbButton(hWndTableBar, L"12345678901234567890");

  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_TABLE_STYLE, buf, MAX_LOAD_STRING);
  AddTbButton(hWndTableBar, buf);
  AddStaticText(m_table_style_caption, hWndTableBar, 2, buf, hFont);
  AddTbButton(hWndTableBar, L"123456789012345");

  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_ID, buf, MAX_LOAD_STRING);
  AddTbButton(hWndTableBar, buf);
  AddStaticText(m_id_table_caption,	hWndTableBar, 4, buf, hFont);
  AddTbButton(hWndTableBar, L"12345678901234567890");
  
  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_STYLE, buf, MAX_LOAD_STRING);
  AddTbButton(hWndTableBar, buf);
  AddStaticText(m_style_caption, hWndTableBar, 6, buf, hFont);
  AddTbButton(hWndTableBar, L"123456789012345");

  // Table's second toolbar preparation
  ::SendMessage(hWndTableBar2, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON),0);
  ::SendMessage(hWndTableBar2, TB_SETDRAWTEXTFLAGS, (WPARAM)DT_CALCRECT, (LPARAM)DT_CALCRECT);

  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_COLSPAN, buf, MAX_LOAD_STRING);
  AddTbButton(hWndTableBar2, buf);
  AddStaticText(m_colspan_caption, hWndTableBar2, 0, buf, hFont);
  AddTbButton(hWndTableBar2, L"12345");

  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_ROWSPAN, buf, MAX_LOAD_STRING);
  AddTbButton(hWndTableBar2, buf);
  AddStaticText(m_rowspan_caption, hWndTableBar2, 2, buf, hFont);
  AddTbButton(hWndTableBar2, L"12345");

  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_TR_ALIGN, buf, MAX_LOAD_STRING);
  AddTbButton(hWndTableBar2, buf);
  AddStaticText(m_tr_allign_caption, hWndTableBar2, 4, buf, hFont);
  AddTbButton(hWndTableBar2, L"12345678");

  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_TD_ALIGN, buf, MAX_LOAD_STRING);
  AddTbButton(hWndTableBar2, buf);
  AddStaticText(m_th_allign_caption, hWndTableBar2, 6, buf, hFont);
  AddTbButton(hWndTableBar2, L"12345678");

  FbeLoadString(_Module.GetResourceInstance(), IDS_TB_CAPT_TD_VALIGN, buf, MAX_LOAD_STRING);
  AddTbButton(hWndTableBar2, buf);
  AddStaticText(m_valign_caption, hWndTableBar2, 8, buf, hFont);
  AddTbButton(hWndTableBar2, L"12345678");

  CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AutoSizeToolbar(m_CmdToolbar);
	AutoSizeToolbar(m_ScriptsToolbar);
	AutoSizeToolbar(hWndLinksBar);
	AutoSizeToolbar(hWndTableBar);
	AutoSizeToolbar(hWndTableBar2);
  
  AddSimpleReBarBand(hWndCmdBar, 0, TRUE, 0);
  AddSimpleReBarBand(m_CmdToolbar, 0, TRUE, 0, FALSE);
  AddSimpleReBarBand(m_ScriptsToolbar, 0, TRUE, 0, FALSE);
  AddSimpleReBarBand(hWndLinksBar, 0, TRUE, 0, TRUE);
  AddSimpleReBarBand(hWndTableBar, 0, TRUE, 0, TRUE) ;
  AddSimpleReBarBand(hWndTableBar2, 0, TRUE, 0, TRUE);
  m_rebar = m_hWndToolBar;
	m_rebar.SendMessage(WM_SIZE);
  StartupTrace::Event(L"mainframe", L"M110", L"menus and toolbars created");

  // add editor controls  
  RECT rc;    
  
  // m_id_caption.SetParent(this->m_hWnd);

  /*HDC hdc = ::GetDC(hWndLinksBar);
  COLORREF bkCollor = GetBkColor(hdc);*/
  HDC hdc1 = ::GetDC(m_id_caption);
  SetBkColor(hdc1, RGB(0,0,0));
  //ReleaseDC(hdc);
  ReleaseDC(hdc1);

  DWORD CBS_COMMON_STYLE =  WS_CHILD | WS_VISIBLE | CBS_AUTOHSCROLL;

  SubclassBox(hWndLinksBar, rc, 1, m_id_box, CBS_COMMON_STYLE, m_id, IDC_ID, hFont);
  SubclassBox(hWndLinksBar, rc, 3, m_href_box, CBS_COMMON_STYLE | WS_VSCROLL | CBS_DROPDOWN | CBS_SORT, m_href, IDC_HREF, hFont);
  SubclassBox(hWndLinksBar, rc, 5, m_section_box, CBS_COMMON_STYLE, m_section, IDC_SECTION, hFont);
  SubclassBox(hWndLinksBar, rc, 7, m_image_title_box, CBS_COMMON_STYLE, m_image_title, IDC_IMAGE_TITLE, hFont);
  
  // add editor-table controls
  HFONT hFontT = (HFONT)::SendMessage(hWndTableBar, WM_GETFONT, 0, 0);
  RECT rcT;

  SubclassBox(hWndTableBar, rcT, 1, m_id_table_id_box, CBS_COMMON_STYLE, m_id_table_id, IDC_IDT, hFontT);
  SubclassBox(hWndTableBar, rcT, 3, m_styleT_table_box, CBS_COMMON_STYLE, m_styleT_table, IDC_STYLET, hFontT);
  SubclassBox(hWndTableBar, rcT, 5, m_id_table_box, CBS_COMMON_STYLE, m_id_table, IDC_ID, hFontT);
  SubclassBox(hWndTableBar, rcT, 7, m_style_table_box, CBS_COMMON_STYLE, m_style_table, IDC_STYLE, hFontT);

  SubclassBox(hWndTableBar2, rcT, 1, m_colspan_table_box, CBS_COMMON_STYLE, m_colspan_table, IDC_COLSPAN, hFontT);
  SubclassBox(hWndTableBar2, rcT, 3, m_rowspan_table_box, CBS_COMMON_STYLE, m_rowspan_table, IDC_ROWSPAN, hFontT);
  SubclassBox(hWndTableBar2, rcT, 5, m_alignTR_table_box, CBS_COMMON_STYLE | WS_VSCROLL | CBS_DROPDOWNLIST, m_alignTR_table, IDC_ALIGNTR, hFontT);
  SubclassBox(hWndTableBar2, rcT, 7, m_align_table_box, CBS_COMMON_STYLE | WS_VSCROLL | CBS_DROPDOWNLIST, m_align_table, IDC_ALIGN, hFontT);
  SubclassBox(hWndTableBar2, rcT, 9, m_valign_table_box, CBS_COMMON_STYLE | WS_VSCROLL | CBS_DROPDOWNLIST, m_valign_table, IDC_VALIGN, hFontT);

  m_align_table_box.InsertString(0,_T(""));
  m_align_table_box.InsertString(1,_T("left"));
  m_align_table_box.InsertString(2,_T("right"));
  m_align_table_box.InsertString(3,_T("center"));

  m_alignTR_table_box.InsertString(0,_T(""));
  m_alignTR_table_box.InsertString(1,_T("left"));
  m_alignTR_table_box.InsertString(2,_T("right"));
  m_alignTR_table_box.InsertString(3,_T("center"));

  m_valign_table_box.InsertString(0,_T(""));
  m_valign_table_box.InsertString(1,_T("top"));
  m_valign_table_box.InsertString(2,_T("middle"));
  m_valign_table_box.InsertString(3,_T("bottom"));

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

  // create splitter contents
//  m_document_tree.Create(m_splitter);
//  m_document_tree.SetTitle(L"Document Tree");
  m_view.Create(m_splitter,rcDefault,NULL,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN);

  // create a tree
  /*m_dummy_pane.Create(m_document_tree,rcDefault,NULL,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,WS_EX_CLIENTEDGE);
  m_document_tree.SetClient(m_dummy_pane);
  m_document_tree.Create(m_dummy_pane, rcDefault);
  m_document_tree.SetBkColor(::GetSysColor(COLOR_WINDOW));
  m_dummy_pane.SetSplitterPane(0,m_document_tree);
  m_dummy_pane.SetSinglePaneMode(SPLIT_PANE_LEFT);*/

  // create a source view
  m_source.Create(_T("Scintilla"),m_view,rcDefault,NULL,WS_CHILD|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,0);
	// Scintilla's built-in popup is English-only. Replace it with the runtime-localized menu below.
	m_source.SendMessage(SCI_USEPOPUP, SC_POPUP_NEVER);
	::SetProp(m_source, L"FBE.Next.SourceContextMenuOwner", reinterpret_cast<HANDLE>(this));
	m_source_window_proc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(m_source, GWLP_WNDPROC,
		reinterpret_cast<LONG_PTR>(&CMainFrame::SourceEditorWindowProc)));
  m_view.AttachWnd(m_source);
  SetupSci();
  SetSciStyles();
  StartupTrace::Event(L"mainframe", L"M120", L"editor controls created");

  // initialize a new blank document
  m_doc=new FB::Doc(*this);
  FB::Doc::m_active_doc = m_doc;
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

    if (m_doc->Load(m_view,startupFileName))
	{
      start_with_params = true;
	  m_file_age = FileAge(startupFileName);
	}
    else
	{
		// added by SeNS: create blank document, and load incorrect XML to Scintilla
		delete m_doc;
		m_doc=new FB::Doc(*this);
		FB::Doc::m_active_doc = m_doc;
		m_doc->CreateBlank(m_view);
		m_file_age = ~0;
		m_bad_xml = true;
	}
  } else 
  {
	m_doc->CreateBlank(m_view);
	m_file_age = ~0;
  }

  StartupTrace::Event(L"mainframe", L"M130", L"document content created");

  if (_Settings.FastMode()) {
		m_doc->SetFastMode(true);
		UISetCheck(ID_VIEW_FASTMODE, TRUE);
  } else
    m_doc->SetFastMode(false);

  AttachDocument(m_doc);
  StartupTrace::Event(L"mainframe", L"M140", L"document attached");
  UISetCheck(ID_VIEW_BODY,1);

  m_document_tree.Create(m_splitter);
  StartupTrace::Event(L"mainframe", L"M150", L"document tree initialized");
  
  if (AU::_ARGS.start_in_desc_mode) 
	ShowView(DESC);

  // init plugins&MRU list
  InitPlugins();  
  StartupTrace::Event(L"mainframe", L"M160", L"plugins and MRU initialized");

  // setup splitter
  m_splitter.SetSplitterPanes(m_document_tree, m_view);

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

	// register object for message filtering and idle updates
  CMessageLoop* pLoop = _Module.GetMessageLoop();
  ATLASSERT(pLoop != NULL);
  pLoop->AddMessageFilter(this);
  pLoop->AddIdleHandler(this);

  // accept dropped files
  ::DragAcceptFiles(*this,TRUE);

  // Modification by Pilgrim
  BOOL bVisible = _Settings.ViewDocumentTree();
  m_document_tree.ShowWindow(bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
  UISetCheck(ID_VIEW_TREE, bVisible);
  m_splitter.SetSinglePaneMode(bVisible ? SPLIT_PANE_NONE : SPLIT_PANE_RIGHT);

  if(start_with_params)
  {
	  m_mru.AddToList(startupFileName);
  	  if(_Settings.RestoreFilePosition())
	  {
			m_restore_pos_cmdline = true;
	  }
  }

  // Change keyboard layout
  if (_Settings.GetChangeKeybLayout())
  {
	  CString layout = _Settings.GetKeyboardLayoutId();
	  if(layout.IsEmpty()) layout = ResolveLegacyKeyboardLayoutId(_Settings.GetKeybLayout()).c_str();
	  if(!layout.IsEmpty() && !LoadKeyboardLayout(layout, KLF_ACTIVATE))
		  StartupTrace::Warning(L"startup", L"ST126", L"Configured keyboard layout could not be loaded.");
	}
  
  // added by SeNS: create blank document, and load incorrect XML to Scintilla
  if (m_bad_xml)
	if (!LoadToScintilla(startupFileName)) return -1;

  // Added by SeNS
  if (m_Speller && m_Speller->Enabled())
  {
	if (!m_Speller->Available())
		UIEnable(ID_TOOLS_SPELLCHECK, false, true);
	else
		UIEnable(ID_TOOLS_SPELLCHECK, true, true);
	m_Speller->SetHighlightMisspells(_Settings.GetHighlightMisspells());
  }
  else UIEnable(ID_TOOLS_SPELLCHECK, false, true);

	// Restore scripts toolbar layout and position
	if (DeploymentContext::RegistryPersistenceAllowed())
		m_ScriptsToolbar.RestoreState(HKEY_CURRENT_USER, _Settings.GetKeyPath() + L"\\Toolbars", L"ScriptsToolbar");
	else
		RestorePortableToolbarLayout(m_ScriptsToolbar, true);

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

  m_need_title_update = true;
  StartupTrace::Event(L"mainframe", L"M199", L"OnCreate completed");
  return 0;
}

LRESULT CMainFrame::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if(m_source_window_proc != NULL && ::IsWindow(m_source))
	{
		::SetWindowLongPtr(m_source, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_source_window_proc));
		::RemoveProp(m_source, L"FBE.Next.SourceContextMenuOwner");
		m_source_window_proc = NULL;
	}
  KillTimer(RECOVERY_TIMER_ID);
  DestroyAcceleratorTable(m_hAccel);
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

LRESULT CALLBACK CMainFrame::SourceEditorWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	CMainFrame* frame = reinterpret_cast<CMainFrame*>(::GetProp(window, L"FBE.Next.SourceContextMenuOwner"));
	if(frame != NULL && message == WM_CONTEXTMENU)
	{
		frame->ShowSourceContextMenu(lParam);
		return 0;
	}
	return frame != NULL && frame->m_source_window_proc != NULL ?
		::CallWindowProc(frame->m_source_window_proc, window, message, wParam, lParam) :
		::DefWindowProc(window, message, wParam, lParam);
}

void CMainFrame::ShowSourceContextMenu(LPARAM screenPosition)
{
	enum SourceContextCommand { SOURCE_CONTEXT_UNDO = 1, SOURCE_CONTEXT_REDO, SOURCE_CONTEXT_CUT,
		SOURCE_CONTEXT_COPY, SOURCE_CONTEXT_PASTE };
	CPoint point = CPoint(screenPosition);
	if(point.x == -1 && point.y == -1)
	{
		const sptr_t position = m_source.SendMessage(SCI_GETCURRENTPOS);
		point.x = m_source.SendMessage(SCI_POINTXFROMPOSITION, 0, position);
		point.y = m_source.SendMessage(SCI_POINTYFROMPOSITION, 0, position);
		m_source.ClientToScreen(&point);
	}
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING | (m_source.SendMessage(SCI_CANUNDO) ? MF_ENABLED : MF_GRAYED), SOURCE_CONTEXT_UNDO,
		FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.edit.undo", L"Undo"));
	menu.AppendMenu(MF_STRING | (m_source.SendMessage(SCI_CANREDO) ? MF_ENABLED : MF_GRAYED), SOURCE_CONTEXT_REDO,
		FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.edit.redo", L"Redo"));
	menu.AppendMenu(MF_SEPARATOR);
	const bool hasSelection = m_source.SendMessage(SCI_GETSELECTIONSTART) != m_source.SendMessage(SCI_GETSELECTIONEND);
	menu.AppendMenu(MF_STRING | (hasSelection ? MF_ENABLED : MF_GRAYED), SOURCE_CONTEXT_CUT,
		FbeLoadRuntimeStringByKey(L"fbe.context.cut", L"Cut"));
	menu.AppendMenu(MF_STRING | (hasSelection ? MF_ENABLED : MF_GRAYED), SOURCE_CONTEXT_COPY,
		FbeLoadRuntimeStringByKey(L"fbe.context.copy", L"Copy"));
	menu.AppendMenu(MF_STRING | (m_source.SendMessage(SCI_CANPASTE) ? MF_ENABLED : MF_GRAYED), SOURCE_CONTEXT_PASTE,
		FbeLoadRuntimeStringByKey(L"fbe.context.paste", L"Paste"));
	const UINT command = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
		point.x, point.y, m_hWnd);
	switch(command)
	{
	case SOURCE_CONTEXT_UNDO: m_source.SendMessage(SCI_UNDO); break;
	case SOURCE_CONTEXT_REDO: m_source.SendMessage(SCI_REDO); break;
	case SOURCE_CONTEXT_CUT: m_source.SendMessage(SCI_CUT); break;
	case SOURCE_CONTEXT_COPY: m_source.SendMessage(SCI_COPY); break;
	case SOURCE_CONTEXT_PASTE: m_source.SendMessage(SCI_PASTE); break;
	}
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
    DeleteRecoveryFile();
	// added by SeNS
	if (m_Speller) 
	{
		m_Speller->EndDocumentCheck();
		m_Speller->SetEnabled(false);
	}
	_Settings.SetViewStatusBar(m_status.IsWindowVisible() != 0);
	//_Settings.SetViewDocumentTree(IsSourceActive() ? m_document_tree.IsWindowVisible()==0 : !m_save_sp_mode);
    _Settings.SetSplitterPos(m_splitter.GetSplitterPos());	
    WINDOWPLACEMENT wpl;
    wpl.length=sizeof(wpl);
    GetWindowPlacement(&wpl);
	_Settings.SetWindowPosition(wpl);
	if (DeploymentContext::RegistryPersistenceAllowed())
		m_mru.WriteToRegistry(_Settings.GetKeyPath());
	else
		WritePortableMru(m_mru);
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

CString CMainFrame::GetRecoveryFileName()
{
	CString directory(DeploymentContext::RecoveryDirectory().c_str());
	::CreateDirectory(directory, NULL);
	return directory + L"Recovery.fb2";
}

void CMainFrame::DeleteRecoveryFile()
{
	if (!m_recovery_written)
		return;

	::DeleteFile(GetRecoveryFileName());
	m_recovery_written = false;
}

bool CMainFrame::SaveSourceRecoveryCopy(const CString& filename)
{
	CString temporaryFile;
	HANDLE file = INVALID_HANDLE_VALUE;
	StartupTrace::Event(L"recovery", L"R100", L"source recovery started");
	try
	{
		CString directory(filename);
		const int separator = directory.ReverseFind(L'\\');
		if (separator < 0)
			directory = L".\\";
		else
			directory.Delete(separator, directory.GetLength() - separator);

		wchar_t temporaryBuffer[MAX_PATH] = {};
		if (::GetTempFileName(directory, L"fbs", 0, temporaryBuffer) == 0)
			return false;
		temporaryFile = temporaryBuffer;

		const LRESULT textLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> text(static_cast<size_t>(textLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, textLength + 1, reinterpret_cast<LPARAM>(text.data()));

		file = ::CreateFile(temporaryFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE)
			throw ::GetLastError();

		DWORD written = 0;
		if (textLength > 0 && (!::WriteFile(file, text.data(), static_cast<DWORD>(textLength),
			&written, NULL) || written != static_cast<DWORD>(textLength)))
			throw ::GetLastError();
		if (!::FlushFileBuffers(file))
			throw ::GetLastError();
		::CloseHandle(file);
		file = INVALID_HANDLE_VALUE;

		if (!::MoveFileEx(temporaryFile, filename,
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			throw ::GetLastError();

		StartupTrace::Event(L"recovery", L"R110", L"source recovery completed");
		return true;
	}
	catch (...)
	{
		if (file != INVALID_HANDLE_VALUE)
			::CloseHandle(file);
		if (!temporaryFile.IsEmpty())
			::DeleteFile(temporaryFile);
		StartupTrace::Error(L"recovery", L"R120", L"source recovery failed");
		return false;
	}
}

bool CMainFrame::SaveRecoveryNow()
{
	if (!m_doc || !DocChanged())
		return false;

	const CString recoveryFile(GetRecoveryFileName());
	if (!m_recovery_written && ::GetFileAttributes(recoveryFile) != INVALID_FILE_ATTRIBUTES)
		return false;

	const bool saved = IsSourceActive()
		? SaveSourceRecoveryCopy(recoveryFile)
		: (!m_bad_xml && m_doc->SaveRecoveryCopy(recoveryFile));
	if (!saved && m_bad_xml)
		StartupTrace::Warning(L"recovery", L"R130", L"source recovery skipped because source XML is invalid");
	m_recovery_written = saved || m_recovery_written;
	return saved;
}
void CMainFrame::TryRestoreRecovery()
{
	if (_ARGV.GetSize() > 0)
		return;

	const CString recoveryFile(GetRecoveryFileName());
	const DWORD attributes = ::GetFileAttributes(recoveryFile);
	if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
		return;

	if (U::MessageBox(MB_YESNO | MB_ICONQUESTION, IDS_RECOVERY_CAPTION, IDS_RECOVERY_MSG) != IDYES)
		return;

	if (LoadFile(recoveryFile) == OK)
	{
		m_doc->m_filename = L"Untitled.fb2";
		m_doc->m_namevalid = false;
		m_doc->ResetSavePoint();
		if (m_bad_xml)
			m_bad_filename = L"Untitled.fb2";
		::DeleteFile(recoveryFile);
		m_recovery_written = false;
	}
}

LRESULT CMainFrame::OnSettingChange(UINT, WPARAM, LPARAM, BOOL&)
{
	UiMetrics::UpdateForWindow(m_hWnd);
	if (::IsWindow(m_MenuBar)) { ::SendMessage(m_MenuBar, WM_SETFONT, reinterpret_cast<WPARAM>(UiMetrics::MenuFont()), TRUE); m_MenuBar.AutoSize(); }
	if (::IsWindow(m_CmdToolbar)) { SetDialogFontForToolbarRow(m_CmdToolbar); AutoSizeToolbar(m_CmdToolbar); }
	if (::IsWindow(m_ScriptsToolbar)) { SetDialogFontForToolbarRow(m_ScriptsToolbar); AutoSizeToolbar(m_ScriptsToolbar); }
	SetDialogFontForToolbarRow(m_hWndLinksBar, true);
	SetDialogFontForToolbarRow(m_hWndTableBar, true);
	SetDialogFontForToolbarRow(m_hWndTableBar2, true);
	AutoSizeToolbar(m_hWndLinksBar);
	AutoSizeToolbar(m_hWndTableBar);
	AutoSizeToolbar(m_hWndTableBar2);
	if (::IsWindow(m_rebar)) m_rebar.SendMessage(WM_SIZE);
	if (::IsWindow(m_hWndStatusBar)) m_status.SetFont(UiMetrics::DialogFont());
	if (m_doc)
		m_doc->ApplyConfChanges();
	if (m_source.IsWindow())
	{
		SetupSci();
		SetSciStyles();
		UpdateSourceLineNumberMargin(true);
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
	SetDialogFontForToolbarRow(m_hWndLinksBar, true);
	SetDialogFontForToolbarRow(m_hWndTableBar, true);
	SetDialogFontForToolbarRow(m_hWndTableBar2, true);
	AutoSizeToolbar(m_hWndLinksBar);
	AutoSizeToolbar(m_hWndTableBar);
	AutoSizeToolbar(m_hWndTableBar2);
	if (::IsWindow(m_hWndStatusBar)) m_status.SetFont(UiMetrics::DialogFont());
	if(m_source.IsWindow())
	{
		SetupSci();
		SetSciStyles();
		UpdateSourceLineNumberMargin(true);
	}
	if (splitterPosition >= 0)
		m_splitter.SetSplitterPos(MulDiv(splitterPosition, newDpi, oldDpi));

	m_rebar.SendMessage(WM_SIZE);
	m_status.SendMessage(WM_SIZE);
	UpdateLayout();
	UpdateStatusBarLayout();
	RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
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
	TryRestoreRecovery();
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
	if (!AU::_ARGS.source_memory_benchmark_path.IsEmpty())
		PostMessage(AU::WM_SOURCE_MEMORY_BENCHMARK);
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
	const bool scriptsReload = IsFbeTestScenario(L"portable-scripts-reload");
	const bool legacyHotkeyRead = IsFbeTestScenario(L"portable-legacy-hotkey-read");
	if (!ordinaryWrite && !ordinaryRead && !emptyToolbarWrite && !emptyToolbarRead && !toolbarLayoutWrite && !toolbarLayoutRead && !scriptsReload && !legacyHotkeyRead)
		return;

	const CString diagnosticsDirectory(DeploymentContext::DiagnosticsDirectory().c_str());
	const CString scriptsDirectory(DeploymentContext::UserScriptsDirectory().c_str());
	const CString diagnosticsMarker(diagnosticsDirectory + L"portable-state-sentinel.txt");
	const CString recoveryMarker(GetRecoveryFileName());
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

	::CreateDirectory(scriptsDirectory, NULL);
	auto catalogButton = [&](HWND toolbar, int ordinal, TBBUTTON& button) -> bool
	{
		const int catalogIndex = m_aButtons.FindKey(static_cast<int>(reinterpret_cast<INT_PTR>(toolbar)));
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
		const int catalogIndex = m_aButtons.FindKey(static_cast<int>(reinterpret_cast<INT_PTR>(toolbar)));
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
	else if (toolbarLayoutWrite || toolbarLayoutRead)
	{
		TBBUTTON commandFirst = {}, commandAdded = {}, scriptButton = {}, lastScriptButton = {}, separator = {};
		separator.iBitmap = 13;
		separator.fsStyle = TBSTYLE_SEP;
		const bool commandCatalogReady = catalogButton(m_CmdToolbar, 0, commandFirst) &&
			catalogButton(m_CmdToolbar, 2, commandAdded);
		int scriptCommand = 0;
		for(int index = 0; index < m_scripts.GetSize(); ++index)
			if(!m_scripts[index].isFolder && m_scripts[index].relativePath == L"foo.js")
			{
				scriptCommand = ID_SCRIPT_BASE + m_scripts[index].wID;
				break;
			}
		const bool scriptsCatalogReady = scriptCommand != 0 &&
			catalogButtonByCommand(m_ScriptsToolbar, scriptCommand, scriptButton) &&
			catalogButtonByCommand(m_ScriptsToolbar, ID_LAST_SCRIPT, lastScriptButton);
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
			m_ScriptsToolbar.AddButton(&scriptButton);
			m_ScriptsToolbar.AddButton(&separator);
			m_ScriptsToolbar.AddButton(&lastScriptButton);
			m_CmdToolbar.AutoSize();
			m_ScriptsToolbar.AutoSize();
		}
		const bool commandLayout = catalogReady && hasButtons(m_CmdToolbar, commandAdded, separator, commandFirst);
		const bool scriptsLayout = catalogReady && hasButtons(m_ScriptsToolbar, scriptButton, separator, lastScriptButton);
		CStringA report;
		report.Format("phase=toolbar-layout-%s\nnonempty-command=%d\nnonempty-scripts=%d\nresult=%s\n",
			toolbarLayoutWrite ? "write" : "read", commandLayout, scriptsLayout,
			commandLayout && scriptsLayout ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (scriptsReload)
	{
		const DWORD before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
		InitPlugins();
		InitPlugins();
		InitPlugins();
		const DWORD after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
		CStringA report;
		report.Format("phase=scripts-reload\ngdi-before=%lu\ngdi-after=%lu\ngdi-stable=%d\nresult=%s\n",
			before, after, after <= before, after <= before ? "pass" : "fail");
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
		m_doc->m_body.AddImage(imagePath, true);
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
			for (size_t index = 0; index < _countof(kTableToolbarCommands); ++index)
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
			for (size_t index = 0; index < _countof(kTableToolbarCommands); ++index)
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
	m_source.SendMessage(SCI_SETWRAPMODE, SC_WRAP_NONE);
	m_source.SendMessage(SCI_COLOURISE, 0, -1);
	appendSnapshot("source-styled-wrap-none");
	FoldAll();
	appendSnapshot("fold-all");
	FoldAll();
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
		XmlMatchedTagsHighlighter tagMatchHighlighter(&m_source, &m_xml_matched_tags_state);
		tagMatchHighlighter.tagMatch(true, false, false);
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
	// Панели ссылок и таблиц используют toolbar-кнопки только как разметку:
	// поверх каждой из них находится CCustomStatic или combo-box. Простая
	// смена текста static-контрола оставляла старую ширину кнопки-разметки,
	// из-за чего подписи накладывались друг на друга после смены языка.
	// Пересобираем только эти пустые кнопки и заново размещаем уже созданные
	// дочерние контролы. Содержимое combo-box при этом не затрагивается.
	struct CaptionToolbarBinding
	{
		CCustomStatic* caption;
		CWindow* editor;
		CString text;
		LPCWSTR editorPlaceholder;
	};

	auto loadCaption = [](UINT resourceId) -> CString
	{
		wchar_t buffer[MAX_LOAD_STRING + 1] = {};
		return FbeLoadString(_Module.GetResourceInstance(), resourceId, buffer, MAX_LOAD_STRING)
			? CString(buffer) : CString();
	};

	auto rebuildCaptionToolbar = [this](HWND toolbar, CaptionToolbarBinding* bindings, size_t bindingCount)
	{
		if(!::IsWindow(toolbar))
			return;

		::SendMessage(toolbar, WM_SETREDRAW, FALSE, 0);
		for(int index = static_cast<int>(::SendMessage(toolbar, TB_BUTTONCOUNT, 0, 0)) - 1; index >= 0; --index)
			::SendMessage(toolbar, TB_DELETEBUTTON, index, 0);

		for(size_t index = 0; index < bindingCount; ++index)
		{
			AddTbButton(toolbar, bindings[index].text);
			AddTbButton(toolbar, bindings[index].editorPlaceholder);
		}

		::SendMessage(toolbar, TB_AUTOSIZE, 0, 0);
		for(size_t index = 0; index < bindingCount; ++index)
		{
			RECT captionRect = {};
			RECT editorRect = {};
			::SendMessage(toolbar, TB_GETITEMRECT, static_cast<WPARAM>(index * 2), reinterpret_cast<LPARAM>(&captionRect));
			::SendMessage(toolbar, TB_GETITEMRECT, static_cast<WPARAM>(index * 2 + 1), reinterpret_cast<LPARAM>(&editorRect));
			--captionRect.bottom;
			--editorRect.bottom;

			bindings[index].caption->SetWindowText(bindings[index].text);
			::SetWindowPos(bindings[index].caption->m_hWnd, NULL,
				captionRect.left, captionRect.top, captionRect.right - captionRect.left, captionRect.bottom - captionRect.top,
				SWP_NOACTIVATE | SWP_NOZORDER);
			::SetWindowPos(bindings[index].editor->m_hWnd, NULL,
				editorRect.left, editorRect.top, editorRect.right - editorRect.left, editorRect.bottom - editorRect.top,
				SWP_NOACTIVATE | SWP_NOZORDER);
		}

		::SendMessage(toolbar, WM_SETREDRAW, TRUE, 0);
		::RedrawWindow(toolbar, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
	};

	CaptionToolbarBinding linksToolbar[] = {
		{ &m_id_caption, &m_id_box, loadCaption(IDS_TB_CAPT_ID), L"123456789012345678901234567890" },
		{ &m_href_caption, &m_href_box, loadCaption(IDS_TB_CAPT_HREF), L"123456789012345678901234567890" },
		{ &m_section_id_caption, &m_section_box, loadCaption(IDS_TB_CAPT_SECTION_ID), L"123456789012345678901234567890" },
		{ &m_image_title_caption, &m_image_title_box, loadCaption(IDS_TB_CAPT_IMAGE_TITLE), L"123456789012345678901234567890" },
	};
	CaptionToolbarBinding tableToolbar[] = {
		{ &m_table_id_caption, &m_id_table_id_box, loadCaption(IDS_TB_CAPT_TABLE_ID), L"12345678901234567890" },
		{ &m_table_style_caption, &m_styleT_table_box, loadCaption(IDS_TB_CAPT_TABLE_STYLE), L"123456789012345" },
		{ &m_id_table_caption, &m_id_table_box, loadCaption(IDS_TB_CAPT_ID), L"12345678901234567890" },
		{ &m_style_caption, &m_style_table_box, loadCaption(IDS_TB_CAPT_STYLE), L"123456789012345" },
	};
	CaptionToolbarBinding tableToolbar2[] = {
		{ &m_colspan_caption, &m_colspan_table_box, loadCaption(IDS_TB_CAPT_COLSPAN), L"12345" },
		{ &m_rowspan_caption, &m_rowspan_table_box, loadCaption(IDS_TB_CAPT_ROWSPAN), L"12345" },
		{ &m_tr_allign_caption, &m_alignTR_table_box, loadCaption(IDS_TB_CAPT_TR_ALIGN), L"12345678" },
		{ &m_th_allign_caption, &m_align_table_box, loadCaption(IDS_TB_CAPT_TD_ALIGN), L"12345678" },
		{ &m_valign_caption, &m_valign_table_box, loadCaption(IDS_TB_CAPT_TD_VALIGN), L"12345678" },
	};

	rebuildCaptionToolbar(m_id_caption.GetParent(), linksToolbar, _countof(linksToolbar));
	rebuildCaptionToolbar(m_table_id_caption.GetParent(), tableToolbar, _countof(tableToolbar));
	rebuildCaptionToolbar(m_colspan_caption.GetParent(), tableToolbar2, _countof(tableToolbar2));

	wchar_t buf[MAX_LOAD_STRING + 1] = {};
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
		U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_END_MSG, m_view->m_fo.pattern);	
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
		U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_END_MSG, m_view->m_fo.pattern);	
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

LRESULT CMainFrame::OnUnhandledCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	HWND hFocus = ::GetFocus();
	UINT idCtl = HIWORD(wParam);

	// only pass messages to the editors
	if (idCtl == 0 || idCtl == 1)
	{
		if (
			hFocus == m_id || hFocus == m_href || hFocus == m_section || ::IsChild(m_id, hFocus)
			|| ::IsChild(m_href, hFocus) || ::IsChild(m_section, hFocus) || hFocus == m_styleT_table
			|| hFocus == m_id_table_id || hFocus == m_id_table || hFocus == m_style_table
			|| hFocus == m_colspan_table || hFocus == m_rowspan_table || hFocus == m_align_table
			|| hFocus == m_valign_table || hFocus == m_alignTR_table || hFocus==m_image_title
			|| ::IsChild(m_id_table_id,hFocus) || ::IsChild(m_id_table, hFocus)
			|| ::IsChild(m_style_table,hFocus) || ::IsChild(m_styleT_table, hFocus)
			|| ::IsChild(m_colspan_table,hFocus) ||::IsChild(m_rowspan_table, hFocus)
			|| ::IsChild(m_alignTR_table,hFocus) || ::IsChild(m_align_table, hFocus)
			|| ::IsChild(m_valign_table,hFocus)|| ::IsChild(m_image_title, hFocus)
			)
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

LRESULT CMainFrame::OnDropFiles(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
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
	  if (IsSupportedFictionBookFile(buf))
	  {
		if (LoadFile(buf)==OK)
			m_mru.AddToList(m_doc->m_filename);
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
	  if (IsSupportedFictionBookFile(url))
	  {
		if (LoadFile(url)==OK)
			m_mru.AddToList(m_doc->m_filename);
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

  FB::Doc *doc=new FB::Doc(*this);
  FB::Doc::m_active_doc = doc;
  doc->CreateBlank(m_view);
  m_file_age = ~0;
  AttachDocument(doc);
  delete m_doc;
  m_doc=doc;
  ResetStatusForDocument();

  return 0;
}

LRESULT CMainFrame::OnFileOpen(WORD, WORD, HWND, BOOL& bHandled)
{
  if (LoadFile()==OK)
  {
    m_mru.AddToList(m_doc->m_filename);
	if(_Settings.RestoreFilePosition())
	{
		int saved_pos = U::GetFileSelectedPos(m_doc->m_filename);
		GoTo(saved_pos);
	}
  }
  return 0;
}

LRESULT CMainFrame::OnFileOpenMRU(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CString filename;
	m_mru.GetFromList(wID, filename);

	switch(LoadFile(filename))
	{
		case OK:
			m_mru.MoveToTop(wID);
			// added by SeNS
			if(_Settings.RestoreFilePosition())
			{
				int saved_pos = U::GetFileSelectedPos(m_doc->m_filename);
				GoTo(saved_pos);
			}
			break;
		case FAIL:
			m_mru.RemoveFromList(wID);
			break;
		case CANCELLED:
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
			if (HasOnlySourceEditorConfigurationChanged(previousConfiguration, currentConfiguration))
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

      CComQIPtr<IFBEImportPlugin>	    ipl(unk);
      TracePluginDiagnostic(L"Import", pluginClsid, L"QueryInterface", ipl ? S_OK : E_NOINTERFACE, 0);

      IDispatchPtr  obj;
      _bstr_t	    filename;
      if (ipl) 
	  {
		m_last_plugin = wID + ID_EXPORT_BASE;
		BSTR	bs=NULL;
		HRESULT hr=ipl->Import((long)m_hWnd,&bs,&obj);
		TracePluginDiagnostic(L"Import", pluginClsid, L"Import", hr, obj ? 1 : 0);
		CheckError(hr);
		filename.Assign(bs);
		if (hr!=S_OK)
		return 0;
	  } 
	  else 
	  {
		U::MessageBox(MB_OK|MB_ICONERROR, IDS_IMPORT_ERR_CPT, IDS_IMPORT_ERR_MSG);
		return 0;
      }

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

			CComQIPtr<IFBEExportPlugin> epl(unk);
			TracePluginDiagnostic(L"Export", pluginClsid, L"QueryInterface", epl ? S_OK : E_NOINTERFACE, 0);

			if(epl)
			{
				m_last_plugin = wID + ID_EXPORT_BASE;
				// Export consumes the in-memory binary payload directly.  Compacting
				// it is only a Save-to-FB2 optimization and can strip MSXML's typed
				// binary representation before the export plugin receives it.
				MSXML2::IXMLDOMDocument2Ptr dom(m_doc->CreateDOM(m_doc->m_encoding, false));
				_bstr_t filename;
				if(m_doc->m_namevalid)
				{
					CString tmp(m_doc->m_filename);
					if(tmp.GetLength() >= 4 && tmp.Right(4).CompareNoCase(_T(".fb2")) == 0)
					{
						tmp.Delete(tmp.GetLength() - 4, 4);
					}
					filename = (const TCHAR*)tmp;
				}
				TracePluginDiagnostic(L"Export", pluginClsid, L"DOM result", dom ? S_OK : E_NOINTERFACE, dom ? 1 : 0);
				if(dom)
				{
					HRESULT exportResult = epl->Export((long)m_hWnd, filename, dom);
					TracePluginDiagnostic(L"Export", pluginClsid, L"Export", exportResult, 1);
					CheckError(exportResult);
					TracePluginDiagnostic(L"Export", pluginClsid, L"completed", S_OK, 1);
				}
				} 
				else 
				{
					U::MessageBox(MB_OK|MB_ICONERROR, IDS_EXPORT_ERR_CPT, IDS_EXPORT_ERR_MSG);
				return 0;
			}
		}
		catch(_com_error& e)
		{
			TracePluginDiagnostic(L"Export", pluginClsid, L"exception", e.Error(), 0);
			U::ReportError(e);
		}
	}
	return 0;
}

LRESULT CMainFrame::OnLastPlugin(WORD, WORD wID, HWND, BOOL&)
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
			if (HasOnlySourceEditorConfigurationChanged(previousConfiguration, currentConfiguration))
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

LRESULT CMainFrame::OnToolsScript(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	wID -= ID_SCRIPT_BASE;

	if(IsSourceActive())
		return 0;

  // ??????? ?? FBE ? ?? FBW ??????????? ?? ???????. ? FBE ??????? ??????????? ????? Active Scripting
  // ? ???????? ? ???? ??????????? ????? ?????????. 
  // ? FBW ??????? ??????????? ? ????? HTML ?????????
	for(int i = 0; i < m_scripts.GetSize(); ++i)
	{
		if(m_scripts[i].wID == -1) continue;

		if(m_scripts[i].Type == 2 && m_scripts[i].wID == wID)
		{
			m_doc->RunScript(m_scripts[i].path);
			m_last_script = &m_scripts[i];
			break;
		}
	}
  
  // TODO ??? ?????? ???? else

  /*if (wID < m_scripts.GetSize()) {
  if (StartScript(this) >= 0) {
		if (SUCCEEDED(ScriptLoad(m_scripts[wID].name))){
			if(m_scripts[wID].Type == 0)
			{
				MSXML2::IXMLDOMDocument2Ptr dom(m_doc->CreateDOM(m_doc->m_encoding));
				if (dom) 
				{
					CComVariant arg;
					V_VT(&arg) = VT_DISPATCH;
					V_DISPATCH(&arg) = dom;
					dom.AddRef();
					if (SUCCEEDED(ScriptCall(L"Run",&arg,1,NULL))) 
					{
						m_doc->SetXML(dom);						
					}
				}
			}
			else if(m_scripts[wID].Type == 1)
			{
				SHD::IWebBrowser2Ptr HTMLdomBody = m_doc->m_body.Browser();
				SHD::IWebBrowser2Ptr HTMLdomDesc = m_doc->m_body.Browser();
				CComVariant* arg = new CComVariant[2];				
				V_VT(&arg[0]) = VT_DISPATCH;
				V_DISPATCH(&arg[0]) = HTMLdomBody;
				HTMLdomBody.AddRef();
				V_VT(&arg[1]) = VT_DISPATCH;
				V_DISPATCH(&arg[1]) = HTMLdomDesc;
				HTMLdomDesc.AddRef();
				
				CComVariant vt;
				if (SUCCEEDED(ScriptCall(L"Run",arg,2,&vt))) 
				{
					//m_doc->SetXML(dom);
				}
			}
			else if(m_scripts[wID].Type == 2)
			{
				ScriptCall(L"Run",0,0,0);
			}
      }
      StopScript();
    }
  }*/

  return 0;
}

LRESULT CMainFrame::OnEditInsSymbol(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
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
		HWND aw = ::GetFocus();
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
LRESULT CMainFrame::OnSelectCtl(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
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
			m_id.SetFocus();
			break;
			case ID_SELECT_HREF:
			{
				if(!IsBandVisible(ATL_IDW_BAND_FIRST + 3))
					OnViewToolBar(0,ATL_IDW_BAND_FIRST + 3, NULL, bHandled);
				m_href.SetFocus();
				CString href(U::GetWindowText(m_href));
				m_href.SetSel(0, href.GetLength(), FALSE);
				break;
			}
		case ID_SELECT_IMAGE:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 3))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 3, NULL, bHandled);
			m_image_title.SetFocus();
			break;
		case ID_SELECT_TEXT:
			m_view.SetFocus();
			break;
		case ID_SELECT_SECTION:
			if (!IsBandVisible(ATL_IDW_BAND_FIRST + 3))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 3, NULL, bHandled);
			m_section.SetFocus();
			break;
		case ID_SELECT_IDT:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 3))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 3, NULL, bHandled);
			m_id_table_id.SetFocus();
			break;
		case ID_SELECT_STYLET:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 3))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 3, NULL, bHandled);
			m_styleT_table.SetFocus();
			break;
		case ID_SELECT_STYLE:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST+  3))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 3, NULL, bHandled);
			m_style_table.SetFocus();
			break;
		case ID_SELECT_COLSPAN:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_colspan_table.SetFocus();
			break;
		case ID_SELECT_ROWSPAN:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_rowspan_table.SetFocus();
			break;
		case ID_SELECT_ALIGNTR:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
			OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_alignTR_table.SetFocus();
			break;
		case ID_SELECT_ALIGN:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_align_table.SetFocus();
			break;
		case ID_SELECT_VALIGN:
			if(!IsBandVisible(ATL_IDW_BAND_FIRST + 4))
				OnViewToolBar(0, ATL_IDW_BAND_FIRST + 4, NULL, bHandled);
			m_valign_table.SetFocus();
			break;
	}

	return 0;
}

LRESULT CMainFrame::OnNextItem(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  ShowView(NEXT);
  return 1;
}

// editor notifications
LRESULT CMainFrame::OnCbEdChange(WORD code, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
  if (m_ignore_cb_changes)
    return 0;

  try {
    if (wID==IDC_HREF) {
      MSHTML::IHTMLElementPtr an(m_doc->m_body.SelectionAnchor());
      _variant_t    href;
      if (an)
		href=an->getAttribute(L"href",2);
      if ((bool)an && V_VT(&href)==VT_BSTR) {
		CString	    newhref(U::GetWindowText(m_href));

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
        m_href_box.SetWindowText(_T(""));
		m_href_box.EnableWindow(FALSE);
		m_href_caption.SetEnabled(false);
      }
    }
    if (wID==IDC_ID) {
      MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionStructCon());
      if (sc)
		sc->id=(const wchar_t *)U::GetWindowText(m_id);
      else
	  {
		m_id_box.EnableWindow(FALSE);
		m_id_caption.SetEnabled(false);
	  }
    }
	if (wID==IDC_SECTION) {
		MSHTML::IHTMLElementPtr		scs(m_doc->m_body.SelectionStructSection());
		if (scs)
			scs->id=(const wchar_t *)U::GetWindowText(m_section);
		else
			m_section.EnableWindow(FALSE);
	}	

	if (wID==IDC_IMAGE_TITLE) {
		MSHTML::IHTMLElementPtr		scs(m_doc->m_body.SelectionStructImage());
		if (scs)
		{
			//scs->title=(const wchar_t *)U::GetWindowText(m_image_title);
			U::ChangeAttribute(scs, L"title", (const wchar_t *)U::GetWindowText(m_image_title));

			IHTMLControlRangePtr r(((MSHTML::IHTMLElement2Ptr)(m_doc->m_body.Document()->body))->createControlRange());
			r->add((IHTMLControlElementPtr)scs);
			r->select();
		}
		else
		{
			m_image_title_box.EnableWindow(FALSE);
			m_image_title_caption.SetEnabled(false);
		}
	}
	
	if (wID==IDC_IDT) {
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionStructTable());
		if (sc)
			sc->id=(const wchar_t *)U::GetWindowText(m_id_table_id);
		else
		{
			m_id_table_id_box.EnableWindow(FALSE);
			m_table_id_caption.SetEnabled(false);
		}
	}
	if (wID==IDC_ID) {
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionStructTableCon());
		if (sc)
			sc->id=(const wchar_t *)U::GetWindowText(m_id_table);
		else
		{
			m_id_table_box.EnableWindow(FALSE);
			m_id_table_caption.SetEnabled(false);
		}
	}
	if (wID==IDC_STYLET) {
		_bstr_t style("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsStyleTB(style));
		if (sc){
			CString	    newsSyleT(U::GetWindowText(m_styleT_table));
			sc->setAttribute(L"fbstyle",_variant_t((const wchar_t *)newsSyleT),0);
		}
		else
		{
			m_style_table_box.EnableWindow(FALSE);
			m_style_caption.SetEnabled(false);
		}
	}
	if (wID==IDC_STYLE) {
		_bstr_t style("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsStyleB(style));
		if (sc){
			CString	    newsSyle(U::GetWindowText(m_style_table));
			sc->setAttribute(L"fbstyle",_variant_t((const wchar_t *)newsSyle),0);
		}	
		else
		{
			m_style_table_box.EnableWindow(FALSE);
			m_style_caption.SetEnabled(false);
		}
	}
	if (wID==IDC_COLSPAN) {
		_bstr_t colspan("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsColspanB(colspan));
		if (sc){
			CString	    newsColspan(U::GetWindowText(m_colspan_table));
			sc->setAttribute(L"fbcolspan",_variant_t((const wchar_t *)newsColspan),0);
		}
		else
		{
			m_colspan_table_box.EnableWindow(FALSE);
			m_colspan_caption.SetEnabled(false);
		}
	}
	if (wID==IDC_ROWSPAN) {
		_bstr_t rowspan("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsRowspanB(rowspan));
		if (sc){
			CString	    newsRowspan(U::GetWindowText(m_rowspan_table));
			sc->setAttribute(L"fbrowspan",_variant_t((const wchar_t *)newsRowspan),0);
		}
		else
		{
			m_rowspan_table_box.EnableWindow(FALSE);
			m_rowspan_caption.SetEnabled(false);
		}
	}
	if (wID==IDC_ALIGNTR) {
		_bstr_t alignTR("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsAlignTRB(alignTR));
		if (sc){
			CString	    newsAlignTR(U::GetWindowText(m_alignTR_table));
			sc->setAttribute(L"fbalign",_variant_t((const wchar_t *)newsAlignTR),0);
		}
		else
		{
			m_alignTR_table_box.EnableWindow(FALSE);
			m_tr_allign_caption.SetEnabled(false);
		}
	}
	if (wID==IDC_ALIGN) {
		_bstr_t align("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsAlignB(align));
		if (sc){
			CString	    newsAlign(U::GetWindowText(m_align_table));
			sc->setAttribute(L"fbalign",_variant_t((const wchar_t *)newsAlign),0);
		}
		else
		{
			m_align_table_box.EnableWindow(FALSE);
			m_th_allign_caption.SetEnabled(false);
		}
	}
	if (wID==IDC_VALIGN) {
		_bstr_t valign("");
		MSHTML::IHTMLElementPtr		sc(m_doc->m_body.SelectionsVAlignB(valign));
		if (sc){
			CString	    newsVAlign(U::GetWindowText(m_valign_table));
			sc->setAttribute(L"fbvalign",_variant_t((const wchar_t *)newsVAlign),0);
		}
		else
		{
			m_valign_table_box.EnableWindow(FALSE);
			m_valign_caption.SetEnabled(false);	
		}
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

LRESULT CMainFrame::OnTreeRestore(WORD, WORD, HWND, BOOL& b)
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
		MSHTML::IHTMLElementPtr elem(ret_node);
		m_document_tree.m_tree.m_tree.SelectElement(elem);
		GoTo(elem);
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
		MSHTML::IHTMLElementPtr elem(ret_node);
		m_document_tree.m_tree.m_tree.SelectElement(elem);
		GoTo(elem);
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

LRESULT CMainFrame::OnTreeClick(WORD, WORD, HWND hWndCtl, BOOL&)
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

static CString ExtractXmlDeclarationEncoding(const CString& xmlText)
{
	const int declStart = xmlText.Find(L"<?xml");
	if (declStart < 0)
		return CString();

	const int declEnd = xmlText.Find(L"?>", declStart);
	if (declEnd < 0)
		return CString();

	CString decl = xmlText.Mid(declStart, declEnd - declStart + 2);
	CString declLower(decl);
	declLower.MakeLower();

	int encPos = declLower.Find(L"encoding");
	if (encPos < 0)
		return CString();

	encPos = decl.Find(L"=", encPos);
	if (encPos < 0)
		return CString();

	++encPos;
	while (encPos < decl.GetLength() &&
		(decl[encPos] == L' ' || decl[encPos] == L'\t' ||
		 decl[encPos] == L'\r' || decl[encPos] == L'\n'))
	{
		++encPos;
	}

	if (encPos >= decl.GetLength())
		return CString();

	const wchar_t quote = decl[encPos];
	if (quote != L'"' && quote != L'\'')
		return CString();

	const int valueStart = encPos + 1;
	const int valueEnd = decl.Find(quote, valueStart);
	if (valueEnd <= valueStart)
		return CString();

	CString encoding = decl.Mid(valueStart, valueEnd - valueStart);
	encoding.Trim();
	return encoding;
}

// Возвращает позицию символа отображаемого текста XML-узла в Source. Теги и
// сущности XML пропускаются, поэтому форматирование Source не влияет на поиск.
static int FindXmlNodeTextPosition(const CString& sourceXml,
	MSXML2::IXMLDOMNodePtr xmlNode, int textPosition, int scopeStart,
	int scopeEnd)
{
	if (!(bool)xmlNode) return -1;
	bstr_t nodeTextValue(xmlNode->text);
	return FBEBodySourceTransfer::FindXmlNodeTextPosition(
		std::wstring((const wchar_t*)sourceXml),
		std::wstring((const wchar_t*)nodeTextValue), textPosition, scopeStart, scopeEnd);
}

// Text refines the exact Source boundaries, while the DOM path supplies both
// the body scope and the expected structural position.  Without that position
// the helper refuses an ambiguous transfer instead of selecting another copy.
static bool FindVisibleXmlTextRange(const CString& sourceXml,
	const CString& visibleText, int scopeStart, int scopeEnd, int expectedStart,
	int& rangeStart, int& rangeEnd)
{
	FBEBodySourceTransfer::XmlTextRange range = { -1, -1 };
	if (!FBEBodySourceTransfer::FindVisibleXmlTextRange(
		std::wstring((const wchar_t*)sourceXml), std::wstring((const wchar_t*)visibleText),
		scopeStart, scopeEnd, expectedStart, range))
		return false;
	rangeStart = range.start;
	rangeEnd = range.end;
	return true;
}

static bool FindEnclosingXmlElementRange(const CString& sourceXml, int position,
	const wchar_t* elementName, int& elementStart, int& elementEnd)
{
	FBEBodySourceTransfer::XmlTextRange range = { -1, -1 };
	if (!FBEBodySourceTransfer::FindEnclosingXmlElementRange(
		std::wstring((const wchar_t*)sourceXml), position, elementName, range))
		return false;
	elementStart = range.start;
	elementEnd = range.end;
	return true;
}

static bool FindEnclosingXmlBodyRange(const CString& sourceXml, int position,
	int& bodyStart, int& bodyEnd)
{
	return FindEnclosingXmlElementRange(sourceXml, position, L"body",
		bodyStart, bodyEnd);
}

// DomPath may fail for a valid Source position (for example inside inline
// markup).  The body ordinal is still available from the source XML and is
// sufficient to constrain the fallback search to the matching visual body.
static int FindXmlBodyIndexAtPosition(const CString& sourceXml, int position)
{
	int currentBody = -1;
	int bodyCount = 0;
	for(int tagStart = sourceXml.Find(L'<'); tagStart >= 0 && tagStart <= position;)
	{
		const int tagEnd = sourceXml.Find(L'>', tagStart + 1);
		if(tagEnd < 0) break;
		CString tag = sourceXml.Mid(tagStart + 1, tagEnd - tagStart - 1);
		tag.TrimLeft();
		const bool closing = !tag.IsEmpty() && tag[0] == L'/';
		if(closing) tag.Delete(0);
		const int nameEnd = tag.FindOneOf(L" \t\r\n/");
		CString name = nameEnd >= 0 ? tag.Left(nameEnd) : tag;
		const int namespaceSeparator = name.ReverseFind(L':');
		if(namespaceSeparator >= 0) name = name.Mid(namespaceSeparator + 1);
		if(name.CompareNoCase(L"body") == 0)
		{
			if(closing) currentBody = -1;
			else currentBody = bodyCount++;
		}
		if(tagEnd >= position) break;
		tagStart = sourceXml.Find(L'<', tagEnd + 1);
	}
	return currentBody;
}

// Resolve the complete serialized range of one top-level FB2 body.  This is
// deliberately independent of DomPath: a native MSHTML text selection can be
// perfectly valid even when DomPath cannot represent one of its inline nodes.
static bool FindXmlBodyRangeByIndex(const CString& sourceXml, int targetIndex,
	int& bodyStart, int& bodyEnd)
{
	bodyStart = bodyEnd = -1;
	if(targetIndex < 0) return false;
	int bodyIndex = 0;
	for(int tagStart = sourceXml.Find(L'<'); tagStart >= 0;)
	{
		const int tagEnd = sourceXml.Find(L'>', tagStart + 1);
		if(tagEnd < 0) break;
		CString tag = sourceXml.Mid(tagStart + 1, tagEnd - tagStart - 1);
		tag.TrimLeft();
		const bool closing = !tag.IsEmpty() && tag[0] == L'/';
		if(closing) tag.Delete(0);
		const int nameEnd = tag.FindOneOf(L" \t\r\n/");
		CString name = nameEnd >= 0 ? tag.Left(nameEnd) : tag;
		const int namespaceSeparator = name.ReverseFind(L':');
		if(namespaceSeparator >= 0) name = name.Mid(namespaceSeparator + 1);
		if(name.CompareNoCase(L"body") == 0)
		{
			if(!closing && bodyIndex++ == targetIndex)
				bodyStart = tagStart;
			else if(closing && bodyStart >= 0)
			{
				bodyEnd = tagEnd + 1;
				return true;
			}
		}
		tagStart = sourceXml.Find(L'<', tagEnd + 1);
	}
	return false;
}

// Преобразует фрагмент Source в отображаемый текст для поиска в Body.
static CString ExtractVisibleXmlText(const CString& sourceFragment)
{
	CString text;
	for (int position = 0; position < sourceFragment.GetLength();)
	{
		if (sourceFragment[position] == L'<')
		{
			const int tagEnd = sourceFragment.Find(L'>', position + 1);
			if (tagEnd < 0)
				break;
			CString tagName = sourceFragment.Mid(position + 1,
				tagEnd - position - 1);
			tagName.TrimLeft();
			if (!tagName.IsEmpty() && tagName[0] == L'/')
				tagName.Delete(0);
			const int tagNameEnd = tagName.FindOneOf(L" \t\r\n/");
			if (tagNameEnd >= 0)
				tagName = tagName.Left(tagNameEnd);
			// В HTML граница абзаца представлена переводом строки, в XML —
			// парой тегов. Сохраняем эту границу для IHTMLTxtRange::findText.
			if (tagName.CompareNoCase(L"p") == 0 ||
				tagName.CompareNoCase(L"empty-line") == 0 ||
				tagName.CompareNoCase(L"title") == 0)
			{
				while (!text.IsEmpty() && text[text.GetLength() - 1] == L' ')
					text.Delete(text.GetLength() - 1);
				if (!text.IsEmpty() && text.Right(2) != L"\r\n")
					text += L"\r\n";
			}
			position = tagEnd + 1;
			continue;
		}

		if (sourceFragment[position] == L'&')
		{
			const int entityEnd = sourceFragment.Find(L';', position + 1);
			if (entityEnd >= 0)
			{
				const CString entity = sourceFragment.Mid(position, entityEnd - position + 1);
				std::wstring decoded;
				if (FBEBodySourceTransfer::DecodeXmlCharacterReference(
					std::wstring((const wchar_t*)entity), decoded))
					text += decoded.c_str();
				else text += entity;
				position = entityEnd + 1;
				continue;
			}
		}

		const wchar_t character = sourceFragment[position++];
		if (iswspace(character) || character == L'\xA0')
		{
			if (!text.IsEmpty() && text.Right(2) != L"\r\n" &&
				text[text.GetLength() - 1] != L' ')
				text += L' ';
		}
		else
		{
			text += character;
		}
	}
	return text;
}

// Находит диапазон в HTML по началу и концу видимого текста. Это покрывает
// Source-выделения, пересекающие абзацы: один вызов findText для всего такого
// диапазона не работает в MSHTML из-за разных представлений перевода строки.
static MSHTML::IHTMLTxtRangePtr FindBodyTextRange(
	MSHTML::IHTMLBodyElementPtr htmlBody, MSHTML::IHTMLElementPtr htmlScope,
	MSHTML::IHTMLElementPtr expectedStartElement, const CString& visibleText)
{
	if (!(bool)htmlBody || !(bool)htmlScope || visibleText.IsEmpty())
		return MSHTML::IHTMLTxtRangePtr();

	MSHTML::IHTMLTxtRangePtr wholeRange = htmlBody->createTextRange();
	if ((bool)wholeRange)
		wholeRange->moveToElementText(expectedStartElement ? expectedStartElement : htmlScope);
	if ((bool)wholeRange && wholeRange->findText((const wchar_t*)visibleText,
		1073741824, 0) == VARIANT_TRUE)
		return wholeRange;

	CString startAnchor = visibleText;
	CString endAnchor = visibleText;
	startAnchor.TrimLeft();
	endAnchor.TrimRight();
	const int firstLineEnd = startAnchor.Find(L"\r\n");
	if (firstLineEnd >= 0)
		startAnchor = startAnchor.Left(firstLineEnd);
	const int lastLineBegin = endAnchor.ReverseFind(L'\n');
	if (lastLineBegin >= 0)
		endAnchor = endAnchor.Mid(lastLineBegin + 1);
	startAnchor.Trim();
	endAnchor.Trim();
	if (startAnchor.IsEmpty() || endAnchor.IsEmpty())
		return MSHTML::IHTMLTxtRangePtr();

	// Длинная граница практически исключает совпадение с другой фразой.
	const int anchorLength = 96;
	if (startAnchor.GetLength() > anchorLength)
		startAnchor = startAnchor.Left(anchorLength);
	if (endAnchor.GetLength() > anchorLength)
		endAnchor = endAnchor.Right(anchorLength);

	MSHTML::IHTMLTxtRangePtr startRange = htmlBody->createTextRange();
	if ((bool)startRange)
		startRange->moveToElementText(expectedStartElement ? expectedStartElement : htmlScope);
	if (!(bool)startRange ||
		startRange->findText((const wchar_t*)startAnchor, 1073741824, 0) != VARIANT_TRUE)
		return MSHTML::IHTMLTxtRangePtr();
	// Search the closing anchor only after the structurally selected start.
	// Searching backwards from the whole document used to join two unrelated
	// duplicate paragraphs into one range.
	MSHTML::IHTMLTxtRangePtr endRange = startRange->duplicate();
	if (!(bool)endRange)
		return MSHTML::IHTMLTxtRangePtr();
	endRange->collapse(VARIANT_FALSE);
	if (endRange->findText((const wchar_t*)endAnchor, 1073741824, 0) != VARIANT_TRUE)
		return MSHTML::IHTMLTxtRangePtr();

	startRange->setEndPoint(L"EndToEnd", endRange);
	return startRange;
}

// Границы выделения Source могут попасть в имя тега или его атрибут. В Body
// таких символов нет, поэтому отсекаем разметку и оставляем только видимый
// текст между тегами.
static int SkipXmlMarkupForward(const CString& sourceXml, int position)
{
	while (position < sourceXml.GetLength())
	{
		const int tagBegin = sourceXml.Left(position).ReverseFind(L'<');
		const int tagEnd = tagBegin >= 0
			? sourceXml.Find(L'>', tagBegin + 1)
			: -1;
		if (tagBegin >= 0 && tagEnd >= position)
			position = tagEnd + 1;
		else
			break;
	}
	return position;
}

static int SkipXmlMarkupBackward(const CString& sourceXml, int position)
{
	while (position > 0)
	{
		const int tagBegin = sourceXml.Left(position).ReverseFind(L'<');
		const int tagEnd = tagBegin >= 0
			? sourceXml.Find(L'>', tagBegin + 1)
			: -1;
		if (tagBegin >= 0 && tagEnd >= position)
			position = tagBegin;
		else
			break;
	}
	return position;
}

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
	m_source_selection_transferred = false;
	LRESULT changed = m_source.SendMessage(SCI_GETMODIFY);
	int	    textlen = 0;
	char*	buffer = 0;

	int begin_char = 0;
	int end_char = 0;
	int bodies_count = 0;
	int selected_body_index = -1;
	
	// ????? ?????
	textlen = m_source.SendMessage(SCI_GETLENGTH);
	buffer = new char[textlen + 1];
	m_source.SendMessage(SCI_GETTEXT, textlen+1, (LPARAM)buffer);
	// ????????? ? UTF16
	DWORD   ulen=::MultiByteToWideChar(CP_UTF8,0,buffer,textlen,NULL,0);

	BSTR    ustr=::SysAllocStringLen(NULL,ulen);
	::MultiByteToWideChar(CP_UTF8,0,buffer,textlen,ustr,ulen);
	
	//	??????? ?????????? ???????	
	int	  selectedPosBegin = m_source.SendMessage(SCI_GETSELECTIONSTART);    
	int	  selectedPosEnd = m_source.SendMessage(SCI_GETSELECTIONEND);    
	bool one_pos = selectedPosEnd == selectedPosBegin;
	if (StartupTrace::Enabled())
	{
		CString trace;
		trace.Format(L"SourceToHTML: source bytes=[%d,%d], text bytes=%d, caret=%d",
			selectedPosBegin, selectedPosEnd, textlen, one_pos ? 1 : 0);
		WriteSelectionTrace(L"E210", trace);
	}
	if(one_pos)
	{
		selectedPosEnd = selectedPosBegin = MultiByteToWideChar(CP_UTF8,0,buffer,selectedPosBegin,NULL,0);
	}
	else
	{
		selectedPosBegin = MultiByteToWideChar(CP_UTF8,0,buffer,selectedPosBegin,NULL,0);
		selectedPosEnd = MultiByteToWideChar(CP_UTF8,0,buffer,selectedPosEnd,NULL,0);
	}
	CString sourceText(ustr);
	selected_body_index = FindXmlBodyIndexAtPosition(sourceText, selectedPosBegin);
	if (!one_pos)
	{
		selectedPosBegin = SkipXmlMarkupForward(sourceText, selectedPosBegin);
		selectedPosEnd = SkipXmlMarkupBackward(sourceText, selectedPosEnd);
		if (selectedPosEnd < selectedPosBegin)
			selectedPosEnd = selectedPosBegin;
	}
	CString selectedSourceText;
	bool selectionCrossesParagraph = false;
	if (selectedPosEnd > selectedPosBegin)
	{
		const CString selectedSourceXml = sourceText.Mid(selectedPosBegin,
			selectedPosEnd - selectedPosBegin);
		selectedSourceText = ExtractVisibleXmlText(selectedSourceXml);
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
		
	if(changed)
	{
		CString sourceEncoding = ExtractXmlDeclarationEncoding(sourceText);
		if (!sourceEncoding.IsEmpty())
			m_doc->m_encoding = sourceEncoding;

		if((bool)m_saved_xml)
		{
			m_saved_xml.Release();
			m_saved_xml = 0;
		}
		
		if(!m_doc->TextToXML(ustr, (MSXML2::IXMLDOMDocument2Ptr*)(&m_saved_xml)))
		{
			// TextToXML performs the FBD structural check.  Unlike generic XML
			// syntax fallback, a structurally invalid FBD must never reach
			// LoadFromDOM through XmlFromText.
			if (IsFbdFile(m_doc->m_filename))
			{
				delete[] buffer;
				SysFreeString(ustr);
				return false;
			}
			CComDispatchDriver	body(m_doc->m_body.Script());
			CComVariant		    args[1];
			CComVariant		    ret;
			args[0]=ustr;
			CheckError(body.Invoke1(L"XmlFromText",&args[0], &ret));
			if(ret.vt == VT_DISPATCH)
			{
				m_saved_xml = ret.pdispVal;
				// ???? ???????? ?? xml, ?????? ????????? ??????
				if(!(bool)m_saved_xml)
				{
					MSXML2::IXMLDOMParseErrorPtr err = ret.pdispVal;
					if(!(bool)err)
					{
						delete[] buffer;
						SysFreeString(ustr);
						return false;
					}
					bstr_t msg = err->reason;
					int line = err->line;
					int linepos = err->linepos;
					::SendMessage(m_doc->m_frame,AU::WM_SETSTATUSTEXT,0,(LPARAM)(const TCHAR *)msg);
					SourceGoTo(line, linepos);
					delete[] buffer;
					SysFreeString(ustr);
					return false;
				}
			}
			else
			{
				delete[] buffer;
				SysFreeString(ustr);
				return false;
			}			
		}
	}

	SysFreeString(ustr);
	
	
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
	if(changed)
	{
		// ?????????? ? HTML
		CComDispatchDriver	body(m_doc->m_body.Script());
		CComVariant		    args[2];
		args[1] = m_saved_xml.GetInterfacePtr();
		args[0] = _Settings.GetInterfaceLanguageName();
		CheckError(body.InvokeN(L"LoadFromDOM", args, 2));
		m_doc->m_body.Init();
		// ? ??? ?????????? ????? HTML ? ????????? ?? ????????? ??????? ?????? ?????????.
		ClearSelection();
		
        //m_saved_xml.Release();
		//m_saved_xml = 0;		
	}

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
			m_body_selection = m_doc->m_body.SetSelection(
				selectedHTMLElementBegin, selectedHTMLElementEnd, begin_char, end_char);
			m_source_selection_transferred = (bool)m_body_selection;
		}
	}

	if(!m_source_selection_transferred && !selectedSourceText.IsEmpty())
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
		MSHTML::IHTMLTxtRangePtr range = FindBodyTextRange(htmlBody, htmlScope,
			expectedStartElement, selectedSourceText);
		if((bool)range)
		{
			m_body_selection = range;
			m_source_selection_transferred = true;
		}
	}
	if (StartupTrace::Enabled())
	{
		CString trace;
		trace.Format(L"SourceToHTML: transfer result=%d, DOM-path=%d, crosses-p=%d",
			m_source_selection_transferred ? 1 : 0,
			selection_path_available ? 1 : 0, selectionCrossesParagraph ? 1 : 0);
		WriteSelectionTrace(L"E240", trace);
	}

	delete[] buffer;
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
	m_body_selection_transferred = false;
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
		if(selectedBeginElement == selectedEndElement && (bool)m_body_selection)
		{
			const CString selectedText((const wchar_t*)m_body_selection->text);
			if(!selectedText.IsEmpty())
				selection_end_char = selection_begin_char + selectedText.GetLength();
		}
		if (StartupTrace::Enabled())
		{
			CString selectedText;
			if ((bool)m_body_selection)
				selectedText = (const wchar_t*)m_body_selection->text;
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

	{
		if (m_doc->DocRelChanged() || !(bool)m_saved_xml)
		{
			if ((bool)m_saved_xml)
			{
				m_saved_xml.Release();
			}
			m_saved_xml = m_doc->CreateDOM(sourceEncoding);
			if (!(bool)m_saved_xml)
			{
				return false;
			}
		}
	}
	phaseProfiler.Mark("CreateDOM");

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

	_bstr_t rawSrc(m_saved_xml->xml);
	phaseProfiler.Mark("m_saved_xml serialization");
	CString srcText((const wchar_t*)rawSrc);
	phaseProfiler.Mark("BSTR to CString");
	CString xmlDecl;
	xmlDecl.Format(L"<?xml version=\"1.0\" encoding=\"%s\"?>", (const wchar_t*)sourceEncoding);
	const CString xmlDeclWithoutEncoding(L"<?xml version=\"1.0\"?>");
	if (srcText.Left(xmlDeclWithoutEncoding.GetLength()).CompareNoCase(xmlDeclWithoutEncoding) == 0)
	{
		srcText.Delete(0, xmlDeclWithoutEncoding.GetLength());
		srcText.Insert(0, xmlDecl);
	}
	else if (srcText.Left(5).CompareNoCase(L"<?xml") != 0)
	{
		srcText.Insert(0, xmlDecl + L"\r\n");
	}
	phaseProfiler.Mark("XML declaration normalization");
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
		if((bool)m_body_selection)
		{
			const CString selectedText((const wchar_t*)m_body_selection->text);
			hasBodySelectionText = !selectedText.IsEmpty();
			if(hasBodySelectionText && selection_path_available &&
				(bool)xml_selected_begin && (bool)xml_selected_end)
			{
				const int expectedBegin = selection_begin_path.GetNodeFromText(src,
					selection_begin_char);
				const int expectedEnd = selection_end_path.GetNodeFromText(src,
					selection_end_char);
				if(expectedBegin >= 0 && expectedEnd >= expectedBegin &&
					FindEnclosingXmlBodyRange(srcText, expectedBegin, bodyStart, bodyEnd))
				{
					FindVisibleXmlTextRange(srcText, selectedText, bodyStart, bodyEnd,
						expectedBegin, beginPosition, endPosition);
				}
			}
			// DomPath is positional refinement, not a prerequisite for a native
			// Body selection.  The selected FB2 body remains a safe base scope;
			// FindVisibleXmlTextRange refuses ambiguous repeated text within it.
			if(hasBodySelectionText && (beginPosition < 0 || endPosition < 0) &&
				FindXmlBodyRangeByIndex(srcText, selected_body_index, bodyStart, bodyEnd))
			{
				FindVisibleXmlTextRange(srcText, selectedText, bodyStart, bodyEnd,
					-1, beginPosition, endPosition);
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
				FindEnclosingXmlBodyRange(srcText, bodyAnchor, bodyStart, bodyEnd);
				if(!FindEnclosingXmlElementRange(srcText, bodyAnchor, L"section",
					caretScopeStart, caretScopeEnd))
				{
					caretScopeStart = bodyStart;
					caretScopeEnd = bodyEnd;
				}
			}
		}

		// Старый DomPath не всегда умеет пройти от XML-документа к узлу
		// отображаемого текста. Сам узел уже получен из DOM, поэтому при
		// таком отказе сопоставляем позицию по его XML-представлению.
		if(!hasBodySelectionText && selection_path_available && (bool)xml_selected_begin &&
			beginPosition < 0 && caretScopeStart >= 0)
			beginPosition = FindXmlNodeTextPosition(srcText, xml_selected_begin,
				selection_begin_char, caretScopeStart, caretScopeEnd);
		if(!hasBodySelectionText && selection_path_available && (bool)xml_selected_end &&
			endPosition < 0 && caretScopeStart >= 0)
			endPosition = FindXmlNodeTextPosition(srcText, xml_selected_end,
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
	m_body_selection_transferred = selection_mapped_to_source;
	m_source_selection_start = savedPosBegin;
	m_source_selection_end = savedPosEnd;
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


void  CMainFrame::ShowView(VIEW_TYPE vt) 
{
  VIEW_TYPE prev = m_current_view;
	if (StartupTrace::Enabled())
	{
		const wchar_t* const viewNames[] = { L"Body", L"Description", L"Source", L"Next" };
		CString trace;
		trace.Format(L"ShowView: requested %s -> %s", viewNames[prev], viewNames[vt]);
		WriteSelectionTrace(L"E280", trace);
	}
  SaveSelection(m_current_view);

  // added by SeNS
  if (vt != BODY)
	if (m_Speller) 
		m_Speller->EndDocumentCheck();

  if (vt == NEXT)
  {
	  if(!m_ctrl_tab)
	  {
		  if(m_current_view !=m_last_ctrl_tab_view)
			vt = m_last_ctrl_tab_view;
		  else		  
		  {
			if((m_last_view == BODY && m_current_view == DESC) ||
				(m_last_view == DESC && m_current_view == BODY))
				vt = SOURCE;
			if((m_last_view == BODY && m_current_view == SOURCE) ||
				(m_last_view == SOURCE && m_current_view == BODY))
				vt = DESC;
			if((m_last_view == SOURCE && m_current_view == DESC) ||
				(m_last_view == DESC && m_current_view == SOURCE))
				vt = BODY;
		  }
          m_last_ctrl_tab_view = m_current_view;
		  m_ctrl_tab = true;		  
	  }
	  else
	  {
		  if((m_last_view == BODY && m_current_view == DESC) ||
			  (m_last_view == DESC && m_current_view == BODY))
			  vt = SOURCE;
		  if((m_last_view == BODY && m_current_view == SOURCE) ||
			  (m_last_view == SOURCE && m_current_view == BODY))
			  vt = DESC;
		  if((m_last_view == SOURCE && m_current_view == DESC) ||
			  (m_last_view == DESC && m_current_view == SOURCE))
			  vt = BODY;
	  }   
  }

  if(prev != vt)
  {  
	  m_doc->m_body.CloseFindDialog(m_doc->m_body.m_find_dlg);
	  m_doc->m_body.CloseFindDialog(m_sci_find_dlg);
	  m_doc->m_body.CloseFindDialog(m_doc->m_body.m_replace_dlg);
	  m_doc->m_body.CloseFindDialog(m_sci_replace_dlg);
  }

	if(!m_ctrl_tab && prev != vt)
	{		
		m_last_ctrl_tab_view = m_current_view;
	}


  if (prev!=vt && prev==SOURCE) {
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
					m_file_age = ~0;
					m_doc->m_namevalid = false;
				}
				else
				{
					m_file_age = FileAge(m_doc->m_filename);
					m_doc->m_namevalid = true;
				}
				m_bad_xml=false;
			}
	  }

    /*if (!SourceToHTML())
      return;*/
	  if(vt == DESC)
	  {
		 if (!SourceToHTML())
			return;
		 m_source.SendMessage(SCI_SETSAVEPOINT);
		// SaveSelection(BODY);
	  }
  }

  if ((vt == BODY || vt == DESC) && (!m_doc || !m_doc->m_body.HasDoc()))
  {
    StartupTrace::Warning(L"selection", L"E281", L"view switch ignored: HTML document is unavailable");
    return;
  }
  if (prev!=vt && vt==SOURCE) 
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

  switch (vt) {
  case BODY:
	  {
	        UISetCheck(ID_VIEW_BODY, 1);
			m_view.ActivateWnd(m_doc->m_body);
			m_sel_changed=true;
			CComDispatchDriver	body(m_doc->m_body.Script());
			CComVariant		    args[1];
			args[0]=false;
			CheckError(body.Invoke1(L"apiShowDesc",&args[0]));
			if(prev == SOURCE)
			{
			  if (!SourceToHTML())
				return;
			  m_source.SendMessage(SCI_SETSAVEPOINT);
			}
			m_status.SetPaneText(ID_PANE_INS, CurrentOverwriteMode() ? strOVR : strINS);

			if (m_Speller) 
				m_Speller->SetDocumentLanguage();
	  }	
    break;
  case DESC:
    UISetCheck(ID_VIEW_DESC, 1);
    m_view.ActivateWnd(m_doc->m_body);
    m_href_box.SetWindowText(_T(""));
    m_href_box.EnableWindow(FALSE);
    m_id_box.SetWindowText(_T(""));
    m_id_box.EnableWindow(FALSE);

	m_image_title_box.SetWindowText(_T(""));
	m_image_title_box.EnableWindow(FALSE);
	
    // Modification by Pilgrim
	m_section_box.SetWindowText(_T(""));
	m_section_box.EnableWindow(FALSE);
	m_id_table_id_box.SetWindowText(_T(""));
	m_id_table_id_box.EnableWindow(FALSE);
	m_id_table_box.SetWindowText(_T(""));
	m_id_table_box.EnableWindow(FALSE);
	m_styleT_table_box.SetWindowText(_T(""));
	m_styleT_table_box.EnableWindow(FALSE);
	m_style_table_box.SetWindowText(_T(""));
	m_style_table_box.EnableWindow(FALSE);
	m_colspan_table_box.SetWindowText(_T(""));
	m_colspan_table_box.EnableWindow(FALSE);
	m_rowspan_table_box.SetWindowText(_T(""));
	m_rowspan_table_box.EnableWindow(FALSE);
	m_alignTR_table_box.SetWindowText(_T(""));
	m_alignTR_table_box.EnableWindow(FALSE);
	m_align_table_box.SetWindowText(_T(""));
	m_align_table_box.EnableWindow(FALSE);
	m_valign_table_box.SetWindowText(_T(""));
	m_valign_table_box.EnableWindow(FALSE);


	m_id_caption.SetEnabled(false);
	m_href_caption.SetEnabled(false);
	m_section_id_caption.SetEnabled(false);
	m_image_title_caption.SetEnabled(false);
	m_table_id_caption.SetEnabled(false);
	m_table_style_caption.SetEnabled(false);
	m_id_table_caption.SetEnabled(false);
	m_style_caption.SetEnabled(false);
	m_colspan_caption.SetEnabled(false);
	m_rowspan_caption.SetEnabled(false);
	m_tr_allign_caption.SetEnabled(false);
	m_th_allign_caption.SetEnabled(false);
	m_valign_caption.SetEnabled(false);	

	SetStatusContext(_T(""));
	{
			CComDispatchDriver	body(m_doc->m_body.Script());
			CComVariant		    args[1];
			args[0]=true;
			CheckError(body.Invoke1(L"apiShowDesc",&args[0]));
	}	
    break;
  case SOURCE:
	// added by SeNS: display line numbers
	UpdateSourceLineNumberMargin(false);

    UISetCheck(ID_VIEW_SOURCE, 1);
    m_view.HideActiveWnd();
    m_splitter.SetSinglePaneMode(SPLIT_PANE_RIGHT);
    m_view.ActivateWnd(m_source);
	if(m_body_selection_transferred)
	{
		m_source.SendMessage(SCI_SETSELECTIONSTART, m_source_selection_start);
		m_source.SendMessage(SCI_SETSELECTIONEND, m_source_selection_end);
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
  m_last_view = m_current_view;
  m_current_view = vt;
	UpdateStatusBar();
	if(!(prev == SOURCE && vt == BODY))
		RestoreSelection();
  m_view.SetFocus();
	if(vt == BODY && prev == SOURCE && m_source_selection_transferred &&
		(bool)m_body_selection)
	{
		// Activating the MSHTML host can clear its visual highlight.  Apply the
		// already mapped range once, after the final focus assignment.  MSHTML
		// can stop extending a new drag-selection when the same IHTMLTxtRange is
		// selected both before and after the host gains focus.
		m_body_selection->select();
	}
	if(vt == SOURCE && m_body_selection_transferred)
	{
		// Source получает фокус и окончательный размер только в конце смены
		// режима. Повторная установка здесь делает прокрутку устойчивой.
		m_source.SendMessage(SCI_SETSEL, m_source_selection_start,
			m_source_selection_end);
		const int sourceLine = m_source.SendMessage(SCI_LINEFROMPOSITION,
			m_source_selection_start);
		m_source.SendMessage(SCI_ENSUREVISIBLEENFORCEPOLICY, sourceLine);
		m_source.SendMessage(SCI_GOTOPOS, m_source_selection_start);
		m_source.SendMessage(SCI_SETSEL, m_source_selection_start,
			m_source_selection_end);
		m_source.SendMessage(SCI_SCROLLCARET);
		// После отображения панели Scintilla может сбросить положение каретки.
		// Повторяем диапазон в очереди сообщений уже после завершения layout.
		::PostMessage(m_source, SCI_ENSUREVISIBLEENFORCEPOLICY, sourceLine, 0);
		::PostMessage(m_source, SCI_GOTOPOS, m_source_selection_start, 0);
		::PostMessage(m_source, SCI_SETSEL, m_source_selection_start,
			m_source_selection_end);
		::PostMessage(m_source, SCI_SCROLLCARET, 0, 0);
		if (StartupTrace::Enabled())
		{
			CString trace;
			trace.Format(L"ShowView: Source final bytes=[%d,%d], line=%d, first-visible=%d",
				m_source_selection_start, m_source_selection_end, sourceLine,
				(int)m_source.SendMessage(SCI_GETFIRSTVISIBLELINE));
			WriteSelectionTrace(L"E290", trace);
		}
	}
	else if (StartupTrace::Enabled())
	{
		CString trace;
		trace.Format(L"ShowView: completed current=%d, body-transfer=%d, source-transfer=%d",
			m_current_view, m_body_selection_transferred ? 1 : 0,
			m_source_selection_transferred ? 1 : 0);
		WriteSelectionTrace(L"E299", trace);
	}
}

static int GetLineNumberDigits(int lineCount)
{
	if(lineCount < 1) lineCount = 1;
	int digits = 1;
	for(int value = lineCount; value >= 10; value /= 10) ++digits;
	return digits < 4 ? 4 : digits;
}

static bool ShouldUpdateSourceLineNumberMargin(int previousDigits, int lineCount)
{
	return previousDigits != GetLineNumberDigits(lineCount);
}

void CMainFrame::UpdateSourceLineNumberMargin(bool force)
{
	if(!m_source.IsWindow()) return;
	if(!_Settings.XMLSrcShowLineNumbers())
	{
		if(force || m_source_line_number_digits != 0)
			m_source.SendMessage(SCI_SETMARGINWIDTHN, 0, 0);
		m_source_line_number_digits = 0;
		return;
	}
	const int lineCount = static_cast<int>(m_source.SendMessage(SCI_GETLINECOUNT));
	if(!force && !ShouldUpdateSourceLineNumberMargin(m_source_line_number_digits, lineCount)) return;
	const int digits = GetLineNumberDigits(lineCount);
	CStringA sample;
	for(int i = 0; i < digits; ++i) sample += '9';
	const int measuredWidth = static_cast<int>(m_source.SendMessage(SCI_TEXTWIDTH,
		STYLE_LINENUMBER, reinterpret_cast<LPARAM>(sample.GetString())));
	const int width = measuredWidth > 0 ? measuredWidth + 8 : 64;
	if(force || static_cast<int>(m_source.SendMessage(SCI_GETMARGINWIDTHN, 0)) != width)
		m_source.SendMessage(SCI_SETMARGINWIDTHN, 0, width);
	m_source_line_number_digits = digits;
}

void  CMainFrame::SetSciStyles() {
  const bool highContrast = IsHighContrastEnabled();
  const COLORREF windowText = highContrast ? ::GetSysColor(COLOR_WINDOWTEXT) :
    _Settings.GetXmlSrcStyleColor(XML_SRC_STYLE_EDITOR_FOREGROUND);
  const COLORREF windowBackground = highContrast ? ::GetSysColor(COLOR_WINDOW) :
    _Settings.GetXmlSrcStyleColor(XML_SRC_STYLE_EDITOR_BACKGROUND);

  // Смена схемы должна применяться как одна операция: Scintilla иначе
  // кратко рисует прежние стили и не всегда перекрашивает уже открытый код.
  m_source.SendMessage(WM_SETREDRAW, FALSE, 0);
  m_source.SendMessage(SCI_STYLERESETDEFAULT);

  CT2A srcFont(_Settings.GetSrcFont());
  m_source.SendMessage(SCI_STYLESETFONT,STYLE_DEFAULT,(LPARAM) srcFont.m_psz);
  m_source.SendMessage(SCI_STYLESETSIZE,STYLE_DEFAULT, _Settings.GetFontSize());
  m_source.SendMessage(SCI_STYLESETFORE, STYLE_DEFAULT, windowText);
  m_source.SendMessage(SCI_STYLESETBACK, STYLE_DEFAULT, windowBackground);

  m_source.SendMessage(SCI_STYLECLEARALL);
  m_source.SendMessage(SCI_STYLESETFORE, STYLE_LINENUMBER, highContrast ? windowText :
    _Settings.GetXmlSrcStyleColor(XML_SRC_STYLE_LINE_NUMBER));
  m_source.SendMessage(SCI_STYLESETBACK, STYLE_LINENUMBER, windowBackground);
  m_source.SendMessage(SCI_SETCARETFORE, highContrast ? windowText :
    _Settings.GetXmlSrcStyleColor(XML_SRC_STYLE_CARET));
  // Current-line highlighting is editor chrome and remains active when XML
  // syntax highlighting is disabled. High contrast never uses a theme color.
  if(highContrast)
    m_source.SendMessage(SCI_SETCARETLINEVISIBLE, FALSE);
  else
  {
    m_source.SendMessage(SCI_SETCARETLINEBACK,
      _Settings.GetXmlSrcStyleColor(XML_SRC_STYLE_CURRENT_LINE_BACKGROUND));
    m_source.SendMessage(SCI_SETCARETLINEVISIBLE, TRUE);
  }
  m_source.SendMessage(SCI_SETSELFORE, TRUE, highContrast ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) :
    _Settings.GetXmlSrcStyleColor(XML_SRC_STYLE_SELECTION_FOREGROUND));
  m_source.SendMessage(SCI_SETSELBACK, TRUE, highContrast ? ::GetSysColor(COLOR_HIGHLIGHT) :
    _Settings.GetXmlSrcStyleColor(XML_SRC_STYLE_SELECTION_BACKGROUND));
  m_source.SendMessage(SCI_STYLESETFORE, STYLE_BRACELIGHT, highContrast ? windowText :
    _Settings.GetXmlSrcStyleColor(XML_SRC_STYLE_XML_TAG_NAME));
  m_source.SendMessage(SCI_STYLESETBACK, STYLE_BRACELIGHT, highContrast ? windowBackground :
    _Settings.GetXmlSrcStyleColor(XML_SRC_STYLE_MATCHING_TAG_BACKGROUND));

  // Номера стилей определены лексером XML из Lexilla (SCE_H_*). Задаём все
  // базовые XML/SGML-стили, чтобы схема не теряла читаемость на CDATA,
  // комментариях и объявлениях, а не только на обычных тегах FB2.
  static struct {
    int style;
    XmlSrcStyleToken token;
  } styles[] = {
    { SCE_H_DEFAULT,                XML_SRC_STYLE_XML_TEXT },
    { SCE_H_TAG,                    XML_SRC_STYLE_XML_TAG_NAME },
    { SCE_H_TAGUNKNOWN,             XML_SRC_STYLE_XML_TAG_NAME },
    { SCE_H_ATTRIBUTE,              XML_SRC_STYLE_XML_ATTRIBUTE_NAME },
    { SCE_H_ATTRIBUTEUNKNOWN,       XML_SRC_STYLE_XML_ATTRIBUTE_NAME },
    { SCE_H_NUMBER,                 XML_SRC_STYLE_XML_ATTRIBUTE_VALUE },
    { SCE_H_DOUBLESTRING,           XML_SRC_STYLE_XML_ATTRIBUTE_VALUE },
    { SCE_H_SINGLESTRING,           XML_SRC_STYLE_XML_ATTRIBUTE_VALUE },
    { SCE_H_OTHER,                  XML_SRC_STYLE_XML_TAG_DELIMITER },
    { SCE_H_COMMENT,                XML_SRC_STYLE_XML_COMMENT },
    { SCE_H_ENTITY,                 XML_SRC_STYLE_XML_ENTITY },
    { SCE_H_TAGEND,                 XML_SRC_STYLE_XML_TAG_DELIMITER },
    { SCE_H_XMLSTART,               XML_SRC_STYLE_XML_PROCESSING_INSTRUCTION },
    { SCE_H_XMLEND,                 XML_SRC_STYLE_XML_PROCESSING_INSTRUCTION },
    { SCE_H_SCRIPT,                 XML_SRC_STYLE_XML_ATTRIBUTE_VALUE },
    { SCE_H_ASP,                    XML_SRC_STYLE_XML_PROCESSING_INSTRUCTION },
    { SCE_H_ASPAT,                  XML_SRC_STYLE_XML_PROCESSING_INSTRUCTION },
    { SCE_H_CDATA,                  XML_SRC_STYLE_XML_CDATA },
    { SCE_H_QUESTION,               XML_SRC_STYLE_XML_PROCESSING_INSTRUCTION },
    { SCE_H_VALUE,                  XML_SRC_STYLE_XML_ATTRIBUTE_VALUE },
    { SCE_H_XCCOMMENT,              XML_SRC_STYLE_XML_COMMENT },
    { SCE_H_SGML_DEFAULT,           XML_SRC_STYLE_XML_DOCTYPE },
    { SCE_H_SGML_COMMAND,           XML_SRC_STYLE_XML_DOCTYPE },
    { SCE_H_SGML_1ST_PARAM,         XML_SRC_STYLE_XML_ATTRIBUTE_NAME },
    { SCE_H_SGML_DOUBLESTRING,      XML_SRC_STYLE_XML_ATTRIBUTE_VALUE },
    { SCE_H_SGML_SIMPLESTRING,      XML_SRC_STYLE_XML_ATTRIBUTE_VALUE },
    { SCE_H_SGML_ERROR,             XML_SRC_STYLE_XML_ERROR },
    { SCE_H_SGML_SPECIAL,           XML_SRC_STYLE_XML_DOCTYPE },
    { SCE_H_SGML_ENTITY,            XML_SRC_STYLE_XML_ENTITY },
    { SCE_H_SGML_COMMENT,           XML_SRC_STYLE_XML_COMMENT },
    { SCE_H_SGML_1ST_PARAM_COMMENT, XML_SRC_STYLE_XML_COMMENT },
    { SCE_H_SGML_BLOCK_DEFAULT,     XML_SRC_STYLE_XML_DOCTYPE },
  };
  if (_Settings.XmlSrcSyntaxHL() && !highContrast)
  {
    for (int i = 0; i < sizeof(styles) / sizeof(styles[0]); ++i)
    {
      m_source.SendMessage(SCI_STYLESETFORE, styles[i].style,
        _Settings.GetXmlSrcStyleColor(styles[i].token));
    }

  }
  m_source.SendMessage(SCI_COLOURISE, 0, -1);
  m_source.SendMessage(WM_SETREDRAW, TRUE, 0);
  m_source.Invalidate();
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

void  CMainFrame::FoldAll() {
  m_source.SendMessage(SCI_COLOURISE, 0, -1);
  int maxLine = m_source.SendMessage(SCI_GETLINECOUNT);
  bool expanding = true;
  for (int lineSeek = 0; lineSeek < maxLine; lineSeek++) {
    if (m_source.SendMessage(SCI_GETFOLDLEVEL, lineSeek) & SC_FOLDLEVELHEADERFLAG) {
      expanding = !m_source.SendMessage(SCI_GETFOLDEXPANDED, lineSeek);
      break;
    }
  }
  for (int line = 0; line < maxLine; line++) {
    int level = m_source.SendMessage(SCI_GETFOLDLEVEL, line);
    if ((level & SC_FOLDLEVELHEADERFLAG) &&
      (SC_FOLDLEVELBASE == (level & SC_FOLDLEVELNUMBERMASK))) {
      if (expanding) {
	m_source.SendMessage(SCI_SETFOLDEXPANDED, line, 1);
	ExpandFold(line, true, false, 0, level);
	line--;
      } else {
	int lineMaxSubord = m_source.SendMessage(SCI_GETLASTCHILD, line, -1);
	m_source.SendMessage(SCI_SETFOLDEXPANDED, line, 0);
	if (lineMaxSubord > line)
	  m_source.SendMessage(SCI_HIDELINES, line + 1, lineMaxSubord);
      }
    }
  }
}

void CMainFrame::ExpandFold(int &line, bool doExpand, bool force, int visLevels, int level) {
  int lineMaxSubord = m_source.SendMessage(SCI_GETLASTCHILD, line, level & SC_FOLDLEVELNUMBERMASK);
  line++;
  while (line <= lineMaxSubord) {
    if (force) {
      if (visLevels > 0)
	m_source.SendMessage(SCI_SHOWLINES, line, line);
      else
	m_source.SendMessage(SCI_HIDELINES, line, line);
    } else {
      if (doExpand)
	m_source.SendMessage(SCI_SHOWLINES, line, line);
    }
    int levelLine = level;
    if (levelLine == -1)
      levelLine = m_source.SendMessage(SCI_GETFOLDLEVEL, line);
    if (levelLine & SC_FOLDLEVELHEADERFLAG) {
      if (force) {
	if (visLevels > 1)
	  m_source.SendMessage(SCI_SETFOLDEXPANDED, line, 1);
	else
	  m_source.SendMessage(SCI_SETFOLDEXPANDED, line, 0);
	ExpandFold(line, doExpand, force, visLevels - 1);
      } else {
	if (doExpand) {
	  if (!m_source.SendMessage(SCI_GETFOLDEXPANDED, line))
	    m_source.SendMessage(SCI_SETFOLDEXPANDED, line, 1);
	  ExpandFold(line, true, force, visLevels - 1);
	} else {
	  ExpandFold(line, false, force, visLevels - 1);
	}
      }
    } else {
      line++;
    }
  }
}

void  CMainFrame::DefineMarker(int marker, int markerType, COLORREF fore,COLORREF back) {
  m_source.SendMessage(SCI_MARKERDEFINE, marker, markerType);
  m_source.SendMessage(SCI_MARKERSETFORE, marker, fore);
  m_source.SendMessage(SCI_MARKERSETBACK, marker, back);
}

void  CMainFrame::SetupSci() 
{
  // Source commands are routed explicitly by FBE; legacy WM_COMMAND events are unnecessary.
  m_source.SendMessage(SCI_SETCOMMANDEVENTS, FALSE);
  // FBE consumes SCN_MODIFIED only to keep folding state consistent.
  m_source.SendMessage(SCI_SETMODEVENTMASK, SC_MOD_CHANGEFOLD);
  m_source.SendMessage(SCI_SETUNDOSELECTIONHISTORY, AU::_ARGS.disable_undo_selection_history ? 0 :
    SC_UNDO_SELECTION_HISTORY_ENABLED | SC_UNDO_SELECTION_HISTORY_SCROLL);
	m_source.SendMessage(SCI_SETCODEPAGE,SC_CP_UTF8);
	ConfigureSourceSpecialCharacterRepresentations();
	m_source.SendMessage(SCI_SETEOLMODE,SC_EOL_CRLF);
  m_source.SendMessage(SCI_SETVIEWEOL, _Settings.XmlSrcShowEOL());
  m_source.SendMessage(SCI_SETVIEWWS, _Settings.XmlSrcShowSpace());
  m_source.SendMessage(SCI_SETWRAPMODE, _Settings.XmlSrcWrap() ? SC_WRAP_WORD : SC_WRAP_NONE);
  // added by SeNS: try to speed-up wrap mode
  m_source.SendMessage(SCI_SETLAYOUTCACHE,SC_CACHE_DOCUMENT);
  m_source.SendMessage(SCI_SETXCARETPOLICY,CARET_SLOP|CARET_EVEN,50);
  m_source.SendMessage(SCI_SETYCARETPOLICY,CARET_SLOP|CARET_EVEN,50);
  // added by SeNS: display line numbers
  UpdateSourceLineNumberMargin(true);
  m_source.SendMessage(SCI_SETMARGINWIDTHN,1,0);
  m_source.SendMessage(SCI_SETFOLDFLAGS, 16);
  m_source.SendMessage(SCI_SETPROPERTY,(WPARAM)"fold",(WPARAM)"1");
  m_source.SendMessage(SCI_SETPROPERTY,(WPARAM)"fold.html",(WPARAM)"1");
  m_source.SendMessage(SCI_SETPROPERTY,(WPARAM)"fold.compact",(WPARAM)"1");
  m_source.SendMessage(SCI_SETPROPERTY,(WPARAM)"fold.flags",(WPARAM)"16");
  // FB2 Source is XML, not a host for embedded ASP/PHP/script languages.
  m_source.SendMessage(SCI_SETPROPERTY, (WPARAM)"lexer.xml.allow.asp", (LPARAM)"0");
  m_source.SendMessage(SCI_SETPROPERTY, (WPARAM)"lexer.xml.allow.php", (LPARAM)"0");
  m_source.SendMessage(SCI_SETPROPERTY, (WPARAM)"lexer.xml.allow.scripts", (LPARAM)"0");

  // added by SeNS: disable Scintilla's control characters
  char sciCtrlChars[] = {'Q','E','R','S','K',':'};
  for (int i=0; i<sizeof(sciCtrlChars); i++)
	m_source.SendMessage(SCI_ASSIGNCMDKEY, sciCtrlChars[i]+(SCMOD_CTRL << 16), SCI_NULL);
  char sciCtrlShiftChars[] = {'Q','W','E','R','Y','O','P','A','S','D','F','G','H','K','Z','X','C','V','B','N',':'};
  for (int i=0; i<sizeof(sciCtrlShiftChars); i++)
    m_source.SendMessage(SCI_ASSIGNCMDKEY, sciCtrlShiftChars[i]+((SCMOD_CTRL+SCMOD_SHIFT) << 16), SCI_NULL);
  ///
  if (_Settings.XmlSrcSyntaxHL()) 
  {
    const bool highContrast = IsHighContrastEnabled();
    const COLORREF markerFore = highContrast ? ::GetSysColor(COLOR_WINDOW) :
      _Settings.GetXmlSrcStyleColor(XML_SRC_STYLE_LINE_NUMBER);
    const COLORREF markerBack = highContrast ? ::GetSysColor(COLOR_WINDOWTEXT) :
      _Settings.GetXmlSrcStyleColor(XML_SRC_STYLE_EDITOR_BACKGROUND);
    const COLORREF indicatorColor = highContrast ? ::GetSysColor(COLOR_HIGHLIGHT) : RGB(128, 128, 255);
    m_source.SendMessage(SCI_SETILEXER, 0, reinterpret_cast<LPARAM>(CreateEditorLexer("xml")));

    m_source.SendMessage(SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);
    m_source.SendMessage(SCI_SETMARGINWIDTHN, 2, 16);
    m_source.SendMessage(SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
    m_source.SendMessage(SCI_SETMARGINSENSITIVEN, 2, 1);
    DefineMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_MINUS, markerFore, markerBack);
    DefineMarker(SC_MARKNUM_FOLDER, SC_MARK_PLUS, markerFore, markerBack);
    DefineMarker(SC_MARKNUM_FOLDERSUB, SC_MARK_EMPTY, markerFore, markerBack);
    DefineMarker(SC_MARKNUM_FOLDERTAIL, SC_MARK_EMPTY, markerFore, markerBack);
    DefineMarker(SC_MARKNUM_FOLDEREND, SC_MARK_EMPTY, markerFore, markerBack);
    DefineMarker(SC_MARKNUM_FOLDEROPENMID, SC_MARK_EMPTY, markerFore, markerBack);
    DefineMarker(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_EMPTY, markerFore, markerBack);

	// indicator for tag match
	m_source.SendMessage(SCI_INDICSETSTYLE, EDITOR_INDICATOR_TAG_MATCH, INDIC_ROUNDBOX);
	m_source.SendMessage(SCI_INDICSETALPHA, EDITOR_INDICATOR_TAG_MATCH, 100);
	m_source.SendMessage(SCI_INDICSETUNDER, EDITOR_INDICATOR_TAG_MATCH, TRUE);
	m_source.SendMessage(SCI_INDICSETFORE,  EDITOR_INDICATOR_TAG_MATCH, indicatorColor);

	m_source.SendMessage(SCI_INDICSETSTYLE, EDITOR_INDICATOR_TAG_ATTRIBUTE, INDIC_ROUNDBOX);
	m_source.SendMessage(SCI_INDICSETALPHA, EDITOR_INDICATOR_TAG_ATTRIBUTE, 100);
	m_source.SendMessage(SCI_INDICSETUNDER, EDITOR_INDICATOR_TAG_ATTRIBUTE, TRUE);
	m_source.SendMessage(SCI_INDICSETFORE,  EDITOR_INDICATOR_TAG_ATTRIBUTE, indicatorColor);

	m_source.SendMessage(SCI_COLOURISE,0,-1);
  } 
  else 
  {
    m_source.SendMessage(SCI_SETILEXER, 0, 0);
    m_source.SendMessage(SCI_SETMARGINWIDTHN, 2, 0);
  }
}

void CMainFrame::ConfigureSourceSpecialCharacterRepresentations()
{
	// Representations affect painting only; the UTF-8 document, lexer and save path remain unchanged.
	struct SpecialCharacterRepresentation
	{
		const char* character;
		const char* label;
	};
	static const SpecialCharacterRepresentation representations[] = {
		{ "\xC2\xA0", "\xC2\xB0" },
		{ "\xC2\xAD", "\xC2\xAC" },
		{ "\xE2\x80\x8B", "ZWSP" },
		{ "\xE2\x80\x8C", "ZWNJ" },
		{ "\xE2\x80\x8D", "ZWJ" },
		{ "\xE2\x80\xAF", "NNBSP" },
		{ "\xE2\x81\xA0", "WJ" },
		{ "\xEF\xBB\xBF", "BOM" }
	};
	static const SpecialCharacterRepresentation textLabels[] = {
		{ "\xC2\xA0", "NBSP" }, { "\xC2\xAD", "SHY" },
		{ "\xE2\x80\x8B", "ZWSP" }, { "\xE2\x80\x8C", "ZWNJ" },
		{ "\xE2\x80\x8D", "ZWJ" }, { "\xE2\x80\xAF", "NNBSP" },
		{ "\xE2\x81\xA0", "WJ" }, { "\xEF\xBB\xBF", "BOM" }
	};
	const SpecialCharacterRepresentation* activeRepresentations =
		_Settings.XmlSrcSpecialCharsStyle() == XML_SRC_SPECIAL_CHARS_TEXT_LABELS ? textLabels : representations;

	for (size_t i = 0; i < _countof(representations); ++i)
	{
		if (_Settings.XmlSrcShowSpecialChars())
		{
			m_source.SendMessage(SCI_SETREPRESENTATION,
				reinterpret_cast<WPARAM>(representations[i].character),
				reinterpret_cast<LPARAM>(activeRepresentations[i].label));
			m_source.SendMessage(SCI_SETREPRESENTATIONAPPEARANCE,
				reinterpret_cast<WPARAM>(representations[i].character), SC_REPRESENTATION_PLAIN);
		}
		else
		{
			m_source.SendMessage(SCI_CLEARREPRESENTATION,
				reinterpret_cast<WPARAM>(representations[i].character));
		}
	}
}

void  CMainFrame::SciModified(const SCNotification& scn) {
  if (scn.modificationType & SC_MOD_CHANGEFOLD) {
    if (scn.foldLevelNow & SC_FOLDLEVELHEADERFLAG) {
      if (!(scn.foldLevelPrev & SC_FOLDLEVELHEADERFLAG))
	m_source.SendMessage(SCI_SETFOLDEXPANDED, scn.line, 1);
    } else if (scn.foldLevelPrev & SC_FOLDLEVELHEADERFLAG) {
      if (!m_source.SendMessage(SCI_GETFOLDEXPANDED, scn.line)) {
	// Removing the fold from one that has been contracted so should expand
	// otherwise lines are left invisible with no way to make them visible
	int tmpline=scn.line;
	ExpandFold(tmpline, true, false, 0, scn.foldLevelPrev);
      }
    }
  }
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
	if (_Settings.XmlSrcTagHL() || gotoTag)
	{
		XmlMatchedTagsHighlighter xmlTagMatchHiliter(&m_source, &m_xml_matched_tags_state);
		UIEnable(ID_GOTO_MATCHTAG, xmlTagMatchHiliter.tagMatch(_Settings.XmlSrcTagHL(), false, gotoTag));
		return true;
	}
	return false;
}

void CMainFrame::SciGotoWrongTag()
{
	CWaitCursor hourglass;
	XmlMatchedTagsHighlighter xmlTagMatchHiliter(&m_source, &m_xml_matched_tags_state);
	xmlTagMatchHiliter.gotoWrongTag();
	
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
  int lineClick = m_source.SendMessage(SCI_LINEFROMPOSITION, scn.position);
  if ((scn.modifiers & SCMOD_SHIFT) && (scn.modifiers & SCMOD_CTRL)) {
    FoldAll();
  } else {
    int levelClick = m_source.SendMessage(SCI_GETFOLDLEVEL, lineClick);
    if (levelClick & SC_FOLDLEVELHEADERFLAG) {
      if (scn.modifiers & SCMOD_SHIFT) {
	// Ensure all children visible
	m_source.SendMessage(SCI_SETFOLDEXPANDED, lineClick, 1);
	ExpandFold(lineClick, true, true, 100, levelClick);
      } else if (scn.modifiers & SCMOD_CTRL) {
	if (m_source.SendMessage(SCI_GETFOLDEXPANDED, lineClick)) {
	  // Contract this line and all children
	  m_source.SendMessage(SCI_SETFOLDEXPANDED, lineClick, 0);
	  ExpandFold(lineClick, false, true, 0, levelClick);
	} else {
	  // Expand this line and all children
	  m_source.SendMessage(SCI_SETFOLDEXPANDED, lineClick, 1);
	  ExpandFold(lineClick, true, true, 100, levelClick);
	}
      } else {
	// Toggle this line
	m_source.SendMessage(SCI_TOGGLEFOLD, lineClick);
      }
    }
  }
}


void CMainFrame::GoToSelectedTreeItem()
{
  CTreeItem ii(m_document_tree.GetSelectedItem());
  if (!ii.IsNull() && ii.GetData())
  {
    if(m_current_view != BODY)
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

LRESULT CMainFrame::OnSciCollapse(WORD cose, WORD wID, HWND, BOOL&)
{
	if(m_current_view == SOURCE)
		SciCollapse(wID - ID_SCI_COLLAPSE_BASE, false);

	if(m_document_tree.IsWindowVisible())
		m_document_tree.m_tree.m_tree.Collapse(0, wID - ID_SCI_COLLAPSE_BASE, false);

	return 0;
}

LRESULT CMainFrame::OnSciExpand(WORD cose, WORD wID, HWND, BOOL&)
{
	if(m_current_view == SOURCE)
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
	if(m_current_view == BODY && (bool)m_body_selection)
	{
		m_body_selection->select();
	}
	if(m_current_view == DESC && (bool)m_desc_selection)
	{
		m_desc_selection->select();
	}
}


void CMainFrame::SaveSelection(VIEW_TYPE vt)
{
	if ((vt == BODY || vt == DESC) && (!m_doc || !m_doc->m_body.HasDoc()))
	{
		StartupTrace::Warning(L"selection", L"E301", L"SaveSelection ignored: HTML document is unavailable");
		return;
	}
	if(vt == BODY)
	{		
		m_body_selection = m_doc->m_body.Document()->selection->createRange();		
		if (StartupTrace::Enabled() && (bool)m_body_selection)
		{
			const CString selectedText((const wchar_t*)m_body_selection->text);
			CString trace;
			trace.Format(L"SaveSelection: Body; selection-chars=%d", selectedText.GetLength());
			WriteSelectionTrace(L"E300", trace);
		}
	}
	if(vt == DESC)
	{		
		m_desc_selection = m_doc->m_body.Document()->selection->createRange();
	}
}

void CMainFrame::ClearSelection()
{
	m_body_selection = NULL;
	m_desc_selection = NULL;
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

unsigned __int64 CMainFrame::FileAge(LPCTSTR FileName)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (::GetFileAttributesEx(FileName, GetFileExInfoStandard, &data))
	{
		return *((unsigned __int64*)&data.ftLastWriteTime);
	}	
	return ~0;
}

bool CMainFrame::CheckFileTimeStamp()
{
	if(m_file_age == FileAge(m_doc->m_filename))
		return false;
	
	if(IDYES == U::MessageBox(MB_YESNO, IDS_FILE_CHANGED_CPT, IDS_FILE_CHANGED_MSG, m_doc->m_filename))
	{
		return ReloadFile();			
	}
	else
	{
		m_file_age = FileAge(m_doc->m_filename);
	}

	return false;
}

bool CMainFrame::ReloadFile()
{
	FB::Doc *doc=new FB::Doc(*this);
	FB::Doc::m_active_doc = doc;

	EnableWindow(FALSE);
	m_status.SetPaneText(ID_DEFAULT_PANE,_T("Loading..."));
	m_file_age = FileAge(m_doc->m_filename);
	bool fLoaded=doc->Load(m_view,m_doc->m_filename);
	EnableWindow(TRUE);
	if (!fLoaded) 
	{
		delete doc;
		FB::Doc::m_active_doc = m_doc;
		return false;
	}

	AttachDocument(doc);	
	delete m_doc;
	m_doc=doc;
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
	const VIEW_TYPE activeView = m_current_view;
	SetupSci();
	SetSciStyles();
	UpdateSourceLineNumberMargin(true);

	XmlMatchedTagsHighlighter xmlTagMatchHiliter(&m_source, &m_xml_matched_tags_state);
	xmlTagMatchHiliter.tagMatch(_Settings.XmlSrcTagHL(), false, false);
	UIEnable(ID_GOTO_MATCHTAG, _Settings.XmlSrcTagHL());
	// Перекраска XML-редактора не должна менять активный режим документа.
	if(activeView == BODY && m_doc)
		m_view.ActivateWnd(m_doc->m_body);
	if(saveSettings)
		_Settings.Save();
}
void CMainFrame::ApplyConfChanges(bool applyDocumentStyles)
{
	CWaitCursor hourglass;
	LONG visible = false;

	wchar_t restartMsg[MAX_LOAD_STRING + 1];
	FbeLoadString(_Module.GetResourceInstance(), IDS_SETTINGS_NEED_RESTART, restartMsg, MAX_LOAD_STRING);

	if (applyDocumentStyles && m_doc)
		m_doc->ApplyConfChanges();
	SetupSci();
	SetSciStyles();

	// added by SeNS: display line numbers
	UpdateSourceLineNumberMargin(true);

	XmlMatchedTagsHighlighter xmlTagMatchHiliter(&m_source, &m_xml_matched_tags_state);
	xmlTagMatchHiliter.tagMatch(_Settings.XmlSrcTagHL(), false, false);
	UIEnable(ID_GOTO_MATCHTAG, _Settings.XmlSrcTagHL());

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

	

	if(_Settings.NeedRestart() && MessageBox(restartMsg, L"", MB_YESNO | MB_ICONINFORMATION) == IDYES)
	{
		return RestartProgram();
	}
}

void CMainFrame::RestartProgram()
{	
	BOOL b = false;
	if(OnClose(0, 0, 0, b))
	{
		const CString filename = U::GetModulePath(_Module.GetModuleInstance());
		CString ofn = m_doc->GetOpenFileName();
//		if(wcschr(filename, L' '))
		ofn.Format(L"\"%s\"", m_doc->GetOpenFileName());
		HINSTANCE hInst = ShellExecute(0, L"open", filename, ofn, 0, SW_SHOW);
	}
}

void CMainFrame::LoadScriptPicture(ScrInfo& item, const CString& path, const CString& baseName)
{
	item.picture = NULL;
	item.pictType = CMainFrame::NO_PICT;

	const CString basePath = path + baseName;
	const CString bitmapPath = basePath + L".bmp";
	const CString iconPath = basePath + L".ico";
	const DWORD bitmapAttributes = ::GetFileAttributes(bitmapPath);
	const DWORD iconAttributes = ::GetFileAttributes(iconPath);

	if(bitmapAttributes != INVALID_FILE_ATTRIBUTES &&
		(bitmapAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
	{
		HBITMAP bitmap = (HBITMAP)::LoadImage(
			NULL, bitmapPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		if(bitmap != NULL)
		{
			item.picture = bitmap;
			item.pictType = CMainFrame::BITMAP;
		}
	}
	else if(iconAttributes != INVALID_FILE_ATTRIBUTES &&
		(iconAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
	{
		HICON icon = (HICON)::LoadImage(
			NULL, iconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
		if(icon != NULL)
		{
			item.picture = icon;
			item.pictType = CMainFrame::ICON;
		}
	}
}

void CMainFrame::CollectScripts(CString path, TCHAR* mask, int lastid, CString refid)
{
	if(U::HasFilesWithExt(path, mask))
	{
		lastid = GrabScripts(path, mask, refid);
	}

	if(U::HasSubFolders(path))
	{
		WIN32_FIND_DATA fd;
		HANDLE found = FindFirstFile(path + L"*.*", &fd);
		if(found != INVALID_HANDLE_VALUE)
		{
			do
			{
				if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if(wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..") && U::HasScriptsEndpoint((path + 
						fd.cFileName) + L"\\", mask))
					{
						ScrInfo folder;

						CString name(fd.cFileName);
						wchar_t* pos = wcschr(fd.cFileName, L'_');

						folder.order = L"0_";

						if(!pos || !U::CheckScriptsVersion(fd.cFileName))
						{
							folder.order += name;
						}
						else
						{
							name = pos + 1;
							folder.order = fd.cFileName;
						}

						folder.name = name;
						folder.relativePath = NormalizeScriptRelativePath(_Settings.GetScriptsFolder(), path + fd.cFileName);

						CString temp;
						temp.Format(L"_%d", lastid);
						folder.id = refid + temp;
						folder.refid = refid;
						folder.isFolder = true;
						//folder.accel.key = 0;

						LoadScriptPicture(folder, path, fd.cFileName);

						m_scripts.Add(folder);
						
						CollectScripts((path + fd.cFileName) + L"\\", mask, 1, folder.id);
						lastid++;
					}
				}
			} while (FindNextFile(found, &fd));

			FindClose(found);
		}		
	}
}

int CMainFrame::GrabScripts(CString path, TCHAR* mask, CString refid)
{
	WIN32_FIND_DATA fd;
	HANDLE found = FindFirstFile(path + mask, &fd);
	int newid = 1;
	
	if(found != INVALID_HANDLE_VALUE)
	 {
		do
		{
			if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				if (StartScript(this) ==0 && SUCCEEDED(ScriptLoad(path + fd.cFileName)) && ScriptFindFunc(L"Run"))
				{
					{
						ScrInfo script;
						CString name(fd.cFileName);
						wchar_t* pos = wcschr(fd.cFileName, L'_');

						script.order = L"0_";

						if(!pos || !U::CheckScriptsVersion(fd.cFileName))
						{
							script.order += name;
						}
						else
						{
							name = pos + 1;
							script.order = fd.cFileName;
						}

						name.Delete(name.GetLength() - 3, 3);
						script.name = name;
						script.path = path + fd.cFileName;
						script.relativePath = NormalizeScriptRelativePath(_Settings.GetScriptsFolder(), script.path);

						CString pictureName(fd.cFileName);
						pictureName.Delete(pictureName.GetLength() - 3, 3);
						LoadScriptPicture(script, path, pictureName);

						/*CComVariant accel;
						ZeroMemory(&script.accel, sizeof(script.accel));
						script.accel.key = 0;*/

						/*if (SUCCEEDED(ScriptCall(L"SetHotkey", NULL, 0, &accel)))
						{
							TCHAR errCaption[MAX_LOAD_STRING + 1];
							FbeLoadString(_Module.GetResourceInstance(), IDS_ERRMSGBOX_CAPTION, errCaption, MAX_LOAD_STRING);

							int j;
							for(j = 0; j < m_scripts.GetSize(); ++j)
							{
								if(m_scripts[j].accel.key == accel.intVal && m_scripts[j].accel.key != 0 && !m_scripts[j].isFolder)
								{
									if(_Settings.GetScriptsHkErrNotify())
									{
										CString errDescr;
										errDescr.Format(IDS_SCRIPT_HOTKEY_CONFLICT, m_scripts[j].path, script.path);									
										MessageBox(errDescr.GetBuffer(), errCaption, MB_OK|MB_ICONSTOP);
									}
									break;
								}
							}
							if(j == m_scripts.GetSize() && keycodes.FindKey(accel.intVal) != -1)
								script.accel.key = accel.intVal;
						}*/
						
						CString temp;
						temp.Format(L"_%d", newid);
						script.id = refid + temp;
						script.refid = refid;
						script.isFolder = false;
						script.Type = 2;			
						m_scripts.Add(script);
						newid++;

					}
					StopScript();
				}				
			 }		 
		 }
		 while(FindNextFile(found, &fd));

		 FindClose(found);
	 }	

	 return newid;
}

void CMainFrame::AssignScriptCommandIds()
{
	std::vector<ScriptCommandId> ids = ParseScriptCommandIds(_Settings.GetScriptCommandIds());
	bool changed = false;

	for(int i = 0; i < m_scripts.GetSize(); ++i)
	{
		ScrInfo& script = m_scripts[i];
		if(script.isFolder)
		{
			script.wID = -1;
			continue;
		}

		int commandId = 0;
		if(script.relativePath.IsEmpty())
		{
			script.wID = -1;
			StartupTrace::Event(L"script", L"S120", L"script path is outside Scripts root");
			continue;
		}

		if(!ContainsScriptPath(ids, script.relativePath, &commandId))
		{
			const int firstCandidate = static_cast<int>(HashScriptRelativePath(script.relativePath) % SCRIPT_COMMAND_COUNT) + 1;
			for(int attempt = 0; attempt < SCRIPT_COMMAND_COUNT; ++attempt)
			{
				const int candidate = ((firstCandidate - 1 + attempt) % SCRIPT_COMMAND_COUNT) + 1;
				if(!IsScriptCommandIdUsed(ids, candidate))
				{
					ScriptCommandId id = { script.relativePath, candidate };
					ids.push_back(id);
					commandId = candidate;
					changed = true;
					break;
				}
			}
		}

		script.wID = commandId ? commandId : -1;
		if(script.wID == -1)
			StartupTrace::Event(L"script", L"S121", L"script command ID capacity exhausted");
	}

	if(changed)
		_Settings.SetScriptCommandIds(SerializeScriptCommandIds(ids));
}

void CMainFrame::AddScriptsSubMenu(HMENU parentItem, CString refid, CSimpleArray<ScrInfo>& scripts, int& nextFolderMenuId)
{
	MENUITEMINFO mi;
	int menupos = 0;

	for(int i = 0; i < scripts.GetSize(); ++i)
	{
		memset(&mi, NULL, sizeof(MENUITEMINFO));
		mi.cbSize = sizeof(MENUITEMINFO);
		mi.fMask = MIIM_TYPE | MIIM_STATE;
		mi.fType = MFT_STRING;

		for(int j = 0; j < scripts.GetSize(); j++)
		{
			if(scripts[j].refid == refid)
				menupos++;
		}

		if (scripts[i].refid == refid)
		{
			if(scripts[i].isFolder)
			{
				mi.fMask |= MIIM_SUBMENU | MIIM_ID;
				mi.hSubMenu = CreateMenu();
				mi.wID = nextFolderMenuId < SCRIPT_FOLDER_MENU_ID_COUNT ? SCRIPT_FOLDER_MENU_ID_BASE + nextFolderMenuId++ : 0;
				scripts[i].wID = -1;
				AddScriptsSubMenu(mi.hSubMenu, scripts[i].id, scripts, nextFolderMenuId);
			}
			else
			{
				if(scripts[i].wID < 1)
					continue;
				mi.fMask |= MIIM_ID;
				mi.wID = ID_SCRIPT_BASE + scripts[i].wID;

				InitScriptHotkey(scripts[i]);
			}

			mi.dwTypeData = scripts[i].name.GetBuffer();
			mi.cch = wcslen(scripts[i].name);

			if(scripts[i].isFolder)
				InsertMenuItem(parentItem, 0, true, &mi);
			else
			{
				InsertMenuItem(parentItem, menupos--, true, &mi);
				// added by SeNS: add scripts with icon to toolbar
				if (scripts[i].pictType == CMainFrame::ICON)
					AddTbButton(m_ScriptsToolbar, scripts[i].name, mi.wID, TBSTATE_ENABLED, (HICON)scripts[i].picture);
			}

			if(!scripts[i].isFolder || mi.wID != 0) switch(scripts[i].pictType)
			{
				case CMainFrame::BITMAP:
					m_MenuBar.AddBitmap((HBITMAP)scripts[i].picture, mi.wID);
					break;
				case CMainFrame::ICON:
					m_MenuBar.AddIcon((HICON)scripts[i].picture, mi.wID);
					break;
			}
		}
	}
}

void CMainFrame::ReleaseScriptResources()
{
	// InitPlugins may be requested more than once.  Return the physical scripts
	// toolbar and its customization catalog to the resource baseline before the
	// next scan, otherwise every scan appends another copy of icon scripts.
	if(::IsWindow(m_ScriptsToolbar))
	{
		const int defaultsIndex = m_aDefaultButtons.FindKey(static_cast<int>(reinterpret_cast<INT_PTR>(m_ScriptsToolbar.m_hWnd)));
		const int catalogIndex = m_aButtons.FindKey(static_cast<int>(reinterpret_cast<INT_PTR>(m_ScriptsToolbar.m_hWnd)));
		if(defaultsIndex >= 0 && catalogIndex >= 0)
		{
			TBBUTTONS defaults = m_aDefaultButtons.GetValueAt(defaultsIndex);
			while(m_ScriptsToolbar.GetButtonCount() > 0) m_ScriptsToolbar.DeleteButton(0);
			if(defaults.GetSize() > 0) m_ScriptsToolbar.AddButtons(defaults.GetSize(), defaults.GetData());
			m_aButtons.SetAt(static_cast<int>(reinterpret_cast<INT_PTR>(m_ScriptsToolbar.m_hWnd)), defaults);
			m_ScriptsToolbar.AutoSize();
		}
	}
	for(int i = 0; i < m_scripts.GetSize(); ++i)
	{
		ScrInfo& script = m_scripts[i];
		if(script.picture != NULL)
		{
			if(script.pictType == BITMAP)
				::DeleteObject(static_cast<HBITMAP>(script.picture));
			else if(script.pictType == ICON)
				::DestroyIcon(static_cast<HICON>(script.picture));
		}
		script.picture = NULL;
		script.pictType = NO_PICT;
	}
	for(int i = 0; i < m_scripts_images.GetSize(); ++i)
		if(m_scripts_images.GetValueAt(i) != NULL)
			::DeleteObject(m_scripts_images.GetValueAt(i));
	m_scripts_images.RemoveAll();
	m_scripts.RemoveAll();
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

void CMainFrame::SortScripts()
{
	std::vector<ScrInfo> sorted;
	sorted.reserve(m_scripts.GetSize());
	for(int i = 0; i < m_scripts.GetSize(); ++i)
		sorted.push_back(m_scripts[i]);
	std::sort(sorted.begin(), sorted.end(), [](const ScrInfo& left, const ScrInfo& right)
	{
		if(left.isFolder != right.isFolder)
			return left.isFolder;
		const int order = left.order.CompareNoCase(right.order);
		if(order != 0) return order < 0;
		return left.relativePath.CompareNoCase(right.relativePath) < 0;
	});
	for(int i = 0; i < static_cast<int>(sorted.size()); ++i)
		m_scripts[i] = sorted[i];
}

void CMainFrame::InitScriptHotkey(CMainFrame::ScrInfo& script)
{
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
				ID_SCRIPT_BASE + script.wID,
				NULL,
				script.relativePath);
			hotkey_groups.at(i).m_hotkeys.push_back(ScriptsHotkey);
		}
	}
}

void CMainFrame::InitPluginHotkey(CString guid, UINT cmd, CString name)
{
	std::vector<CHotkeysGroup>& hotkey_groups = _Settings.m_hotkey_groups;
	for(unsigned int i = 0; i < hotkey_groups.size(); ++i)
	{
		if(hotkey_groups.at(i).m_reg_name == L"Plugins")
		{
			CHotkey PluginsHotkey(guid,
				name,
				NULL,
				cmd,
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
	return m_current_view == SOURCE ? m_last_sci_ovr :
		m_current_view == BODY ? m_last_ie_ovr : false;
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
	const bool fbd = m_doc && IsFbdFile(m_doc->m_filename);
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
		m_current_view == SOURCE, m_current_view == BODY && m_doc != NULL);
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
	if (m_current_view == SOURCE)
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
	else if (m_current_view == BODY && m_doc && m_doc->m_body.Document())
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
