#pragma once

namespace FbeScripts
{
class MenuBuilder
{
public:
	MenuBuilder(UINT folderCommandBase, UINT folderCommandCount)
		: m_folderCommandBase(folderCommandBase), m_folderCommandCount(folderCommandCount), m_nextFolderCommand(0) {}
	UINT NextFolderCommand() { return m_nextFolderCommand < m_folderCommandCount ? m_folderCommandBase + m_nextFolderCommand++ : 0; }

private:
	UINT m_folderCommandBase;
	UINT m_folderCommandCount;
	UINT m_nextFolderCommand;
};
}
