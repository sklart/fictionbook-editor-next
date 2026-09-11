#pragma once

#include <windows.h>
#include <atlstr.h>

// Snapshot assembled by CMainFrame from application preferences.
struct SourceEditorConfig
{
	bool showEol = false, showWhitespace = false, wrap = false, showLineNumbers = true, syntaxHighlight = true;
	bool showSpecialCharacters = false;
	DWORD specialCharactersStyle = 0;
	CString fontName;
	int fontSize = 10;
	COLORREF colors[64] = {};
};
