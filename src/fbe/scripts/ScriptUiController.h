#pragma once

#include "ScriptMenuBuilder.h"
#include "ScriptVisualResources.h"
#include <functional>

namespace FbeScripts
{
class UiController
{
public:
	UiController(UINT folderCommandBase, UINT folderCommandCount);
	MenuBuilder& Menu() { return m_menu; }
	const MenuBuilder& Menu() const { return m_menu; }
	VisualResources& Visuals() { return m_visuals; }
	void SetLastScript(const ScriptDescriptor& script);
	void ClearLastScript() { m_lastRelativePath.Empty(); }
	const CString& LastRelativePath() const { return m_lastRelativePath; }
	const ScriptDescriptor* LastScript() const;
	bool HasLastScript() const { return !m_lastRelativePath.IsEmpty(); }
	bool Initialize(const CString& folder, const CString& persistedCommandIds, CString& updatedCommandIds,
		HMENU scriptsMenu, const CString& noScriptsText,
		const std::function<bool(const CString&)>& isRunnable,
		const std::function<void(const ScriptDescriptor&, const VisualResource&, UINT)>& addVisual,
		const std::function<void(ScriptDescriptor&)>& registerHotkey);

private:
	MenuBuilder m_menu;
	VisualResources m_visuals;
	CString m_lastRelativePath;
};
}
