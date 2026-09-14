#include "stdafx.h"
#include "ScriptToolbarCollection.h"

ScriptToolbarCollection::ScriptToolbarCollection() { EnsureMain(); }
void ScriptToolbarCollection::EnsureMain() { if(Find(L"scripts-main") == NULL) { ScriptToolbarDefinition main; main.id = L"scripts-main"; main.name = L"Scripts"; m_items.insert(m_items.begin(), main); } }
ScriptToolbarDefinition* ScriptToolbarCollection::Find(const CString& id) { for(size_t i = 0; i < m_items.size(); ++i) if(m_items[i].id == id) return &m_items[i]; return NULL; }
const ScriptToolbarDefinition* ScriptToolbarCollection::Find(const CString& id) const { for(size_t i = 0; i < m_items.size(); ++i) if(m_items[i].id == id) return &m_items[i]; return NULL; }
CString ScriptToolbarCollection::NextId() { for(unsigned int n = 1; ; ++n) { CString id; id.Format(L"toolbar-%u", n); if(Find(id) == NULL) return id; } }
ScriptToolbarDefinition& ScriptToolbarCollection::Add(const CString& name) { ScriptToolbarDefinition definition; definition.id = NextId(); definition.name = name.IsEmpty() ? definition.id : name; m_items.push_back(definition); return m_items.back(); }
bool ScriptToolbarCollection::Remove(const CString& id) { if(id == L"scripts-main") return false; for(std::vector<ScriptToolbarDefinition>::iterator it = m_items.begin(); it != m_items.end(); ++it) if(it->id == id) { m_items.erase(it); return true; } return false; }
bool ScriptToolbarCollection::Rename(const CString& id, const CString& name) { ScriptToolbarDefinition* definition = Find(id); if(definition == NULL || name.IsEmpty()) return false; definition->name = name; return true; }
bool ScriptToolbarCollection::Move(const CString& id, size_t destination) { if(destination >= m_items.size()) return false; for(size_t i = 0; i < m_items.size(); ++i) if(m_items[i].id == id) { ScriptToolbarDefinition item = m_items[i]; m_items.erase(m_items.begin() + i); m_items.insert(m_items.begin() + destination, item); return true; } return false; }
