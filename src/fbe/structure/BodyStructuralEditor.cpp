#include "stdafx.h"
#include "BodyStructuralEditor.h"
#include "StructuralTrace.h"
#include "../dom/MarkupUndoUnitScope.h"
#include "../view/VisualDomNormalizer.h"
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
StructuralOperationResult BodyStructuralEditor::InsertCite(bool checkOnly)
{
	bool documentChanged = false;
	try {
		Before(L"cite-enter"); After(L"cite-enter");
		Before(L"selection-create"); MSHTML::IHTMLTxtRangePtr range(m_document->selection->createRange()); After(L"selection-create");
		if (!range) return StructuralOperationResult::NotApplicable();
		Before(L"parent-resolved"); MSHTML::IHTMLElementPtr parent(FindParentDiv(range->parentElement())); After(L"parent-resolved");
		if (!parent) return StructuralOperationResult::NotApplicable();
		Before(L"start-range-created"); MSHTML::IHTMLTxtRangePtr start(range->duplicate()); start->collapse(VARIANT_TRUE); After(L"start-range-created");
		if (FindParentDiv(start->parentElement()) != parent) { After(L"preflight-rejected"); return StructuralOperationResult::NotApplicable(); }
		_bstr_t cls(parent->className);
		if (U::scmp(cls, L"section") && U::scmp(cls, L"epigraph") && U::scmp(cls, L"annotation") && U::scmp(cls, L"history")) return StructuralOperationResult::NotApplicable();
		After(L"preflight-complete");
		Before(L"expand"); MSHTML::IHTMLElementPtr beginElement, endElement; if (!ExpandRangeToParagraphs(range, beginElement, endElement)) return StructuralOperationResult::NotApplicable(); After(L"expand");
		if (checkOnly) { After(L"cite-check-success"); return { StructuralOperationStatus::Applied, S_OK, false }; }
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
		documentChanged = true;
		Before(L"selection-move"); range->moveToElementText(cite); After(L"selection-move");
		Before(L"selection-collapse"); range->collapse(VARIANT_FALSE); After(L"selection-collapse");
		Before(L"selection-select"); range->select(); After(L"selection-select");
		After(L"cite-success"); return StructuralOperationResult::Applied();
	} catch (const _com_error& error) { if (m_trace) m_trace->Exception(L"cite", error.Error(), error.Description()); return StructuralOperationResult::Failed(error.Error(), documentChanged); }
}
StructuralOperationResult BodyStructuralEditor::InsertPoem(bool checkOnly)
{
	bool documentChanged = false;
	try {
		Before(L"poem-enter"); After(L"poem-enter");
		Before(L"selection-create"); MSHTML::IHTMLTxtRangePtr range(m_document->selection->createRange()); After(L"selection-create");
		if (!range) return StructuralOperationResult::NotApplicable();
		Before(L"parent-resolved"); MSHTML::IHTMLElementPtr parent(FindParentDiv(range->parentElement())); After(L"parent-resolved");
		if (!parent) return StructuralOperationResult::NotApplicable();
		Before(L"collapsed-state"); const bool wasCollapsed = range->compareEndPoints(L"StartToEnd", range) == 0; After(L"collapsed-state");
		Before(L"start-range-created"); MSHTML::IHTMLTxtRangePtr start(range->duplicate()); start->collapse(VARIANT_TRUE); After(L"start-range-created");
		if (FindParentDiv(start->parentElement()) != parent) { After(L"preflight-rejected"); return StructuralOperationResult::NotApplicable(); }
		_bstr_t cls(parent->className);
		if (U::scmp(cls,L"section") && U::scmp(cls,L"epigraph") && U::scmp(cls,L"annotation") && U::scmp(cls,L"history") && U::scmp(cls,L"cite")) return StructuralOperationResult::NotApplicable();
		After(L"preflight-complete");
		Before(L"expand"); MSHTML::IHTMLElementPtr beginElement, endElement; if (!ExpandRangeToParagraphs(range, beginElement, endElement)) return StructuralOperationResult::NotApplicable(); After(L"expand");
		if (checkOnly) { After(L"poem-check-success"); return { StructuralOperationStatus::Applied, S_OK, false }; }
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
		documentChanged = true;
		Before(L"selection-move"); range->moveToElementText(poem); After(L"selection-move");
		Before(L"selection-collapse"); range->collapse(VARIANT_FALSE); After(L"selection-collapse");
		Before(L"selection-select"); range->select(); After(L"selection-select");
		After(L"poem-success"); return StructuralOperationResult::Applied();
	} catch (const _com_error& error) { if (m_trace) m_trace->Exception(L"poem", error.Error(), error.Description()); return StructuralOperationResult::Failed(error.Error(), documentChanged); }
}

StructuralOperationResult BodyStructuralEditor::SplitContainer(bool checkOnly)
{
	return SplitContainer(checkOnly, SplitFailurePoint::None);
}

StructuralOperationResult BodyStructuralEditor::SplitContainer(bool checkOnly, SplitFailurePoint failurePoint)
{
	bool documentChanged = false;
	try {
		Before(L"split-enter"); After(L"split-enter");
		Before(L"selection-create"); MSHTML::IHTMLTxtRangePtr range(m_document->selection->createRange()); After(L"selection-create");
		if (!range) return StructuralOperationResult::NotApplicable();
		Before(L"parent-resolved"); MSHTML::IHTMLElementPtr parent(FindParentDiv(range->parentElement())); After(L"parent-resolved");
		if (!parent || (U::scmp(parent->className, L"section") && U::scmp(parent->className, L"stanza"))) return StructuralOperationResult::NotApplicable();
		Before(L"container-range-create"); MSHTML::IHTMLTxtRangePtr containerRange(range->duplicate()); After(L"container-range-create");
		Before(L"container-range-move"); containerRange->moveToElementText(parent); After(L"container-range-move");
		Before(L"range-start-validated"); const bool startsAtContainer = range->compareEndPoints(L"StartToStart", containerRange) == 0; After(L"range-start-validated");
		if (startsAtContainer) return StructuralOperationResult::NotApplicable();
		Before(L"range-start-create"); MSHTML::IHTMLTxtRangePtr start(range->duplicate()); start->collapse(VARIANT_TRUE); After(L"range-start-create");
		Before(L"range-end-create"); MSHTML::IHTMLTxtRangePtr end(range->duplicate()); end->collapse(VARIANT_FALSE); After(L"range-end-create");
		Before(L"range-end-validated"); const bool endpointsMatch = FindParentDiv(start->parentElement()) == parent && FindParentDiv(end->parentElement()) == parent; After(L"range-end-validated");
		if (!endpointsMatch) return StructuralOperationResult::NotApplicable();
		After(L"preflight-complete");
		if (checkOnly) { After(L"split-check-success"); return { StructuralOperationStatus::Applied, S_OK, false }; }

		CString undoName(L"split "); undoName += static_cast<const wchar_t*>(parent->className);
		Before(L"new-container-create"); MSHTML::IHTMLElementPtr next(m_document->createElement(L"DIV")); next->className = parent->className; After(L"new-container-create");
		_bstr_t className = parent->className;

		Before(L"title-prototype-create"); MSHTML::IHTMLElementPtr parentTitle(m_document->createElement(L"DIV")); After(L"title-prototype-create");
		MSHTML::IHTMLElementCollectionPtr parentChildren = parent->children;
		{
			MSHTML::IHTMLElementPtr firstChild = parentChildren->item(0);
			if (!U::scmp(firstChild->tagName, L"DIV") && !U::scmp(firstChild->className, L"title")) parentTitle->innerHTML = firstChild->outerHTML;
			else parentTitle = NULL;
		}

		MSHTML::IMarkupPointerPtr selectionStart, selectionEnd, elementBegin, elementEnd;
		Before(L"selection-start-pointer-create"); HRESULT hr = m_markupServices->CreateMarkupPointer(&selectionStart); Hr(L"selection-start-pointer-create", hr); if (FAILED(hr)) return StructuralOperationResult::Failed(hr);
		Before(L"selection-end-pointer-create"); hr = m_markupServices->CreateMarkupPointer(&selectionEnd); Hr(L"selection-end-pointer-create", hr); if (FAILED(hr)) return StructuralOperationResult::Failed(hr);
		Before(L"element-start-pointer-create"); hr = m_markupServices->CreateMarkupPointer(&elementBegin); Hr(L"element-start-pointer-create", hr); if (FAILED(hr)) return StructuralOperationResult::Failed(hr);
		Before(L"element-end-pointer-create"); hr = m_markupServices->CreateMarkupPointer(&elementEnd); Hr(L"element-end-pointer-create", hr); if (FAILED(hr)) return StructuralOperationResult::Failed(hr);

		Before(L"title-range-create"); MSHTML::IHTMLTxtRangePtr titleRange(range->duplicate()); After(L"title-range-create");
		Before(L"selection-pointers-move"); hr = m_markupServices->MovePointersToRange(titleRange, selectionStart, selectionEnd); Hr(L"selection-pointers-move", hr); if (FAILED(hr)) return StructuralOperationResult::Failed(hr);
		U::ElTextHTML title(titleRange->htmlText, titleRange->text);
		Before(L"pre-range-create"); MSHTML::IHTMLTxtRangePtr preRange(range->duplicate()); After(L"pre-range-create");
		Before(L"element-start-pointer-move"); hr = elementBegin->MoveAdjacentToElement(parent, MSHTML::ELEM_ADJ_AfterBegin); Hr(L"element-start-pointer-move", hr); if (FAILED(hr)) return StructuralOperationResult::Failed(hr);
		Before(L"pre-range-move"); hr = m_markupServices->MoveRangeToPointers(elementBegin, selectionStart, preRange); Hr(L"pre-range-move", hr); if (FAILED(hr)) return StructuralOperationResult::Failed(hr);
		U::ElTextHTML pre(preRange->htmlText, preRange->text);

		const bool hasTitle = !title.text.IsEmpty();
		if (hasTitle && title.html.Find(L"<P") == -1) title.html = CString(L"<P>") + title.html + L"</P>";

		// Build the replacement while it is detached.  MSHTML records the
		// insertion itself as the undoable mutation; filling an already attached
		// DIV leaves its child markup outside that transaction on older engines.
		Before(L"post-range-create"); MSHTML::IHTMLTxtRangePtr postRange(range->duplicate()); After(L"post-range-create");
		Before(L"element-end-pointer-move"); hr = elementEnd->MoveAdjacentToElement(parent, MSHTML::ELEM_ADJ_BeforeEnd); Hr(L"element-end-pointer-move", hr); if (FAILED(hr)) return StructuralOperationResult::Failed(hr);
		Before(L"post-range-move"); hr = m_markupServices->MoveRangeToPointers(selectionEnd, elementEnd, postRange); Hr(L"post-range-move", hr); if (FAILED(hr)) return StructuralOperationResult::Failed(hr);
		U::ElTextHTML post(postRange->htmlText, postRange->text);
		const bool hasContent = !post.html.IsEmpty();
		const CString id(static_cast<const wchar_t*>(parent->id));
		if (hasContent && post.html.Find(L"<P") == -1) post.html = CString(L"<P>") + post.html + L"</P>";
		title.html.Remove(L'\r'); title.html.Remove(L'\n'); post.html.Remove(L'\r'); post.html.Remove(L'\n');
		if (post.html.Find(L"<P>&nbsp;</P>") == 0 && post.html.GetLength() > 13 && hasTitle && title.html.Find(L"<P>&nbsp;</P>") != title.html.GetLength() - 14) post.html.Delete(0, 13);
		if (hasContent) {
			if (post.html == L"<P>&nbsp;</P>") post.html += L"<P>&nbsp;</P>";
			Before(L"new-container-content"); next->innerHTML = post.html.AllocSysString(); After(L"new-container-content");
		} else {
			Before(L"empty-paragraph-create"); MSHTML::IHTMLElementPtr paragraph(m_document->createElement(L"P")); MSHTML::IHTMLElement3Ptr(paragraph)->inflateBlock = VARIANT_TRUE; MSHTML::IHTMLElement2Ptr(next)->insertAdjacentElement(L"beforeEnd", paragraph); After(L"empty-paragraph-create");
		}
		if (hasTitle) {
			Before(L"title-create"); MSHTML::IHTMLElementPtr nextTitle(m_document->createElement(L"DIV")); nextTitle->className = L"title"; MSHTML::IHTMLElement2Ptr(next)->insertAdjacentElement(L"afterBegin", nextTitle); nextTitle->innerHTML = title.html.AllocSysString(); FbeVisualDom::KillDivs(nextTitle); FbeVisualDom::KillStyles(nextTitle); After(L"title-create");
		}
		// Detached construction above is not a document mutation.  Start the
		// unit immediately before changing the source container or inserting next.
		Before(L"undo-begin"); FbeDom::MarkupUndoUnitScope undo(m_markupServices, static_cast<const wchar_t*>(undoName)); After(L"undo-begin");
		if (failurePoint == SplitFailurePoint::BeforeMutation) {
			After(L"fault-before-mutation"); undo.Close();
			return StructuralOperationResult::Failed(E_FAIL, false);
		}
		parent->id = L"";
		// From this point on the live document has changed, even if a later
		// cleanup or caret call fails.
		documentChanged = true;
		if (failurePoint == SplitFailurePoint::AfterFirstMutation) {
			After(L"fault-after-first-mutation"); undo.Close();
			return StructuralOperationResult::Failed(E_FAIL, true);
		}
		if (hasContent) {
			Before(L"insert-container"); MSHTML::IHTMLDOMNodePtr parentNode(parent), nextNode(next), sibling(parentNode->nextSibling); parentNode->parentNode->insertBefore(nextNode, sibling.GetInterfacePtr()); After(L"insert-container");
			next->id = _bstr_t(id.GetString());
			if (m_trace) m_trace->After(L"new-container-id", static_cast<const wchar_t*>(next->id));
		}
		if (pre.html.Find(L"<P") == -1) pre.html = pre.html.IsEmpty() ? L"<P>&nbsp;</P>" : CString(L"<P>") + pre.html + L"</P>";
		auto replaceChildren = [&](MSHTML::IHTMLElementPtr destination, const CString& html, const wchar_t* phase) {
			Before(phase); MSHTML::IHTMLElementPtr staging(m_document->createElement(L"DIV")); staging->innerHTML = html.AllocSysString();
			MSHTML::IHTMLDOMNodePtr destinationNode(destination), stagingNode(staging);
			while (destinationNode->firstChild) { MSHTML::IHTMLDOMNodePtr(destinationNode->firstChild)->removeNode(VARIANT_TRUE); }
			while (stagingNode->firstChild) { MSHTML::IHTMLDOMNodePtr node(stagingNode->firstChild); destinationNode->appendChild(node); }
			After(phase);
		};
		Before(L"source-cleanup"); range->pasteHTML(L""); postRange->pasteHTML(L""); FbeVisualDom::FixupParagraphs(parent); FbeVisualDom::PackText(parent, m_document); FbeVisualDom::FixupParagraphs(next); FbeVisualDom::PackText(next, m_document); After(L"source-cleanup");
		if (!hasContent) {
			Before(L"insert-container"); MSHTML::IHTMLDOMNodePtr parentNode(parent), nextNode(next), sibling(parentNode->nextSibling); parentNode->parentNode->insertBefore(nextNode, sibling.GetInterfacePtr()); After(L"insert-container");
			next->id = _bstr_t(id.GetString());
			if (m_trace) m_trace->After(L"new-container-id", static_cast<const wchar_t*>(next->id));
		}
		if (m_trace) m_trace->After(L"new-container-id-after-cleanup", static_cast<const wchar_t*>(next->id));
		parentChildren = parent->children;
		if (parentChildren->length == 1) { MSHTML::IHTMLElementPtr child = parentChildren->item(0); if (!U::scmp(child->tagName, L"DIV") && !U::scmp(child->className, className.GetBSTR())) { Before(L"source-wrapper-remove"); hr = m_markupServices->RemoveElement(child); Hr(L"source-wrapper-remove", hr); if (FAILED(hr)) return StructuralOperationResult::Failed(hr, documentChanged); } }
		MSHTML::IHTMLElementCollectionPtr nextChildren = next->children;
		if (nextChildren->length == 1) { MSHTML::IHTMLElementPtr child = nextChildren->item(0); if (!U::scmp(child->tagName, L"DIV") && !U::scmp(child->className, className.GetBSTR())) { Before(L"new-wrapper-remove"); hr = m_markupServices->RemoveElement(child); Hr(L"new-wrapper-remove", hr); if (FAILED(hr)) return StructuralOperationResult::Failed(hr, documentChanged); } }
		CString titleSection;
		if (parentTitle) { titleSection = parentTitle->innerHTML.GetBSTR(); titleSection += L"<P>&nbsp;</P>"; }
		CString parentText = parent->innerText; parentText.Remove(L'\r'); parentText.Remove(L'\n');
		CString titleText = parentTitle ? parentTitle->innerText : L""; titleText.Remove(L'\r'); titleText.Remove(L'\n');
		if (parentTitle && !U::scmp(parentText, titleText)) replaceChildren(parent, titleSection, L"source-title-replace");
		Before(L"undo-end"); undo.Close(); After(L"undo-end");
		documentChanged = true;
		Before(L"selection-update");
		HRESULT selectionWarning = S_OK;
		try {
			MSHTML::IHTMLTxtRangePtr selection(MSHTML::IHTMLBodyElementPtr(m_document->body)->createTextRange());
			// MSHTML rejects moveToElementText(DIV.stanza) with E_INVALIDARG. Put
			// the caret in the first leaf of the newly created container instead.
			MSHTML::IHTMLElementPtr selectionTarget(next);
			for (;;) {
				MSHTML::IHTMLElementCollectionPtr selectionChildren(selectionTarget ? selectionTarget->children : MSHTML::IHTMLElementCollectionPtr());
				if (!selectionChildren || selectionChildren->length == 0) break;
				MSHTML::IHTMLElementPtr child(selectionChildren->item(0));
				if (!child) break;
				selectionTarget = child;
			}
			// BODY text ranges reject some visual P/V descendants.  Place the
			// pointer just inside the leaf instead of at its element boundary.
			MSHTML::IMarkupPointerPtr selectionPointer;
			Before(L"selection-pointer-create"); hr = m_markupServices->CreateMarkupPointer(&selectionPointer); Hr(L"selection-pointer-create", hr); if (FAILED(hr)) { selectionWarning = hr; }
			const MSHTML::_ELEMENT_ADJACENCY selectionEdge = CString((const wchar_t*)selectionTarget->innerText).IsEmpty()
				? MSHTML::ELEM_ADJ_AfterBegin : MSHTML::ELEM_ADJ_BeforeEnd;
			if (SUCCEEDED(selectionWarning)) { Before(L"selection-pointer-move"); hr = selectionPointer->MoveAdjacentToElement(selectionTarget, selectionEdge); Hr(L"selection-pointer-move", hr); if (FAILED(hr)) selectionWarning = hr; }
			if (SUCCEEDED(selectionWarning)) { Before(L"selection-range-move"); hr = m_markupServices->MoveRangeToPointers(selectionPointer, selectionPointer, selection); Hr(L"selection-range-move", hr); if (FAILED(hr)) selectionWarning = hr; }
			MSHTML::IHTMLElement2Ptr(m_document->body)->focus();
			if (SUCCEEDED(selectionWarning)) selection->select();
		} catch (_com_error& error) { selectionWarning = error.Error(); Hr(L"selection-update", error.Error()); }
		After(L"selection-update");
		After(L"split-success"); return StructuralOperationResult::Applied(selectionWarning);
	} catch (const _com_error& error) { if (m_trace) m_trace->Exception(L"split", error.Error(), error.Description()); return StructuralOperationResult::Failed(error.Error(), documentChanged); }
}
} // namespace FbeStructure
