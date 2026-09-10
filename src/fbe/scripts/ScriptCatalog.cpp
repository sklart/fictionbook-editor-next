#include "stdafx.h"
#include "ScriptCatalog.h"
#include "..\\utils\\utils.h"
#include <algorithm>

namespace
{
CString NormalizedRelativePath(const CString& root, const CString& fullPath)
{
	DWORD rootLength = ::GetFullPathName(root, 0, NULL, NULL);
	DWORD pathLength = ::GetFullPathName(fullPath, 0, NULL, NULL);
	if (rootLength == 0 || pathLength == 0) return CString();
	std::vector<wchar_t> rootBuffer(rootLength + 1), pathBuffer(pathLength + 1);
	if (::GetFullPathName(root, static_cast<DWORD>(rootBuffer.size()), &rootBuffer[0], NULL) == 0 ||
		::GetFullPathName(fullPath, static_cast<DWORD>(pathBuffer.size()), &pathBuffer[0], NULL) == 0) return CString();
	CString normalizedRoot(&rootBuffer[0]), normalizedPath(&pathBuffer[0]);
	while (normalizedRoot.GetLength() > 3 && (normalizedRoot[normalizedRoot.GetLength() - 1] == L'\\' || normalizedRoot[normalizedRoot.GetLength() - 1] == L'/'))
		normalizedRoot.Delete(normalizedRoot.GetLength() - 1);
	if (normalizedPath.GetLength() <= normalizedRoot.GetLength() || normalizedPath.Left(normalizedRoot.GetLength()).CompareNoCase(normalizedRoot) != 0) return CString();
	const wchar_t separator = normalizedPath[normalizedRoot.GetLength()];
	if (separator != L'\\' && separator != L'/') return CString();
	CString relative = normalizedPath.Mid(normalizedRoot.GetLength() + 1);
	relative.Replace(L'\\', L'/');
	relative.MakeLower();
	return relative;
}

void SetNameAndOrder(const CString& fileName, CString& name, CString& order)
{
	name = fileName;
	order = L"0_" + fileName;
	const int separator = name.Find(L'_');
	if (separator >= 0 && U::CheckScriptsVersion(fileName))
	{
		name = name.Mid(separator + 1);
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
