#pragma once

#include <atlstr.h>
#include <atlwin.h>
#include <vector>

enum class SourceTransitionResult
{
	Success,
	InvalidSource,
	Failed
};

struct SourceDocumentText
{
	CString text;
	std::vector<char> utf8;
	int selectionStartByte = 0;
	int selectionEndByte = 0;
	int selectionStart = 0;
	int selectionEnd = 0;
	bool caret = true;
};

// Conversion-side representation of the current Scintilla buffer.  It is
// deliberately independent of the application frame and FB::Doc; document
// parsing and view activation stay in their respective layers.
class SourceDocumentTransfer
{
public:
	static bool ReadSourceText(CWindow& source, SourceDocumentText& result);
	static CString ExtractXmlDeclarationEncoding(const CString& xmlText);
};
