#include "SearchSession.h"

namespace AU {
namespace Search {

SearchSession::SearchSession()
	: m_currentIndex(kNoHit),
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

void SearchSession::SetHits(const std::vector<SearchHit>& hits)
{
	m_hits = hits;
	m_currentIndex = kNoHit;
	m_valid = true;
}

void SearchSession::Invalidate()
{
	m_hits.clear();
	m_currentIndex = kNoHit;
	m_valid = false;
}

bool SearchSession::IsValid() const
{
	return m_valid;
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

}
}
