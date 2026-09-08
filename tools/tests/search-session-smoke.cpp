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
	SearchQuery query;
	query.Text = L"needle";
	if (!session.SetQuery(query) || session.IsValid())
		return 1;

	session.SetHits(std::vector<SearchHit>{ SearchHit(2, 6), SearchHit(12, 6), SearchHit(30, 6) });
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

	query.Text = L"other";
	if (!session.SetQuery(query) || session.IsValid() || session.GetHitCount() != 0)
		return 10;
	if (session.Next(&wrapped) != NULL || wrapped)
		return 11;

	session.SetHits(std::vector<SearchHit>{ SearchHit(1, 0) });
	session.Invalidate();
	if (session.IsValid() || session.HasCurrentHit() || session.GetHitCount() != 0)
		return 12;

	SearchResults results;
	SearchResult first = { SearchHit(2, 6), L"Chapter 1", L"...needle..." };
	SearchResult second = { SearchHit(12, 6), L"Chapter 2", L"...other needle..." };
	results.SetResults(std::vector<SearchResult>{ first, second }, 41);
	if (!results.IsValidFor(41) || results.IsValidFor(42) || results.GetCount() != 2)
		return 13;
	if (results.GetSelected() != NULL || results.Select(1)->Hit.Start != 12)
		return 14;
	if (results.GetSelectedIndex() != 1 || results.Select(2) != NULL)
		return 15;
	results.Invalidate();
	if (results.IsValidFor(41) || results.GetCount() != 0 || results.GetSelected() != NULL)
		return 16;

	SearchQuery literal;
	literal.Text = L"word";
	std::vector<SearchHit> literalHits = FindLiteralMatches(L"word Word pass word", literal);
	if (literalHits.size() != 3 || literalHits[0] != SearchHit(0, 4) || literalHits[1] != SearchHit(5, 4) || literalHits[2] != SearchHit(15, 4))
		return 17;
	literal.MatchCase = true;
	literal.WholeWord = true;
	literalHits = FindLiteralMatches(L"word Word pass word", literal);
	if (literalHits.size() != 2 || literalHits[1].Start != 15)
		return 18;
	literal.MatchCase = false;
	literal.Text = L"\x0441\x043B\x043E\x0432\x043E";
	literalHits = FindLiteralMatches(L"\x0421\x041B\x041E\x0412\x041E \x0441\x043B\x043E\x0432\x043E", literal);
	if (literalHits.size() != 2 || literalHits[1].Start != 6)
		return 19;
	literal.Text = L"a\x00A0" L"b";
	literalHits = FindLiteralMatches(L"x a\x00A0" L"b " L"\xD83D\xDE00" L"a\x00A0" L"b", literal);
	if (literalHits.size() != 2 || literalHits[1].Start != 8)
		return 20;
	literal.Text = L"e";
	literal.WholeWord = true;
	literalHits = FindLiteralMatches(L"e e\x0301 e", literal);
	if (literalHits.size() != 2 || literalHits[0].Start != 0 || literalHits[1].Start != 5)
		return 21;

	SearchTextSnapshotBuilder snapshotBuilder(99);
	snapshotBuilder.Append(L"plain ", { 101, 0 });
	snapshotBuilder.Append(L"text", { 202, 4 });
	snapshotBuilder.Append(L" \xD83D\xDE00", { 303, 0 });
	SearchTextSnapshot snapshot = snapshotBuilder.Build();
	SearchDocumentPosition position = {};
	std::size_t searchOffset = 0;
	if (snapshot.Text != L"plain text \xD83D\xDE00" || snapshot.DocumentGeneration != 99)
		return 22;
	if (!snapshot.TryGetDocumentPosition(7, &position) || position.SourceId != 202 || position.SourceOffset != 5)
		return 23;
	if (!snapshot.TryGetSearchOffset({ 303, 1 }, &searchOffset) || searchOffset != 11)
		return 24;
	if (snapshot.TryGetDocumentPosition(snapshot.Text.size(), &position) || snapshot.TryGetSearchOffset({ 202, 8 }, &searchOffset))
		return 25;
	return 0;
}
