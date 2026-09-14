#pragma once

#include <atlstr.h>

#include "DocumentSearchCoordinator.h"
#include "SearchDocumentGeneration.h"

// Logical Design-mode search runtime. UI, live selection acquisition and
// editor mutations stay at the editor boundary; this owner keeps semantic
// search validity state internally consistent.
class DesignSearchController
{
public:
	struct ReplacePreviewState
	{
		CString pattern;
		CString replacement;
		std::uint64_t generation = 0;
		std::uint64_t revision = 0;
		int flags = 0;
		AU::Search::SearchScope scope = AU::Search::SearchScope::WholeDocument;
		bool regexp = false;
		bool unicodeProperties = false;
		bool valid = false;
	};

	DocumentSearchCoordinator& Coordinator() { return m_coordinator; }
	const DocumentSearchCoordinator& Coordinator() const { return m_coordinator; }
	std::uint64_t Generation() const { return m_generation.Value(); }
	void Advance();

	bool HasValidScope(AU::Search::SearchScope scope) const;
	void SetScope(const AU::Search::SearchRange& range, AU::Search::SearchScope scope);
	void ResetScope();
	const AU::Search::SearchRange* Scope(AU::Search::SearchScope scope) const;

	bool ShouldSkipZeroLength(const AU::Search::SearchQuery& query, const AU::Search::SearchRange& selection) const;
	void RecordHit(const AU::Search::SearchQuery& query, const AU::Search::SearchHit& hit);

	const ReplacePreviewState& ReplacePreview() const { return m_replacePreview; }
	bool HasCurrentReplacePreview(std::uint64_t generation, std::uint64_t revision,
		const CString& pattern, const CString& replacement, int flags,
		AU::Search::SearchScope scope, bool regexp, bool unicodeProperties) const;
	void SetReplacePreview(std::uint64_t generation, std::uint64_t revision,
		const CString& pattern, const CString& replacement, int flags,
		AU::Search::SearchScope scope, bool regexp, bool unicodeProperties);
	void ClearReplacePreview() { m_replacePreview.valid = false; }

	bool ControlledReplaceAllMutation() const { return m_controlledReplaceAllMutation; }
	void SetControlledReplaceAllMutation(bool value) { m_controlledReplaceAllMutation = value; }
	bool ReplaceAllCompletionPending() const { return m_replaceAllCompletionPending; }
	void SetReplaceAllCompletion(int count) { m_replaceAllCompletionCount = count; m_replaceAllCompletionPending = true; }
	int TakeReplaceAllCompletion();

	void ResetZeroLengthHit();
	void PrepareZeroLengthSearch(const AU::Search::SearchQuery& query);

private:
	DocumentSearchCoordinator m_coordinator;
	AU::Search::SearchDocumentGeneration m_generation;
	AU::Search::SearchRange m_scopeRange;
	std::uint64_t m_scopeGeneration = 0;
	AU::Search::SearchScope m_scopeKind = AU::Search::SearchScope::WholeDocument;
	bool m_hasScope = false;
	std::size_t m_lastZeroLengthHit = 0;
	std::uint64_t m_lastZeroLengthGeneration = 0;
	AU::Search::SearchQuery m_lastZeroLengthQuery;
	bool m_hasLastZeroLengthHit = false;
	ReplacePreviewState m_replacePreview;
	bool m_controlledReplaceAllMutation = false;
	bool m_replaceAllCompletionPending = false;
	int m_replaceAllCompletionCount = 0;
};
