#include "stdafx.h"
#include "HotkeyGroup.h"

CHotkeysGroup::CHotkeysGroup() : m_name_resource_id(0) {}
CHotkeysGroup::CHotkeysGroup(CString reg_name, int nameResourceId) : m_reg_name(reg_name), m_name_resource_id(nameResourceId) { m_name = FbeLoadCString(nameResourceId); }
bool CHotkeysGroup::operator < (const CHotkeysGroup& other) const { return m_name.CompareNoCase(other.m_name) < 0; }
int CHotkeysGroup::GetProperties(std::vector<CString>& properties) { properties.push_back(L"GroupName"); properties.push_back(L"Hotkey"); return properties.size(); }
bool CHotkeysGroup::GetPropertyValue(const CString& propertyName, CProperty& property)
{
	if(propertyName == L"GroupName") { property = m_reg_name; return true; }
	if(propertyName != L"Hotkey") return false;
	for(unsigned long i = 0; i < m_hotkeys.size(); ++i) m_ptr_hotkeys.push_back(&m_hotkeys[i]);
	property = m_ptr_hotkeys; property.SetFactory(&m_hotkey_factory); return true;
}
bool CHotkeysGroup::SetPropertyValue(const CString& propertyName, CProperty& property)
{
	if(propertyName == L"GroupName") { m_reg_name = property.GetStringValue(); return true; }
	if(propertyName != L"Hotkey") return false;
	for(std::vector<void*>::iterator iter = m_ptr_hotkeys.begin(); iter != m_ptr_hotkeys.end(); ++iter) delete static_cast<CHotkey*>(*iter);
	CProperty::CopyPtrList(m_ptr_hotkeys, property.GetObjectList());
	m_hotkeys.clear();
	for(unsigned long i = 0; i < m_ptr_hotkeys.size(); ++i) m_hotkeys.push_back(*static_cast<CHotkey*>(m_ptr_hotkeys[i]));
	return true;
}
bool CHotkeysGroup::HasMultipleInstances() { return true; }
CString CHotkeysGroup::GetClassName() { return L"HkGroup"; }
CString CHotkeysGroup::GetID() { return L""; }
ISerializable* CHotkeysGroup::Create() { return new CHotkeysGroup; }
void CHotkeysGroup::Destroy(ISerializable* obj) { delete obj; }
