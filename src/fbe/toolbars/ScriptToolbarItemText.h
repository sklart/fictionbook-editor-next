#pragma once

#include "PortableToolbarLayout.h"

namespace ScriptToolbarItemText
{
inline CString Name(const PortableToolbarItem& item)
{
	if(!item.scriptName.IsEmpty()) return item.scriptName;
	CString name(item.relativePath); name.Replace(L'\\', L'/');
	const int slash = name.ReverseFind(L'/');
	if(slash >= 0) name = name.Mid(slash + 1);
	if(name.Right(3).CompareNoCase(L".js") == 0) name.Delete(name.GetLength() - 3, 3);
	return name;
}

inline CString UnavailableLabel(const PortableToolbarItem& item, const CString& unavailable)
{
	CString text = Name(item);
	if(!item.relativePath.IsEmpty()) text += (text.IsEmpty() ? L"" : L" — ") + item.relativePath;
	if(text.IsEmpty()) text = item.scriptUid;
	return text.IsEmpty() ? unavailable : text + L" (" + unavailable + L")";
}

inline CString UnavailableDetails(const PortableToolbarItem& item, const CString& unavailable)
{
	CString text(unavailable), name(Name(item));
	if(!name.IsEmpty()) text += L"\n" + name;
	if(!item.relativePath.IsEmpty()) text += L"\n" + item.relativePath;
	if(!item.scriptUid.IsEmpty()) text += L"\n" + item.scriptUid;
	return text;
}
}
