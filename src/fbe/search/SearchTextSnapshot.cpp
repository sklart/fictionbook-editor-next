#include "SearchTextSnapshot.h"

namespace AU {
namespace Search {
namespace {

bool ContainsSearchOffset(const SearchTextSegment& segment, std::size_t searchOffset)
{
	return searchOffset >= segment.SearchOffset &&
		searchOffset < segment.SearchOffset + segment.Length;
}

}

bool SearchTextSnapshot::TryGetDocumentPosition(
	std::size_t searchOffset,
	SearchDocumentPosition* position) const
{
	if (position == NULL || searchOffset > Text.size())
		return false;
	// Prefer the right segment at an immediately adjacent boundary. This is
	// important for a non-empty hit that begins exactly at that boundary.
	for (std::size_t index = 0; index < Segments.size(); ++index)
	{
		const SearchTextSegment& segment = Segments[index];
		if (searchOffset == segment.SearchOffset)
		{
			position->SourceId = segment.DocumentStart.SourceId;
			position->SourceOffset = segment.DocumentStart.SourceOffset;
			return true;
		}
	}
	for (std::size_t index = 0; index < Segments.size(); ++index)
	{
		const SearchTextSegment& segment = Segments[index];
		if (!ContainsSearchOffset(segment, searchOffset))
			continue;
		position->SourceId = segment.DocumentStart.SourceId;
		position->SourceOffset = segment.DocumentStart.SourceOffset + searchOffset - segment.SearchOffset;
		return true;
	}
	// A boundary before unmapped text has no right source coordinate. Retain
	// the left endpoint so `$` and other zero-length matches can be selected.
	for (std::size_t index = 0; index < Segments.size(); ++index)
	{
		const SearchTextSegment& segment = Segments[index];
		if (searchOffset == segment.SearchOffset + segment.Length)
		{
			position->SourceId = segment.DocumentStart.SourceId;
			position->SourceOffset = segment.DocumentStart.SourceOffset + segment.Length;
			return true;
		}
	}
	// The synthetic separator after a terminal empty paragraph represents the
	// boundary after that paragraph. PCRE2 can report `$` at its end; resolve
	// it back to the same explicit zero-length source anchor.
	if (searchOffset == Text.size())
	{
		for (std::size_t index = Segments.size(); index > 0; --index)
		{
			const SearchTextSegment& segment = Segments[index - 1];
			if (segment.Length == 0 && segment.SearchOffset + 1 == searchOffset)
			{
				position->SourceId = segment.DocumentStart.SourceId;
				position->SourceOffset = segment.DocumentStart.SourceOffset;
				return true;
			}
		}
	}
	return false;
}

bool SearchTextSnapshot::TryGetSearchOffset(
	const SearchDocumentPosition& position,
	std::size_t* searchOffset) const
{
	if (searchOffset == NULL)
		return false;
	for (std::size_t index = 0; index < Segments.size(); ++index)
	{
		const SearchTextSegment& segment = Segments[index];
		if (segment.DocumentStart.SourceId != position.SourceId ||
			position.SourceOffset < segment.DocumentStart.SourceOffset)
			continue;
		const std::size_t offsetInSegment = position.SourceOffset - segment.DocumentStart.SourceOffset;
		if (offsetInSegment > segment.Length)
			continue;
		*searchOffset = segment.SearchOffset + offsetInSegment;
		return true;
	}
	return false;
}

SearchTextSnapshotBuilder::SearchTextSnapshotBuilder(std::uint64_t documentGeneration)
{
	m_snapshot.DocumentGeneration = documentGeneration;
}

void SearchTextSnapshotBuilder::Append(
	const std::wstring& text,
	const SearchDocumentPosition& documentStart)
{
	// Keep an explicit anchor for an empty source paragraph. A zero-length
	// regex hit (^$, ^, $, lookaround) must still map back to its DOM position.
	SearchTextSegment segment = {};
	segment.SearchOffset = m_snapshot.Text.size();
	segment.Length = text.size();
	segment.DocumentStart = documentStart;
	m_snapshot.Segments.push_back(segment);
	m_snapshot.Text += text;
}

void SearchTextSnapshotBuilder::AppendUnmapped(const std::wstring& text)
{
	m_snapshot.Text += text;
}

SearchTextSnapshot SearchTextSnapshotBuilder::Build() const
{
	return m_snapshot;
}

}
}
