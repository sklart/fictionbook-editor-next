#include "stdafx.h"
#include "DesignSearchController.h"

void DesignSearchController::Advance()
{
	m_generation.Advance();
	m_coordinator.Invalidate();
	ResetScope();
	m_hasLastZeroLengthHit = false;
	ClearReplacePreview();
}

bool DesignSearchController::HasValidScope(AU::Search::SearchScope scope) const
{
	return m_hasScope && m_scopeKind == scope && m_scopeGeneration == Generation();
}

void DesignSearchController::SetScope(const AU::Search::SearchRange& range, AU::Search::SearchScope scope)
{
	m_scopeRange = range;
	m_scopeKind = scope;
	m_scopeGeneration = Generation();
	m_hasScope = true;
}

void DesignSearchController::ResetScope()
{
	m_hasScope = false;
	m_scopeGeneration = 0;
}

const AU::Search::SearchRange* DesignSearchController::Scope(AU::Search::SearchScope scope) const
{
	return HasValidScope(scope) ? &m_scopeRange : NULL;
}

bool DesignSearchController::HasCurrentReplacePreview(std::uint64_t generation, std::uint64_t revision,
	const CString& pattern, const CString& replacement, int flags,
	AU::Search::SearchScope scope, bool regexp, bool unicodeProperties) const
{
	return m_replacePreview.valid && m_replacePreview.generation == generation &&
		m_replacePreview.revision == revision && m_replacePreview.pattern == pattern &&
		m_replacePreview.replacement == replacement && m_replacePreview.flags == flags &&
		m_replacePreview.scope == scope && m_replacePreview.regexp == regexp &&
		m_replacePreview.unicodeProperties == unicodeProperties;
}

void DesignSearchController::SetReplacePreview(std::uint64_t generation, std::uint64_t revision,
	const CString& pattern, const CString& replacement, int flags,
	AU::Search::SearchScope scope, bool regexp, bool unicodeProperties)
{
	m_replacePreview.pattern = pattern;
	m_replacePreview.replacement = replacement;
	m_replacePreview.generation = generation;
	m_replacePreview.revision = revision;
	m_replacePreview.flags = flags;
	m_replacePreview.scope = scope;
	m_replacePreview.regexp = regexp;
	m_replacePreview.unicodeProperties = unicodeProperties;
	m_replacePreview.valid = true;
}

bool DesignSearchController::ShouldSkipZeroLength(const AU::Search::SearchQuery& query, const AU::Search::SearchRange& selection) const
{
	return m_hasLastZeroLengthHit && m_lastZeroLengthGeneration == Generation() &&
		AU::Search::HasSameSearchCriteria(m_lastZeroLengthQuery, query) &&
		m_lastZeroLengthQuery.Direction == query.Direction && selection.Length == 0 &&
		selection.Start == m_lastZeroLengthHit;
}

void DesignSearchController::RecordHit(const AU::Search::SearchQuery& query, const AU::Search::SearchHit& hit)
{
	m_hasLastZeroLengthHit = hit.Length == 0;
	m_lastZeroLengthHit = hit.Start;
	m_lastZeroLengthGeneration = Generation();
	m_lastZeroLengthQuery = query;
}

void DesignSearchController::ResetZeroLengthHit()
{
	m_hasLastZeroLengthHit = false;
}

void DesignSearchController::PrepareZeroLengthSearch(const AU::Search::SearchQuery& query)
{
	if (!m_hasLastZeroLengthHit || !AU::Search::HasSameSearchCriteria(m_lastZeroLengthQuery, query) ||
		m_lastZeroLengthQuery.Direction != query.Direction)
		ResetZeroLengthHit();
}

int DesignSearchController::TakeReplaceAllCompletion()
{
	if (!m_replaceAllCompletionPending) return 0;
	m_replaceAllCompletionPending = false;
	const int count = m_replaceAllCompletionCount;
	m_replaceAllCompletionCount = 0;
	return count;
}
