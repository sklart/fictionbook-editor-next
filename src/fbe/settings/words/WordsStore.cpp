#include "stdafx.h"
#include "../SettingsFileTransaction.h"
#include "WordsStore.h"
#include "..\SettingsPaths.h"
#include <algorithm>

namespace
{
class WordsSortComparator
{
public:
	bool operator()(void* left, void* right) const
	{
		return reinterpret_cast<WordsItem*>(left)->m_word.Compare(reinterpret_cast<WordsItem*>(right)->m_word) < 0;
	}
};
}

namespace FbeSettings { namespace Words {
void Load(std::vector<WordsItem>& words)
{
	CXMLSerializer serializer(FbeSettings::WordsFilePath(), L"FBE", true);
	WordsItem word; std::vector<void*> objects; serializer.Deserialize(&word, objects);
	words.clear(); std::sort(objects.begin(), objects.end(), WordsSortComparator()); words.reserve(objects.size());
	CString previousWord; bool havePreviousWord = false;
	for(std::vector<void*>::iterator item = objects.begin(); item != objects.end(); ++item)
	{
		WordsItem* loadedWord = reinterpret_cast<WordsItem*>(*item);
		if(!havePreviousWord || previousWord.Compare(loadedWord->m_word) != 0) { words.push_back(*loadedWord); previousWord = loadedWord->m_word; havePreviousWord = true; }
		word.Destroy(loadedWord);
	}
}
bool SaveToFile(const std::vector<WordsItem>& words, const CString& path)
{
	MSXML2::IXMLDOMDocument2Ptr document; HRESULT hr = document.CreateInstance(__uuidof(DOMDocument));
	if(FAILED(hr)) return false;
	try {
	CString xml(L"<FBE>\n\t<Words>\n");
	for(unsigned int i = 0; i < words.size(); ++i)
	{
		xml += L"\t\t<Word>\n\t\t\t<Value>" + words[i].m_word + L"</Value>\n";
		CString count; count.Format(L"%d", words[i].m_count);
		xml += L"\t\t\t<Counted>" + count + L"</Counted>\n\t\t</Word>";
	}
	xml += L"\t</Words>\n</FBE>"; if(document->loadXML(_bstr_t(xml)) != VARIANT_TRUE) return false;
	MSXML2::IXMLDOMElementPtr root = document->GetdocumentElement();
	MSXML2::IXMLDOMProcessingInstructionPtr declaration = document->createProcessingInstruction(L"xml", L" version='1.0' encoding='UTF-8'");
	_variant_t rootObject; rootObject.vt = VT_DISPATCH; rootObject.pdispVal = root; rootObject.pdispVal->AddRef(); document->insertBefore(declaration, rootObject);
	return document->save(_variant_t(path)) == S_OK;
	} catch(const _com_error&) { return false; }
}
bool Save(const std::vector<WordsItem>& words)
{
	const CString path(FbeSettings::WordsFilePath());
	const CString temporary = SettingsFileTransaction::Temporary(path);
	if(temporary.IsEmpty()) return false;
	SettingsFileTransaction transaction; transaction.Add(path, temporary);
	return SaveToFile(words, temporary) && transaction.Commit();
}
} }
