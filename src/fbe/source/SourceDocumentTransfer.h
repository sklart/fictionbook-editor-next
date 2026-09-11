#pragma once

#include <atlstr.h>
#include <atlwin.h>
#include <mshtml.h>
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
	struct TextRange { int start = -1; int end = -1; };
	static bool ReadSourceText(CWindow& source, SourceDocumentText& result);
	static CString ExtractXmlDeclarationEncoding(const CString& xmlText);
	static int SkipXmlMarkupForward(const CString& sourceXml, int position);
	static int SkipXmlMarkupBackward(const CString& sourceXml, int position);
	static int FindXmlBodyIndexAtPosition(const CString& sourceXml, int position);
	static bool FindXmlBodyRangeByIndex(const CString& sourceXml, int targetIndex, TextRange& result);
	static CString ExtractVisibleXmlText(const CString& sourceFragment);
	static bool FindVisibleXmlTextRange(const CString& sourceXml, const CString& visibleText, int scopeStart, int scopeEnd, int expectedStart, TextRange& result);
	static bool FindEnclosingXmlElementRange(const CString& sourceXml, int position, const wchar_t* elementName, TextRange& result);
	static MSHTML::IHTMLTxtRangePtr FindBodyTextRange(MSHTML::IHTMLBodyElementPtr htmlBody, MSHTML::IHTMLElementPtr htmlScope, MSHTML::IHTMLElementPtr expectedStartElement, const CString& visibleText);
};
