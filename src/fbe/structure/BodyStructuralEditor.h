#pragma once

#include <mshtml.h>

namespace FbeStructure { class StructuralTrace; }

namespace FbeStructure {

class BodyStructuralEditor
{
public:
	BodyStructuralEditor(MSHTML::IHTMLDocument2Ptr document, MSHTML::IMarkupServices2Ptr markupServices, StructuralTrace* trace = nullptr);
	bool InsertCite(bool checkOnly);
	bool InsertPoem(bool checkOnly);

private:
	static MSHTML::IHTMLElementPtr FindParentDiv(MSHTML::IHTMLElementPtr element);
	bool ExpandRangeToParagraphs(MSHTML::IHTMLTxtRangePtr& range, MSHTML::IHTMLElementPtr& begin, MSHTML::IHTMLElementPtr& end) const;
	void Before(const wchar_t* phase) const;
	void After(const wchar_t* phase) const;
	void Hr(const wchar_t* phase, HRESULT hr) const;
	MSHTML::IHTMLDocument2Ptr m_document;
	MSHTML::IMarkupServices2Ptr m_markupServices;
	StructuralTrace* m_trace;
};

} // namespace FbeStructure
