#include "stdafx.h"
#include "BodyStructuralEditor.h"
#include "../dom/MarkupUndoUnitScope.h"
#include "../utils/utils.h"

namespace FbeStructure {
BodyStructuralEditor::BodyStructuralEditor(MSHTML::IHTMLDocument2Ptr document, MSHTML::IMarkupServices2Ptr markupServices) : m_document(document), m_markupServices(markupServices) {}
MSHTML::IHTMLElementPtr BodyStructuralEditor::FindParentDiv(MSHTML::IHTMLElementPtr element) { while (element && U::scmp(element->tagName, L"DIV")) element = element->parentElement; return element; }
bool BodyStructuralEditor::ExpandRangeToParagraphs(MSHTML::IHTMLTxtRangePtr& range, MSHTML::IHTMLElementPtr& begin, MSHTML::IHTMLElementPtr& end) const {
	MSHTML::IHTMLTxtRangePtr first = range->duplicate(); first->collapse(VARIANT_TRUE); if (!FindParentDiv(first->parentElement())) return false;
	MSHTML::IHTMLTxtRangePtr last = range->duplicate(); last->collapse(VARIANT_FALSE);
	begin = first->parentElement(); while (begin && U::scmp(begin->tagName, L"P")) begin = begin->parentElement;
	end = last->parentElement(); while (end && U::scmp(end->tagName, L"P")) end = end->parentElement;
	if (!begin || !end) return false;
	if (begin == end) range->moveToElementText(begin);
	else { MSHTML::IMarkupPointerPtr start, finish; m_markupServices->CreateMarkupPointer(&start); m_markupServices->CreateMarkupPointer(&finish); start->MoveAdjacentToElement(begin, MSHTML::ELEM_ADJ_AfterBegin); finish->MoveAdjacentToElement(end, MSHTML::ELEM_ADJ_BeforeEnd); m_markupServices->MoveRangeToPointers(start, finish, range); }
	return true;
}
bool BodyStructuralEditor::InsertCite(bool checkOnly) {
	try {
		MSHTML::IHTMLTxtRangePtr range(m_document->selection->createRange()); if (!range) return false;
		MSHTML::IHTMLElementPtr parent(FindParentDiv(range->parentElement())); if (!parent) return false;
		MSHTML::IHTMLTxtRangePtr start(range->duplicate()); start->collapse(VARIANT_TRUE); if (FindParentDiv(start->parentElement()) != parent) return false;
		_bstr_t cls(parent->className); if (U::scmp(cls,L"section") && U::scmp(cls,L"epigraph") && U::scmp(cls,L"annotation") && U::scmp(cls,L"history")) return false;
		MSHTML::IHTMLElementPtr beginElement,endElement; if (!ExpandRangeToParagraphs(range,beginElement,endElement)) return false; if (checkOnly) return true;
		CString html; MSHTML::IHTMLDOMNodePtr sibling=beginElement; do { html += MSHTML::IHTMLElementPtr(sibling)->outerHTML.GetBSTR(); if(sibling==endElement) break; } while((sibling=sibling->nextSibling));
		MSHTML::IHTMLElementPtr cite(m_document->createElement(L"<DIV class=cite>")), accumulator(m_document->createElement(L"DIV")); accumulator->innerHTML=html.AllocSysString(); CString citeHtml; MSHTML::IHTMLElementCollectionPtr children=accumulator->children;
		for(long i=0;children&&i<children->length;++i) { MSHTML::IHTMLElementPtr current(children->item(i)); if(!U::scmp(current->tagName,L"DIV") && U::scmp(current->className,L"table") && U::scmp(current->className,L"poem")) { if(current->innerText.GetBSTR()) citeHtml += CString(L"<P>")+current->innerText.GetBSTR()+L"</P>"; } else citeHtml += current->outerHTML.GetBSTR(); }
		cite->innerHTML=citeHtml.AllocSysString(); FbeDom::MarkupUndoUnitScope undo(m_markupServices,L"insert cite"); MSHTML::IHTMLDOMNodePtr begin=beginElement,end=endElement; MSHTML::IHTMLDOMNodePtr(parent)->insertBefore(MSHTML::IHTMLDOMNodePtr(cite),begin.GetInterfacePtr()); while(begin!=end) { sibling=begin->nextSibling; begin->removeNode(VARIANT_TRUE); begin=sibling; } end->removeNode(VARIANT_TRUE); undo.Close(); range->moveToElementText(cite); range->collapse(VARIANT_FALSE); range->select(); return true;
	} catch(const _com_error&) { return false; }
}
bool BodyStructuralEditor::InsertPoem(bool checkOnly) {
	try {
		MSHTML::IHTMLTxtRangePtr range(m_document->selection->createRange()); if (!range) return false;
		MSHTML::IHTMLElementPtr parent(FindParentDiv(range->parentElement())); if (!parent) return false;
		const bool wasCollapsed = range->compareEndPoints(L"StartToEnd", range) == 0;
		MSHTML::IHTMLTxtRangePtr start(range->duplicate()); start->collapse(VARIANT_TRUE); if (FindParentDiv(start->parentElement()) != parent) return false;
		_bstr_t cls(parent->className); if (U::scmp(cls,L"section") && U::scmp(cls,L"epigraph") && U::scmp(cls,L"annotation") && U::scmp(cls,L"history") && U::scmp(cls,L"cite")) return false;
		MSHTML::IHTMLElementPtr beginElement,endElement; if (!ExpandRangeToParagraphs(range,beginElement,endElement)) return false; if (checkOnly) return true;
		CString html; MSHTML::IHTMLDOMNodePtr sibling=beginElement; do { html+=MSHTML::IHTMLElementPtr(sibling)->outerHTML.GetBSTR(); if(sibling==endElement)break; } while((sibling=sibling->nextSibling));
		bool expandedHasContent=false; for(MSHTML::IHTMLDOMNodePtr paragraph=beginElement;paragraph;paragraph=paragraph->nextSibling) { CString text=MSHTML::IHTMLElementPtr(paragraph)->innerText; for(int i=0;i<text.GetLength();++i) { const wchar_t ch=text[i]; if(ch!=L' '&&ch!=L'\t'&&ch!=L'\r'&&ch!=L'\n'&&ch!=0x00A0){expandedHasContent=true;break;} } if(expandedHasContent||paragraph==endElement)break; }
		MSHTML::IHTMLElementPtr poem(m_document->createElement(L"<DIV class=poem>"));
		if(wasCollapsed&&!expandedHasContent) poem->innerHTML=L"<DIV class=stanza><P>&nbsp;</P></DIV>";
		else { MSHTML::IHTMLElementPtr accumulator(m_document->createElement(L"DIV")); accumulator->innerHTML=html.AllocSysString(); MSHTML::IHTMLElementCollectionPtr children=accumulator->children; bool trim=true; CString stanzaHtml; for(long i=0;children&&i<children->length;++i){MSHTML::IHTMLElementPtr current(children->item(i)); CString line=current->innerText; if(line.Trim().IsEmpty()){if(trim)continue;MSHTML::IHTMLElementPtr stanza(m_document->createElement(L"<DIV class=stanza>"));stanza->innerHTML=stanzaHtml.AllocSysString();MSHTML::IHTMLElement2Ptr(poem)->insertAdjacentElement(L"beforeEnd",stanza);stanzaHtml.Empty();trim=true;}else{if(!U::scmp(current->tagName,L"DIV")){if(current->innerText.GetBSTR())stanzaHtml+=CString(L"<P>")+current->innerText.GetBSTR()+L"</P>";else continue;}else stanzaHtml+=current->outerHTML.GetBSTR();trim=false;}} if(!stanzaHtml.IsEmpty()){MSHTML::IHTMLElementPtr stanza(m_document->createElement(L"<DIV class=stanza>"));stanza->innerHTML=stanzaHtml.AllocSysString();MSHTML::IHTMLElement2Ptr(poem)->insertAdjacentElement(L"beforeEnd",stanza);} MSHTML::IHTMLElementCollectionPtr poemChildren=poem->children;if(!poemChildren||poemChildren->length==0)poem->innerHTML=L"<DIV class=stanza><P>&nbsp;</P></DIV>"; }
		FbeDom::MarkupUndoUnitScope undo(m_markupServices,L"insert poem"); MSHTML::IHTMLDOMNodePtr begin=beginElement,end=endElement; MSHTML::IHTMLDOMNodePtr(parent)->insertBefore(MSHTML::IHTMLDOMNodePtr(poem),begin.GetInterfacePtr()); while(begin!=end){sibling=begin->nextSibling;begin->removeNode(VARIANT_TRUE);begin=sibling;}end->removeNode(VARIANT_TRUE);undo.Close();range->moveToElementText(poem);range->collapse(VARIANT_FALSE);range->select();return true;
	} catch(const _com_error&) { return false; }
}
} // namespace FbeStructure
