#pragma once

#include <windows.h>
#include <atlstr.h>

enum SourceEditorColorRole
{
	SourceEditorColorEditorBackground,
	SourceEditorColorEditorForeground,
	SourceEditorColorSelectionBackground,
	SourceEditorColorSelectionForeground,
	SourceEditorColorCurrentLineBackground,
	SourceEditorColorCaret,
	SourceEditorColorLineNumber,
	SourceEditorColorLineNumberActive,
	SourceEditorColorMatchingTagBackground,
	SourceEditorColorMatchingTagBorder,
	SourceEditorColorXmlText,
	SourceEditorColorXmlTagName,
	SourceEditorColorXmlTagDelimiter,
	SourceEditorColorXmlAttributeName,
	SourceEditorColorXmlAttributeValue,
	SourceEditorColorXmlNamespace,
	SourceEditorColorXmlComment,
	SourceEditorColorXmlEntity,
	SourceEditorColorXmlCdata,
	SourceEditorColorXmlProcessingInstruction,
	SourceEditorColorXmlDoctype,
	SourceEditorColorXmlError,
	SourceEditorColorXmlWarning,
	SourceEditorColorCount
};

// Snapshot assembled by CMainFrame from application preferences.
struct SourceEditorConfig
{
	bool showEol = false, showWhitespace = false, wrap = false, showLineNumbers = true, syntaxHighlight = true;
	bool showSpecialCharacters = false;
	bool undoSelectionHistory = true;
	bool tagHighlight = false;
	bool tagHighlightFullTag = false;
	bool tagHighlightAttributes = false;
	bool tagHighlightErrors = true;
	DWORD specialCharactersStyle = 0;
	CString fontName;
	int fontSize = 10;
	COLORREF colors[SourceEditorColorCount] = {};
};
