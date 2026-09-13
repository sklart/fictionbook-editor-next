#pragma once

#include "ScriptMenuBuilder.h"
#include "ScriptVisualResources.h"

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

private:
	MenuBuilder m_menu;
	VisualResources m_visuals;
	CString m_lastRelativePath;
};
}
