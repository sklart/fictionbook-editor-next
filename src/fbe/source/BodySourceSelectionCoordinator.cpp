#include "../stdafx.h"
#include "BodySourceSelectionCoordinator.h"
#include "SourceDocumentTransfer.h"
#include "ui/SourceEditorControl.h"
#include "../apputils.h"
#include "../utils/utils.h"
#include "../FBDoc.h"
#include "../view/EditorSelectionState.h"

void BodySourceSelectionCoordinator::MapSourceSelectionToBody(FB::Doc& document,
	MSXML2::IXMLDOMDocumentPtr xml, const SourceDocumentText& source, EditorSelectionState& selection)
{
	selection.BodySource().sourceToBodyTransferred = false;
	const bool caret = source.caret;
	int begin = source.selectionStart;
	int end = source.selectionEnd;
	if(!caret)
	{
		begin = SourceDocumentTransfer::SkipXmlMarkupForward(source.text, begin);
		end = SourceDocumentTransfer::SkipXmlMarkupBackward(source.text, end);
		if(end < begin) end = begin;
	}
	const int bodyAtSelection = SourceDocumentTransfer::FindXmlBodyIndexAtPosition(source.text, begin);
	CString visibleText;
	bool crossesParagraph = false;
	if(end > begin)
	{
		const CString selectedXml = source.text.Mid(begin, end - begin);
		visibleText = SourceDocumentTransfer::ExtractVisibleXmlText(selectedXml);
		crossesParagraph = selectedXml.Find(L"</p") >= 0 || selectedXml.Find(L"<p") >= 0;
	}
	BSTR rawText = ::SysAllocStringLen(source.text, source.text.GetLength());
	if(!rawText) return;
	int beginCharacter = 0, endCharacter = 0;
	U::DomPath beginPath, endPath;
	bool pathAvailable = beginPath.CreatePathFromText(rawText, begin, &beginCharacter);
	if(caret) { endPath = beginPath; endCharacter = beginCharacter; }
	else pathAvailable = endPath.CreatePathFromText(rawText, end, &endCharacter) && pathAvailable;
	::SysFreeString(rawText);

	MSXML2::IXMLDOMNodePtr selectedBody;
	MSXML2::IXMLDOMElementPtr beginElement, endElement;
	U::DomPath fallbackBeginPath, fallbackScopePath;
	bool fallbackPathAvailable = false;
	int selectedBodyIndex = bodyAtSelection;
	if(pathAvailable)
	{
		beginElement = beginPath.GetNodeFromXMLDOM(xml);
		MSXML2::IXMLDOMNodeListPtr children = xml->documentElement->childNodes;
		int bodyIndex = 0;
		for(int i = 0; beginElement && i < children->length; ++i)
		{
			if(U::scmp(children->item[i]->nodeName, L"body") != 0) continue;
			if(U::IsParentElement(beginElement, children->item[i])) { selectedBody = children->item[i]; selectedBodyIndex = bodyIndex; break; }
			++bodyIndex;
		}
		pathAvailable = (bool)selectedBody;
		if(pathAvailable)
		{
			MSXML2::IXMLDOMNodePtr scope = beginElement;
			for(MSXML2::IXMLDOMNodePtr parent = scope->parentNode; parent && parent != selectedBody; parent = parent->parentNode)
				if(U::scmp(parent->nodeName, L"section") == 0) { scope = parent; break; }
			fallbackPathAvailable = fallbackBeginPath.CreatePathFromXMLDOM(selectedBody, beginElement) &&
				fallbackScopePath.CreatePathFromXMLDOM(selectedBody, scope);
			if(!crossesParagraph)
			{
				pathAvailable = beginPath.CreatePathFromXMLDOM(selectedBody, beginElement);
				if(caret) endPath = beginPath;
				else { endElement = endPath.GetNodeFromXMLDOM(xml); pathAvailable = endElement && endPath.CreatePathFromXMLDOM(selectedBody, endElement) && pathAvailable; }
			}
		}
	}
	if(pathAvailable && !crossesParagraph)
	{
		MSHTML::IHTMLDOMNodePtr root = document.m_body.Document()->body;
		if(root) root = root->firstChild; if(root) root = root->nextSibling; if(root) root = root->firstChild;
		int bodyIndex = selectedBodyIndex;
		while(root)
		{
			if(U::scmp(MSHTML::IHTMLElementPtr(root)->className, L"body") == 0 && bodyIndex-- == 0)
			{
				MSHTML::IHTMLElementPtr htmlBegin = beginPath.GetNodeFromHTMLDOM(root);
				MSHTML::IHTMLElementPtr htmlEnd = caret ? htmlBegin : endPath.GetNodeFromHTMLDOM(root);
				if(htmlBegin && htmlEnd) { document.m_body.GoTo(htmlBegin); selection.BodyRange() = document.m_body.SetSelection(htmlBegin, htmlEnd, beginCharacter, endCharacter); selection.BodySource().sourceToBodyTransferred = (bool)selection.BodyRange(); }
				break;
			}
			root = root->nextSibling;
		}
	}
	if(selection.BodySource().sourceToBodyTransferred || visibleText.IsEmpty()) return;
	MSHTML::IHTMLDOMNodePtr root = document.m_body.Document()->body;
	if(root) root = root->firstChild; if(root) root = root->nextSibling; if(root) root = root->firstChild;
	int bodyIndex = selectedBodyIndex >= 0 ? selectedBodyIndex : 0;
	MSHTML::IHTMLElementPtr scope, expected;
	while(root)
	{
		MSHTML::IHTMLElementPtr element(root);
		if(element && U::scmp(element->className, L"body") == 0)
		{
			if(bodyIndex-- == 0) { scope = element; if(fallbackPathAvailable) { MSHTML::IHTMLElementPtr refined = fallbackScopePath.GetNodeFromHTMLDOM(root); if(refined) scope = refined; expected = fallbackBeginPath.GetNodeFromHTMLDOM(root); } break; }
		}
		root = root->nextSibling;
	}
	MSHTML::IHTMLTxtRangePtr range = SourceDocumentTransfer::FindBodyTextRange(
		MSHTML::IHTMLBodyElementPtr(document.m_body.Document()->body), scope, expected, visibleText);
	if(range) { selection.BodyRange() = range; selection.BodySource().sourceToBodyTransferred = true; }
}

void BodySourceSelectionCoordinator::MapBodySelectionToSource(FB::Doc& document,
	MSXML2::IXMLDOMDocumentPtr xml, SourceEditorControl& source,
	const CString& serializedSource, EditorSelectionState& selection)
{
	selection.BodySource().bodyToSourceTransferred = false;
	int beginCharacter = 0, endCharacter = 0, selectedBodyIndex = -1;
	MSHTML::IHTMLElementPtr beginElement, endElement;
	document.m_body.GetSelectionInfo((MSHTML::IHTMLElementPtr*)(&beginElement),
		(MSHTML::IHTMLElementPtr*)(&endElement), &beginCharacter, &endCharacter, 0);
	if(beginElement == endElement && selection.BodyRange())
	{
		const CString text((const wchar_t*)selection.BodyRange()->text);
		if(!text.IsEmpty()) endCharacter = beginCharacter + text.GetLength();
	}
	U::DomPath beginPath, endPath;
	bool pathAvailable = false;
	bool sameElement = beginElement == endElement;
	MSHTML::IHTMLDOMNodePtr root = document.m_body.Document()->body;
	if(root) root = root->firstChild; if(root) root = root->nextSibling; if(root) root = root->firstChild;
	int bodyIndex = 0;
	while(root && beginElement && endElement)
	{
		if(U::scmp(MSHTML::IHTMLElementPtr(root)->className, L"body") == 0)
		{
			if(U::IsParentElement(endElement, root))
			{
				selectedBodyIndex = bodyIndex;
				pathAvailable = beginPath.CreatePathFromHTMLDOM(root, beginElement);
				if(sameElement) endPath = beginPath;
				else pathAvailable = endPath.CreatePathFromHTMLDOM(root, endElement) && pathAvailable;
				break;
			}
			++bodyIndex;
		}
		root = root->nextSibling;
	}
	CString serialized(serializedSource);
	int beginPosition = -1, endPosition = -1;
	bool hasSelectionText = false;
	if(selection.BodyRange())
	{
		const CString selectedText((const wchar_t*)selection.BodyRange()->text);
		hasSelectionText = !selectedText.IsEmpty();
		if(!selectedText.IsEmpty())
		{
			SourceDocumentTransfer::TextRange bodyRange;
			if(SourceDocumentTransfer::FindXmlBodyRangeByIndex(serialized, selectedBodyIndex, bodyRange))
			{
				int expectedBegin = -1;
				if(pathAvailable)
				{
					_bstr_t text((const wchar_t*)serialized);
					expectedBegin = beginPath.GetNodeFromText(text, beginCharacter);
				}
				SourceDocumentTransfer::TextRange visible;
				if(SourceDocumentTransfer::FindVisibleXmlTextRange(serialized, selectedText,
					bodyRange.start, bodyRange.end, expectedBegin, visible))
				{
					beginPosition = visible.start; endPosition = visible.end;
				}
			}
		}
	}
	if(beginPosition < 0 && endPosition < 0 && pathAvailable && !hasSelectionText)
	{
		_bstr_t text((const wchar_t*)serialized);
		beginPosition = beginPath.GetNodeFromText(text, beginCharacter);
		endPosition = endPath.GetNodeFromText(text, endCharacter);
	}
	int beginByte = 0, endByte = 0;
	if(beginPosition >= 0 && endPosition >= 0)
	{
		beginByte = ::WideCharToMultiByte(CP_UTF8, 0, serialized, beginPosition, NULL, 0, NULL, NULL);
		endByte = ::WideCharToMultiByte(CP_UTF8, 0, serialized, endPosition, NULL, 0, NULL, NULL);
		selection.BodySource().bodyToSourceTransferred = true;
	}
	selection.BodySource().sourceStart = beginByte;
	selection.BodySource().sourceEnd = endByte;
	source.SendMessage(SCI_SETSELECTIONSTART, beginByte);
	source.SendMessage(SCI_SETSELECTIONEND, endByte);
	source.SendMessage(SCI_SCROLLCARET);
}
