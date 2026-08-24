#pragma once

#include <atlstr.h>
#include <atlcoll.h>
#include <atlpath.h>
#include <fstream>
#include "SpellText.h"

inline bool FbeCustomDictionaryContains(const CSimpleArray<CString>& words, const CString& word)
{
	return words.Find(word) >= 0;
}

inline void FbeLoadCustomDictionary(const CString& path, UINT codePage, CSimpleArray<CString>& words)
{
	words.RemoveAll();
	char buffer[256];
	if (!ATLPath::FileExists(path))
		return;
	try
	{
		std::ifstream input;
		input.open(path);
		if (!input.is_open())
			return;
		while (input.getline(buffer, sizeof(buffer), '\n'))
		{
			CString word = FbeDecodeDictionaryWord(buffer, codePage);
			if (!word.IsEmpty())
				words.Add(word);
		}
	}
	catch (...) {}
}

inline void FbeSaveCustomDictionary(const CString& path, UINT codePage, const CSimpleArray<CString>& words)
{
	try
	{
		std::ofstream output;
		output.open(path, std::ios_base::out | std::ios_base::trunc);
		if (!output.is_open())
			return;
		for (int i = 0; i < words.GetSize(); ++i)
		{
			CString word(words[i]);
			word.Replace(L"\u00AD", L"");
			output << FbeEncodeDictionaryWord(word, codePage) << '\n';
		}
	}
	catch (...) {}
}
