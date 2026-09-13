#include "stdafx.h"
#include "ScriptUiController.h"

namespace FbeScripts
{
UiController::UiController(UINT folderCommandBase, UINT folderCommandCount)
	: m_menu(folderCommandBase, folderCommandCount) {}

void UiController::SetLastScript(const ScriptDescriptor& script)
{
	m_lastRelativePath = script.relativePath;
}

const ScriptDescriptor* UiController::LastScript() const
{
	for(int index = 0; index < m_menu.Count(); ++index)
	{
		const ScriptDescriptor& script = m_menu.Item(index);
		if(!script.isFolder && script.relativePath == m_lastRelativePath)
			return &script;
	}
	return NULL;
}
}
