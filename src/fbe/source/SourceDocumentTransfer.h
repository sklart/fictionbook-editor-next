#pragma once

#include <atlstr.h>
#include <atlwin.h>
#include <mshtml.h>
#include <msxml6.h>
#include <vector>

namespace FB { class Doc; }

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

// Details for the coordinator to present an existing validation error without
// coupling conversion code to a frame, window, or dialog implementation.
struct SourceDocumentApplyResult
{
	SourceTransitionResult result = SourceTransitionResult::Failed;
	bool documentChanged = false;
	int errorLine = 0;
	int errorColumn = 0;
	CString errorMessage;
};

// Conversion-side representation of the current Scintilla buffer.  The
// transfer layer may use FB::Doc for production conversion, but deliberately
// remains independent of the application frame and all view/UI ownership.
class SourceDocumentTransfer
{
public:
	struct TextRange { int start = -1; int end = -1; };
	static SourceTransitionResult ReadSourceText(CWindow& source, SourceDocumentText& result);
	static SourceTransitionResult PrepareSerializedSource(FB::Doc& document,
		MSXML2::IXMLDOMDocumentPtr& cachedXml, const CString& encoding,
		CString& sourceText);
	static SourceDocumentApplyResult ApplySourceDocument(FB::Doc& document,
		const SourceDocumentText& source, bool sourceChanged,
		MSXML2::IXMLDOMDocumentPtr& cachedXml, const CString& interfaceLanguage);
	static CString ExtractXmlDeclarationEncoding(const CString& xmlText);
	static int SkipXmlMarkupForward(const CString& sourceXml, int position);
	static int SkipXmlMarkupBackward(const CString& sourceXml, int position);
	static int FindXmlBodyIndexAtPosition(const CString& sourceXml, int position);
	static bool FindXmlBodyRangeByIndex(const CString& sourceXml, int targetIndex, TextRange& result);
	static CString ExtractVisibleXmlText(const CString& sourceFragment);
	static bool FindVisibleXmlTextRange(const CString& sourceXml, const CString& visibleText, int scopeStart, int scopeEnd, int expectedStart, TextRange& result);
	static bool FindEnclosingXmlElementRange(const CString& sourceXml, int position, const wchar_t* elementName, TextRange& result);
	static MSHTML::IHTMLTxtRangePtr FindBodyTextRange(MSHTML::IHTMLBodyElementPtr htmlBody, MSHTML::IHTMLElementPtr htmlScope, MSHTML::IHTMLElementPtr expectedStartElement, const CString& visibleText);
	static int FindXmlNodeTextPosition(const CString& sourceXml, MSXML2::IXMLDOMNodePtr xmlNode, int textPosition, int scopeStart, int scopeEnd);
};
