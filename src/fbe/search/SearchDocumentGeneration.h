#pragma once

#include <cstdint>

namespace AU { namespace Search {

// Search offsets belong to the hosted document, not to MSHTML's internal
// layout/version bookkeeping.  Only a content/structure mutation or a full
// document rebuild advances this value.  Viewport, focus and selection events
// intentionally have no operation on this type.
class SearchDocumentGeneration
{
public:
	SearchDocumentGeneration() : m_value(1) {}

	std::uint64_t Value() const { return m_value; }

	void Advance()
	{
		++m_value;
		// Generation zero is reserved as an invalid/uninitialised value in a
		// few cached Search Core structures.
		if (m_value == 0)
			++m_value;
	}

private:
	std::uint64_t m_value;
};

} } // namespace AU::Search
