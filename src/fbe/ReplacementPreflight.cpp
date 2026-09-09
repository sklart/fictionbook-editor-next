#include "stdafx.h"
#include "ReplacementPreflight.h"

ReplacementPreflightResult CheckReplacementRange(MSHTML::IHTMLTxtRangePtr range, bool regexp)
{
	if (!regexp)
		return ReplacementPreflightResult::Allowed;
	if (!range)
		return ReplacementPreflightResult::CrossParagraph;
	try
	{
		CString html(static_cast<LPCWSTR>(_bstr_t(range->htmlText)));
		html.MakeUpper();
		return html.Find(L"</P") >= 0 && html.Find(L"<P") >= 0
			? ReplacementPreflightResult::CrossParagraph
			: ReplacementPreflightResult::Allowed;
	}
	catch (const _com_error&)
	{
		return ReplacementPreflightResult::CrossParagraph;
	}
}
