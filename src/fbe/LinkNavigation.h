#pragma once

#include <algorithm>
#include <cwctype>
#include <string>

namespace FBELinkNavigation
{
inline std::wstring Trim(const std::wstring& value)
{
	std::wstring::size_type first = 0, last = value.size();
	while(first < last && iswspace(value[first])) ++first;
	while(last > first && iswspace(value[last - 1])) --last;
	return value.substr(first, last - first);
}

inline std::wstring Lower(const std::wstring& value)
{
	std::wstring result(value);
	std::transform(result.begin(), result.end(), result.begin(), towlower);
	return result;
}

inline std::wstring WithoutFragment(const std::wstring& value)
{
	const std::wstring::size_type fragment = value.rfind(L'#');
	return fragment == std::wstring::npos ? value : value.substr(0, fragment);
}

inline std::wstring GetInternalTargetId(const std::wstring& source,
	const std::wstring& currentDocumentUrl = std::wstring())
{
	const std::wstring href = Trim(source);
	const std::wstring lower = Lower(href);
	if(href.size() > 1 && href[0] == L'#') return href.substr(1);
	const std::wstring internalPrefix = L"fbw-internal:#";
	if(lower.compare(0, internalPrefix.size(), internalPrefix) == 0 && href.size() > internalPrefix.size())
		return href.substr(internalPrefix.size());
	// MSHTML sometimes expands a local fragment to file:///...#id.  Accept it
	// only when the document part matches the currently edited document.
	if(lower.compare(0, 5, L"file:") == 0)
	{
		const std::wstring::size_type fragment = href.rfind(L'#');
		if(fragment != std::wstring::npos && fragment + 1 < href.size() &&
			!currentDocumentUrl.empty() &&
			Lower(WithoutFragment(href)) == Lower(WithoutFragment(Trim(currentDocumentUrl))))
			return href.substr(fragment + 1);
	}
	return std::wstring();
}

inline bool IsExternalHttpUrl(const std::wstring& source)
{
	const std::wstring lower = Lower(Trim(source));
	return lower.compare(0, 7, L"http://") == 0 || lower.compare(0, 8, L"https://") == 0;
}

inline bool IsBlockedUrl(const std::wstring& source)
{
	const std::wstring lower = Lower(Trim(source));
	return lower.compare(0, 11, L"javascript:") == 0 || lower.compare(0, 5, L"data:") == 0;
}
}
