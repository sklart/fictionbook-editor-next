#include <vector>

#include "SearchSession.h"
#include "SearchResults.h"
#include "LiteralSearch.h"
#include "SearchTextSnapshot.h"
#include "SearchDocumentGeneration.h"
#include "SearchViewportResults.h"

using AU::Search::SearchDirection;
using AU::Search::SearchHit;
using AU::Search::SearchQuery;
using AU::Search::SearchResult;
using AU::Search::SearchResults;
using AU::Search::SearchSession;
using AU::Search::FindLiteralMatches;
using AU::Search::SearchDocumentPosition;
using AU::Search::SearchTextSnapshot;
using AU::Search::SearchTextSnapshotBuilder;
using AU::Search::SearchDocumentGeneration;

int wmain()
{
	// The semantic document generation is independent of viewport activity:
	// consumers only observe an advance when the editor's mutation/reload path
	// explicitly invokes Advance().
	SearchDocumentGeneration documentGeneration;
	if (documentGeneration.Value() == 0)
		return 48;
	const std::uint64_t viewportGeneration = documentGeneration.Value();
	if (documentGeneration.Value() != viewportGeneration)
		return 49;
	documentGeneration.Advance();
	if (documentGeneration.Value() == viewportGeneration)
		return 50;

	SearchSession session;
	SearchHit captured(2, 6);
	captured.Captures.push_back(AU::Search::SearchCapture(1, 3, 2, L"part"));
	if (captured.Captures.size() != 1 || captured.Captures[0].GroupIndex != 1 ||
		captured.Captures[0].Name != L"part" || !captured.Captures[0].Matched)
		return 44;
	SearchQuery query;
	query.Text = L"needle";
	if (!session.SetQuery(query) || session.IsValid())
		return 1;

	session.SetHits(std::vector<SearchHit>{ SearchHit(2, 6), SearchHit(12, 6), SearchHit(30, 6) }, 100);
	if (!session.IsValid() || session.GetHitCount() != 3 || session.HasCurrentHit())
		return 2;
	bool wrapped = true;
	if (session.Next(&wrapped)->Start != 2 || wrapped)
		return 3;
	if (session.Next(&wrapped)->Start != 12 || wrapped)
		return 4;
	if (session.Next(&wrapped)->Start != 30 || wrapped)
		return 5;
	if (session.Next(&wrapped)->Start != 2 || !wrapped)
		return 6;
	if (session.Previous(&wrapped)->Start != 30 || !wrapped)
		return 7;

	query.Direction = SearchDirection::Backward;
	if (session.SetQuery(query) || !session.IsValid())
		return 8;
	if (session.MoveInQueryDirection(&wrapped)->Start != 12 || wrapped)
		return 9;
	if (session.IsValidFor(101) || session.GetCurrentHitFor(101) != NULL || session.MoveInQueryDirectionFor(101, &wrapped) != NULL || wrapped)
		return 10;
	if (!session.IsValidFor(100) || session.MoveInQueryDirectionFor(100, &wrapped)->Start != 2)
		return 11;
	if (session.SelectNearestFor(100, 12, SearchDirection::Forward, &wrapped)->Start != 12 || wrapped)
		return 33;
	if (session.SelectNearestFor(100, 13, SearchDirection::Forward, &wrapped)->Start != 30 || wrapped)
		return 34;
	if (session.SelectNearestFor(100, 31, SearchDirection::Forward, &wrapped)->Start != 2 || !wrapped)
		return 35;
	if (session.SelectNearestFor(100, 18, SearchDirection::Backward, &wrapped)->Start != 12 || wrapped)
		return 36;
	if (session.SelectNearestFor(100, 1, SearchDirection::Backward, &wrapped)->Start != 30 || !wrapped)
		return 37;
	if (session.SelectNearestFor(101, 12, SearchDirection::Forward, &wrapped) != NULL || wrapped)
		return 38;

	query.Text = L"other";
	if (!session.SetQuery(query) || session.IsValid() || session.GetHitCount() != 0)
		return 12;
	if (session.Next(&wrapped) != NULL || wrapped)
		return 13;

	session.SetHits(std::vector<SearchHit>{ SearchHit(1, 0) }, 101);
	if (session.SelectNearestFor(101, 1, SearchDirection::Forward, &wrapped)->Start != 1 || wrapped ||
		session.SelectNearestFor(101, 1, SearchDirection::Backward, &wrapped)->Start != 1 || wrapped)
		return 39;
	// A collapsed regexp hit at the caret is selectable initially, but a
	// repeated Find Next/Previous must advance to a neighbouring anchor rather
	// than select the same zero-length match forever.
	session.SetHits(std::vector<SearchHit>{ SearchHit(0, 0), SearchHit(1, 0), SearchHit(2, 0) }, 101);
	if (session.SelectNearestFor(101, 1, SearchDirection::Forward, &wrapped)->Start != 1 || wrapped ||
		session.SelectNearestFor(101, 1, SearchDirection::Forward, &wrapped, true)->Start != 2 || wrapped ||
		session.SelectNearestFor(101, 2, SearchDirection::Forward, &wrapped, true)->Start != 0 || !wrapped ||
		session.SelectNearestFor(101, 1, SearchDirection::Backward, &wrapped, true)->Start != 0 || wrapped ||
		session.SelectNearestFor(101, 0, SearchDirection::Backward, &wrapped, true)->Start != 2 || !wrapped)
		return 45;
	session.Invalidate();
	if (session.IsValid() || session.HasCurrentHit() || session.GetHitCount() != 0)
		return 14;
	session.SetHits(std::vector<SearchHit>{ SearchHit(1, 2), SearchHit(4, 0), SearchHit(6, 3) }, 102);
	session.RestrictToRange(AU::Search::SearchRange(1, 4));
	if (session.GetHitCount() != 2 || session.Next()->Start != 1 || session.Next()->Start != 4)
		return 43;

	SearchResults results;
	SearchResult first = { SearchHit(2, 6), L"Chapter 1", L"...needle..." };
	SearchResult second = { SearchHit(12, 6), L"Chapter 2", L"...other needle..." };
	results.SetResults(std::vector<SearchResult>{ first, second }, 41);
	const std::uint64_t firstResultsRevision = results.GetRevision();
	results.SetResults(std::vector<SearchResult>{ second }, 41);
	if (results.GetRevision() == firstResultsRevision || results.GetCount() != 1)
		return 46;
	results.SetResults(std::vector<SearchResult>{ first, second }, 41);
	if (!results.IsValidFor(41) || results.IsValidFor(42) || results.GetCount() != 2)
		return 15;
	if (results.FindFirstAtOrAfter(0) != 0 || results.FindFirstAtOrAfter(2) != 0 ||
		results.FindFirstAtOrAfter(3) != 1 || results.FindFirstAtOrAfter(99) != 2)
		return 47;
	// Results may arrive from UI adapters in arbitrary order; SearchResults
	// owns the sorted-offset invariant required by viewport selection.
	results.SetResults(std::vector<SearchResult>{ second, first }, 41);
	if (results.GetAt(0)->Hit.Start != 2 || results.GetAt(1)->Hit.Start != 12)
		return 51;
	AU::Search::SearchResult third = { SearchHit(18, 8), L"Chapter 3", L"...crossing..." };
	results.SetResults(std::vector<SearchResult>{ first, second, third }, 41);
	AU::Search::SearchViewportSubset viewport = AU::Search::SelectViewportResults(results, 20, 24, 128);
	if (viewport.FirstIndex != 2 || viewport.Count != 1)
		return 52;
	// A preceding hit that extends into the viewport is included exactly once,
	// and the limit is enforced after intersection filtering.
	viewport = AU::Search::SelectViewportResults(results, 15, 23, 1);
	if (viewport.FirstIndex != 1 || viewport.Count != 1)
		return 53;
	if (results.GetSelected() != NULL || results.Select(1)->Hit.Start != 12)
		return 16;
	if (results.GetSelectedIndex() != 1 || results.Select(3) != NULL || results.GetSelected() != NULL)
		return 17;
	results.Invalidate();
	if (results.IsValidFor(41) || results.GetCount() != 0 || results.GetSelected() != NULL)
		return 18;

	SearchQuery literal;
	literal.Text = L"word";
	std::vector<SearchHit> literalHits = FindLiteralMatches(L"word Word pass word", literal);
	if (literalHits.size() != 3 || literalHits[0] != SearchHit(0, 4) || literalHits[1] != SearchHit(5, 4) || literalHits[2] != SearchHit(15, 4))
		return 19;
	literal.MatchCase = true;
	literal.WholeWord = true;
	literalHits = FindLiteralMatches(L"word Word pass word", literal);
	if (literalHits.size() != 2 || literalHits[1].Start != 15)
		return 20;
	literal.MatchCase = false;
	literal.Text = L"\x0441\x043B\x043E\x0432\x043E";
	literalHits = FindLiteralMatches(L"\x0421\x041B\x041E\x0412\x041E \x0441\x043B\x043E\x0432\x043E", literal);
	if (literalHits.size() != 2 || literalHits[1].Start != 6)
		return 21;
	literal.Text = L"a\x00A0" L"b";
	literalHits = FindLiteralMatches(L"x a\x00A0" L"b " L"\xD83D\xDE00" L"a\x00A0" L"b", literal);
	if (literalHits.size() != 2 || literalHits[1].Start != 8)
		return 22;
	literal.Text = L"e";
	literal.WholeWord = true;
	literalHits = FindLiteralMatches(L"e e\x0301 e", literal);
	if (literalHits.size() != 2 || literalHits[0].Start != 0 || literalHits[1].Start != 5)
		return 23;

	SearchTextSnapshotBuilder snapshotBuilder(99);
	snapshotBuilder.Append(L"plain ", { 101, 0 });
	snapshotBuilder.Append(L"text", { 202, 4 });
	snapshotBuilder.Append(L" \xD83D\xDE00", { 303, 0 });
	SearchTextSnapshot snapshot = snapshotBuilder.Build();
	SearchDocumentPosition position = {};
	std::size_t searchOffset = 0;
	if (snapshot.Text != L"plain text \xD83D\xDE00" || snapshot.DocumentGeneration != 99)
		return 24;
	if (!snapshot.TryGetDocumentPosition(7, &position) || position.SourceId != 202 || position.SourceOffset != 5)
		return 25;
	if (!snapshot.TryGetSearchOffset({ 303, 1 }, &searchOffset) || searchOffset != 11)
		return 26;
	if (!snapshot.TryGetDocumentPosition(snapshot.Text.size(), &position) || position.SourceId != 303 || position.SourceOffset != 3)
		return 27;
	if (!snapshot.TryGetSearchOffset({ 303, 3 }, &searchOffset) || searchOffset != snapshot.Text.size() ||
		!snapshot.TryGetSearchOffset({ 202, 8 }, &searchOffset) || searchOffset != 10)
		return 28;

	SearchTextSnapshotBuilder paragraphs(7);
	paragraphs.Append(L"\x043F\x0435\x0440\x0432\x044B\x0439", { 1, 0 });
	paragraphs.AppendUnmapped(L"\n");
	paragraphs.Append(L"\x0432\x0442\x043E\x0440\x043E\x0439", { 2, 0 });
	SearchTextSnapshot paragraphSnapshot = paragraphs.Build();
	if (!paragraphSnapshot.TryGetDocumentPosition(6, &position) || position.SourceId != 1 || position.SourceOffset != 6)
		return 29;
	if (!paragraphSnapshot.TryGetDocumentPosition(7, &position) || position.SourceId != 2 || position.SourceOffset != 0)
		return 31;
	if (!paragraphSnapshot.TryGetSearchOffset({ 1, 6 }, &searchOffset) || searchOffset != 6 ||
		!paragraphSnapshot.TryGetSearchOffset({ 2, 0 }, &searchOffset) || searchOffset != 7)
		return 32;

	SearchTextSnapshotBuilder adjacent(8);
	adjacent.Append(L"left", { 10, 0 });
	adjacent.Append(L"right", { 20, 0 });
	SearchTextSnapshot adjacentSnapshot = adjacent.Build();
	if (!adjacentSnapshot.TryGetDocumentPosition(4, &position) || position.SourceId != 20 || position.SourceOffset != 0 ||
		!adjacentSnapshot.TryGetSearchOffset(position, &searchOffset) || searchOffset != 4)
		return 40;
	if (!adjacentSnapshot.TryGetDocumentPosition(0, &position) || position.SourceId != 10 || position.SourceOffset != 0 ||
		!adjacentSnapshot.TryGetDocumentPosition(adjacentSnapshot.Text.size(), &position) || position.SourceId != 20 || position.SourceOffset != 5)
		return 41;
	if (!adjacentSnapshot.TryGetDocumentPosition(5, &position) || position.SourceId != 20 || position.SourceOffset != 1)
		return 42;
	// A single Design replacement invalidates stale hits and then rebuilds from
	// the current document. The selected replacement supplies the live caret:
	// its end for forward search and its start for backward search.
	auto rebuildSingleReplaceSearch = [](SearchSession& singleSession, const std::wstring& text,
		const SearchQuery& singleQuery, std::uint64_t generation) {
		singleSession.Invalidate();
		if (singleSession.IsValid()) return false;
		singleSession.SetQuery(singleQuery);
		singleSession.SetHits(FindLiteralMatches(text, singleQuery), generation);
		return singleSession.IsValidFor(generation);
	};
	SearchQuery singleReplaceQuery;
	singleReplaceQuery.Text = L"...";
	SearchSession singleReplaceSession;
	std::wstring ellipsisDocument = L"... ... ... ...";
	if (!rebuildSingleReplaceSearch(singleReplaceSession, ellipsisDocument, singleReplaceQuery, 201) ||
		singleReplaceSession.SelectNearestFor(201, 0, SearchDirection::Forward, &wrapped)->Start != 0)
		return 54;
	std::size_t forwardCaret = 1;
	ellipsisDocument.replace(0, 3, L"\x2026");
	for (const std::size_t expected : std::vector<std::size_t>{ 2, 4, 6 })
	{
		if (!rebuildSingleReplaceSearch(singleReplaceSession, ellipsisDocument, singleReplaceQuery,
			202 + expected)) return 55;
		const SearchHit* next = singleReplaceSession.SelectNearestFor(202 + expected, forwardCaret,
			SearchDirection::Forward, &wrapped);
		if (!next || next->Start != expected || wrapped) return 56;
		ellipsisDocument.replace(next->Start, next->Length, L"\x2026");
		forwardCaret = next->Start + 1;
	}
	if (!rebuildSingleReplaceSearch(singleReplaceSession, ellipsisDocument, singleReplaceQuery, 210) ||
		singleReplaceSession.SelectNearestFor(210, forwardCaret, SearchDirection::Forward, &wrapped) != NULL)
		return 57;

	for (const std::wstring& replacement : std::vector<std::wstring>{ L"X", L"ABCDE", L"" })
	{
		std::wstring document = L"abc abc";
		SearchQuery query;
		query.Text = L"abc";
		if (!rebuildSingleReplaceSearch(singleReplaceSession, document, query, 220) ||
			singleReplaceSession.SelectNearestFor(220, 0, SearchDirection::Forward, &wrapped)->Start != 0)
			return 58;
		document.replace(0, 3, replacement);
		const std::size_t nextOffset = replacement.size();
		if (!rebuildSingleReplaceSearch(singleReplaceSession, document, query, 221)) return 59;
		const SearchHit* next = singleReplaceSession.SelectNearestFor(221, nextOffset, SearchDirection::Forward, &wrapped);
		if (!next || next->Start != replacement.size() + 1 || wrapped) return 60;

		document = L"abc abc";
		if (!rebuildSingleReplaceSearch(singleReplaceSession, document, query, 222) ||
			singleReplaceSession.SelectNearestFor(222, document.size(), SearchDirection::Backward, &wrapped)->Start != 4)
			return 61;
		document.replace(4, 3, replacement);
		if (!rebuildSingleReplaceSearch(singleReplaceSession, document, query, 223)) return 62;
		const SearchHit* previous = singleReplaceSession.SelectNearestFor(223, 4, SearchDirection::Backward, &wrapped);
		if (!previous || previous->Start != 0 || wrapped) return 63;
	}
	// Replacing with the same text still rebuilds the generation. Navigation
	// must skip the selected hit and only wrap after the next/previous candidate.
	std::wstring sameText = L"abc abc";
	SearchQuery sameQuery;
	sameQuery.Text = L"abc";
	if (!rebuildSingleReplaceSearch(singleReplaceSession, sameText, sameQuery, 230) ||
		singleReplaceSession.SelectNearestFor(230, 0, SearchDirection::Forward, &wrapped)->Start != 0)
		return 64;
	if (!rebuildSingleReplaceSearch(singleReplaceSession, sameText, sameQuery, 231)) return 65;
	const SearchHit* forwardNext = singleReplaceSession.SelectNearestFor(231, 3, SearchDirection::Forward, &wrapped);
	if (!forwardNext || forwardNext->Start != 4 || wrapped) return 66;
	if (!rebuildSingleReplaceSearch(singleReplaceSession, sameText, sameQuery, 232)) return 67;
	const SearchHit* forwardWrapped = singleReplaceSession.SelectNearestFor(232, 7, SearchDirection::Forward, &wrapped);
	if (!forwardWrapped || forwardWrapped->Start != 0 || !wrapped) return 68;
	if (!rebuildSingleReplaceSearch(singleReplaceSession, sameText, sameQuery, 233) ||
		singleReplaceSession.SelectNearestFor(233, 7, SearchDirection::Backward, &wrapped)->Start != 4)
		return 69;
	if (!rebuildSingleReplaceSearch(singleReplaceSession, sameText, sameQuery, 234)) return 70;
	const SearchHit* backwardPrevious = singleReplaceSession.SelectNearestFor(234, 4, SearchDirection::Backward, &wrapped);
	if (!backwardPrevious || backwardPrevious->Start != 0 || wrapped) return 71;
	if (!rebuildSingleReplaceSearch(singleReplaceSession, sameText, sameQuery, 235)) return 72;
	const SearchHit* backwardWrapped = singleReplaceSession.SelectNearestFor(235, 0, SearchDirection::Backward, &wrapped);
	if (!backwardWrapped || backwardWrapped->Start != 4 || !wrapped) return 73;
	return 0;
}
