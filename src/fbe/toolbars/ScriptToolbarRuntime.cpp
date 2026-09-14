#include "stdafx.h"
#include "ScriptToolbarRuntime.h"

void ScriptToolbarRuntimeCollection::Reset() { m_items.clear(); }
ScriptToolbarRuntime& ScriptToolbarRuntimeCollection::Add(const ScriptToolbarDefinition& definition) { ScriptToolbarRuntime runtime; runtime.definition = definition; m_items.push_back(runtime); return m_items.back(); }
ScriptToolbarRuntime* ScriptToolbarRuntimeCollection::Find(const CString& id) { for(size_t i = 0; i < m_items.size(); ++i) if(m_items[i].definition.id == id) return &m_items[i]; return NULL; }
