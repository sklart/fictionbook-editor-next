#include "stdafx.h"

#include "apputils.h"
#include "RegexBackend.h"

namespace AU {
namespace {

IMatchCollection* BuildMatchCollection(const CSimpleArray<RegexBackend::MatchData>& backendMatches)
{
	IMatchCollection* matches = new IMatchCollection();

	for (int matchIndex = 0; matchIndex < backendMatches.GetSize(); ++matchIndex)
	{
		const RegexBackend::MatchData& backendMatch = backendMatches[matchIndex];
		IMatch2* item = new IMatch2(backendMatch.Value, backendMatch.FirstIndex);

		for (int subMatchIndex = 0; subMatchIndex < backendMatch.SubMatches.GetSize(); ++subMatchIndex)
			item->AddSubMatch(backendMatch.SubMatches[subMatchIndex]);

		matches->AddItem(item);
	}

	return matches;
}

void ThrowRegexSyntaxError(const CString& errorText)
{
	ICreateErrorInfoPtr cerrinf = 0;
	if (::CreateErrorInfo(&cerrinf) == S_OK)
	{
		cerrinf->SetDescription(errorText.AllocSysString());
		cerrinf->SetSource(const_cast<LPOLESTR>(RegexBackend::GetBackendDisplayName()));
		cerrinf->SetGUID(GUID_NULL);
		IErrorInfoPtr ei = cerrinf;
		throw _com_error(MK_E_SYNTAX, ei, true);
	}
}

}

const wchar_t* RegexBackend::GetBackendDisplayName()
{
	return L"Perl Compatible Regular Expressions 2";
}

IMatchCollection* IRegExp2::Execute(CString sourceString)
{
	RegexBackend::Options options;
	options.Pattern = m_pattern;
	options.IgnoreCase = IgnoreCase;
	options.Global = Global;
	options.Multiline = Multiline;

	CSimpleArray<RegexBackend::MatchData> backendMatches;
	CString errorText;
	if (RegexBackend::Execute(options, sourceString, backendMatches, errorText))
		return BuildMatchCollection(backendMatches);

	ThrowRegexSyntaxError(errorText);
	return new IMatchCollection();
}

}

