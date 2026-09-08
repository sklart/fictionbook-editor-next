#include <vector>

#include "SearchSession.h"
#include "SearchResults.h"
#include "LiteralSearch.h"
#include "SearchTextSnapshot.h"

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

int wmain()
{
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
	if (!results.IsValidFor(41) || results.IsValidFor(42) || results.GetCount() != 2)
		return 15;
	if (results.GetSelected() != NULL || results.Select(1)->Hit.Start != 12)
		return 16;
	if (results.GetSelectedIndex() != 1 || results.Select(2) != NULL)
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
	return 0;
}
