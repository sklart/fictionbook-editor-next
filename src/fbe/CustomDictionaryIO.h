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

inline bool FbeSaveCustomDictionary(const CString& path, UINT codePage, const CSimpleArray<CString>& words)
{
	const CString temporaryPath = path + L".tmp";
	try
	{
		::DeleteFile(temporaryPath);
		std::ofstream output;
		output.open(temporaryPath, std::ios_base::out | std::ios_base::trunc);
		if (!output.is_open())
			return false;
		for (int i = 0; i < words.GetSize(); ++i)
		{
			CString word(words[i]);
			word.Replace(L"\u00AD", L"");
			output << FbeEncodeDictionaryWord(word, codePage) << '\n';
		}
		output.flush();
		const bool written = output.good();
		output.close();
		if(!written || !output.good()) { ::DeleteFile(temporaryPath); return false; }
		const bool replacingExisting = ::GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES;
		const BOOL moved = replacingExisting
			? ::ReplaceFile(path, temporaryPath, NULL, REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)
			: ::MoveFileEx(temporaryPath, path, MOVEFILE_WRITE_THROUGH);
		if(!moved) { ::DeleteFile(temporaryPath); return false; }
		return true;
	}
	catch (...) { ::DeleteFile(temporaryPath); return false; }
}
