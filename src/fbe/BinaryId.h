#pragma once

#include <atlstr.h>
#include <windows.h>

// xs:ID is derived from XML NCName.  File-system names are often already
// valid IDs (including Unicode letters), so retain them verbatim.  For names
// that need normalization, replace only invalid characters and ensure an
// XML-name start character without transliterating the rest of the name.
namespace FbeBinary
{
	inline bool IsXmlIdStart(TCHAR value)
	{
		return value == _T('_') || ::IsCharAlphaW(value) != FALSE;
	}

	inline bool IsXmlIdCharacter(TCHAR value)
	{
		return IsXmlIdStart(value) || (value >= _T('0') && value <= _T('9')) ||
			value == _T('-') || value == _T('.');
	}

	inline CString NormalizeXmlId(const CString& pathOrId)
	{
		int start = pathOrId.ReverseFind(_T('\\'));
		const int slash = pathOrId.ReverseFind(_T('/'));
		if (slash > start) start = slash;
		CString source = pathOrId.Mid(start < 0 ? 0 : start + 1);
		if (source.IsEmpty()) return CString(L"image");

		bool valid = IsXmlIdStart(source[0]);
		for (int index = 1; valid && index < source.GetLength(); ++index)
			valid = IsXmlIdCharacter(source[index]);
		if (valid) return source;

		CString normalized;
		for (int index = 0; index < source.GetLength(); ++index)
			normalized.AppendChar(IsXmlIdCharacter(source[index]) ? source[index] : _T('_'));
		if (normalized.IsEmpty()) normalized = L"image";
		if (!IsXmlIdStart(normalized[0])) normalized.Insert(0, _T('_'));
		return normalized;
	}
}
