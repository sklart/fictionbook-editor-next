#pragma once

#include <string>

struct Fb2AutocompleteResult
{
	std::string candidates;
	bool needsDocumentIds;
	Fb2AutocompleteResult() : needsDocumentIds(false) {}
};

// Parses only the text preceding the caret. Scintilla UI and document reads
// deliberately stay outside this component.
class Fb2SourceAutocomplete
{
public:
	Fb2AutocompleteResult Complete(const std::string& beforeCaret, int character) const;
	std::string CompleteIds(const std::string& document) const;

private:
	static bool IsSuppressed(const std::string& beforeCaret);
	static std::string CurrentOpenTag(const std::string& beforeCaret);
	static std::string XLinkPrefix(const std::string& beforeCaret);
};
