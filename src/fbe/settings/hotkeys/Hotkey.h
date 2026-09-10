#pragma once

#include "..\\..\\XMLSerializer\\Serializable.h"

class CHotkey : public ISerializable, public IObjectFactory
{
private:
	static BYTE DefaultVirt(WORD fVirt);

public:
	CString m_name;
	CString m_reg_name;
	UINT m_name_resource_id;
	ACCEL m_accel;
	ACCEL m_def_accel;
	CString m_desc;
	wchar_t m_char_val;

	CHotkey();
	CHotkey(CString reg_name, int nameResourceId, WORD fVirt, WORD cmd, WORD key, CString descr = L"");
	CHotkey(CString reg_name, int nameResourceId, CString uchar, WORD fVirt, WORD cmd, WORD key, CString descr = L"");
	CHotkey(CString reg_name, CString name, wchar_t symbol, WORD fVirt, WORD cmd, WORD key, CString descr = L"");
	CHotkey(CString reg_name, CString cmdName, WORD fVirt, WORD cmd, WORD key, CString descr = L"");

	bool operator < (const CHotkey& other) const;
	int GetProperties(std::vector<CString>& properties);
	bool GetPropertyValue(const CString& propertyName, CProperty& property);
	bool SetPropertyValue(const CString& propertyName, CProperty& property);
	bool HasMultipleInstances();
	CString GetClassName();
	CString GetID();
	ISerializable* Create();
	void Destroy(ISerializable* obj);
};
