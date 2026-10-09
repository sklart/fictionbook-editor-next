#include "stdafx.h"
#include "Fb2QualityChecker.h"
#include "resource.h"
#include <cwctype>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace Fb2Quality {
namespace {
using Node = MSXML2::IXMLDOMNodePtr;

CString Name(const Node& node)
{
	CString name(static_cast<const wchar_t*>(_bstr_t(node->nodeName)));
	const int colon = name.Find(L':');
	return colon < 0 ? name : name.Mid(colon + 1);
}

CString Attribute(const Node& node, const wchar_t* name)
{
	MSXML2::IXMLDOMElementPtr element(node);
	if (!element) return CString();
	_variant_t value = element->getAttribute(name);
	return value.vt == VT_NULL || value.vt == VT_EMPTY ? CString() : CString(static_cast<const wchar_t*>(_bstr_t(value)));
}

bool HasDescendant(const Node& node, const wchar_t* wanted)
{
	std::vector<Node> pending;
	MSXML2::IXMLDOMNodeListPtr children = node->childNodes;
	for (long i = 0; i < children->length; ++i) pending.push_back(children->item[i]);
	while (!pending.empty()) {
		Node current = pending.back();
		pending.pop_back();
		if (current->nodeType != MSXML2::NODE_ELEMENT) continue;
		if (Name(current) == wanted) return true;
		MSXML2::IXMLDOMNodeListPtr descendants = current->childNodes;
		for (long i = 0; i < descendants->length; ++i) pending.push_back(descendants->item[i]);
	}
	return false;
}

bool HasDirectChild(const Node& node, const wchar_t* wanted)
{
	MSXML2::IXMLDOMNodeListPtr children = node->childNodes;
	for (long i = 0; i < children->length; ++i) {
		Node child = children->item[i];
		if (child->nodeType == MSXML2::NODE_ELEMENT && Name(child) == wanted) return true;
	}
	return false;
}

bool MeaningfulContent(const Node& node)
{
	CString content(static_cast<const wchar_t*>(_bstr_t(node->text)));
	if (!content.IsEmpty()) {
		for (Node current = node; current; current = current->parentNode) {
			if (current->nodeType != MSXML2::NODE_ELEMENT) break;
			const CString space = Attribute(current, L"xml:space");
			if (space.CompareNoCase(L"preserve") == 0) return true;
			if (space.CompareNoCase(L"default") == 0) break;
		}
	}
	content.Trim();
	return !content.IsEmpty() || HasDescendant(node, L"image");
}

bool PlausibleLanguageCode(const CString& value)
{
	if (value.IsEmpty() || value.GetLength() > 255 || value[0] == L'-' || value[value.GetLength() - 1] == L'-') return false;
	wchar_t previous = 0;
	for (int i = 0; i < value.GetLength(); ++i) {
		const wchar_t c = value[i];
		if (!((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'-') ||
			(c == L'-' && previous == L'-')) return false;
		previous = c;
	}
	return true;
}

bool IntegerSyntax(const CString& value)
{
	if (value.IsEmpty()) return false;
	int i = value[0] == L'+' || value[0] == L'-' ? 1 : 0;
	if (i == value.GetLength()) return false;
	for (; i < value.GetLength(); ++i) if (value[i] < L'0' || value[i] > L'9') return false;
	return true;
}

struct HrefAttribute { CString name; CString value; bool present = false; };

HrefAttribute XLinkHref(const Node& node)
{
	MSXML2::IXMLDOMNamedNodeMapPtr attributes = node->attributes;
	if (!attributes) return HrefAttribute();
	for (long i = 0; i < attributes->length; ++i) {
		Node attribute = attributes->item[i];
		if (Name(attribute) == L"href" &&
			CString(static_cast<const wchar_t*>(_bstr_t(attribute->namespaceURI))) == L"http://www.w3.org/1999/xlink")
			return { CString(static_cast<const wchar_t*>(_bstr_t(attribute->nodeName))),
				CString(static_cast<const wchar_t*>(_bstr_t(attribute->text))), true };
	}
	return HrefAttribute();
}

bool ValidLocalHref(const CString& href)
{
	if (href.GetLength() < 2 || href[0] != L'#') return false;
	for (int i = 1; i < href.GetLength(); ++i)
		if (iswspace(href[i]) || href[i] == L'#') return false;
	return true;
}

struct Base64Result {
	bool hasData = false;
	bool valid = true;
	unsigned char prefix[16] = {};
	int prefixLength = 0;
};

void AppendDecodedPrefix(Base64Result& result, const int sextets[4], int padding)
{
	if (result.prefixLength == static_cast<int>(_countof(result.prefix))) return;
	const unsigned char bytes[3] = {
		static_cast<unsigned char>((sextets[0] << 2) | (sextets[1] >> 4)),
		static_cast<unsigned char>((sextets[1] << 4) | (sextets[2] >> 2)),
		static_cast<unsigned char>((sextets[2] << 6) | sextets[3])
	};
	for (int i = 0; i < 3 - padding && result.prefixLength < static_cast<int>(_countof(result.prefix)); ++i)
		result.prefix[result.prefixLength++] = bytes[i];
}

CString DetectImageMime(const Base64Result& data)
{
	const unsigned char* p = data.prefix;
	if (data.prefixLength >= 8 && p[0] == 0x89 && p[1] == 'P' && p[2] == 'N' && p[3] == 'G' &&
		p[4] == 0x0D && p[5] == 0x0A && p[6] == 0x1A && p[7] == 0x0A) return L"image/png";
	if (data.prefixLength >= 3 && p[0] == 0xFF && p[1] == 0xD8 && p[2] == 0xFF) return L"image/jpeg";
	if (data.prefixLength >= 6 && p[0] == 'G' && p[1] == 'I' && p[2] == 'F' && p[3] == '8' &&
		(p[4] == '7' || p[4] == '9') && p[5] == 'a') return L"image/gif";
	if (data.prefixLength >= 12 && p[0] == 'R' && p[1] == 'I' && p[2] == 'F' && p[3] == 'F' &&
		p[8] == 'W' && p[9] == 'E' && p[10] == 'B' && p[11] == 'P') return L"image/webp";
	if (data.prefixLength >= 2 && p[0] == 'B' && p[1] == 'M') return L"image/bmp";
	return CString();
}

Base64Result CheckBase64(const Node& binary)
{
	Base64Result result;
	int quartet = 0;
	int padding = 0;
	int sextets[4] = {};
	bool finished = false;
	MSXML2::IXMLDOMNodeListPtr children = binary->childNodes;
	for (long i = 0; i < children->length; ++i) {
		Node child = children->item[i];
		if (child->nodeType == MSXML2::NODE_COMMENT || child->nodeType == MSXML2::NODE_PROCESSING_INSTRUCTION) continue;
		if (child->nodeType != MSXML2::NODE_TEXT && child->nodeType != MSXML2::NODE_CDATA_SECTION) return { result.hasData, false };
		MSXML2::IXMLDOMCharacterDataPtr characters(child);
		const long length = characters->length;
		for (long offset = 0; offset < length; offset += 4096) {
			const long count = length - offset < 4096 ? length - offset : 4096;
			const _bstr_t chunk = characters->substringData(offset, count);
			const wchar_t* text = static_cast<const wchar_t*>(chunk);
			for (unsigned int j = 0; j < chunk.length(); ++j) {
				const wchar_t c = text[j];
				if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') continue;
				result.hasData = true;
				if (finished) return { true, false };
				if (c == L'=') {
					if (quartet < 2 || padding == 2) return { true, false };
					sextets[quartet] = 0;
					++padding;
				} else {
					if (padding || !((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
						(c >= L'0' && c <= L'9') || c == L'+' || c == L'/')) return { true, false };
					sextets[quartet] = c >= L'A' && c <= L'Z' ? c - L'A' :
						c >= L'a' && c <= L'z' ? c - L'a' + 26 :
						c >= L'0' && c <= L'9' ? c - L'0' + 52 : c == L'+' ? 62 : 63;
				}
				if (++quartet == 4) {
				AppendDecodedPrefix(result, sextets, padding);
					finished = padding != 0;
					quartet = 0;
					padding = 0;
				}
			}
		}
	}
	result.valid = quartet == 0;
	return result;
}

bool ImageMimeMismatch(const CString& declared, const Base64Result& data)
{
	if (!data.valid) return false;
	CString normalized(declared);
	const int parameters = normalized.Find(L';');
	if (parameters >= 0) normalized = normalized.Left(parameters);
	normalized.Trim();
	if (normalized.CompareNoCase(L"image/jpg") == 0) normalized = L"image/jpeg";
	if (normalized.CompareNoCase(L"image/x-ms-bmp") == 0) normalized = L"image/bmp";
	const CString detected = DetectImageMime(data);
	if (detected.IsEmpty()) return false;
	const bool supported = normalized.CompareNoCase(L"image/png") == 0 || normalized.CompareNoCase(L"image/jpeg") == 0 ||
		normalized.CompareNoCase(L"image/gif") == 0 || normalized.CompareNoCase(L"image/webp") == 0 ||
		normalized.CompareNoCase(L"image/bmp") == 0;
	return supported && normalized.CompareNoCase(detected) != 0;
}

void Add(Report& report, Severity severity, const CString& message, const wchar_t* code)
{
	Issue issue = { severity, message };
	issue.code = code;
	report.issues.push_back(issue);
}

// MSXML validates the XML and supplies the semantic DOM, but does not retain source
// spans. This scanner only indexes the original start tags in DOM preorder. It
// never interprets XML structure or entities; any mismatch disables navigation.
struct SourceIndexer {
	struct AttributeSpan { CString name; int start; int end; };
	struct TagSpan { int start = -1; int end = -1; std::vector<AttributeSpan> attributes; };
	const CString& xml;
	int cursor = 0;
	bool valid = true;
	explicit SourceIndexer(const CString& source) : xml(source) {}

	bool Next(const CString& expectedName, TagSpan& span)
	{
		if (!valid) return false;
		const int length = xml.GetLength();
		while (cursor < length) {
			const int open = xml.Find(L'<', cursor);
			if (open < 0 || open + 1 >= length) break;
			const wchar_t kind = xml[open + 1];
			if (kind == L'!' && xml.Mid(open, 4) == L"<!--") {
				const int close = xml.Find(L"-->", open + 4);
				if (close < 0) break;
				cursor = close + 3;
				continue;
			}
			if (kind == L'!' && xml.Mid(open, 9) == L"<![CDATA[") {
				const int close = xml.Find(L"]]>", open + 9);
				if (close < 0) break;
				cursor = close + 3;
				continue;
			}
			if (kind == L'?') {
				const int close = xml.Find(L"?>", open + 2);
				if (close < 0) break;
				cursor = close + 2;
				continue;
			}
			if (kind == L'!' || kind == L'/') {
				const int close = xml.Find(L'>', open + 2);
				if (close < 0) break;
				cursor = close + 1;
				continue;
			}
			int pos = open + 1;
			while (pos < length && xml[pos] != L'>' && xml[pos] != L'/' && !iswspace(xml[pos])) ++pos;
			const CString name(xml.GetString() + open + 1, pos - open - 1);
			if (name != expectedName) break;
			span.start = open;
			while (pos < length) {
				while (pos < length && iswspace(xml[pos])) ++pos;
				if (pos >= length) break;
				if (xml[pos] == L'>') { span.end = ++pos; cursor = pos; return true; }
				if (xml[pos] == L'/') { ++pos; continue; }
				const int attributeStart = pos;
				while (pos < length && xml[pos] != L'=' && xml[pos] != L'>' && !iswspace(xml[pos])) ++pos;
				const CString attributeName(xml.GetString() + attributeStart, pos - attributeStart);
				while (pos < length && iswspace(xml[pos])) ++pos;
				if (pos >= length || xml[pos++] != L'=') break;
				while (pos < length && iswspace(xml[pos])) ++pos;
				if (pos >= length || (xml[pos] != L'\'' && xml[pos] != L'"')) break;
				const wchar_t quote = xml[pos++];
				while (pos < length && xml[pos] != quote) ++pos;
				if (pos >= length) break;
				++pos;
				span.attributes.push_back({ attributeName, attributeStart, pos });
			}
			break;
		}
		valid = false;
		return false;
	}
};

struct Scan {
	Report report;
	SourceIndexer index;
	std::map<std::vector<int>, SourceIndexer::TagSpan> spans;
	std::set<std::wstring> ids;
	std::set<std::wstring> duplicateIds;
	std::set<std::wstring> binaries;
	std::set<std::wstring> usedBinaries;
	std::set<std::wstring> noteIds;
	std::map<std::wstring, std::vector<int>> binaryPaths;
	struct Link { CString href; CString sourceValue; CString attributeName; CString sourceNoteId; std::vector<int> path; bool note; bool image; };
	std::vector<Link> links;
	bool description = false;
	bool body = false;
	bool bodySection = false;
	bool titleInfo = false;
	bool bookTitle = false;
	bool language = false;
	bool author = false;
	std::vector<int> descriptionPath;
	std::vector<int> bodyPath;
	std::vector<int> titleInfoPath;
	explicit Scan(const CString& xml) : index(xml) {}

	void AddAt(Severity severity, const wchar_t* code, const CString& message,
		const std::vector<int>& path, const CString& attributeName = CString(), const CString& attributeValue = CString())
	{
		Issue issue = { severity, message };
		issue.code = code;
		issue.elementPath = path;
		issue.attributeName = attributeName;
		issue.attributeValue = attributeValue;
		const auto found = spans.find(path);
		if (index.valid && found != spans.end()) {
			const SourceIndexer::TagSpan& tag = found->second;
			if (attributeName.IsEmpty()) { issue.start = tag.start; issue.end = tag.end; }
			else {
				for (const auto& attribute : tag.attributes) {
					if (attribute.name == attributeName) { issue.start = attribute.start; issue.end = attribute.end; break; }
				}
				if (issue.start < 0 && attributeValue.IsEmpty()) { issue.start = tag.start; issue.end = tag.end; }
			}
		}
		report.issues.push_back(issue);
	}

	void Visit(const Node& root, bool initialNotes, bool initialTitleInfo, const std::vector<int>& rootPath)
	{
		struct Frame { Node node; bool inNotes; bool inTitleInfo; CString sourceNoteId; std::vector<int> path; };
		std::vector<Frame> pending;
		pending.push_back({ root, initialNotes, initialTitleInfo, CString(), rootPath });
		while (!pending.empty()) {
			Frame frame(std::move(pending.back()));
			pending.pop_back();
			const Node& node = frame.node;
			bool inNotes = frame.inNotes;
			bool inTitleInfo = frame.inTitleInfo;
			CString sourceNoteId(frame.sourceNoteId);
			const std::vector<int>& path = frame.path;
			if (node->nodeType != MSXML2::NODE_ELEMENT) continue;
			SourceIndexer::TagSpan span;
			if (index.Next(CString(static_cast<const wchar_t*>(_bstr_t(node->nodeName))), span)) spans[path] = span;
			const CString name = Name(node);
			const CString id = Attribute(node, L"id");
			if (!id.IsEmpty()) {
				if (!ids.insert(std::wstring(id.GetString())).second) {
					duplicateIds.insert(std::wstring(id.GetString()));
					if (name != L"binary" || binaries.find(std::wstring(id.GetString())) == binaries.end()) {
						CString message; message.Format(L"Повторяется идентификатор %s", id.GetString());
						AddAt(Severity::Error, L"Q-LINK-DUPLICATE-ID", message, path, L"id", id);
					}
				}
				if (inNotes && name == L"section") { noteIds.insert(std::wstring(id.GetString())); sourceNoteId = id; }
			}
			if (name == L"description") { description = true; if (descriptionPath.empty()) descriptionPath = path; }
			if (name == L"body") {
				body = true;
				const CString bodyName = Attribute(node, L"name");
				inNotes = bodyName.CompareNoCase(L"notes") == 0 || bodyName.CompareNoCase(L"comments") == 0;
				sourceNoteId.Empty();
				if (!inNotes && bodyPath.empty()) bodyPath = path;
			}
			if (name == L"section" && !inNotes) bodySection = true;
			if (name == L"title-info") { titleInfo = true; inTitleInfo = true; if (titleInfoPath.empty()) titleInfoPath = path; }
			if (inTitleInfo && (name == L"book-title" || name == L"lang")) {
				CString content(static_cast<const wchar_t*>(_bstr_t(node->text)));
				content.Trim();
				if (name == L"book-title") {
					bookTitle = true;
					if (content.IsEmpty()) AddAt(Severity::Warning, L"Q-METADATA-BOOK-TITLE", L"Не указано название книги", path);
				}
				if (name == L"lang") {
					language = true;
					if (content.IsEmpty()) AddAt(Severity::Warning, L"Q-METADATA-LANGUAGE", L"Не указан язык документа", path);
					else if (!PlausibleLanguageCode(content)) AddAt(Severity::Warning, L"Q-METADATA-INVALID-LANG", L"Подозрительный код языка документа", path);
				}
			}
			if (inTitleInfo && name == L"author") {
				author = true;
				bool named = false;
				MSXML2::IXMLDOMNodeListPtr fields = node->childNodes;
				for (long i = 0; i < fields->length; ++i) {
					Node field = fields->item[i];
					if (field->nodeType != MSXML2::NODE_ELEMENT) continue;
					const CString fieldName = Name(field);
					if (fieldName != L"nickname" && fieldName != L"first-name" && fieldName != L"middle-name" && fieldName != L"last-name") continue;
					CString value(static_cast<const wchar_t*>(_bstr_t(field->text)));
					value.Trim();
					if (!value.IsEmpty()) { named = true; break; }
				}
				if (!named) AddAt(Severity::Warning, L"Q-METADATA-EMPTY-AUTHOR", L"Пустые сведения об авторе", path);
			}
			if (inTitleInfo && name == L"sequence") {
				CString sequenceName = Attribute(node, L"name");
				sequenceName.Trim();
				if (sequenceName.IsEmpty()) AddAt(Severity::Warning, L"Q-METADATA-EMPTY-SEQUENCE", L"Не указано название серии", path, L"name");
				MSXML2::IXMLDOMElementPtr element(node);
				if (element->getAttributeNode(L"number")) {
					const CString number = Attribute(node, L"number");
					if (!IntegerSyntax(number)) AddAt(Severity::Warning, L"Q-METADATA-SEQUENCE-NUMBER", L"Некорректный номер серии", path, L"number", number);
				}
			}
			if (name == L"binary") {
				if (id.IsEmpty()) AddAt(Severity::Error, L"Q-BINARY-MISSING-ID", L"У binary не указан id", path, L"id");
				else {
					if (!binaries.insert(std::wstring(id.GetString())).second)
						AddAt(Severity::Error, L"Q-BINARY-DUPLICATE-ID", L"Повторяется binary id " + id, path, L"id", id);
					binaryPaths.emplace(std::wstring(id.GetString()), path);
				}
				const CString contentType = Attribute(node, L"content-type");
				if (contentType.IsEmpty()) AddAt(Severity::Error, L"Q-BINARY-MISSING-MIME", L"У binary не указан content-type", path, L"content-type");
				else if (contentType.Find(L'/') <= 0 || contentType.Right(1) == L"/")
					AddAt(Severity::Error, L"Q-BINARY-INVALID-MIME", L"Некорректный content-type у binary", path, L"content-type", contentType);
				const Base64Result base64 = CheckBase64(node);
				if (!base64.hasData) AddAt(Severity::Error, L"Q-BINARY-EMPTY", L"Пустое содержимое binary", path);
				else if (!base64.valid) AddAt(Severity::Error, L"Q-BINARY-INVALID-BASE64", L"Некорректные данные Base64 в binary", path);
				else if (ImageMimeMismatch(contentType, base64))
					AddAt(Severity::Error, L"Q-BINARY-MIME-MISMATCH", L"Формат данных binary не соответствует content-type", path, L"content-type", contentType);
			}
			if (name == L"a" || name == L"image") {
				const HrefAttribute href = XLinkHref(node);
				const bool note = name == L"a" && Attribute(node, L"type").CompareNoCase(L"note") == 0;
				if (href.present && !href.value.IsEmpty() && href.value[0] == L'#') {
					if (ValidLocalHref(href.value)) {
						links.push_back({ href.value.Mid(1), href.value, href.name, sourceNoteId, path, note, name == L"image" });
						if (name == L"image") usedBinaries.insert(std::wstring(href.value.Mid(1).GetString()));
					} else AddAt(Severity::Error, name == L"image" ? L"Q-IMAGE-INVALID-HREF" : L"Q-LINK-INVALID-HREF",
						L"Некорректная внутренняя ссылка", path, href.name, href.value);
				} else if (name == L"image")
					AddAt(Severity::Error, L"Q-IMAGE-NONLOCAL", L"У изображения отсутствует внутренняя ссылка на binary", path, href.name, href.value);
				else if (note)
					AddAt(Severity::Error, L"Q-NOTE-NONLOCAL", L"Ссылка на примечание должна быть внутренней", path, href.name, href.value);
			}
			if ((name == L"p" || name == L"subtitle" || name == L"title" || name == L"cite") && !MeaningfulContent(node)) {
				const CString parentName = node->parentNode ? Name(node->parentNode) : CString();
				if (name != L"p" || (parentName != L"title" && parentName != L"cite")) {
					const wchar_t* code = name == L"p" ? L"Q-STRUCTURE-EMPTY-P" : name == L"subtitle" ? L"Q-STRUCTURE-EMPTY-SUBTITLE" :
						name == L"title" ? L"Q-STRUCTURE-EMPTY-TITLE" : L"Q-STRUCTURE-EMPTY-CITE";
					AddAt(Severity::Warning, code, L"Подозрительный пустой элемент " + name, path);
				}
			}
			if (name == L"section" && !HasDirectChild(node, L"section") && !MeaningfulContent(node))
				AddAt(Severity::Warning, L"Q-STRUCTURE-EMPTY-SECTION", L"Пустой раздел section", path);
			MSXML2::IXMLDOMNodeListPtr children = node->childNodes;
			int elementIndex = 0;
			for (long i = 0; i < children->length; ++i) {
				Node child = children->item[i];
				if (child->nodeType == MSXML2::NODE_ELEMENT) ++elementIndex;
			}
			for (long i = children->length; i > 0; --i) {
				Node child = children->item[i - 1];
				if (child->nodeType == MSXML2::NODE_ELEMENT) {
					std::vector<int> childPath(path);
					childPath.push_back(--elementIndex);
					pending.push_back({ child, inNotes, inTitleInfo, sourceNoteId, std::move(childPath) });
				}
			}
		}
	}

	void AddNoteCycles()
	{
		std::map<std::wstring, int> positions;
		for (const std::wstring& id : noteIds) {
			if (duplicateIds.find(id) == duplicateIds.end())
				positions.emplace(id, static_cast<int>(positions.size()));
		}
		const size_t count = positions.size();
		std::vector<std::vector<int>> edges(count), reverse(count);
		for (const Link& link : links) {
			if (!link.note || link.sourceNoteId.IsEmpty()) continue;
			const auto source = positions.find(std::wstring(link.sourceNoteId.GetString()));
			const auto target = positions.find(std::wstring(link.href.GetString()));
			if (source == positions.end() || target == positions.end()) continue;
			edges[source->second].push_back(target->second);
			reverse[target->second].push_back(source->second);
		}
		std::vector<bool> visited(count, false);
		std::vector<int> order;
		for (size_t i = 0; i < count; ++i) {
			if (visited[i]) continue;
			std::vector<std::pair<int, bool>> pending{ { static_cast<int>(i), false } };
			while (!pending.empty()) {
				const auto current = pending.back();
				pending.pop_back();
				if (current.second) { order.push_back(current.first); continue; }
				if (visited[current.first]) continue;
				visited[current.first] = true;
				pending.push_back({ current.first, true });
				for (int target : edges[current.first]) if (!visited[target]) pending.push_back({ target, false });
			}
		}
		std::vector<int> component(count, -1), sizes;
		for (auto it = order.rbegin(); it != order.rend(); ++it) {
			if (component[*it] >= 0) continue;
			const int group = static_cast<int>(sizes.size());
			int size = 0;
			std::vector<int> pending{ *it };
			component[*it] = group;
			while (!pending.empty()) {
				const int current = pending.back();
				pending.pop_back();
				++size;
				for (int source : reverse[current]) if (component[source] < 0) {
					component[source] = group;
					pending.push_back(source);
				}
			}
			sizes.push_back(size);
		}
		for (const Link& link : links) {
			if (!link.note || link.sourceNoteId.IsEmpty()) continue;
			const auto source = positions.find(std::wstring(link.sourceNoteId.GetString()));
			const auto target = positions.find(std::wstring(link.href.GetString()));
			if (source == positions.end() || target == positions.end()) continue;
			if (source->second == target->second)
				AddAt(Severity::Warning, L"Q-NOTE-SELF-REFERENCE", L"Примечание ссылается на себя", link.path, link.attributeName, link.sourceValue);
			else if (component[source->second] == component[target->second] && sizes[component[source->second]] > 1)
				AddAt(Severity::Warning, L"Q-NOTE-CYCLE", L"Цикл ссылок между примечаниями", link.path, link.attributeName, link.sourceValue);
		}
	}

	Report Finish()
	{
		const std::vector<int> rootPath{ 0 };
		if (!description) AddAt(Severity::Error, L"Q-STRUCTURE-DESCRIPTION", L"Отсутствует description", rootPath);
		if (!body) AddAt(Severity::Error, L"Q-STRUCTURE-BODY", L"Отсутствует body", rootPath);
		else if (!bodySection) AddAt(Severity::Error, L"Q-STRUCTURE-BODY-SECTION", L"В основном body нет разделов section", bodyPath.empty() ? rootPath : bodyPath);
		if (!titleInfo) AddAt(Severity::Error, L"Q-METADATA-TITLE-INFO", L"Отсутствует title-info", descriptionPath.empty() ? rootPath : descriptionPath);
		const std::vector<int>& metadataPath = titleInfoPath.empty() ? (descriptionPath.empty() ? rootPath : descriptionPath) : titleInfoPath;
		if (!bookTitle) AddAt(Severity::Warning, L"Q-METADATA-BOOK-TITLE", L"Не указано название книги", metadataPath);
		if (!language) AddAt(Severity::Warning, L"Q-METADATA-LANGUAGE", L"Не указан язык документа", metadataPath);
		if (!author) AddAt(Severity::Warning, L"Q-METADATA-AUTHOR", L"Не указан автор", metadataPath);
		for (const Link& link : links) {
			const std::wstring target(link.href.GetString());
			if (link.note && noteIds.find(target) == noteIds.end()) {
				const bool missing = ids.find(target) == ids.end();
				CString message;
				message.Format(missing ? L"Ссылка на примечание #%s не найдена" : L"Ссылка на примечание #%s ведёт не в раздел примечаний",
					link.href.GetString());
				AddAt(Severity::Error, missing ? L"Q-NOTE-MISSING" : L"Q-NOTE-WRONG-TARGET",
					message, link.path, link.attributeName, link.sourceValue);
			} else if (!link.image && ids.find(target) == ids.end()) {
				CString message; message.Format(L"Ссылка #%s не найдена", link.href.GetString());
				AddAt(Severity::Error, L"Q-LINK-MISSING", message, link.path, link.attributeName, link.sourceValue);
			} else if (link.image && binaries.find(target) == binaries.end()) {
				CString message; message.Format(L"Binary %s отсутствует", link.href.GetString());
				AddAt(Severity::Error, L"Q-IMAGE-MISSING-BINARY", message, link.path, link.attributeName, link.sourceValue);
			}
		}
		AddNoteCycles();
		for (const std::wstring& id : binaries) if (usedBinaries.find(id) == usedBinaries.end()) {
			CString message; message.Format(L"Binary %s не используется", id.c_str());
			AddAt(Severity::Warning, L"Q-BINARY-UNUSED", message, binaryPaths[id], L"id", id.c_str());
		}
		if (!index.valid) for (Issue& issue : report.issues) issue.start = issue.end = -1;
		return report;
	}
};

class ResultsDialog : public CDialogImpl<ResultsDialog> {
public:
	enum { IDD = IDD_FB2_QUALITY_RESULTS };
	explicit ResultsDialog(const Report& report) : m_report(report) {}
	int selected = -1;
	BEGIN_MSG_MAP(ResultsDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInit)
		COMMAND_ID_HANDLER(IDC_FB2_QUALITY_GOTO, OnGoTo)
		COMMAND_ID_HANDLER(IDC_FB2_QUALITY_SAVE, OnSave)
		COMMAND_ID_HANDLER(IDCANCEL, OnClose)
		NOTIFY_HANDLER(IDC_FB2_QUALITY_LIST, NM_DBLCLK, OnActivate)
		NOTIFY_HANDLER(IDC_FB2_QUALITY_LIST, LVN_ITEMCHANGED, OnSelectionChanged)
	END_MSG_MAP()
private:
	const Report& m_report;
	LRESULT OnInit(UINT, WPARAM, LPARAM, BOOL&) {
		CString summary; summary.Format(L"Ошибки: %d     Предупреждения: %d", m_report.ErrorCount(), m_report.WarningCount());
		SetDlgItemText(IDC_FB2_QUALITY_SUMMARY, summary);
		CListViewCtrl list = GetDlgItem(IDC_FB2_QUALITY_LIST);
		list.InsertColumn(0, L"Тип", LVCFMT_LEFT, 100);
		list.InsertColumn(1, L"Проблема", LVCFMT_LEFT, 410);
		for (size_t i = 0; i < m_report.issues.size(); ++i) {
			const Issue& issue = m_report.issues[i];
			list.InsertItem(static_cast<int>(i), issue.severity == Severity::Error ? L"Ошибка" : L"Предупреждение");
			list.SetItemText(static_cast<int>(i), 1, issue.message);
		}
		if (!m_report.issues.empty()) list.SelectItem(0);
		UpdateGoTo();
		return TRUE;
	}
	void UpdateGoTo() {
		CListViewCtrl list = GetDlgItem(IDC_FB2_QUALITY_LIST);
		const int index = list.GetNextItem(-1, LVNI_SELECTED);
		const bool available = index >= 0 && static_cast<size_t>(index) < m_report.issues.size() && m_report.issues[index].start >= 0;
		::EnableWindow(GetDlgItem(IDC_FB2_QUALITY_GOTO), available ? TRUE : FALSE);
	}
	LRESULT OnSelectionChanged(int, LPNMHDR, BOOL&) { UpdateGoTo(); return 0; }
	LRESULT OnGoTo(WORD, WORD, HWND, BOOL&) {
		CListViewCtrl list = GetDlgItem(IDC_FB2_QUALITY_LIST);
		const int index = list.GetNextItem(-1, LVNI_SELECTED);
		if (index >= 0 && static_cast<size_t>(index) < m_report.issues.size()) selected = index;
		if (selected >= 0) EndDialog(IDOK);
		return 0;
	}
	LRESULT OnActivate(int, LPNMHDR, BOOL& handled) { BOOL ignored = FALSE; handled = TRUE; return OnGoTo(0, 0, NULL, ignored); }
	LRESULT OnSave(WORD, WORD, HWND, BOOL&) {
		wchar_t path[MAX_PATH] = L"fb2-quality-report.txt";
		OPENFILENAMEW file = {}; file.lStructSize = sizeof(file); file.hwndOwner = m_hWnd;
		file.lpstrFilter = L"Текстовый отчёт (*.txt)\0*.txt\0Все файлы (*.*)\0*.*\0";
		file.lpstrFile = path; file.nMaxFile = _countof(path); file.lpstrDefExt = L"txt";
		file.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
		if (!::GetSaveFileNameW(&file)) return 0;
		const CString report = FormatReport(m_report);
		const int size = ::WideCharToMultiByte(CP_UTF8, 0, report, report.GetLength(), NULL, 0, NULL, NULL);
		if (size <= 0) return 0;
		std::vector<char> bytes(static_cast<size_t>(size));
		::WideCharToMultiByte(CP_UTF8, 0, report, report.GetLength(), bytes.data(), size, NULL, NULL);
		HANDLE output = ::CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (output == INVALID_HANDLE_VALUE) { ::MessageBoxW(m_hWnd, L"Не удалось сохранить отчёт.", L"Расширенная проверка FB2", MB_ICONERROR); return 0; }
		DWORD written = 0; const char bom[] = "\xEF\xBB\xBF";
		const BOOL ok = ::WriteFile(output, bom, 3, &written, NULL) && written == 3 &&
			::WriteFile(output, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL) && written == bytes.size();
		::CloseHandle(output);
		if (!ok) ::MessageBoxW(m_hWnd, L"Не удалось полностью сохранить отчёт.", L"Расширенная проверка FB2", MB_ICONERROR);
		return 0;
	}
	LRESULT OnClose(WORD, WORD, HWND, BOOL&) { EndDialog(IDCANCEL); return 0; }
};
}

int Report::ErrorCount() const { int result = 0; for (const Issue& issue : issues) if (issue.severity == Severity::Error) ++result; return result; }
int Report::WarningCount() const { int result = 0; for (const Issue& issue : issues) if (issue.severity == Severity::Warning) ++result; return result; }

Report Check(const CString& xml)
{
	Report report;
	try {
	MSXML2::IXMLDOMDocument2Ptr document;
	if (FAILED(document.CreateInstance(L"Msxml2.DOMDocument.6.0"))) { Add(report, Severity::Error, L"Не удалось создать XML-анализатор.", L"Q-XML-ANALYZER"); return report; }
	document->async = VARIANT_FALSE;
	document->validateOnParse = VARIANT_FALSE;
	document->resolveExternals = VARIANT_FALSE;
	document->setProperty(L"ProhibitDTD", _variant_t(VARIANT_TRUE));
	if (document->loadXML(_bstr_t(xml)) == VARIANT_FALSE) {
		MSXML2::IXMLDOMParseErrorPtr error = document->parseError;
		CString message; message.Format(L"Некорректный XML, строка %ld: %s", error->line, static_cast<const wchar_t*>(_bstr_t(error->reason)));
		Add(report, Severity::Error, message, L"Q-XML-PARSE");
		Issue& issue = report.issues.back();
		int line = 1;
		for (int offset = 0; offset < xml.GetLength(); ++offset) {
			if (line == error->line) { issue.start = offset + static_cast<int>(error->linepos) - 1; break; }
			if (xml[offset] == L'\n') ++line;
		}
		// MSXML reports line=0 for an unclosed tag at EOF (0xC00CE553).
		if (issue.start < 0 && error->errorCode == static_cast<long>(0xC00CE553) && !xml.IsEmpty())
			issue.start = xml.GetLength() - 1;
		if (issue.start == xml.GetLength() && !xml.IsEmpty()) --issue.start;
		if (issue.start >= 0 && issue.start < xml.GetLength()) issue.end = issue.start + 1;
		else issue.start = issue.end = -1;
		return report;
	}
	Node root = document->documentElement;
	if (!root || Name(root) != L"FictionBook") { Add(report, Severity::Error, L"Корневой элемент должен быть FictionBook.", L"Q-XML-ROOT"); return report; }
	if (CString(static_cast<const wchar_t*>(_bstr_t(root->namespaceURI))) != L"http://www.gribuser.ru/xml/fictionbook/2.0")
		Add(report, Severity::Error, L"Неверное пространство имён FictionBook.", L"Q-XML-NAMESPACE");
	Scan scan(xml); scan.Visit(root, false, false, { 0 });
	Report findings = scan.Finish();
	report.issues.insert(report.issues.end(), findings.issues.begin(), findings.issues.end());
	return report;
	} catch (const _com_error&) {
		Add(report, Severity::Error, L"Не удалось завершить анализ XML.", L"Q-XML-ANALYSIS");
		return report;
	}
}

CString FormatReport(const Report& report)
{
	CString text; text.Format(L"Расширенная проверка FB2\r\nОшибки: %d\r\nПредупреждения: %d\r\n\r\n", report.ErrorCount(), report.WarningCount());
	for (const Issue& issue : report.issues) { text += issue.severity == Severity::Error ? L"[Ошибка] " : L"[Предупреждение] "; text += issue.message + L"\r\n"; }
	if (report.issues.empty()) text += L"Проблем не обнаружено.\r\n";
	return text;
}

int ShowReport(HWND parent, const Report& report)
{
	ResultsDialog dialog(report);
	dialog.DoModal(parent);
	return dialog.selected;
}

bool ResolveSourceRange(const Issue& issue, const CString& currentSource, SourceRange& range)
{
	range = SourceRange();
	if (issue.start < 0 || issue.end <= issue.start || issue.code.IsEmpty()) return false;
	const Report current = Check(currentSource);
	const Issue* match = nullptr;
	for (const Issue& candidate : current.issues) {
		if (candidate.code != issue.code || candidate.elementPath != issue.elementPath ||
			candidate.attributeName != issue.attributeName || candidate.attributeValue != issue.attributeValue)
			continue;
		if (issue.code == L"Q-XML-PARSE" && candidate.message != issue.message) continue;
		if (match) return false;
		match = &candidate;
	}
	if (!match || match->start < 0 || match->end <= match->start || match->end > currentSource.GetLength()) return false;
	range.start = match->start;
	range.end = match->end;
	return true;
}
}
