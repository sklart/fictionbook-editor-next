#pragma once

#include "ScriptMenuBuilder.h"
#include "ScriptVisualResources.h"
#include <functional>

namespace FbeScripts
{
enum { ScriptCommandCount = 999 };

class UiController
{
public:
	UiController(UINT folderCommandBase, UINT folderCommandCount);
	MenuBuilder& Menu() { return m_menu; }
	const MenuBuilder& Menu() const { return m_menu; }
	VisualResources& Visuals() { return m_visuals; }
	void SetLastScript(const ScriptDescriptor& script);
	void ClearLastScript() { m_lastUid.Empty(); }
	const CString& LastScriptUid() const { return m_lastUid; }
	const ScriptDescriptor* LastScript() const;
	bool HasLastScript() const { return !m_lastUid.IsEmpty(); }
	bool Initialize(const CString& folder, const CString& persistedCommandIds, CString& updatedCommandIds,
		HMENU scriptsMenu, const CString& noScriptsText,
		const std::function<bool(const CString&)>& isRunnable,
		const std::function<void(const ScriptDescriptor&, const VisualResource&, UINT)>& addVisual,
		const std::function<void(ScriptDescriptor&)>& registerHotkey);

private:
	MenuBuilder m_menu;
	VisualResources m_visuals;
	CString m_lastUid;
};
}
