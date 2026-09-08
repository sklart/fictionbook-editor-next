#pragma once

#include <string>
#include <vector>

#include "SearchTypes.h"

namespace AU {
namespace Search {

// UTF-16 literal matching used by the future document snapshot path.  This
// intentionally knows nothing about MSHTML ranges or editor selection.
std::vector<SearchHit> FindLiteralMatches(
	const std::wstring& text,
	const SearchQuery& query);

}
}
