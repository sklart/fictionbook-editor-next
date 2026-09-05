#include "Fb2SourceStructuralContext.h"
#include <vector>

namespace {
const std::size_t kReadChunkSize = 32 * 1024;

class BackwardReader
{
public:
	BackwardReader(const Fb2SourceTextReader& reader, std::size_t chunkSize) : m_reader(reader), m_chunkSize(chunkSize), m_blockStart(0) {}
	bool ByteAt(std::size_t position, char& value)
	{
		if(position >= m_reader.Length()) return false;
		if(m_block.empty() || position < m_blockStart || position >= m_blockStart + m_block.size())
		{
			m_blockStart = position >= m_chunkSize - 1 ? position - (m_chunkSize - 1) : 0;
			m_reader.Read(m_blockStart, m_chunkSize, m_block);
		}
		if(position < m_blockStart || position >= m_blockStart + m_block.size()) return false;
		value = m_block[position - m_blockStart];
		return true;
	}
	bool MatchesBefore(std::size_t end, const char* token)
	{
		std::string value(token); if(end < value.size()) return false;
		for(std::size_t i = 0; i < value.size(); ++i) { char c; if(!ByteAt(end - value.size() + i, c) || c != value[i]) return false; }
		return true;
	}
	bool FindBackward(std::size_t before, const char* token, std::size_t& found, std::size_t minimum = 0)
	{
		std::string value(token);
		while(before >= value.size() && before > minimum) {
			if(MatchesBefore(before, token)) { found = before - value.size(); return true; }
			--before;
		}
		return false;
	}
	std::string Range(std::size_t from, std::size_t to)
	{
		std::string value; char c;
		for(std::size_t p = from; p < to && ByteAt(p, c); ++p) value += c;
		return value;
	}
private:
	const Fb2SourceTextReader& m_reader;
	std::size_t m_chunkSize;
	std::size_t m_blockStart;
	std::string m_block;
};

bool IsNameChar(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ':' || c == '.'; }
bool ParseTag(const std::string& tag, std::string& name, bool& closing, bool& selfClosing)
{
	if(tag.size() < 3 || tag[0] != '<' || tag[tag.size() - 1] != '>') return false;
	if(tag.compare(0, 4, "<!--") == 0 || tag.compare(0, 2, "<?") == 0 || tag.compare(0, 2, "<!") == 0) return false;
	std::size_t at = 1; closing = at < tag.size() && tag[at] == '/'; if(closing) ++at;
	const std::size_t begin = at; while(at < tag.size() && IsNameChar(tag[at])) ++at;
	if(begin == at) return false; name.assign(tag, begin, at - begin);
	char quote = 0;
	for(; at + 1 < tag.size(); ++at) {
		const char c = tag[at]; if(quote) { if(c == quote) quote = 0; } else if(c == '\'' || c == '"') quote = c;
	}
	if(quote) return false;
	std::size_t last = tag.size() - 2; while(last > 0 && (tag[last] == ' ' || tag[last] == '\t' || tag[last] == '\r' || tag[last] == '\n')) --last;
	selfClosing = !closing && tag[last] == '/'; return true;
}

bool PreviousTag(BackwardReader& reader, std::size_t& cursor, std::string& tag, std::size_t minimum = 0)
{
	while(cursor > minimum) {
		std::size_t end; if(!reader.FindBackward(cursor, ">", end, minimum)) return false;
		std::size_t start; if(!reader.FindBackward(end, "<", start, minimum)) { cursor = end; continue; }
		char previous;
		if(start > 0 && reader.ByteAt(start - 1, previous) && (previous == '\'' || previous == '"')) { cursor = start; continue; }
		const std::string candidate = reader.Range(start, end + 1);
		std::string name; bool closing, selfClosing;
		if(ParseTag(candidate, name, closing, selfClosing)) { tag = candidate; cursor = start; return true; }
		cursor = start;
	}
	return false;
}

bool IsSuppressed(BackwardReader& reader, std::size_t caret)
{
	std::size_t open, close;
	if(reader.FindBackward(caret, "<!--", open) && (!reader.FindBackward(caret, "-->", close) || open > close)) return true;
	if(reader.FindBackward(caret, "<![CDATA[", open) && (!reader.FindBackward(caret, "]]>", close) || open > close)) return true;
	if(reader.FindBackward(caret, "<?", open) && (!reader.FindBackward(caret, "?>", close) || open > close)) return true;
	return false;
}

void SkipClosedSpecial(BackwardReader& reader, std::size_t& cursor)
{
	std::size_t start;
	if(reader.MatchesBefore(cursor, "-->")) { if(reader.FindBackward(cursor - 3, "<!--", start)) cursor = start; return; }
	if(reader.MatchesBefore(cursor, "]]>") ) { if(reader.FindBackward(cursor - 3, "<![CDATA[", start)) cursor = start; return; }
	if(reader.MatchesBefore(cursor, "?>")) { if(reader.FindBackward(cursor - 2, "<?", start)) cursor = start; return; }
}

std::string ResolveParent(BackwardReader& reader, std::size_t cursor)
{
	std::vector<std::string> expected;
	while(cursor > 0) {
		SkipClosedSpecial(reader, cursor);
		std::string tag; if(!PreviousTag(reader, cursor, tag)) break;
		std::string name; bool closing, selfClosing; if(!ParseTag(tag, name, closing, selfClosing) || selfClosing) continue;
		if(closing) { expected.push_back(name); continue; }
		if(!expected.empty()) { if(expected.back() != name) return ""; expected.pop_back(); continue; }
		return name;
	}
	return "";
}

std::vector<std::string> ResolveBreadcrumb(BackwardReader& reader, std::size_t cursor, std::size_t minimum, bool& truncated)
{
	std::vector<std::string> expected, reverse;
	while(cursor > minimum) {
		SkipClosedSpecial(reader, cursor);
		std::string tag; if(!PreviousTag(reader, cursor, tag, minimum)) break;
		std::string name; bool closing, selfClosing;
		if(!ParseTag(tag, name, closing, selfClosing) || selfClosing) continue;
		if(closing) { expected.push_back(name); continue; }
		if(!expected.empty()) { if(expected.back() != name) return std::vector<std::string>(); expected.pop_back(); continue; }
		reverse.push_back(name);
	}
	truncated = cursor > 0;
	return std::vector<std::string>(reverse.rbegin(), reverse.rend());
}

std::string LocalBeforeCaret(BackwardReader& reader, std::size_t caret)
{
	std::size_t start; if(!reader.FindBackward(caret, "<", start)) return "";
	const std::string local = reader.Range(start, caret);
	char quote = 0;
	for(std::size_t i = 1; i < local.size(); ++i) {
		if(quote) { if(local[i] == quote) quote = 0; }
		else if(local[i] == '\'' || local[i] == '"') quote = local[i];
		else if(local[i] == '>') return "";
	}
	return local;
}
}

Fb2SourceStructuralContextResolver::Fb2SourceStructuralContextResolver(std::size_t chunkSize) : m_chunkSize(chunkSize ? chunkSize : kReadChunkSize) {}

Fb2SourceStructuralContext Fb2SourceStructuralContextResolver::Resolve(const Fb2SourceTextReader& source, std::size_t caret, int character) const
{
	Fb2SourceStructuralContext context; if(caret > source.Length()) caret = source.Length();
	BackwardReader reader(source, m_chunkSize);
	context.localBeforeCaret = LocalBeforeCaret(reader, caret);
	if(character == 0) {
		const std::size_t kBreadcrumbBudget = 256 * 1024;
		const std::size_t minimum = caret > kBreadcrumbBudget ? caret - kBreadcrumbBudget : 0;
		context.breadcrumb = ResolveBreadcrumb(reader, caret, minimum, context.breadcrumbTruncated);
	}
	if(character == '<' && !context.localBeforeCaret.empty()) context.parentElement = ResolveParent(reader, caret - 1);
	if(character == '/' && context.localBeforeCaret == "</" && caret >= 2) context.closingElement = ResolveParent(reader, caret - 2);
	if((character == '<' && context.parentElement.empty()) || (character == '/' && context.closingElement.empty()))
		context.suppressed = IsSuppressed(reader, caret);
	return context;
}
