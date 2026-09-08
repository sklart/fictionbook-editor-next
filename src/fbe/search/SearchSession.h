#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "SearchTypes.h"

namespace AU {
namespace Search {

// Owns search navigation state only.  Producing hits and mapping their UTF-16
// offsets back into the editor are deliberately responsibilities of adapters.
class SearchSession {
public:
	SearchSession();

	bool SetQuery(const SearchQuery& query);
	const SearchQuery& GetQuery() const;

	void SetHits(const std::vector<SearchHit>& hits, std::uint64_t documentGeneration);
	void Invalidate();
	bool IsValid() const;
	bool IsValidFor(std::uint64_t documentGeneration) const;
	std::uint64_t GetDocumentGeneration() const;

	std::size_t GetHitCount() const;
	bool HasCurrentHit() const;
	std::size_t GetCurrentIndex() const;
	const SearchHit* GetCurrentHit() const;
	const SearchHit* GetCurrentHitFor(std::uint64_t documentGeneration) const;

	const SearchHit* Next(bool* wrapped = NULL);
	const SearchHit* Previous(bool* wrapped = NULL);
	const SearchHit* MoveInQueryDirection(bool* wrapped = NULL);
	const SearchHit* MoveInQueryDirectionFor(std::uint64_t documentGeneration, bool* wrapped = NULL);

private:
	static const std::size_t kNoHit = static_cast<std::size_t>(-1);

	SearchQuery m_query;
	std::vector<SearchHit> m_hits;
	std::size_t m_currentIndex;
	std::uint64_t m_documentGeneration;
	bool m_valid;
};

}
}
