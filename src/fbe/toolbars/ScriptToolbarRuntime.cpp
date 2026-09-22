#include "stdafx.h"
#include "ScriptToolbarRuntime.h"

void ScriptToolbarRuntimeCollection::Reset() { m_items.clear(); }
ScriptToolbarRuntime& ScriptToolbarRuntimeCollection::Add(const ScriptToolbarDefinition& definition) { ScriptToolbarRuntime runtime; runtime.definition = definition; m_items.push_back(runtime); return m_items.back(); }
ScriptToolbarRuntime* ScriptToolbarRuntimeCollection::Find(const CString& id) { for(size_t i = 0; i < m_items.size(); ++i) if(m_items[i].definition.id == id) return &m_items[i]; return NULL; }
const ScriptToolbarRuntime* ScriptToolbarRuntimeCollection::Find(const CString& id) const { for(size_t i = 0; i < m_items.size(); ++i) if(m_items[i].definition.id == id) return &m_items[i]; return NULL; }
bool ScriptToolbarRuntimeCollection::Remove(const CString& id) { for(std::vector<ScriptToolbarRuntime>::iterator item = m_items.begin(); item != m_items.end(); ++item) if(item->definition.id == id) { m_items.erase(item); return true; } return false; }
void ScriptToolbarRuntimeCollection::Reorder(const std::vector<ScriptToolbarDefinition>& definitions) { std::vector<ScriptToolbarRuntime> reordered; for(size_t definition = 0; definition < definitions.size(); ++definition) { ScriptToolbarRuntime* existing = Find(definitions[definition].id); if(existing != NULL) { existing->definition = definitions[definition]; reordered.push_back(*existing); } else { ScriptToolbarRuntime runtime; runtime.definition = definitions[definition]; reordered.push_back(runtime); } } m_items.swap(reordered); }
