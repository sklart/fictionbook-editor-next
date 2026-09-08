#pragma once

#include <cstddef>
#include <string>
#include <vector>

// All positions deliberately are offsets in the UTF-8 byte buffer.  They can
// be passed to Scintilla's Sci_Position without a UTF-16 conversion.
typedef std::size_t XmlBytePosition;

struct XmlByteRange {
	XmlBytePosition start = 0;
	XmlBytePosition end = 0;
	bool empty() const { return start >= end; }
};

enum class XmlTagTokenType {
	Text, OpeningTag, ClosingTag, SelfClosingTag, Comment, CData,
	ProcessingInstruction, DocType, Incomplete, Invalid
};

struct XmlTagToken {
	XmlTagTokenType type = XmlTagTokenType::Text;
	XmlByteRange fullRange;
	XmlByteRange nameRange;
	std::vector<XmlByteRange> attributeRanges;
	std::string name;
};

enum class XmlTagMatchState {
	None, Matched, SelfClosing, MissingOpening, MissingClosing, Mismatched,
	Invalid, Incomplete
};

struct XmlTagMatchResult {
	XmlTagMatchState state = XmlTagMatchState::None;
	XmlByteRange currentTagRange;
	XmlByteRange currentNameRange;
	XmlByteRange matchingTagRange;
	XmlByteRange matchingNameRange;
	std::vector<XmlByteRange> attributeRanges;
};

class XmlTagTokenizer {
public:
	std::vector<XmlTagToken> Tokenize(const std::string& utf8) const;
};

class XmlTagMatcher {
public:
	explicit XmlTagMatcher(const std::string& utf8);
	const std::vector<XmlTagToken>& Tokens() const { return m_tokens; }
	const std::vector<XmlTagMatchResult>& Diagnostics() const { return m_diagnostics; }
	XmlTagMatchResult ResultAt(XmlBytePosition position) const;

private:
	std::vector<XmlTagToken> m_tokens;
	std::vector<XmlTagMatchResult> m_results;
	std::vector<XmlTagMatchResult> m_diagnostics;
};
