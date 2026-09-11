#include "../stdafx.h"
#include "SourceDocumentTransfer.h"
#include "Scintilla.h"

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
