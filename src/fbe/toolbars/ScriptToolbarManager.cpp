#include "stdafx.h"
#include "ScriptToolbarManager.h"

bool ScriptToolbarManager::SetVisible(const CString& id, bool visible)
{
	ScriptToolbarDefinition* definition = m_collection.Find(id);
	if(definition == NULL) return false;
	definition->visible = visible;
	return true;
}
