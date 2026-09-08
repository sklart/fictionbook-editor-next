#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace AU {
namespace Search {

// Opaque document-side coordinates. Search Core deliberately does not know
// whether a source is an MSHTML node, a future editor buffer, or test data.
struct SearchDocumentPosition {
	std::uint64_t SourceId;
	std::size_t SourceOffset;
};

struct SearchTextSegment {
	std::size_t SearchOffset;
	std::size_t Length;
	SearchDocumentPosition DocumentStart;
};

struct SearchTextSnapshot {
	std::wstring Text;
	std::uint64_t DocumentGeneration;
	std::vector<SearchTextSegment> Segments;

	SearchTextSnapshot() : DocumentGeneration(0) {}

	bool TryGetDocumentPosition(std::size_t searchOffset, SearchDocumentPosition* position) const;
	bool TryGetSearchOffset(const SearchDocumentPosition& position, std::size_t* searchOffset) const;
};

class SearchTextSnapshotBuilder {
public:
	explicit SearchTextSnapshotBuilder(std::uint64_t documentGeneration);

	void Append(const std::wstring& text, const SearchDocumentPosition& documentStart);
	void AppendUnmapped(const std::wstring& text);
	SearchTextSnapshot Build() const;

private:
	SearchTextSnapshot m_snapshot;
};

}
}
