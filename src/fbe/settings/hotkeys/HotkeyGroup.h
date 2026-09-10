#pragma once

#include "Hotkey.h"

class CHotkeysGroup : public ISerializable, public IObjectFactory
{
public:
	CString m_name;
	CString m_reg_name;
	UINT m_name_resource_id;
	std::vector<CHotkey> m_hotkeys;
	CHotkey m_hotkey_factory;
	std::vector<void*> m_ptr_hotkeys;

	CHotkeysGroup();
	CHotkeysGroup(CString reg_name, int nameResourceId);
	bool operator < (const CHotkeysGroup& other) const;
	int GetProperties(std::vector<CString>& properties);
	bool GetPropertyValue(const CString& propertyName, CProperty& property);
	bool SetPropertyValue(const CString& propertyName, CProperty& property);
	bool HasMultipleInstances();
	CString GetClassName();
	CString GetID();
	ISerializable* Create();
	void Destroy(ISerializable* obj);
};
