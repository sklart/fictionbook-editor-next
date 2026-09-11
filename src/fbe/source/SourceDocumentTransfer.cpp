#include "../stdafx.h"
#include "SourceDocumentTransfer.h"
#include "Scintilla.h"
#include "../apputils.h"
#include "../FBDoc.h"
#include "../XmlDeclaration.h"
#include "BodySourceSelectionTransfer.h"

SourceTransitionResult SourceDocumentTransfer::ReadSourceText(CWindow& source, SourceDocumentText& result)
{
	result = SourceDocumentText();
	const sptr_t length = source.SendMessage(SCI_GETLENGTH);
	if(length < 0 || length > INT_MAX - 1) return SourceTransitionResult::Failed;
	result.utf8.resize(static_cast<size_t>(length) + 1);
	if(!result.utf8.empty()) source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(result.utf8.data()));
	const int characters = ::MultiByteToWideChar(CP_UTF8, 0, result.utf8.data(), static_cast<int>(length), NULL, 0);
	if(length != 0 && characters == 0) return SourceTransitionResult::InvalidSource;
	LPWSTR destination = result.text.GetBuffer(characters);
	if(characters != 0) ::MultiByteToWideChar(CP_UTF8, 0, result.utf8.data(), static_cast<int>(length), destination, characters);
	result.text.ReleaseBuffer(characters);
	const sptr_t start = source.SendMessage(SCI_GETSELECTIONSTART);
	const sptr_t end = source.SendMessage(SCI_GETSELECTIONEND);
	if(start < 0 || end < 0 || start > length || end > length) return SourceTransitionResult::Failed;
	result.caret = start == end;
	result.selectionStartByte = static_cast<int>(start);
	result.selectionEndByte = static_cast<int>(end);
	result.selectionStart = ::MultiByteToWideChar(CP_UTF8, 0, result.utf8.data(), static_cast<int>(start), NULL, 0);
	result.selectionEnd = result.caret ? result.selectionStart : ::MultiByteToWideChar(CP_UTF8, 0, result.utf8.data(), static_cast<int>(end), NULL, 0);
	return SourceTransitionResult::Success;
}

SourceTransitionResult SourceDocumentTransfer::PrepareSerializedSource(FB::Doc& document,
	MSXML2::IXMLDOMDocumentPtr& cachedXml, const CString& encoding,
	CString& sourceText)
{
	if(document.DocRelChanged() || !(bool)cachedXml)
	{
		MSXML2::IXMLDOMDocument2Ptr candidate = document.CreateDOM(encoding);
		if(!(bool)candidate) return SourceTransitionResult::Failed;
		cachedXml = candidate;
	}

	_bstr_t serialized(cachedXml->xml);
	sourceText = static_cast<const wchar_t*>(serialized);
	CString declaration;
	declaration.Format(L"<?xml version=\"1.0\" encoding=\"%s\"?>", static_cast<const wchar_t*>(encoding));
	const CString declarationWithoutEncoding(L"<?xml version=\"1.0\"?>");
	if(sourceText.Left(declarationWithoutEncoding.GetLength()).CompareNoCase(declarationWithoutEncoding) == 0)
	{
		sourceText.Delete(0, declarationWithoutEncoding.GetLength());
		sourceText.Insert(0, declaration);
	}
	else if(sourceText.Left(5).CompareNoCase(L"<?xml") != 0)
	{
		sourceText.Insert(0, declaration + L"\r\n");
	}
	return SourceTransitionResult::Success;
}

SourceDocumentApplyResult SourceDocumentTransfer::ApplySourceDocument(FB::Doc& document,
	const SourceDocumentText& source, bool sourceChanged,
	MSXML2::IXMLDOMDocumentPtr& cachedXml, const CString& interfaceLanguage)
{
	SourceDocumentApplyResult result;
	if(!sourceChanged)
	{
		result.result = cachedXml ? SourceTransitionResult::Success : SourceTransitionResult::Failed;
		return result;
	}

	BSTR sourceText = ::SysAllocStringLen(source.text, source.text.GetLength());
	if(!sourceText) return result;
	MSXML2::IXMLDOMDocument2Ptr candidate;
	if(!document.TextToXML(sourceText, (MSXML2::IXMLDOMDocument2Ptr*)(&candidate)))
	{
		// FBD's structural check is authoritative.  Do not pass a rejected FBD
		// through the generic script parser.
		if(document.GetDocumentFileType() == FictionBookFileType::Fbd)
		{
			::SysFreeString(sourceText);
			result.result = SourceTransitionResult::InvalidSource;
			return result;
		}
		CComDispatchDriver body(document.m_body.Script());
		CComVariant args[1];
		CComVariant parsed;
		args[0] = sourceText;
		CheckError(body.Invoke1(L"XmlFromText", &args[0], &parsed));
		if(parsed.vt != VT_DISPATCH)
		{
			::SysFreeString(sourceText);
			return result;
		}
		candidate = parsed.pdispVal;
		if(!(bool)candidate)
		{
			MSXML2::IXMLDOMParseErrorPtr error = parsed.pdispVal;
			if((bool)error)
			{
				result.errorMessage = static_cast<const wchar_t*>(bstr_t(error->reason));
				result.errorLine = error->line;
				result.errorColumn = error->linepos;
			}
			::SysFreeString(sourceText);
			result.result = SourceTransitionResult::InvalidSource;
			return result;
		}
	}
	::SysFreeString(sourceText);

	// The old cache and the displayed document remain untouched until the
	// candidate has passed parsing.  Commit their new values only at this point.
	CComDispatchDriver script(document.m_body.Script());
	CComVariant args[2];
	args[1] = candidate.GetInterfacePtr();
	args[0] = interfaceLanguage;
	CheckError(script.InvokeN(L"LoadFromDOM", args, 2));
	document.m_body.Init();
	const CString encoding = ExtractXmlDeclarationEncoding(source.text);
	if(!encoding.IsEmpty()) document.m_encoding = encoding;
	cachedXml = candidate;
	result.result = SourceTransitionResult::Success;
	result.documentChanged = true;
	return result;
}

CString SourceDocumentTransfer::ExtractXmlDeclarationEncoding(const CString& xmlText)
{
	return CString(FbeExtractXmlDeclarationEncoding(std::wstring(static_cast<const wchar_t*>(xmlText))).c_str());
}

int SourceDocumentTransfer::SkipXmlMarkupForward(const CString& sourceXml, int position)
{
	while(position < sourceXml.GetLength())
	{
		const int tagBegin = sourceXml.Left(position).ReverseFind(L'<');
		const int tagEnd = tagBegin >= 0 ? sourceXml.Find(L'>', tagBegin + 1) : -1;
		if(tagBegin >= 0 && tagEnd >= position) position = tagEnd + 1;
		else break;
	}
	return position;
}

int SourceDocumentTransfer::SkipXmlMarkupBackward(const CString& sourceXml, int position)
{
	while(position > 0)
	{
		const int tagBegin = sourceXml.Left(position).ReverseFind(L'<');
		const int tagEnd = tagBegin >= 0 ? sourceXml.Find(L'>', tagBegin + 1) : -1;
		if(tagBegin >= 0 && tagEnd >= position) position = tagBegin;
		else break;
	}
	return position;
}

int SourceDocumentTransfer::FindXmlBodyIndexAtPosition(const CString& sourceXml, int position)
{
	int currentBody = -1;
	int bodyCount = 0;
	for(int tagStart = sourceXml.Find(L'<'); tagStart >= 0 && tagStart <= position;)
	{
		const int tagEnd = sourceXml.Find(L'>', tagStart + 1);
		if(tagEnd < 0) break;
		CString tag = sourceXml.Mid(tagStart + 1, tagEnd - tagStart - 1); tag.TrimLeft();
		const bool closing = !tag.IsEmpty() && tag[0] == L'/'; if(closing) tag.Delete(0);
		const int nameEnd = tag.FindOneOf(L" \t\r\n/"); CString name = nameEnd >= 0 ? tag.Left(nameEnd) : tag;
		const int separator = name.ReverseFind(L':'); if(separator >= 0) name = name.Mid(separator + 1);
		if(name.CompareNoCase(L"body") == 0) { if(closing) currentBody = -1; else currentBody = bodyCount++; }
		if(tagEnd >= position) break;
		tagStart = sourceXml.Find(L'<', tagEnd + 1);
	}
	return currentBody;
}

bool SourceDocumentTransfer::FindXmlBodyRangeByIndex(const CString& sourceXml, int targetIndex, TextRange& result)
{
	result = TextRange(); if(targetIndex < 0) return false;
	int bodyIndex = 0;
	for(int tagStart = sourceXml.Find(L'<'); tagStart >= 0;)
	{
		const int tagEnd = sourceXml.Find(L'>', tagStart + 1); if(tagEnd < 0) break;
		CString tag = sourceXml.Mid(tagStart + 1, tagEnd - tagStart - 1); tag.TrimLeft();
		const bool closing = !tag.IsEmpty() && tag[0] == L'/'; if(closing) tag.Delete(0);
		const int nameEnd = tag.FindOneOf(L" \t\r\n/"); CString name = nameEnd >= 0 ? tag.Left(nameEnd) : tag;
		const int separator = name.ReverseFind(L':'); if(separator >= 0) name = name.Mid(separator + 1);
		if(name.CompareNoCase(L"body") == 0) { if(!closing && bodyIndex++ == targetIndex) result.start = tagStart; else if(closing && result.start >= 0) { result.end = tagEnd + 1; return true; } }
		tagStart = sourceXml.Find(L'<', tagEnd + 1);
	}
	return false;
}

CString SourceDocumentTransfer::ExtractVisibleXmlText(const CString& sourceFragment)
{
	CString text;
	for(int position = 0; position < sourceFragment.GetLength();)
	{
		if(sourceFragment[position] == L'<')
		{
			const int tagEnd = sourceFragment.Find(L'>', position + 1); if(tagEnd < 0) break;
			CString tagName = sourceFragment.Mid(position + 1, tagEnd - position - 1); tagName.TrimLeft(); if(!tagName.IsEmpty() && tagName[0] == L'/') tagName.Delete(0);
			const int tagNameEnd = tagName.FindOneOf(L" \t\r\n/"); if(tagNameEnd >= 0) tagName = tagName.Left(tagNameEnd);
			if(tagName.CompareNoCase(L"p") == 0 || tagName.CompareNoCase(L"empty-line") == 0 || tagName.CompareNoCase(L"title") == 0) { while(!text.IsEmpty() && text[text.GetLength() - 1] == L' ') text.Delete(text.GetLength() - 1); if(!text.IsEmpty() && text.Right(2) != L"\r\n") text += L"\r\n"; }
			position = tagEnd + 1; continue;
		}
		if(sourceFragment[position] == L'&')
		{
			const int entityEnd = sourceFragment.Find(L';', position + 1);
			if(entityEnd >= 0) { const CString entity = sourceFragment.Mid(position, entityEnd - position + 1); std::wstring decoded; if(FBEBodySourceTransfer::DecodeXmlCharacterReference(std::wstring(static_cast<const wchar_t*>(entity)), decoded)) text += decoded.c_str(); else text += entity; position = entityEnd + 1; continue; }
		}
		const wchar_t character = sourceFragment[position++];
		if(iswspace(character) || character == L'\xA0') { if(!text.IsEmpty() && text.Right(2) != L"\r\n" && text[text.GetLength() - 1] != L' ') text += L' '; }
		else text += character;
	}
	return text;
}

bool SourceDocumentTransfer::FindVisibleXmlTextRange(const CString& sourceXml, const CString& visibleText, int scopeStart, int scopeEnd, int expectedStart, TextRange& result)
{
	FBEBodySourceTransfer::XmlTextRange range = { -1, -1 };
	if(!FBEBodySourceTransfer::FindVisibleXmlTextRange(std::wstring(static_cast<const wchar_t*>(sourceXml)), std::wstring(static_cast<const wchar_t*>(visibleText)), scopeStart, scopeEnd, expectedStart, range)) return false;
	result.start = range.start; result.end = range.end; return true;
}

bool SourceDocumentTransfer::FindEnclosingXmlElementRange(const CString& sourceXml, int position, const wchar_t* elementName, TextRange& result)
{
	FBEBodySourceTransfer::XmlTextRange range = { -1, -1 };
	if(!FBEBodySourceTransfer::FindEnclosingXmlElementRange(std::wstring(static_cast<const wchar_t*>(sourceXml)), position, elementName, range)) return false;
	result.start = range.start; result.end = range.end; return true;
}

MSHTML::IHTMLTxtRangePtr SourceDocumentTransfer::FindBodyTextRange(MSHTML::IHTMLBodyElementPtr htmlBody, MSHTML::IHTMLElementPtr htmlScope, MSHTML::IHTMLElementPtr expectedStartElement, const CString& visibleText)
{
	if(!htmlBody || !htmlScope || visibleText.IsEmpty()) return MSHTML::IHTMLTxtRangePtr();
	MSHTML::IHTMLTxtRangePtr wholeRange = htmlBody->createTextRange();
	if(wholeRange) wholeRange->moveToElementText(expectedStartElement ? expectedStartElement : htmlScope);
	if(wholeRange && wholeRange->findText(static_cast<const wchar_t*>(visibleText), 1073741824, 0) == VARIANT_TRUE) return wholeRange;
	CString startAnchor = visibleText, endAnchor = visibleText; startAnchor.TrimLeft(); endAnchor.TrimRight();
	const int firstLineEnd = startAnchor.Find(L"\r\n"); if(firstLineEnd >= 0) startAnchor = startAnchor.Left(firstLineEnd);
	const int lastLineBegin = endAnchor.ReverseFind(L'\n'); if(lastLineBegin >= 0) endAnchor = endAnchor.Mid(lastLineBegin + 1);
	startAnchor.Trim(); endAnchor.Trim(); if(startAnchor.IsEmpty() || endAnchor.IsEmpty()) return MSHTML::IHTMLTxtRangePtr();
	const int anchorLength = 96; if(startAnchor.GetLength() > anchorLength) startAnchor = startAnchor.Left(anchorLength); if(endAnchor.GetLength() > anchorLength) endAnchor = endAnchor.Right(anchorLength);
	MSHTML::IHTMLTxtRangePtr startRange = htmlBody->createTextRange(); if(startRange) startRange->moveToElementText(expectedStartElement ? expectedStartElement : htmlScope);
	if(!startRange || startRange->findText(static_cast<const wchar_t*>(startAnchor), 1073741824, 0) != VARIANT_TRUE) return MSHTML::IHTMLTxtRangePtr();
	MSHTML::IHTMLTxtRangePtr endRange = startRange->duplicate(); if(!endRange) return MSHTML::IHTMLTxtRangePtr(); endRange->collapse(VARIANT_FALSE);
	if(endRange->findText(static_cast<const wchar_t*>(endAnchor), 1073741824, 0) != VARIANT_TRUE) return MSHTML::IHTMLTxtRangePtr();
	startRange->setEndPoint(L"EndToEnd", endRange); return startRange;
}

int SourceDocumentTransfer::FindXmlNodeTextPosition(const CString& sourceXml, MSXML2::IXMLDOMNodePtr xmlNode, int textPosition, int scopeStart, int scopeEnd)
{
	if(!xmlNode) return -1;
	bstr_t nodeTextValue(xmlNode->text);
	return FBEBodySourceTransfer::FindXmlNodeTextPosition(std::wstring(static_cast<const wchar_t*>(sourceXml)), std::wstring(static_cast<const wchar_t*>(nodeTextValue)), textPosition, scopeStart, scopeEnd);
}
