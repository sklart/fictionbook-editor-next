#include "stdafx.h"
#include "ScriptUiController.h"
#include "ScriptCatalog.h"
#include "ScriptRegistry.h"
#include "..\resource.h"

namespace FbeScripts
{
UiController::UiController(UINT folderCommandBase, UINT folderCommandCount)
	: m_menu(folderCommandBase, folderCommandCount), m_initializeCount(0), m_discoveryCount(0) {}

void UiController::SetLastScript(const ScriptDescriptor& script)
{
	m_lastUid = script.uid;
}

const ScriptDescriptor* UiController::LastScript() const
{
	for(int index = 0; index < m_menu.Count(); ++index)
	{
		const ScriptDescriptor& script = m_menu.Item(index);
		if(!script.isFolder && script.uid == m_lastUid)
			return &script;
	}
	return NULL;
}

bool UiController::Initialize(const CString& folder, const CString& persistedCommandIds, CString& updatedCommandIds,
	HMENU scriptsMenu, const CString& noScriptsText,
	const std::function<bool(const CString&)>& isRunnable,
	const std::function<void(const ScriptDescriptor&, const VisualResource&, UINT)>& addVisual,
	const std::function<void(ScriptDescriptor&)>& registerHotkey)
{
	if(scriptsMenu == NULL) return false;
	++m_initializeCount;
	m_menu.Clear(); ClearLastScript();
	ScriptRegistry registry; if(!registry.Load()) return false;
	Catalog catalog; ++m_discoveryCount; if(!catalog.Discover(folder, L"*.js", &registry)) return false;
	const std::vector<ScriptDescriptor>& candidates = catalog.Items();
	for(size_t index = 0; index < candidates.size(); ++index)
	{
		const ScriptDescriptor& candidate = candidates[index];
		if(!candidate.isFolder && !isRunnable(candidate.path)) continue;
		const CString directory = candidate.isFolder ? candidate.path : candidate.path.Left(candidate.path.ReverseFind(L'\\') + 1);
		CString picture(candidate.path.Mid(candidate.path.ReverseFind(L'\\') + 1));
		if(!candidate.isFolder && picture.GetLength() >= 3) picture.Delete(picture.GetLength() - 3, 3);
		VisualResource visual = m_visuals.Load(directory, picture); m_menu.Add(candidate, static_cast<VisualResource&&>(visual));
	}
	const bool changed = m_menu.AssignCommandIds(ScriptCommandCount, persistedCommandIds, updatedCommandIds);
	while(::GetMenuItemCount(scriptsMenu) > 0) ::RemoveMenu(scriptsMenu, 0, MF_BYPOSITION);
	if(m_menu.Count())
	{
		m_menu.Build(scriptsMenu, [](ScriptDescriptor&) {}, addVisual);
		for(int index = 0; index < m_menu.Count(); ++index) if(!m_menu.Item(index).isFolder) registerHotkey(m_menu.Item(index));
	}
	else ::AppendMenu(scriptsMenu, MF_STRING | MF_DISABLED | MF_GRAYED, IDCANCEL, noScriptsText);
	return changed;
}
}
