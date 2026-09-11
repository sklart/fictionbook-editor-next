#include "stdafx.h"
#include "PortableToolbarStore.h"
#include "..\\..\\common\\DeploymentContext.h"

namespace
{
CString PortableToolbarsPath() { return CString(DeploymentContext::SettingsDirectory().c_str()) + L"Toolbars.xml"; }
CString XmlEscape(const CString& value) { CString escaped(value); escaped.Replace(L"&", L"&amp;"); escaped.Replace(L"\"", L"&quot;"); escaped.Replace(L"<", L"&lt;"); escaped.Replace(L">", L"&gt;"); return escaped; }
CString XmlUnescape(const CString& value) { CString unescaped(value); unescaped.Replace(L"&quot;", L"\""); unescaped.Replace(L"&lt;", L"<"); unescaped.Replace(L"&gt;", L">"); unescaped.Replace(L"&amp;", L"&"); return unescaped; }
bool WriteText(const CString& text)
{
	const CString directory(DeploymentContext::SettingsDirectory().c_str()); if(!::CreateDirectory(directory, NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS) return false;
	const CString path = PortableToolbarsPath(), temporary = path + L".tmp"; HANDLE file = ::CreateFile(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); if(file == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0; const DWORD bytes = static_cast<DWORD>(text.GetLength() * sizeof(wchar_t)); const bool ok = ::WriteFile(file, text, bytes, &written, NULL) != FALSE && written == bytes; if(ok) ::FlushFileBuffers(file); ::CloseHandle(file);
	if(!ok || !::MoveFileEx(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { ::DeleteFile(temporary); return false; } return true;
}
void AppendToolbar(CString& xml, const wchar_t* name, const std::vector<PortableToolbarItem>& items)
{
	xml.AppendFormat(L"  <Toolbar name=\"%s\">\r\n", name); for(size_t index = 0; index < items.size(); ++index) { const PortableToolbarItem& item = items[index]; if(item.separator) xml.AppendFormat(L"    <Separator width=\"%d\" />\r\n", item.width); else if(!item.relativePath.IsEmpty()) xml.AppendFormat(L"    <Script path=\"%s\" />\r\n", static_cast<LPCWSTR>(XmlEscape(item.relativePath))); else if(item.command != 0) xml.AppendFormat(L"    <Command id=\"%d\" />\r\n", item.command); } xml.Append(L"  </Toolbar>\r\n");
}
}

bool PortableToolbarStore::Load(PortableToolbarLayout& layout)
{
	const CString path = PortableToolbarsPath(); HANDLE file = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); if(file == INVALID_HANDLE_VALUE) return false;
	const DWORD length = ::GetFileSize(file, NULL); if(length == INVALID_FILE_SIZE || length > 256 * 1024 || (length % sizeof(wchar_t)) != 0) { ::CloseHandle(file); return false; }
	std::vector<wchar_t> text(length / sizeof(wchar_t) + 1, 0); DWORD read = 0; const BOOL ok = ::ReadFile(file, &text[0], length, &read, NULL); ::CloseHandle(file); if(!ok || read != length) return false;
	CString content(&text[0]); if(content.Find(L"<Toolbars version=\"1\">") < 0 || content.Find(L"</Toolbars>") < 0) return false;
	PortableToolbarLayout parsed; int cursor = 0; CString active; bool closedRoot = false;
	while(cursor >= 0) { const int start = content.Find(L'<', cursor); if(start < 0) break; const int end = content.Find(L'>', start + 1); if(end < 0) return false; CString tag = content.Mid(start + 1, end - start - 1); cursor = end + 1;
		if(tag == L"/Toolbars") { if(!active.IsEmpty()) return false; closedRoot = true; break; }
		if(tag.Left(8) == L"Toolbar ") { if(!active.IsEmpty()) return false; active = tag.Find(L"name=\"Command\"") >= 0 ? L"Command" : tag.Find(L"name=\"Scripts\"") >= 0 ? L"Scripts" : CString(); if(active == L"Command") parsed.commandToolbarPresent = true; if(active == L"Scripts") parsed.scriptsToolbarPresent = true; continue; }
		if(tag.Left(8) == L"/Toolbar") { if(active.IsEmpty()) return false; active.Empty(); continue; }
		if(active.IsEmpty()) { if(tag.Left(10) == L"LastScript") { const int value = tag.Find(L"path=\""); const int tail = value >= 0 ? tag.Find(L'\"', value + 6) : -1; if(value < 0 || tail < 0) return false; parsed.lastScript = XmlUnescape(tag.Mid(value + 6, tail - value - 6)); } continue; }
		PortableToolbarItem item = {}; if(tag.Left(9) == L"Separator") { item.separator = true; const int width = tag.Find(L"width=\""); if(width >= 0) item.width = _wtoi(tag.Mid(width + 7)); } else if(tag.Left(7) == L"Command") { const int id = tag.Find(L"id=\""); if(id < 0) return false; item.command = _wtoi(tag.Mid(id + 4)); } else if(tag.Left(6) == L"Script") { const int value = tag.Find(L"path=\""); const int tail = value >= 0 ? tag.Find(L'\"', value + 6) : -1; if(value < 0 || tail < 0) return false; item.relativePath = XmlUnescape(tag.Mid(value + 6, tail - value - 6)); } else continue;
		(active == L"Command" ? parsed.commands : parsed.scripts).push_back(item);
	}
	if(!closedRoot || (!parsed.commandToolbarPresent && !parsed.scriptsToolbarPresent)) return false; layout = parsed; return true;
}
bool PortableToolbarStore::Save(const PortableToolbarLayout& layout)
{
	CString xml(L"<Toolbars version=\"1\">\r\n"); if(layout.commandToolbarPresent) AppendToolbar(xml, L"Command", layout.commands); if(layout.scriptsToolbarPresent) AppendToolbar(xml, L"Scripts", layout.scripts); if(!layout.lastScript.IsEmpty()) xml.AppendFormat(L"  <LastScript path=\"%s\" />\r\n", static_cast<LPCWSTR>(XmlEscape(layout.lastScript))); xml.Append(L"</Toolbars>\r\n"); return WriteText(xml);
}
