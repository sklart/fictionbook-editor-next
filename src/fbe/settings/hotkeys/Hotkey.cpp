#include "stdafx.h"
#include "Hotkey.h"

BYTE CHotkey::DefaultVirt(WORD fVirt)
{
	const WORD flags = FVIRTKEY | fVirt;
	ATLASSERT(flags <= UCHAR_MAX);
	return static_cast<BYTE>(flags);
}

CHotkey::CHotkey() : m_name_resource_id(0), m_accel(), m_def_accel(), m_char_val(0) {}
CHotkey::CHotkey(CString reg_name, int nameResourceId, WORD fVirt, WORD cmd, WORD key, CString descr) : m_reg_name(reg_name), m_name_resource_id(nameResourceId), m_accel(), m_def_accel(), m_desc(descr), m_char_val(0)
{
	m_name = FbeLoadCString(nameResourceId);
	m_def_accel.fVirt = DefaultVirt(fVirt); m_def_accel.cmd = cmd; m_def_accel.key = key; m_accel = m_def_accel;
}
CHotkey::CHotkey(CString reg_name, int nameResourceId, CString uchar, WORD fVirt, WORD cmd, WORD key, CString descr) : m_reg_name(reg_name), m_name_resource_id(nameResourceId), m_accel(), m_def_accel(), m_desc(descr), m_char_val(0)
{
	m_name = FbeLoadCString(nameResourceId); m_name += uchar;
	m_def_accel.fVirt = DefaultVirt(fVirt); m_def_accel.cmd = cmd; m_def_accel.key = key; m_accel = m_def_accel;
}
CHotkey::CHotkey(CString reg_name, CString name, wchar_t symbol, WORD fVirt, WORD cmd, WORD key, CString descr) : m_name(name), m_reg_name(reg_name), m_name_resource_id(0), m_accel(), m_def_accel(), m_desc(descr), m_char_val(symbol)
{
	m_def_accel.fVirt = DefaultVirt(fVirt); m_def_accel.cmd = cmd; m_def_accel.key = key; m_accel = m_def_accel;
}
CHotkey::CHotkey(CString reg_name, CString cmdName, WORD fVirt, WORD cmd, WORD key, CString descr) : m_name(cmdName), m_reg_name(reg_name), m_name_resource_id(0), m_accel(), m_def_accel(), m_desc(descr), m_char_val(0)
{
	m_def_accel.fVirt = DefaultVirt(fVirt); m_def_accel.cmd = cmd; m_def_accel.key = key; m_accel = m_def_accel;
}
bool CHotkey::operator < (const CHotkey& other) const { return m_name.CompareNoCase(other.m_name) < 0; }
int CHotkey::GetProperties(std::vector<CString>& properties) { properties.push_back(L"Name"); properties.push_back(L"Accel"); return properties.size(); }
bool CHotkey::GetPropertyValue(const CString& propertyName, CProperty& property)
{
	if(propertyName == L"Name") { property = m_reg_name; return true; }
	if(propertyName == L"Accel") { CString temp; temp.Format(L"%u;%u", m_accel.fVirt, m_accel.key); property = temp; return true; }
	return false;
}
bool CHotkey::SetPropertyValue(const CString& propertyName, CProperty& property)
{
	if(propertyName == L"Name") { m_reg_name = property.GetStringValue(); return true; }
	if(propertyName != L"Accel") return false;
	CString str = property.GetStringValue(); int n = 0, curPos = 0;
	while(!str.Tokenize(L";", curPos).IsEmpty()) n++;
	CString* tokens = new CString[n]; curPos = n = 0; CString temp;
	while(!(temp = str.Tokenize(L";", curPos)).IsEmpty()) { tokens[n] = temp; n++; }
	if(n == 2) { const int fVirt = StrToInt(tokens[0]); const int key = StrToInt(tokens[1]); if(fVirt < 0 || fVirt > UCHAR_MAX || key < 0 || key > USHRT_MAX) { delete[] tokens; return false; } m_accel.fVirt = static_cast<BYTE>(fVirt); m_accel.key = static_cast<WORD>(key); }
	delete[] tokens; return true;
}
bool CHotkey::HasMultipleInstances() { return false; }
CString CHotkey::GetClassName() { return L"Hotkey"; }
CString CHotkey::GetID() { return L""; }
ISerializable* CHotkey::Create() { return new CHotkey; }
void CHotkey::Destroy(ISerializable* obj) { delete obj; }
