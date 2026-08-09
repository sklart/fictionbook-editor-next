#include "Fb2SourceAutocomplete.h"
#include "generated/Fb2SchemaMetadata.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <vector>

namespace {
const Fb2SchemaElementMetadata* FindMetadata(const std::string& name) {
	for (size_t i = 0; i < kFb2SchemaMetadataCount; ++i)
		if (name == kFb2SchemaMetadata[i].name) return &kFb2SchemaMetadata[i];
	return NULL;
}
std::string Join(const std::set<std::string>& values) {
	std::string result;
	for (std::set<std::string>::const_iterator it = values.begin(); it != values.end(); ++it) {
		if (!result.empty()) result += ' ';
		result += *it;
	}
	return result;
}
std::set<std::string> Words(const char* value) {
	std::set<std::string> result; std::istringstream input(value ? value : ""); std::string word;
	while (input >> word) result.insert(word); return result;
}
void SkipWhitespace(const std::string& text, size_t& at) {
	while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\r' || text[at] == '\n')) ++at;
}
bool IsXLinkHrefAttribute(const std::string& text, const std::string& attribute) {
	const std::string::size_type colon = attribute.rfind(':');
	if (colon == std::string::npos || attribute.substr(colon + 1) != "href") return false;
	const std::string prefix = attribute.substr(0, colon);
	if (prefix == "l" || prefix == "xlink") return true;
	const std::string declaration = "xmlns:" + prefix;
	const std::string::size_type xmlns = text.rfind(declaration);
	if (xmlns == std::string::npos) return false;
	size_t at = xmlns + declaration.size();
	SkipWhitespace(text, at);
	if (at >= text.size() || text[at++] != '=') return false;
	SkipWhitespace(text, at);
	if (at >= text.size() || (text[at] != '\'' && text[at] != '"')) return false;
	const char quote = text[at++];
	const size_t end = text.find(quote, at);
	return end != std::string::npos && text.compare(at, end - at, "http://www.w3.org/1999/xlink") == 0;
}
bool IsCurrentXLinkHrefValue(const std::string& text) {
	const std::string::size_type start = text.rfind('<');
	if (start == std::string::npos || start + 1 >= text.size() || text[start + 1] == '/' || text.find('>', start) != std::string::npos) return false;
	size_t at = text.find_first_of(" \t\r\n/>", start + 1);
	if (at == std::string::npos) return false;
	while (at < text.size()) {
		SkipWhitespace(text, at);
		if (at >= text.size() || text[at] == '/' || text[at] == '>') return false;
		const size_t nameBegin = at;
		at = text.find_first_of(" \t\r\n=/>", at);
		if (at == std::string::npos) return false;
		const std::string attribute = text.substr(nameBegin, at - nameBegin);
		SkipWhitespace(text, at);
		if (at >= text.size() || text[at++] != '=') return false;
		SkipWhitespace(text, at);
		if (at >= text.size() || (text[at] != '\'' && text[at] != '"')) return false;
		const char quote = text[at++];
		const size_t end = text.find(quote, at);
		if (end == std::string::npos) return IsXLinkHrefAttribute(text, attribute);
		at = end + 1;
	}
	return false;
}
std::vector<std::string> OpenElements(const std::string& text) {
	std::vector<std::string> stack;
	for (size_t at = 0; (at = text.find('<', at)) != std::string::npos;) {
		if (text.compare(at, 4, "<!--") == 0) { const size_t end = text.find("-->", at + 4); if (end == std::string::npos) break; at = end + 3; continue; }
		if (text.compare(at, 2, "<?") == 0 || text.compare(at, 2, "<!") == 0) { const size_t end = text.find('>', at + 2); if (end == std::string::npos) break; at = end + 1; continue; }
		const size_t end = text.find('>', at + 1); if (end == std::string::npos) break;
		const bool close = at + 1 < text.size() && text[at + 1] == '/'; const size_t begin = at + (close ? 2 : 1);
		const size_t nameEnd = text.find_first_of(" \t\r\n/>", begin); const std::string name = text.substr(begin, (nameEnd == std::string::npos ? end : nameEnd) - begin);
		if (!name.empty()) { if (close) { if (!stack.empty() && stack.back() == name) stack.pop_back(); } else if (text[end - 1] != '/') stack.push_back(name); }
		at = end + 1;
	}
	return stack;
}
}

bool Fb2SourceAutocomplete::IsSuppressed(const std::string& text) {
	auto unterminated = [&text](const char* open, const char* close) {
		const std::string::size_type opened = text.rfind(open);
		const std::string::size_type closed = text.rfind(close);
		return opened != std::string::npos && (closed == std::string::npos || opened > closed);
	};
	return unterminated("<!--", "-->") || unterminated("<![CDATA[", "]]>") || unterminated("<?", "?>");
}

std::string Fb2SourceAutocomplete::CurrentOpenTag(const std::string& text) {
	const std::string::size_type start = text.rfind('<');
	if (start == std::string::npos || start + 1 >= text.size() || text[start + 1] == '/' || text.find('>', start) != std::string::npos) return "";
	const std::string::size_type end = text.find_first_of(" \t\r\n/>", start + 1);
	return text.substr(start + 1, (end == std::string::npos ? text.size() : end) - start - 1);
}

std::string Fb2SourceAutocomplete::XLinkPrefix(const std::string& text) {
	const std::string::size_type tagStart = text.rfind('<');
	if (tagStart != std::string::npos && text.find('>', tagStart) == std::string::npos) {
		const std::string current = text.substr(tagStart);
		const char* standardPrefixes[] = { "l", "xlink" };
		for (size_t i = 0; i < sizeof(standardPrefixes) / sizeof(standardPrefixes[0]); ++i) {
			const std::string needle = std::string(standardPrefixes[i]) + ":";
			for (std::string::size_type at = current.find(needle); at != std::string::npos; at = current.find(needle, at + needle.size())) {
				if (at > 0 && (current[at - 1] == ' ' || current[at - 1] == '\t' || current[at - 1] == '\r' || current[at - 1] == '\n')) return standardPrefixes[i];
			}
		}
	}
	const std::string uri = "http://www.w3.org/1999/xlink";
	const std::string::size_type uriAt = text.rfind(uri);
	if (uriAt == std::string::npos) return "l";
	const std::string::size_type xmlns = text.rfind("xmlns:", uriAt);
	if (xmlns == std::string::npos || xmlns > uriAt) return "l";
	const std::string::size_type begin = xmlns + 6;
	const std::string::size_type end = text.find_first_of("= \t\r\n", begin);
	return text.substr(begin, end - begin);
}

Fb2AutocompleteResult Fb2SourceAutocomplete::Complete(const Fb2SourceStructuralContext& context, int character) const {
	Fb2AutocompleteResult result;
	if (context.suppressed) return result;
	const std::string& text = context.localBeforeCaret;
	if (character == '<') {
		const Fb2SchemaElementMetadata* parent = context.parentElement.empty() ? NULL : FindMetadata(context.parentElement);
		result.candidates = parent ? parent->children : "FictionBook";
		return result;
	}
	if (character == '/' && text.size() >= 2 && text.compare(text.size() - 2, 2, "</") == 0) {
		if (!context.closingElement.empty()) result.candidates = context.closingElement + ">";
		return result;
	}
	const std::string tag = CurrentOpenTag(text);
	if (tag.empty()) return result;
	const Fb2SchemaElementMetadata* metadata = FindMetadata(tag);
	if (!metadata) return result;
	if (character == ' ') {
		std::set<std::string> attributes = Words(metadata->attributes);
		std::set<std::string> filtered;
		const std::string current = text.substr(text.rfind('<'));
		for (std::set<std::string>::const_iterator it = attributes.begin(); it != attributes.end(); ++it) {
			const std::string actual = it->find("xlink:") == 0 ? XLinkPrefix(text) + it->substr(5) : *it;
			if (current.find(actual + "=") == std::string::npos) filtered.insert(actual);
		}
		std::set<std::string> completed;
		for (std::set<std::string>::const_iterator it = filtered.begin(); it != filtered.end(); ++it) completed.insert(*it + "=");
		result.candidates = Join(completed); return result;
	}
	if (character == ':' && text.substr(text.size() - std::min<size_t>(text.size(), 16)).find(XLinkPrefix(text) + ":") != std::string::npos) {
		result.candidates = "href= type="; return result;
	}
	if (character == '#' && IsCurrentXLinkHrefValue(text)) result.needsDocumentIds = true;
	return result;
}

Fb2AutocompleteResult Fb2SourceAutocomplete::Complete(const std::string& text, int character) const {
	Fb2SourceStructuralContext context;
	context.localBeforeCaret = text;
	context.suppressed = IsSuppressed(text);
	if (!context.suppressed && character == '<' && !text.empty()) {
		std::vector<std::string> stack = OpenElements(text.substr(0, text.size() - 1));
		if (!stack.empty()) context.parentElement = stack.back();
	}
	if (!context.suppressed && character == '/' && text.size() >= 2 && text.compare(text.size() - 2, 2, "</") == 0) {
		std::vector<std::string> stack = OpenElements(text.substr(0, text.size() - 2));
		if (!stack.empty()) context.closingElement = stack.back();
	}
	return Complete(context, character);
}

std::string Fb2SourceAutocomplete::CompleteIds(const std::string& document) const {
	std::set<std::string> ids;
	for (size_t at = 0; (at = document.find('<', at)) != std::string::npos;) {
		if (document.compare(at, 4, "<!--") == 0) { const size_t end = document.find("-->", at + 4); at = end == std::string::npos ? document.size() : end + 3; continue; }
		if (document.compare(at, 9, "<![CDATA[") == 0) { const size_t end = document.find("]]>", at + 9); at = end == std::string::npos ? document.size() : end + 3; continue; }
		if (document.compare(at, 2, "<?") == 0) { const size_t end = document.find("?>", at + 2); at = end == std::string::npos ? document.size() : end + 2; continue; }
		if (at + 1 >= document.size() || document[at + 1] == '/' || document[at + 1] == '!') { const size_t end = document.find('>', at + 1); at = end == std::string::npos ? document.size() : end + 1; continue; }
		size_t cursor = document.find_first_of(" \t\r\n/>", at + 1);
		if (cursor == std::string::npos) break;
		while (cursor < document.size()) {
			SkipWhitespace(document, cursor);
			if (cursor >= document.size() || document[cursor] == '/' || document[cursor] == '>') break;
			const size_t nameBegin = cursor;
			cursor = document.find_first_of(" \t\r\n=/>", cursor);
			if (cursor == std::string::npos) break;
			const std::string attribute = document.substr(nameBegin, cursor - nameBegin);
			SkipWhitespace(document, cursor);
			if (cursor >= document.size() || document[cursor++] != '=') break;
			SkipWhitespace(document, cursor);
			if (cursor >= document.size() || (document[cursor] != '\'' && document[cursor] != '\"')) break;
			const char quote = document[cursor++]; const size_t end = document.find(quote, cursor);
			if (end == std::string::npos) break;
			if (attribute == "id") ids.insert(document.substr(cursor, end - cursor));
			cursor = end + 1;
		}
		const size_t end = document.find('>', at + 1); at = end == std::string::npos ? document.size() : end + 1;
	}
	return Join(ids);
}
