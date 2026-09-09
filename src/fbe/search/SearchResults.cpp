#include "SearchResults.h"

namespace AU {
namespace Search {

SearchResults::SearchResults()
	: m_documentGeneration(0),
	  m_revision(0),
	  m_selectedIndex(kNoResult),
	  m_valid(false)
{
}

void SearchResults::SetResults(const std::vector<SearchResult>& results, std::uint64_t documentGeneration)
{
	m_results = results;
	m_documentGeneration = documentGeneration;
	++m_revision;
	m_selectedIndex = kNoResult;
	m_valid = true;
}

void SearchResults::Invalidate()
{
	m_results.clear();
	m_documentGeneration = 0;
	++m_revision;
	m_selectedIndex = kNoResult;
	m_valid = false;
}

bool SearchResults::IsValidFor(std::uint64_t documentGeneration) const
{
	return m_valid && m_documentGeneration == documentGeneration;
}

std::uint64_t SearchResults::GetRevision() const
{
	return m_revision;
}

std::size_t SearchResults::GetCount() const
{
	return m_results.size();
}

const SearchResult* SearchResults::GetAt(std::size_t index) const
{
	return index < m_results.size() ? &m_results[index] : NULL;
}

const SearchResult* SearchResults::GetSelected() const
{
	return GetAt(m_selectedIndex);
}

std::size_t SearchResults::GetSelectedIndex() const
{
	return m_selectedIndex;
}

const SearchResult* SearchResults::Select(std::size_t index)
{
	if (index >= m_results.size())
		return NULL;
	m_selectedIndex = index;
	return GetSelected();
}

std::size_t SearchResults::FindFirstAtOrAfter(std::size_t offset) const
{
	std::size_t first = 0;
	std::size_t last = m_results.size();
	while (first < last)
	{
		const std::size_t middle = first + (last - first) / 2;
		if (m_results[middle].Hit.Start < offset)
			first = middle + 1;
		else
			last = middle;
	}
	return first;
}

}
}
