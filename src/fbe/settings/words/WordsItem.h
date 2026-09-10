#pragma once

#include "..\..\XMLSerializer\Serializable.h"

class WordsItem : public ISerializable, public IObjectFactory
{
public:
	CString m_word;
	int m_count;
	CString m_sCount;
	int m_percent;
	int m_prc_idx;

	WordsItem();
	WordsItem(CString word, int count);
	int GetProperties(std::vector<CString>& properties);
	bool GetPropertyValue(const CString& propertyName, CProperty& property);
	bool SetPropertyValue(const CString& propertyName, CProperty& property);
	bool HasMultipleInstances();
	CString GetClassName();
	CString GetID();
	ISerializable* Create();
	void Destroy(ISerializable* obj);
	bool operator==(const WordsItem& other);
};
