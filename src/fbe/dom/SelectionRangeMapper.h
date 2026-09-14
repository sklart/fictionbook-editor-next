#pragma once

#include <mshtml.h>

namespace FbeDom {

// Maps MSHTML text ranges to the element-relative character coordinates used
// by the Body/Source transfer. Historical range heuristics live here so
// callers only need an explicit MSHTML document.
class SelectionRangeMapper {
public:
	static int GetRangePos(const MSHTML::IHTMLTxtRangePtr& range,
		MSHTML::IHTMLElementPtr& element, int& pos);
	static bool GetSelectionInfo(MSHTML::IHTMLDocument2Ptr document,
		MSHTML::IHTMLElementPtr* begin, MSHTML::IHTMLElementPtr* end,
		int* beginChar, int* endChar, MSHTML::IHTMLTxtRangePtr range);
	static MSHTML::IHTMLTxtRangePtr SetSelection(MSHTML::IHTMLDocument2Ptr document,
		MSHTML::IHTMLElementPtr begin, MSHTML::IHTMLElementPtr end,
		int beginPos, int endPos);

private:
	static int GetRelationalCharPos(MSHTML::IHTMLDOMNodePtr node, int pos);
	static int GetRealCharPos(MSHTML::IHTMLDOMNodePtr node, int pos);
	static int CountNodeChars(MSHTML::IHTMLDOMNodePtr node);
};

} // namespace FbeDom
