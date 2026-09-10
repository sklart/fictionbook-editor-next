#include "stdafx.h"
#include "WordsItem.h"

WordsItem::WordsItem() : m_count(0), m_percent(0), m_prc_idx(0) {}
WordsItem::WordsItem(CString word, int count) : m_word(word), m_count(count), m_percent(0), m_prc_idx(0) {}
int WordsItem::GetProperties(std::vector<CString>& properties) { properties.push_back(L"Value"); properties.push_back(L"Counted"); return properties.size(); }
bool WordsItem::GetPropertyValue(const CString& propertyName, CProperty& property)
{
	if(propertyName == L"Value") { property = m_word; return true; }
	if(propertyName == L"Counted") { CString counted; counted.Format(L"%d", m_count); property = counted; return true; }
	return false;
}
bool WordsItem::SetPropertyValue(const CString& propertyName, CProperty& property)
{
	if(propertyName == L"Value") { m_word = property; return true; }
	if(propertyName == L"Counted") { m_count = StrToInt(property.GetStringValue()); return true; }
	return false;
}
bool WordsItem::HasMultipleInstances() { return true; }
CString WordsItem::GetClassName() { return L"Word"; }
CString WordsItem::GetID() { return L""; }
ISerializable* WordsItem::Create() { return new WordsItem; }
void WordsItem::Destroy(ISerializable* obj) { delete obj; }
bool WordsItem::operator==(const WordsItem& other) { return m_word.CompareNoCase(other.m_word) == 0; }
