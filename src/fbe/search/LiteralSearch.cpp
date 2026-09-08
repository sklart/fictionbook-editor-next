#include "LiteralSearch.h"

#include <windows.h>

namespace AU {
namespace Search {
namespace {

bool IsWordCharacter(wchar_t character)
{
	if (character == L'_')
		return true;

	WORD characterType = 0;
	WORD characterType3 = 0;
	if (!::GetStringTypeW(CT_CTYPE1, &character, 1, &characterType) ||
		!::GetStringTypeW(CT_CTYPE3, &character, 1, &characterType3))
		return false;
	return (characterType & (C1_ALPHA | C1_DIGIT)) != 0 ||
		(characterType3 & (C3_LEXICAL | C3_NONSPACING | C3_DIACRITIC | C3_VOWELMARK)) != 0;
}

bool IsWholeWordMatch(const std::wstring& text, std::size_t start, std::size_t length)
{
	const bool hasWordBefore = start > 0 && IsWordCharacter(text[start - 1]);
	const std::size_t end = start + length;
	const bool hasWordAfter = end < text.size() && IsWordCharacter(text[end]);
	return !hasWordBefore && !hasWordAfter;
}

bool MatchesAt(const std::wstring& text, const SearchQuery& query, std::size_t start)
{
	const wchar_t* candidate = text.data() + start;
	const wchar_t* pattern = query.Text.data();
	return ::CompareStringOrdinal(
		candidate, static_cast<int>(query.Text.size()),
		pattern, static_cast<int>(query.Text.size()),
		query.MatchCase ? FALSE : TRUE) == CSTR_EQUAL;
}

}

std::vector<SearchHit> FindLiteralMatches(const std::wstring& text, const SearchQuery& query)
{
	std::vector<SearchHit> hits;
	if (query.Mode != SearchMode::Literal || query.Text.empty() || query.Text.size() > text.size())
		return hits;

	const std::size_t length = query.Text.size();
	for (std::size_t start = 0; start + length <= text.size(); )
	{
		if (MatchesAt(text, query, start) &&
			(!query.WholeWord || IsWholeWordMatch(text, start, length)))
		{
			hits.push_back(SearchHit(start, length));
			start += length;
		}
		else
		{
			++start;
		}
	}
	return hits;
}

}
}
