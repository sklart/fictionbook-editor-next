#include "XmlTagMatcher.h"

#include <algorithm>

namespace {
bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool IsNameStart(unsigned char c) { return c == ':' || c == '_' || c >= 0x80 || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool IsNameChar(unsigned char c) { return IsNameStart(c) || c == '-' || c == '.' || (c >= '0' && c <= '9'); }

bool Starts(const std::string& s, size_t at, const char* text) {
	for (size_t i = 0; text[i]; ++i) if (at + i >= s.size() || s[at + i] != text[i]) return false;
	return true;
}

size_t FindEnd(const std::string& s, size_t at, const char* terminator) {
	for (; at < s.size(); ++at) if (Starts(s, at, terminator)) return at;
	return std::string::npos;
}

XmlTagToken MakeSpecial(XmlTagTokenType type, size_t start, size_t end) {
	XmlTagToken token; token.type = type; token.fullRange = {start, end}; return token;
}
}

std::vector<XmlTagToken> XmlTagTokenizer::Tokenize(const std::string& s) const {
	std::vector<XmlTagToken> out;
	for (size_t i = 0; i < s.size();) {
		if (s[i] != '<') { ++i; continue; }
		const size_t start = i;
		if (Starts(s, i, "<!--")) {
			size_t end = FindEnd(s, i + 4, "-->");
			if (end == std::string::npos) { out.push_back(MakeSpecial(XmlTagTokenType::Incomplete, start, s.size())); break; }
			out.push_back(MakeSpecial(XmlTagTokenType::Comment, start, end + 3)); i = end + 3; continue;
		}
		if (Starts(s, i, "<![CDATA[")) {
			size_t end = FindEnd(s, i + 9, "]]>");
			if (end == std::string::npos) { out.push_back(MakeSpecial(XmlTagTokenType::Incomplete, start, s.size())); break; }
			out.push_back(MakeSpecial(XmlTagTokenType::CData, start, end + 3)); i = end + 3; continue;
		}
		if (Starts(s, i, "<?")) {
			size_t end = FindEnd(s, i + 2, "?>");
			if (end == std::string::npos) { out.push_back(MakeSpecial(XmlTagTokenType::Incomplete, start, s.size())); break; }
			out.push_back(MakeSpecial(XmlTagTokenType::ProcessingInstruction, start, end + 2)); i = end + 2; continue;
		}
		if (Starts(s, i, "<!DOCTYPE") || Starts(s, i, "<!doctype")) {
			size_t p = i + 2, subset = 0; char quote = 0;
			for (; p < s.size(); ++p) { char c = s[p]; if (quote) { if (c == quote) quote = 0; } else if (c == '\'' || c == '"') quote = c; else if (c == '[') ++subset; else if (c == ']' && subset) --subset; else if (c == '>' && !subset) break; }
			if (p == s.size()) { out.push_back(MakeSpecial(XmlTagTokenType::Incomplete, start, s.size())); break; }
			out.push_back(MakeSpecial(XmlTagTokenType::DocType, start, p + 1)); i = p + 1; continue;
		}
		if (Starts(s, i, "<!")) { size_t end = FindEnd(s, i + 2, ">"); if (end == std::string::npos) { out.push_back(MakeSpecial(XmlTagTokenType::Incomplete, start, s.size())); break; } out.push_back(MakeSpecial(XmlTagTokenType::Invalid, start, end + 1)); i = end + 1; continue; }

		size_t p = i + 1; bool closing = false;
		if (p < s.size() && s[p] == '/') { closing = true; ++p; }
		if (p == s.size()) { out.push_back(MakeSpecial(XmlTagTokenType::Incomplete, start, s.size())); break; }
		if (!IsNameStart(static_cast<unsigned char>(s[p]))) { out.push_back(MakeSpecial(XmlTagTokenType::Invalid, start, (std::min)(s.size(), p + 1))); i = (std::min)(s.size(), p + 1); continue; }
		const size_t nameStart = p++; while (p < s.size() && IsNameChar(static_cast<unsigned char>(s[p]))) ++p;
		const size_t nameEnd = p; bool invalid = false; bool incomplete = false; bool selfClosing = false; std::vector<XmlByteRange> attributes;
		while (p < s.size()) {
			while (p < s.size() && IsSpace(s[p])) ++p;
			if (p == s.size()) { incomplete = true; break; }
			if (s[p] == '>') break;
			if (!closing && s[p] == '/' && p + 1 < s.size() && s[p + 1] == '>') { selfClosing = true; ++p; break; }
			if (closing || !IsNameStart(static_cast<unsigned char>(s[p]))) { invalid = true; break; }
			const size_t attributeStart = p++; while (p < s.size() && IsNameChar(static_cast<unsigned char>(s[p]))) ++p;
			while (p < s.size() && IsSpace(s[p])) ++p;
			if (p == s.size()) { incomplete = true; break; }
			if (s[p] != '=') { invalid = true; break; }
			++p; while (p < s.size() && IsSpace(s[p])) ++p;
			if (p == s.size()) { incomplete = true; break; }
			if (s[p] == '\'' || s[p] == '"') { const char quote = s[p++]; while (p < s.size() && s[p] != quote) ++p; if (p == s.size()) { incomplete = true; break; } ++p; }
			else { const size_t valueStart = p; while (p < s.size() && !IsSpace(s[p]) && s[p] != '>' && s[p] != '/') { if (s[p] == '<') { invalid = true; break; } ++p; } if (p == valueStart) invalid = true; }
			if (invalid) break;
			attributes.push_back({attributeStart, p});
		}
		if (incomplete || p == s.size()) { out.push_back(MakeSpecial(XmlTagTokenType::Incomplete, start, s.size())); break; }
		if (invalid || s[p] != '>') { out.push_back(MakeSpecial(XmlTagTokenType::Invalid, start, (std::min)(s.size(), p + 1))); i = (std::min)(s.size(), p + 1); continue; }
		XmlTagToken token; token.type = closing ? XmlTagTokenType::ClosingTag : (selfClosing ? XmlTagTokenType::SelfClosingTag : XmlTagTokenType::OpeningTag); token.fullRange = {start, p + 1}; token.nameRange = {nameStart, nameEnd}; token.name.assign(s, nameStart, nameEnd - nameStart); token.attributeRanges.swap(attributes); out.push_back(token); i = p + 1;
	}
	return out;
}

XmlTagMatcher::XmlTagMatcher(const std::string& utf8) : m_tokens(XmlTagTokenizer().Tokenize(utf8)) {
	m_results.resize(m_tokens.size()); std::vector<size_t> stack;
	for (size_t i = 0; i < m_tokens.size(); ++i) {
		const XmlTagToken& t = m_tokens[i]; XmlTagMatchResult& r = m_results[i]; r.currentTagRange = t.fullRange; r.currentNameRange = t.nameRange; r.attributeRanges = t.attributeRanges;
		if (t.type == XmlTagTokenType::SelfClosingTag) r.state = XmlTagMatchState::SelfClosing;
		else if (t.type == XmlTagTokenType::Incomplete) r.state = XmlTagMatchState::Incomplete;
		else if (t.type == XmlTagTokenType::Invalid) { r.state = XmlTagMatchState::Invalid; m_diagnostics.push_back(r); }
		else if (t.type == XmlTagTokenType::OpeningTag) stack.push_back(i);
		else if (t.type == XmlTagTokenType::ClosingTag) {
			if (stack.empty()) { r.state = XmlTagMatchState::MissingOpening; m_diagnostics.push_back(r); }
			else if (m_tokens[stack.back()].name == t.name) { size_t open = stack.back(); stack.pop_back(); r.state = XmlTagMatchState::Matched; r.matchingTagRange = m_tokens[open].fullRange; r.matchingNameRange = m_tokens[open].nameRange; m_results[open].state = XmlTagMatchState::Matched; m_results[open].matchingTagRange = t.fullRange; m_results[open].matchingNameRange = t.nameRange; }
			else { r.state = XmlTagMatchState::Mismatched; m_diagnostics.push_back(r); }
		}
	}
	for (size_t i = 0; i < stack.size(); ++i) { XmlTagMatchResult& r = m_results[stack[i]]; r.state = XmlTagMatchState::MissingClosing; m_diagnostics.push_back(r); }
}

XmlTagMatchResult XmlTagMatcher::ResultAt(XmlBytePosition position) const {
	const std::vector<XmlTagToken>::const_iterator after = std::upper_bound(m_tokens.begin(), m_tokens.end(), position,
		[](XmlBytePosition value, const XmlTagToken& token) { return value < token.fullRange.start; });
	if (after != m_tokens.begin()) {
		const size_t index = static_cast<size_t>((after - m_tokens.begin()) - 1);
		if (position >= m_tokens[index].fullRange.start && position <= m_tokens[index].fullRange.end) return m_results[index];
	}
	return XmlTagMatchResult();
}
