#pragma once

#include <cstddef>
#include <string>

struct Fb2SourceStructuralContext
{
	std::string localBeforeCaret;
	std::string parentElement;
	std::string closingElement;
	bool suppressed;
	Fb2SourceStructuralContext() : suppressed(false) {}
};

// This narrow read-only interface keeps Scintilla and WTL out of parser logic.
class Fb2SourceTextReader
{
public:
	virtual ~Fb2SourceTextReader() {}
	virtual std::size_t Length() const = 0;
	virtual void Read(std::size_t position, std::size_t length, std::string& text) const = 0;
};

class Fb2SourceStructuralContextResolver
{
public:
	explicit Fb2SourceStructuralContextResolver(std::size_t chunkSize = 32 * 1024);
	Fb2SourceStructuralContext Resolve(const Fb2SourceTextReader& reader, std::size_t caret, int character) const;

private:
	std::size_t m_chunkSize;
};
