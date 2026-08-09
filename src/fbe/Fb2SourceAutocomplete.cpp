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
	const std::string uri = "http://www.w3.org/1999/xlink";
	const std::string::size_type uriAt = text.rfind(uri);
	if (uriAt == std::string::npos) return "xlink";
	const std::string::size_type xmlns = text.rfind("xmlns:", uriAt);
	if (xmlns == std::string::npos || xmlns > uriAt) return "xlink";
	const std::string::size_type begin = xmlns + 6;
	const std::string::size_type end = text.find_first_of("= \t\r\n", begin);
	return text.substr(begin, end - begin);
}

Fb2AutocompleteResult Fb2SourceAutocomplete::Complete(const std::string& text, int character) const {
	Fb2AutocompleteResult result;
	if (IsSuppressed(text)) return result;
	if (character == '<') {
		std::vector<std::string> stack = OpenElements(text.substr(0, text.size() - 1));
		const Fb2SchemaElementMetadata* parent = stack.empty() ? NULL : FindMetadata(stack.back());
		result.candidates = parent ? parent->children : "FictionBook";
		return result;
	}
	if (character == '/' && text.size() >= 2 && text.compare(text.size() - 2, 2, "</") == 0) {
		std::vector<std::string> stack = OpenElements(text.substr(0, text.size() - 2));
		if (!stack.empty()) result.candidates = stack.back() + ">";
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
	if (character == '#') result.needsDocumentIds = true;
	return result;
}

std::string Fb2SourceAutocomplete::CompleteIds(const std::string& document) const {
	std::set<std::string> ids; size_t at = 0;
	while ((at = document.find("id=", at)) != std::string::npos) {
		at += 3; if (at >= document.size() || (document[at] != '\'' && document[at] != '\"')) continue;
		const char quote = document[at++]; const size_t end = document.find(quote, at);
		if (end != std::string::npos) { ids.insert(document.substr(at, end - at)); at = end + 1; }
	}
	return Join(ids);
}
