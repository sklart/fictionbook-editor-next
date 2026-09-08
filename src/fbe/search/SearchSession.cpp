#include "SearchSession.h"

namespace AU {
namespace Search {

SearchSession::SearchSession()
	: m_currentIndex(kNoHit),
	  m_documentGeneration(0),
	  m_valid(false)
{
}

bool SearchSession::SetQuery(const SearchQuery& query)
{
	const bool criteriaChanged = !HasSameSearchCriteria(m_query, query);
	m_query = query;
	if (criteriaChanged)
		Invalidate();
	return criteriaChanged;
}

const SearchQuery& SearchSession::GetQuery() const
{
	return m_query;
}

void SearchSession::SetHits(const std::vector<SearchHit>& hits, std::uint64_t documentGeneration)
{
	m_hits = hits;
	m_currentIndex = kNoHit;
	m_documentGeneration = documentGeneration;
	m_valid = true;
}

void SearchSession::Invalidate()
{
	m_hits.clear();
	m_currentIndex = kNoHit;
	m_documentGeneration = 0;
	m_valid = false;
}

bool SearchSession::IsValid() const
{
	return m_valid;
}

bool SearchSession::IsValidFor(std::uint64_t documentGeneration) const
{
	return m_valid && m_documentGeneration == documentGeneration;
}

std::uint64_t SearchSession::GetDocumentGeneration() const
{
	return m_documentGeneration;
}

std::size_t SearchSession::GetHitCount() const
{
	return m_hits.size();
}

bool SearchSession::HasCurrentHit() const
{
	return m_currentIndex != kNoHit;
}

std::size_t SearchSession::GetCurrentIndex() const
{
	return m_currentIndex;
}

const SearchHit* SearchSession::GetCurrentHit() const
{
	return HasCurrentHit() ? &m_hits[m_currentIndex] : NULL;
}

const SearchHit* SearchSession::GetCurrentHitFor(std::uint64_t documentGeneration) const
{
	return IsValidFor(documentGeneration) ? GetCurrentHit() : NULL;
}

const SearchHit* SearchSession::Next(bool* wrapped)
{
	if (wrapped != NULL)
		*wrapped = false;
	if (m_hits.empty())
		return NULL;
	if (!HasCurrentHit()) {
		m_currentIndex = 0;
		return GetCurrentHit();
	}
	if (m_currentIndex + 1 == m_hits.size()) {
		m_currentIndex = 0;
		if (wrapped != NULL)
			*wrapped = true;
	} else {
		++m_currentIndex;
	}
	return GetCurrentHit();
}

const SearchHit* SearchSession::Previous(bool* wrapped)
{
	if (wrapped != NULL)
		*wrapped = false;
	if (m_hits.empty())
		return NULL;
	if (!HasCurrentHit()) {
		m_currentIndex = m_hits.size() - 1;
		return GetCurrentHit();
	}
	if (m_currentIndex == 0) {
		m_currentIndex = m_hits.size() - 1;
		if (wrapped != NULL)
			*wrapped = true;
	} else {
		--m_currentIndex;
	}
	return GetCurrentHit();
}

const SearchHit* SearchSession::MoveInQueryDirection(bool* wrapped)
{
	return m_query.Direction == SearchDirection::Forward
		? Next(wrapped)
		: Previous(wrapped);
}

const SearchHit* SearchSession::MoveInQueryDirectionFor(
	std::uint64_t documentGeneration,
	bool* wrapped)
{
	if (!IsValidFor(documentGeneration))
	{
		if (wrapped != NULL)
			*wrapped = false;
		return NULL;
	}
	return MoveInQueryDirection(wrapped);
}

const SearchHit* SearchSession::SelectNearest(
	std::size_t offset,
	SearchDirection direction,
	bool* wrapped)
{
	if (wrapped != NULL)
		*wrapped = false;
	if (m_hits.empty())
		return NULL;

	if (direction == SearchDirection::Forward)
	{
		for (std::size_t index = 0; index < m_hits.size(); ++index)
		{
			if (m_hits[index].Start >= offset)
			{
				m_currentIndex = index;
				return GetCurrentHit();
			}
		}
		m_currentIndex = 0;
	}
	else
	{
		for (std::size_t index = m_hits.size(); index != 0; --index)
		{
			const SearchHit& hit = m_hits[index - 1];
			if (hit.Start <= offset && hit.Length <= offset - hit.Start)
			{
				m_currentIndex = index - 1;
				return GetCurrentHit();
			}
		}
		m_currentIndex = m_hits.size() - 1;
	}
	if (wrapped != NULL)
		*wrapped = true;
	return GetCurrentHit();
}

const SearchHit* SearchSession::SelectNearestFor(
	std::uint64_t documentGeneration,
	std::size_t offset,
	SearchDirection direction,
	bool* wrapped)
{
	if (!IsValidFor(documentGeneration))
	{
		if (wrapped != NULL)
			*wrapped = false;
		return NULL;
	}
	return SelectNearest(offset, direction, wrapped);
}

}
}
