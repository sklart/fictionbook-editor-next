#include "stdafx.h"
#include "HotkeyExport.h"
#include "..\\..\\RuntimeLocalization.h"
#include "..\\..\\utils\\utils.h"

namespace
{
CString GetHotkeyDisplayName(const CHotkey& hotkey)
{
	return hotkey.m_name_resource_id ? FbeLoadRuntimeString(hotkey.m_name_resource_id, hotkey.m_name) : hotkey.m_name;
}

CString GetHotkeyGroupDisplayName(const CHotkeysGroup& group)
{
	return group.m_name_resource_id ? FbeLoadRuntimeString(group.m_name_resource_id, group.m_name) : group.m_name;
}

CString EscapeHtml(const CString& text)
{
	CString escaped;
	for(int index = 0; index < text.GetLength(); ++index)
	{
		switch(text[index])
		{
		case L'&': escaped += L"&amp;"; break;
		case L'<': escaped += L"&lt;"; break;
		case L'>': escaped += L"&gt;"; break;
		case L'\"': escaped += L"&quot;"; break;
		default: escaped += text[index]; break;
		}
	}
	return escaped;
}

CString HtmlLanguage(LPCWSTR localeName)
{
	if(localeName != NULL && localeName[0] != 0)
	{
		CString language(localeName);
		const int separator = language.Find(L'-');
		if(separator > 0) language = language.Left(separator);
		return language;
	}
	return L"en";
}
}

namespace FbeSettings { namespace Hotkeys {
ExportData BuildExportData(const std::vector<CHotkeysGroup>& groups)
{
	ExportData data;
	for(size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
	{
		const CHotkeysGroup& group = groups[groupIndex];
		ExportGroup exportGroup;
		exportGroup.displayName = GetHotkeyGroupDisplayName(group);
		for(size_t hotkeyIndex = 0; hotkeyIndex < group.m_hotkeys.size(); ++hotkeyIndex)
		{
			const CHotkey& hotkey = group.m_hotkeys[hotkeyIndex];
			if(hotkey.m_accel.key == 0) continue;
			ExportRow row;
			row.commandName = GetHotkeyDisplayName(hotkey);
			row.shortcut = U::AccelToString(hotkey.m_accel);
			exportGroup.rows.push_back(row);
		}
		if(!exportGroup.rows.empty()) data.push_back(exportGroup);
	}
	return data;
}

CString BuildTextExport(const ExportData& data)
{
	CString text(L"FictionBook Editor Next\r\n");
	text += FbeLoadRuntimeStringByKey(L"fbe.hotkey.export.title", L"Hotkeys");
	text += L"\r\n";
	for(size_t groupIndex = 0; groupIndex < data.size(); ++groupIndex)
	{
		const ExportGroup& group = data[groupIndex];
		text += L"\r\n" + group.displayName + L"\r\n";
		for(size_t rowIndex = 0; rowIndex < group.rows.size(); ++rowIndex)
			text += group.rows[rowIndex].commandName + L"\t" + group.rows[rowIndex].shortcut + L"\r\n";
	}
	return text;
}

CString BuildTextExport(const std::vector<CHotkeysGroup>& groups)
{
	return BuildTextExport(BuildExportData(groups));
}

CString BuildHtmlExport(const ExportData& data, LPCWSTR localeName)
{
	const CString title(FbeLoadRuntimeStringByKey(L"fbe.hotkey.export.title", L"Hotkeys"));
	const CString command(FbeLoadRuntimeStringByKey(L"fbe.hotkey.export.column.command", L"Command"));
	const CString shortcut(FbeLoadRuntimeStringByKey(L"fbe.hotkey.export.column.shortcut", L"Shortcut"));
	CString html;
	html.Format(L"<!doctype html>\r\n<html lang=\"%s\">\r\n<head>\r\n<meta charset=\"utf-8\">\r\n<title>%s — FictionBook Editor Next</title>\r\n"
		L"<style>body{font-family:system-ui,-apple-system,Segoe UI,sans-serif;margin:2rem auto;max-width:58rem;padding:0 1rem;color:#222}h1{margin-bottom:.1rem}h2{font-size:1.25rem;font-weight:500;margin-top:0;color:#555}table{width:100%%;border-collapse:collapse;margin-top:1.5rem}th,td{padding:.5rem .65rem;border-bottom:1px solid #d8d8d8;text-align:left}.group th{background:#f1f3f5;border-top:1px solid #bfc5ca;font-size:1rem}.shortcut{white-space:nowrap;width:1%%}kbd{font:inherit;padding:.08rem .35rem;border:1px solid #aaa;border-radius:.25rem;background:#fafafa}@media print{body{margin:0;max-width:none}tr{break-inside:avoid}}</style>\r\n</head>\r\n<body>\r\n<h1>FictionBook Editor Next</h1>\r\n<h2>%s</h2>\r\n<table>\r\n<thead><tr><th>%s</th><th>%s</th></tr></thead>\r\n",
		static_cast<LPCWSTR>(HtmlLanguage(localeName)), static_cast<LPCWSTR>(EscapeHtml(title)), static_cast<LPCWSTR>(EscapeHtml(title)), static_cast<LPCWSTR>(EscapeHtml(command)), static_cast<LPCWSTR>(EscapeHtml(shortcut)));
	for(size_t groupIndex = 0; groupIndex < data.size(); ++groupIndex)
	{
		const ExportGroup& group = data[groupIndex];
		html += L"<tbody>\r\n<tr class=\"group\"><th colspan=\"2\">" + EscapeHtml(group.displayName) + L"</th></tr>\r\n";
		for(size_t rowIndex = 0; rowIndex < group.rows.size(); ++rowIndex)
		{
			const ExportRow& row = group.rows[rowIndex];
			html += L"<tr><td>" + EscapeHtml(row.commandName) + L"</td><td class=\"shortcut\"><kbd>" + EscapeHtml(row.shortcut) + L"</kbd></td></tr>\r\n";
		}
		html += L"</tbody>\r\n";
	}
	html += L"</table>\r\n</body>\r\n</html>\r\n";
	return html;
}

CString BuildHtmlExport(const std::vector<CHotkeysGroup>& groups, LPCWSTR localeName)
{
	return BuildHtmlExport(BuildExportData(groups), localeName);
}

bool WriteUtf8ExportFile(const CString& path, const CString& text)
{
	const int size = ::WideCharToMultiByte(CP_UTF8, 0, text, text.GetLength(), NULL, 0, NULL, NULL);
	if(size == 0 && !text.IsEmpty()) return false;
	std::vector<char> bytes(static_cast<size_t>(size));
	if(size && !::WideCharToMultiByte(CP_UTF8, 0, text, text.GetLength(), &bytes[0], size, NULL, NULL)) return false;
	HANDLE file = ::CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) return false;
	const BYTE bom[] = { 0xEF, 0xBB, 0xBF }; DWORD written = 0;
	const bool ok = ::WriteFile(file, bom, sizeof(bom), &written, NULL) != FALSE && written == sizeof(bom) &&
		(size == 0 || (::WriteFile(file, &bytes[0], static_cast<DWORD>(bytes.size()), &written, NULL) != FALSE && written == bytes.size()));
	::CloseHandle(file); return ok;
}
}}