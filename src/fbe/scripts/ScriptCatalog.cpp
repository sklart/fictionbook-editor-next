#include "stdafx.h"
#include "ScriptCatalog.h"
#include "..\\utils\\utils.h"
#include <algorithm>

namespace
{
CString NormalizedRelativePath(const CString& root, const CString& fullPath)
{
	CString relative(fullPath);
	if (relative.Left(root.GetLength()).CompareNoCase(root) == 0) relative = relative.Mid(root.GetLength());
	while (!relative.IsEmpty() && (relative[0] == L'\\' || relative[0] == L'/')) relative = relative.Mid(1);
	relative.Replace(L'/', L'\\');
	return relative;
}

void SetNameAndOrder(const CString& fileName, CString& name, CString& order)
{
	name = fileName;
	order = L"0_" + fileName;
	wchar_t* separator = wcschr(name.GetBuffer(), L'_');
	name.ReleaseBuffer();
	if (separator != NULL && U::CheckScriptsVersion(fileName))
	{
		name = separator + 1;
		order = fileName;
	}
}
}

namespace FbeScripts
{
void Catalog::Discover(const CString& root, const CString& mask)
{
	m_items.clear();
	DiscoverFolder(root, root, mask, CString());
	std::sort(m_items.begin(), m_items.end(), [](const ScriptDescriptor& left, const ScriptDescriptor& right)
	{
		if (left.isFolder != right.isFolder) return left.isFolder;
		const int order = left.order.CompareNoCase(right.order);
		return order != 0 ? order < 0 : left.relativePath.CompareNoCase(right.relativePath) < 0;
	});
}

void Catalog::DiscoverFolder(const CString& root, const CString& path, const CString& mask, const CString& parentId)
{
	WIN32_FIND_DATA data; HANDLE found = ::FindFirstFile(path + L"*.*", &data); if (found == INVALID_HANDLE_VALUE) return;
	int sequence = 1;
	do
	{
		if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
		const CString full = path + data.cFileName;
		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			const CString folderPath = full + L"\\";
			if (!U::HasScriptsEndpoint(folderPath, const_cast<TCHAR*>(static_cast<LPCTSTR>(mask)))) continue;
			ScriptDescriptor item = {}; item.path = folderPath; item.relativePath = NormalizedRelativePath(root, full); item.parentId = parentId; item.id.Format(L"%s_%d", static_cast<LPCWSTR>(parentId), sequence++); SetNameAndOrder(data.cFileName, item.name, item.order); item.isFolder = true; item.commandId = -1;
			m_items.push_back(item); DiscoverFolder(root, folderPath, mask, item.id);
		}
		else if (::PathMatchSpec(data.cFileName, mask))
		{
			ScriptDescriptor item = {}; item.path = full; item.relativePath = NormalizedRelativePath(root, full); item.parentId = parentId; item.id.Format(L"%s_%d", static_cast<LPCWSTR>(parentId), sequence++); SetNameAndOrder(data.cFileName, item.name, item.order); if (item.name.GetLength() >= 3) item.name.Delete(item.name.GetLength() - 3, 3); item.isFolder = false; item.commandId = -1; m_items.push_back(item);
		}
	} while (::FindNextFile(found, &data));
	::FindClose(found);
}
}
