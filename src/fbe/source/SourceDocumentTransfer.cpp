#include "../stdafx.h"
#include "SourceDocumentTransfer.h"
#include "Scintilla.h"
#include "../XmlDeclaration.h"
#include "BodySourceSelectionTransfer.h"

bool SourceDocumentTransfer::ReadSourceText(CWindow& source, SourceDocumentText& result)
{
	result = SourceDocumentText();
	const sptr_t length = source.SendMessage(SCI_GETLENGTH);
	if(length < 0 || length > INT_MAX - 1) return false;
	result.utf8.resize(static_cast<size_t>(length) + 1);
	if(!result.utf8.empty()) source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(result.utf8.data()));
	const int characters = ::MultiByteToWideChar(CP_UTF8, 0, result.utf8.data(), static_cast<int>(length), NULL, 0);
	if(length != 0 && characters == 0) return false;
	LPWSTR destination = result.text.GetBuffer(characters);
	if(characters != 0) ::MultiByteToWideChar(CP_UTF8, 0, result.utf8.data(), static_cast<int>(length), destination, characters);
	result.text.ReleaseBuffer(characters);
	const sptr_t start = source.SendMessage(SCI_GETSELECTIONSTART);
	const sptr_t end = source.SendMessage(SCI_GETSELECTIONEND);
	if(start < 0 || end < 0 || start > length || end > length) return false;
	result.caret = start == end;
	result.selectionStartByte = static_cast<int>(start);
	result.selectionEndByte = static_cast<int>(end);
	result.selectionStart = ::MultiByteToWideChar(CP_UTF8, 0, result.utf8.data(), static_cast<int>(start), NULL, 0);
	result.selectionEnd = result.caret ? result.selectionStart : ::MultiByteToWideChar(CP_UTF8, 0, result.utf8.data(), static_cast<int>(end), NULL, 0);
	return true;
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
