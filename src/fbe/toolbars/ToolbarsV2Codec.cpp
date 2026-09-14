#include "stdafx.h"
#include "ToolbarsV2Codec.h"

namespace
{
CString Escape(const CString& value) { CString result(value); result.Replace(L"&", L"&amp;"); result.Replace(L"\"", L"&quot;"); result.Replace(L"<", L"&lt;"); result.Replace(L">", L"&gt;"); return result; }
CString Unescape(const CString& value) { CString result(value); result.Replace(L"&quot;", L"\""); result.Replace(L"&lt;", L"<"); result.Replace(L"&gt;", L">"); result.Replace(L"&amp;", L"&"); return result; }
bool Attr(const CString& tag, const wchar_t* key, CString& value) { CString prefix(key); prefix += L"=\""; int at = tag.Find(prefix); if(at < 0) return false; at += prefix.GetLength(); const int end = tag.Find(L'\"', at); if(end < 0) return false; value = Unescape(tag.Mid(at, end - at)); return true; }
void WriteItems(CString& xml, const std::vector<PortableToolbarItem>& items) { for(size_t i = 0; i < items.size(); ++i) { const PortableToolbarItem& item = items[i]; if(item.separator) xml.AppendFormat(L"    <Separator width=\"%d\" />\r\n", item.width); else if(!item.scriptUid.IsEmpty()) xml.AppendFormat(L"    <Script uid=\"%s\" />\r\n", static_cast<LPCWSTR>(Escape(item.scriptUid))); else if(item.command) xml.AppendFormat(L"    <Command id=\"%d\" />\r\n", item.command); } }
}

CString ToolbarsV2Codec::Serialize(const PortableToolbarLayout& layout)
{
	CString xml(L"<Toolbars version=\"2\">\r\n"); if(layout.commandToolbarPresent) { xml.Append(L"  <Toolbar id=\"commands\" kind=\"commands\" name=\"Commands\">\r\n"); WriteItems(xml, layout.commands); xml.Append(L"  </Toolbar>\r\n"); }
	std::vector<ScriptToolbarDefinition> definitions = layout.scriptToolbars; if(definitions.empty() && layout.scriptsToolbarPresent) { ScriptToolbarDefinition main; main.id = L"scripts-main"; main.name = L"Scripts"; main.items = layout.scripts; definitions.push_back(main); }
	for(size_t i = 0; i < definitions.size(); ++i) { const ScriptToolbarDefinition& d = definitions[i]; xml.AppendFormat(L"  <Toolbar id=\"%s\" kind=\"scripts\" name=\"%s\" visible=\"%d\">\r\n", static_cast<LPCWSTR>(Escape(d.id)), static_cast<LPCWSTR>(Escape(d.name)), d.visible ? 1 : 0); WriteItems(xml, d.items); xml.Append(L"  </Toolbar>\r\n"); }
	if(!layout.lastScript.IsEmpty()) xml.AppendFormat(L"  <LastScript uid=\"%s\" />\r\n", static_cast<LPCWSTR>(Escape(layout.lastScript))); xml.Append(L"</Toolbars>\r\n"); return xml;
}

bool ToolbarsV2Codec::Parse(const CString& xml, PortableToolbarLayout& layout)
{
	if(xml.Find(L"<Toolbars version=\"2\">") < 0 || xml.Find(L"</Toolbars>") < 0) return false; PortableToolbarLayout parsed; ScriptToolbarDefinition* current = NULL; bool commands = false; int cursor = 0;
	while(true) { const int begin = xml.Find(L'<', cursor); if(begin < 0) break; const int end = xml.Find(L'>', begin + 1); if(end < 0) return false; const CString tag = xml.Mid(begin + 1, end - begin - 1); cursor = end + 1;
		if(tag.Left(8) == L"Toolbar ") { if(current != NULL || commands) return false; CString id, kind, name; if(!Attr(tag,L"id",id) || !Attr(tag,L"kind",kind) || id.IsEmpty()) return false; if(kind == L"commands") { if(id != L"commands" || parsed.commandToolbarPresent) return false; parsed.commandToolbarPresent = true; commands = true; } else if(kind == L"scripts") { for(size_t i = 0; i < parsed.scriptToolbars.size(); ++i) if(parsed.scriptToolbars[i].id == id) return false; ScriptToolbarDefinition d; d.id = id; Attr(tag,L"name",name); d.name = name.IsEmpty() ? id : name; d.visible = tag.Find(L"visible=\"0\"") < 0; parsed.scriptToolbars.push_back(d); current = &parsed.scriptToolbars.back(); if(id == L"scripts-main") parsed.scriptsToolbarPresent = true; } else return false; continue; }
		if(tag == L"/Toolbar") { if(current == NULL && !commands) return false; current = NULL; commands = false; continue; }
		if(tag.Left(10) == L"LastScript") { if(!Attr(tag,L"uid",parsed.lastScript)) return false; continue; }
		if(current == NULL && !commands) continue; PortableToolbarItem item = {}; if(tag.Left(9) == L"Separator") { item.separator = true; CString width; if(Attr(tag,L"width",width)) item.width=_wtoi(width); } else if(tag.Left(7) == L"Command") { CString id; if(!Attr(tag,L"id",id)) return false; item.command=_wtoi(id); } else if(tag.Left(6) == L"Script") { if(!Attr(tag,L"uid",item.scriptUid)) return false; } else continue; if(commands) parsed.commands.push_back(item); else { current->items.push_back(item); if(current->id == L"scripts-main") parsed.scripts.push_back(item); }
	}
	if(current != NULL || commands) return false; layout=parsed; return true;
}
