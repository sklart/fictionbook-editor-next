#include "stdafx.h"
#include "ScriptCatalog.h"
#include <algorithm>

namespace FbeScripts
{
void Catalog::Discover(const CString& root, const CString& mask)
{
	m_items.clear();
	DiscoverFolder(root, root, mask, CString());
	std::sort(m_items.begin(), m_items.end(), [](const ScriptDescriptor& left, const ScriptDescriptor& right) { return left.order.CompareNoCase(right.order) < 0; });
}

void Catalog::DiscoverFolder(const CString& root, const CString& path, const CString& mask, const CString& parentId)
{
	WIN32_FIND_DATA data; HANDLE found = ::FindFirstFile(path + L"*.*", &data); if (found == INVALID_HANDLE_VALUE) return;
	int sequence = 1;
	do
	{
		if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
		const CString full = path + data.cFileName;
		ScriptDescriptor item = {}; item.path = full; item.relativePath = full.Mid(root.GetLength()); item.parentId = parentId; item.id.Format(L"%s_%d", static_cast<LPCWSTR>(parentId), sequence++); item.name = data.cFileName; item.order = L"0_" + item.name;
		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { item.isFolder = true; item.path += L"\\"; m_items.push_back(item); DiscoverFolder(root, item.path, mask, item.id); }
		else if (::PathMatchSpec(data.cFileName, mask)) { item.isFolder = false; item.commandId = -1; if (item.name.GetLength() >= 3) item.name.Delete(item.name.GetLength() - 3, 3); m_items.push_back(item); }
	} while (::FindNextFile(found, &data));
	::FindClose(found);
}
}
