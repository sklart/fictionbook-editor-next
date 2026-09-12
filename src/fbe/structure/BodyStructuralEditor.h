#pragma once

#include <mshtml.h>

namespace FbeStructure {

class BodyStructuralEditor
{
public:
	BodyStructuralEditor(MSHTML::IHTMLDocument2Ptr document, MSHTML::IMarkupServices2Ptr markupServices);
	bool InsertCite(bool checkOnly);
	bool InsertPoem(bool checkOnly);

private:
	static MSHTML::IHTMLElementPtr FindParentDiv(MSHTML::IHTMLElementPtr element);
	bool ExpandRangeToParagraphs(MSHTML::IHTMLTxtRangePtr& range, MSHTML::IHTMLElementPtr& begin, MSHTML::IHTMLElementPtr& end) const;
	MSHTML::IHTMLDocument2Ptr m_document;
	MSHTML::IMarkupServices2Ptr m_markupServices;
};

} // namespace FbeStructure
