#pragma once

#include <atlstr.h>

// Reviewed exact word forms only. See docs/russian-yo-usage.md for sources.
// An absent form must never be classified from Hunspell rejection alone.
inline bool FbeConfirmedRussianYoCorrection(const CString& word, CString& correction)
{
	CString lower(word);
	lower.MakeLower();
	if(lower == L"газёта") correction = L"газета";
	else if(lower == L"дёлал") correction = L"делал";
	else return false;
	return true;
}

// Classification above uses complete forms; this only preserves case and
// optional combining accents / soft hyphens in the offered replacement.
inline CString FbePreserveRussianYoCorrectionFormatting(CString source)
{
	source.Replace(L'ё', L'е');
	source.Replace(L'Ё', L'Е');
	return source;
}
