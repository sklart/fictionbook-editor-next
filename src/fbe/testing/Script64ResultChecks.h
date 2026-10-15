#pragma once

// Exact line-oriented checks shared by the runtime gate and its negative probes.
static bool Script64ResultIs(const CString& results, const wchar_t* key, const wchar_t* expected)
{
	int position = 0, count = 0;
	bool matches = false;
	while(position >= 0)
	{
		CString line = results.Tokenize(L"\n", position);
		line.TrimRight(L"\r");
		const int equals = line.Find(L'=');
		if(equals >= 0 && line.Left(equals) == key)
		{
			++count;
			matches = line.Mid(equals + 1) == expected;
		}
	}
	return count == 1 && matches;
}

static bool Script64CasesPass(const CString& results)
{
	for(const wchar_t* key : { L"id-collision-0", L"id-collision-1", L"id-collision-2", L"id-collision-3", L"id-collision-4",
		L"cleanup-mode-0", L"cleanup-mode-1", L"cleanup-mode-2", L"textarea-history-observed",
		L"textarea-undo", L"textarea-redo",
		L"ordinary", L"special", L"backslash", L"quote",
		L"replacement-special", L"empty-selection", L"empty-list", L"invalid-list",
		L"mixed-special", L"newlines", L"cyrillic", L"no-text", L"invalid-late",
		L"nodes", L"textarea", L"numeric-order", L"duplicates", L"cascade", L"utf16-reject" })
		if(!Script64ResultIs(results, key, L"1")) return false;
	return true;
}

static bool Script64ParserSelfTest()
{
	const CString good = L"id-collision-0=1\nid-collision-1=1\nid-collision-2=1\nid-collision-3=1\nid-collision-4=1\ncleanup-mode-0=1\ncleanup-mode-1=1\ncleanup-mode-2=1\ntextarea-history-observed=1\nordinary=1\nspecial=1\nbackslash=1\nquote=1\nreplacement-special=1\nempty-selection=1\nempty-list=1\ninvalid-list=1\nmixed-special=1\nnewlines=1\ncyrillic=1\nno-text=1\ninvalid-late=1\nnodes=1\ntextarea=1\nnumeric-order=1\nduplicates=1\ncascade=1\nutf16-reject=1\n";
	CString bad(good);
	bad.Replace(L"\nspecial=1\n", L"\nspecial=0\n");
	CString missing(good);
	missing.Replace(L"\nspecial=1\n", L"\n");
	const CString complete = good + L"textarea-undo=1\ntextarea-redo=1\nresult=pass\n";
	for(const wchar_t* key : { L"textarea-undo", L"textarea-redo" })
	{
		CString entry, zero;
		entry.Format(L"%s=1\n", key);
		zero.Format(L"%s=0\n", key);
		CString disabled(complete), absent(complete);
		disabled.Replace(entry, zero);
		absent.Replace(entry, L"");
		if(Script64CasesPass(disabled) || Script64CasesPass(absent) ||
			Script64CasesPass(complete + entry)) return false;
	}
	const CString fieldResults = L"textarea-undo=1\ntextarea-redo=1\n";
	return Script64CasesPass(complete) && !Script64CasesPass(bad + fieldResults) &&
		!Script64CasesPass(missing + fieldResults) && !Script64CasesPass(complete + L"special=1\n") &&
		!Script64ResultIs(L"special=10\n", L"special", L"1");
}
