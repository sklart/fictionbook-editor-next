#include "stdafx.h"
#include "BodyStructuralEditor.h"
#include "StructuralTrace.h"
#include "../dom/MarkupUndoUnitScope.h"
#include "../utils/utils.h"

namespace FbeStructure {
BodyStructuralEditor::BodyStructuralEditor(MSHTML::IHTMLDocument2Ptr document, MSHTML::IMarkupServices2Ptr markupServices, StructuralTrace* trace)
	: m_document(document), m_markupServices(markupServices), m_trace(trace) {}

void BodyStructuralEditor::Before(const wchar_t* phase) const { if (m_trace) m_trace->Before(phase); }
void BodyStructuralEditor::After(const wchar_t* phase) const { if (m_trace) m_trace->After(phase); }
void BodyStructuralEditor::Hr(const wchar_t* phase, HRESULT hr) const { if (m_trace) m_trace->Hr(phase, hr); }

MSHTML::IHTMLElementPtr BodyStructuralEditor::FindParentDiv(MSHTML::IHTMLElementPtr element)
{
	while (element && U::scmp(element->tagName, L"DIV")) element = element->parentElement;
	return element;
}
bool BodyStructuralEditor::ExpandRangeToParagraphs(MSHTML::IHTMLTxtRangePtr& range, MSHTML::IHTMLElementPtr& begin, MSHTML::IHTMLElementPtr& end) const
{
	Before(L"expand-enter");
	Before(L"duplicate-start"); MSHTML::IHTMLTxtRangePtr first = range->duplicate(); After(L"duplicate-start");
	Before(L"collapse-start"); first->collapse(VARIANT_TRUE); After(L"collapse-start");
	if (!FindParentDiv(first->parentElement())) return false;
	Before(L"duplicate-end"); MSHTML::IHTMLTxtRangePtr last = range->duplicate(); After(L"duplicate-end");
	Before(L"collapse-end"); last->collapse(VARIANT_FALSE); After(L"collapse-end");
	begin = first->parentElement(); while (begin && U::scmp(begin->tagName, L"P")) begin = begin->parentElement;
	if (begin) After(L"begin-paragraph-found");
	end = last->parentElement(); while (end && U::scmp(end->tagName, L"P")) end = end->parentElement;
	if (end) After(L"end-paragraph-found");
	if (!begin || !end) return false;
	if (begin == end) {
		Before(L"single-paragraph-move"); range->moveToElementText(begin); After(L"single-paragraph-move");
	} else {
		MSHTML::IMarkupPointerPtr start, finish;
		Before(L"create-start-pointer"); HRESULT hr = m_markupServices->CreateMarkupPointer(&start); Hr(L"create-start-pointer", hr); if (FAILED(hr)) return false;
		Before(L"create-end-pointer"); hr = m_markupServices->CreateMarkupPointer(&finish); Hr(L"create-end-pointer", hr); if (FAILED(hr)) return false;
		Before(L"move-start-pointer"); hr = start->MoveAdjacentToElement(begin, MSHTML::ELEM_ADJ_AfterBegin); Hr(L"move-start-pointer", hr); if (FAILED(hr)) return false;
		Before(L"move-end-pointer"); hr = finish->MoveAdjacentToElement(end, MSHTML::ELEM_ADJ_BeforeEnd); Hr(L"move-end-pointer", hr); if (FAILED(hr)) return false;
		Before(L"move-range-to-pointers"); hr = m_markupServices->MoveRangeToPointers(start, finish, range); Hr(L"move-range-to-pointers", hr); if (FAILED(hr)) return false;
	}
	After(L"expand-success");
	return true;
}
bool BodyStructuralEditor::InsertCite(bool checkOnly)
{
	try {
		Before(L"cite-enter"); After(L"cite-enter");
		Before(L"selection-create"); MSHTML::IHTMLTxtRangePtr range(m_document->selection->createRange()); After(L"selection-create");
		if (!range) return false;
		Before(L"parent-resolved"); MSHTML::IHTMLElementPtr parent(FindParentDiv(range->parentElement())); After(L"parent-resolved");
		if (!parent) return false;
		Before(L"start-range-created"); MSHTML::IHTMLTxtRangePtr start(range->duplicate()); start->collapse(VARIANT_TRUE); After(L"start-range-created");
		if (FindParentDiv(start->parentElement()) != parent) { After(L"preflight-rejected"); return false; }
		_bstr_t cls(parent->className);
		if (U::scmp(cls, L"section") && U::scmp(cls, L"epigraph") && U::scmp(cls, L"annotation") && U::scmp(cls, L"history")) return false;
		After(L"preflight-complete");
		Before(L"expand"); MSHTML::IHTMLElementPtr beginElement, endElement; if (!ExpandRangeToParagraphs(range, beginElement, endElement)) return false; After(L"expand");
		if (checkOnly) { After(L"cite-check-success"); return true; }
		Before(L"capture-html");
		CString html; MSHTML::IHTMLDOMNodePtr sibling = beginElement;
		do { html += MSHTML::IHTMLElementPtr(sibling)->outerHTML.GetBSTR(); if (sibling == endElement) break; sibling = sibling->nextSibling; } while (sibling);
		After(L"capture-html");
		Before(L"create-cite"); MSHTML::IHTMLElementPtr cite(m_document->createElement(L"<DIV class=cite>")); After(L"create-cite");
		Before(L"create-accumulator"); MSHTML::IHTMLElementPtr accumulator(m_document->createElement(L"DIV")); After(L"create-accumulator");
		Before(L"accumulator-innerhtml"); accumulator->innerHTML = html.AllocSysString(); After(L"accumulator-innerhtml");
		CString citeHtml; MSHTML::IHTMLElementCollectionPtr children = accumulator->children;
		for (long index = 0; children && index < children->length; ++index) {
			MSHTML::IHTMLElementPtr current(children->item(index));
			if (!U::scmp(current->tagName, L"DIV") && U::scmp(current->className, L"table") && U::scmp(current->className, L"poem")) { if (current->innerText.GetBSTR()) citeHtml += CString(L"<P>") + current->innerText.GetBSTR() + L"</P>"; }
			else citeHtml += current->outerHTML.GetBSTR();
		}
		After(L"cite-html-built");
		Before(L"cite-innerhtml"); cite->innerHTML = citeHtml.AllocSysString(); After(L"cite-innerhtml");
		Before(L"undo-begin"); FbeDom::MarkupUndoUnitScope undo(m_markupServices, L"insert cite"); After(L"undo-begin");
		MSHTML::IHTMLDOMNodePtr begin = beginElement, end = endElement;
		Before(L"insert-before"); MSHTML::IHTMLDOMNodePtr(parent)->insertBefore(MSHTML::IHTMLDOMNodePtr(cite), begin.GetInterfacePtr()); After(L"insert-before");
		After(L"remove-loop-enter");
		while (begin != end) { sibling = begin->nextSibling; Before(L"remove-node"); begin->removeNode(VARIANT_TRUE); After(L"remove-node"); begin = sibling; }
		Before(L"remove-end"); end->removeNode(VARIANT_TRUE); After(L"remove-end");
		Before(L"undo-end"); undo.Close(); After(L"undo-end");
		Before(L"selection-move"); range->moveToElementText(cite); After(L"selection-move");
		Before(L"selection-collapse"); range->collapse(VARIANT_FALSE); After(L"selection-collapse");
		Before(L"selection-select"); range->select(); After(L"selection-select");
		After(L"cite-success"); return true;
	} catch (const _com_error& error) { if (m_trace) m_trace->Exception(L"cite", error.Error(), error.Description()); return false; }
}
bool BodyStructuralEditor::InsertPoem(bool checkOnly)
{
	try {
		Before(L"poem-enter"); After(L"poem-enter");
		Before(L"selection-create"); MSHTML::IHTMLTxtRangePtr range(m_document->selection->createRange()); After(L"selection-create");
		if (!range) return false;
		Before(L"parent-resolved"); MSHTML::IHTMLElementPtr parent(FindParentDiv(range->parentElement())); After(L"parent-resolved");
		if (!parent) return false;
		Before(L"collapsed-state"); const bool wasCollapsed = range->compareEndPoints(L"StartToEnd", range) == 0; After(L"collapsed-state");
		Before(L"start-range-created"); MSHTML::IHTMLTxtRangePtr start(range->duplicate()); start->collapse(VARIANT_TRUE); After(L"start-range-created");
		if (FindParentDiv(start->parentElement()) != parent) { After(L"preflight-rejected"); return false; }
		_bstr_t cls(parent->className);
		if (U::scmp(cls,L"section") && U::scmp(cls,L"epigraph") && U::scmp(cls,L"annotation") && U::scmp(cls,L"history") && U::scmp(cls,L"cite")) return false;
		After(L"preflight-complete");
		Before(L"expand"); MSHTML::IHTMLElementPtr beginElement, endElement; if (!ExpandRangeToParagraphs(range, beginElement, endElement)) return false; After(L"expand");
		if (checkOnly) { After(L"poem-check-success"); return true; }
		Before(L"capture-html"); CString html; MSHTML::IHTMLDOMNodePtr sibling = beginElement;
		do { html += MSHTML::IHTMLElementPtr(sibling)->outerHTML.GetBSTR(); if (sibling == endElement) break; sibling = sibling->nextSibling; } while (sibling);
		After(L"capture-html");
		Before(L"expanded-content-scan"); bool expandedHasContent = false;
		for (MSHTML::IHTMLDOMNodePtr paragraph = beginElement; paragraph; paragraph = paragraph->nextSibling) { CString text = MSHTML::IHTMLElementPtr(paragraph)->innerText; for (int index = 0; index < text.GetLength(); ++index) { const wchar_t ch = text[index]; if (ch != L' ' && ch != L'\t' && ch != L'\r' && ch != L'\n' && ch != 0x00A0) { expandedHasContent = true; break; } } if (expandedHasContent || paragraph == endElement) break; }
		After(L"expanded-content-scan");
		Before(L"poem-create"); MSHTML::IHTMLElementPtr poem(m_document->createElement(L"<DIV class=poem>")); After(L"poem-create");
		if (wasCollapsed && !expandedHasContent) { Before(L"empty-poem-fallback"); poem->innerHTML = L"<DIV class=stanza><P>&nbsp;</P></DIV>"; After(L"empty-poem-fallback"); }
		else {
			Before(L"accumulator-create"); MSHTML::IHTMLElementPtr accumulator(m_document->createElement(L"DIV")); After(L"accumulator-create");
			Before(L"accumulator-innerhtml"); accumulator->innerHTML = html.AllocSysString(); After(L"accumulator-innerhtml");
			MSHTML::IHTMLElementCollectionPtr children = accumulator->children; bool trim = true; CString stanzaHtml; After(L"stanza-loop-enter");
			for (long index = 0; children && index < children->length; ++index) { MSHTML::IHTMLElementPtr current(children->item(index)); CString line = current->innerText; if (line.Trim().IsEmpty()) { if (trim) continue; Before(L"stanza-create"); MSHTML::IHTMLElementPtr stanza(m_document->createElement(L"<DIV class=stanza>")); After(L"stanza-create"); Before(L"stanza-innerhtml"); stanza->innerHTML = stanzaHtml.AllocSysString(); After(L"stanza-innerhtml"); Before(L"stanza-insert"); MSHTML::IHTMLElement2Ptr(poem)->insertAdjacentElement(L"beforeEnd", stanza); After(L"stanza-insert"); stanzaHtml.Empty(); trim = true; } else { if (!U::scmp(current->tagName,L"DIV")) { if (current->innerText.GetBSTR()) stanzaHtml += CString(L"<P>") + current->innerText.GetBSTR() + L"</P>"; else continue; } else stanzaHtml += current->outerHTML.GetBSTR(); trim = false; } }
			if (!stanzaHtml.IsEmpty()) { Before(L"stanza-create"); MSHTML::IHTMLElementPtr stanza(m_document->createElement(L"<DIV class=stanza>")); After(L"stanza-create"); Before(L"stanza-innerhtml"); stanza->innerHTML = stanzaHtml.AllocSysString(); After(L"stanza-innerhtml"); Before(L"stanza-insert"); MSHTML::IHTMLElement2Ptr(poem)->insertAdjacentElement(L"beforeEnd", stanza); After(L"stanza-insert"); }
			MSHTML::IHTMLElementCollectionPtr poemChildren = poem->children;
			if (!poemChildren || poemChildren->length == 0) { Before(L"empty-poem-fallback"); poem->innerHTML = L"<DIV class=stanza><P>&nbsp;</P></DIV>"; After(L"empty-poem-fallback"); }
		}
		After(L"poem-dom-ready");
		Before(L"undo-begin"); FbeDom::MarkupUndoUnitScope undo(m_markupServices,L"insert poem"); After(L"undo-begin");
		MSHTML::IHTMLDOMNodePtr begin = beginElement, end = endElement;
		Before(L"insert-before"); MSHTML::IHTMLDOMNodePtr(parent)->insertBefore(MSHTML::IHTMLDOMNodePtr(poem), begin.GetInterfacePtr()); After(L"insert-before");
		After(L"remove-loop-enter"); while (begin != end) { sibling = begin->nextSibling; Before(L"remove-node"); begin->removeNode(VARIANT_TRUE); After(L"remove-node"); begin = sibling; }
		Before(L"remove-end"); end->removeNode(VARIANT_TRUE); After(L"remove-end");
		Before(L"undo-end"); undo.Close(); After(L"undo-end");
		Before(L"selection-move"); range->moveToElementText(poem); After(L"selection-move");
		Before(L"selection-collapse"); range->collapse(VARIANT_FALSE); After(L"selection-collapse");
		Before(L"selection-select"); range->select(); After(L"selection-select");
		After(L"poem-success"); return true;
	} catch (const _com_error& error) { if (m_trace) m_trace->Exception(L"poem", error.Error(), error.Description()); return false; }
}
} // namespace FbeStructure
