#pragma once
#include <atlstr.h>
namespace FbeScripts {
inline CString JsonEscape(const CString& value)
{
	CString escaped;
	for(int index = 0; index < value.GetLength(); ++index)
	{
		const wchar_t character = value[index];
		switch(character)
		{
		case L'\\': escaped += L"\\\\"; break;
		case L'\"': escaped += L"\\\""; break;
		case L'\r': escaped += L"\\r"; break;
		case L'\n': escaped += L"\\n"; break;
		case L'\t': escaped += L"\\t"; break;
		default:
			if(character < 0x20) { CString encoded; encoded.Format(L"\\u%04X", static_cast<unsigned int>(character)); escaped += encoded; }
			else escaped += character;
		}
	}
	return escaped;
}
}
