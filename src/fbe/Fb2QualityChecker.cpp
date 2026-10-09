#include "stdafx.h"
#include "Fb2QualityChecker.h"
#include "resource.h"
#include "RuntimeLocalization.h"
#include "ThemeManager.h"
#include "UiMetrics.h"
#include "../common/ModernFileDialog.h"
#include "utils/utils.h"
#include "../version.h"
#include <algorithm>
#include <cwctype>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <utility>

namespace Fb2Quality {
namespace {
using Node = MSXML2::IXMLDOMNodePtr;

struct AnalysisCancelled {};

void CheckCancellation(const std::atomic_bool* requested)
{
	if (requested && requested->load(std::memory_order_relaxed)) throw AnalysisCancelled();
}

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

Base64Result CheckBase64(const Node& binary, const std::atomic_bool* cancelRequested)
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
		if (child->nodeType != MSXML2::NODE_TEXT && child->nodeType != MSXML2::NODE_CDATA_SECTION) return { true, false };
		MSXML2::IXMLDOMCharacterDataPtr characters(child);
		const long length = characters->length;
		for (long offset = 0; offset < length; offset += 4096) {
			CheckCancellation(cancelRequested);
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

CString RuleMessage(const wchar_t* code, const CString& fallback, const CString& parameter = CString())
{
	CString suffix(code);
	if (suffix.Left(2) == L"Q-") suffix = suffix.Mid(2);
	suffix.MakeLower();
	CString key = L"fbe.quality.rule." + suffix;
	const CString pattern = FbeLoadRuntimeStringByKey(key, fallback);
	if (pattern.Find(L"%s") < 0) return pattern;
	CString result;
	result.Format(pattern, parameter.GetString());
	return result;
}

Category CategoryForCode(const CString& code)
{
	if (code.Find(L"Q-LINK-") == 0) return Category::Links;
	if (code.Find(L"Q-NOTE-") == 0) return Category::Notes;
	if (code.Find(L"Q-IMAGE-") == 0 || code.Find(L"Q-BINARY-") == 0) return Category::Images;
	if (code.Find(L"Q-STRUCTURE-") == 0) return Category::Structure;
	if (code.Find(L"Q-METADATA-") == 0) return Category::Metadata;
	return Category::Xml;
}

void FillIssueHelp(Issue& issue)
{
	const wchar_t* suffix = L"xml";
	const wchar_t* detail = L"The XML document could not be analyzed reliably.";
	const wchar_t* action = L"Correct the XML syntax and run the check again.";
	switch (issue.category) {
	case Category::Links: suffix = L"links"; detail = L"An internal reference is missing, invalid, or ambiguous."; action = L"Use a unique existing target ID in the link."; break;
	case Category::Notes: suffix = L"notes"; detail = L"A note reference or the links between notes need attention."; action = L"Point to a note section and remove circular note references."; break;
	case Category::Images: suffix = L"images"; detail = L"The referenced image data or its declaration needs attention."; action = L"Check the binary ID, Base64 data, and content-type."; break;
	case Category::Structure: suffix = L"structure"; detail = L"A structural element is missing or has no meaningful content."; action = L"Add the required element or meaningful content."; break;
	case Category::Metadata: suffix = L"metadata"; detail = L"A book metadata field is missing or has a suspicious value."; action = L"Review title-info and correct the named field."; break;
	default: break;
	}
	issue.details = FbeLoadRuntimeStringByKey(CString(L"fbe.quality.detail.") + suffix, detail);
	issue.recommendation = FbeLoadRuntimeStringByKey(CString(L"fbe.quality.action.") + suffix, action);
}

void Add(Report& report, Severity severity, const CString& message, const wchar_t* code)
{
	Issue issue = { severity, message };
	issue.code = code;
	if (issue.code != L"Q-XML-PARSE") issue.message = RuleMessage(code, message);
	issue.category = CategoryForCode(issue.code);
	FillIssueHelp(issue);
	report.issues.push_back(issue);
}

void PopulateLocations(Report& report, const CString& xml, const std::atomic_bool* cancelRequested = nullptr)
{
	std::vector<int> starts{ 0 };
	for (int i = 0; i < xml.GetLength(); ++i) {
		if ((i & 4095) == 0) CheckCancellation(cancelRequested);
		if (xml[i] == L'\n') starts.push_back(i + 1);
	}
	for (Issue& issue : report.issues) {
		if (issue.start < 0 || issue.start >= xml.GetLength()) continue;
		const auto line = std::upper_bound(starts.begin(), starts.end(), issue.start);
		issue.line = static_cast<int>(line - starts.begin());
		issue.column = issue.start - *(line - 1) + 1;
	}
}

// MSXML validates the XML and supplies the semantic DOM, but does not retain source
// spans. This scanner only indexes the original start tags in DOM preorder. It
// never interprets XML structure or entities; any mismatch disables navigation.
struct SourceIndexer {
	struct AttributeSpan { CString name; int start; int end; };
	struct TagSpan { int start = -1; int end = -1; std::vector<AttributeSpan> attributes; };
	const CString& xml;
	const std::atomic_bool* cancelRequested;
	int cursor = 0;
	bool valid = true;
	explicit SourceIndexer(const CString& source, const std::atomic_bool* requested) : xml(source), cancelRequested(requested) {}

	bool Next(const CString& expectedName, TagSpan& span)
	{
		if (!valid) return false;
		const int length = xml.GetLength();
		while (cursor < length) {
			CheckCancellation(cancelRequested);
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
	const std::atomic_bool* cancelRequested;
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
	explicit Scan(const CString& xml, const std::atomic_bool* requested) : index(xml, requested), cancelRequested(requested) {}

	void AddAt(Severity severity, const wchar_t* code, const CString& message,
		const std::vector<int>& path, const CString& attributeName = CString(), const CString& attributeValue = CString(),
		const CString& messageParameter = CString())
	{
		Issue issue = { severity, message };
		issue.code = code;
		issue.message = RuleMessage(code, message, messageParameter.IsEmpty() ? attributeValue : messageParameter);
		issue.category = CategoryForCode(issue.code);
		FillIssueHelp(issue);
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
		struct Frame { Node node; bool inNotes; bool inTitleInfo; bool inMainBody; CString sourceNoteId; std::vector<int> path; };
		std::vector<Frame> pending;
		pending.push_back({ root, initialNotes, initialTitleInfo, false, CString(), rootPath });
		while (!pending.empty()) {
			CheckCancellation(cancelRequested);
			Frame frame(std::move(pending.back()));
			pending.pop_back();
			const Node& node = frame.node;
			bool inNotes = frame.inNotes;
			bool inTitleInfo = frame.inTitleInfo;
			bool inMainBody = frame.inMainBody;
			CString sourceNoteId(frame.sourceNoteId);
			const std::vector<int>& path = frame.path;
			if (node->nodeType != MSXML2::NODE_ELEMENT) continue;
			SourceIndexer::TagSpan span;
			if (index.Next(CString(static_cast<const wchar_t*>(_bstr_t(node->nodeName))), span)) spans[path] = span;
			const CString name = Name(node);
			const CString parentName = node->parentNode ? Name(node->parentNode) : CString();
			if ((name == L"section" && parentName != L"body" && parentName != L"section") ||
				(name == L"binary" && parentName != L"FictionBook") ||
				(name == L"body" && parentName != L"FictionBook"))
				AddAt(Severity::Error, L"Q-STRUCTURE-NESTING", L"Element is in an invalid position", path,
					CString(), CString(), name);
			const CString id = Attribute(node, L"id");
			if (!id.IsEmpty()) {
				if (!ids.insert(std::wstring(id.GetString())).second) {
					duplicateIds.insert(std::wstring(id.GetString()));
					if (name != L"binary" || binaries.find(std::wstring(id.GetString())) == binaries.end()) {
						CString message; message.Format(L"Duplicate identifier %s", id.GetString());
						AddAt(Severity::Error, L"Q-LINK-DUPLICATE-ID", message, path, L"id", id);
					}
				}
				if (inNotes && name == L"section") { noteIds.insert(std::wstring(id.GetString())); sourceNoteId = id; }
			}
			if (name == L"description") { description = true; if (descriptionPath.empty()) descriptionPath = path; }
			if (name == L"body") {
				inMainBody = !body;
				body = true;
				const CString bodyName = Attribute(node, L"name");
				inNotes = bodyName.CompareNoCase(L"notes") == 0 || bodyName.CompareNoCase(L"comments") == 0;
				sourceNoteId.Empty();
				if (inMainBody) bodyPath = path;
				else if (bodyName.IsEmpty())
					AddAt(Severity::Info, L"Q-STRUCTURE-UNNAMED-AUX-BODY", L"Additional body has no name", path);
			}
			if (name == L"section" && inMainBody) bodySection = true;
			if (name == L"title-info") { titleInfo = true; inTitleInfo = true; if (titleInfoPath.empty()) titleInfoPath = path; }
			if (inTitleInfo && (name == L"book-title" || name == L"lang")) {
				CString content(static_cast<const wchar_t*>(_bstr_t(node->text)));
				content.Trim();
				if (name == L"book-title") {
					bookTitle = true;
					if (report.title.IsEmpty()) report.title = content;
					if (content.IsEmpty()) AddAt(Severity::Warning, L"Q-METADATA-BOOK-TITLE", L"Book title is missing", path);
				}
				if (name == L"lang") {
					language = true;
					if (content.IsEmpty()) AddAt(Severity::Warning, L"Q-METADATA-LANGUAGE", L"Document language is missing", path);
					else if (!PlausibleLanguageCode(content)) AddAt(Severity::Warning, L"Q-METADATA-INVALID-LANG", L"Suspicious document language code", path);
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
				if (!named) AddAt(Severity::Warning, L"Q-METADATA-EMPTY-AUTHOR", L"Author information is empty", path);
			}
			if (inTitleInfo && name == L"sequence") {
				CString sequenceName = Attribute(node, L"name");
				sequenceName.Trim();
				if (sequenceName.IsEmpty()) AddAt(Severity::Warning, L"Q-METADATA-EMPTY-SEQUENCE", L"Series name is missing", path, L"name");
				MSXML2::IXMLDOMElementPtr element(node);
				if (element->getAttributeNode(L"number")) {
					const CString number = Attribute(node, L"number");
					if (!IntegerSyntax(number)) AddAt(Severity::Warning, L"Q-METADATA-SEQUENCE-NUMBER", L"Invalid series number", path, L"number", number);
				}
			}
			if (name == L"binary") {
				if (id.IsEmpty()) AddAt(Severity::Error, L"Q-BINARY-MISSING-ID", L"Binary has no ID", path, L"id");
				else {
					if (!binaries.insert(std::wstring(id.GetString())).second)
						AddAt(Severity::Error, L"Q-BINARY-DUPLICATE-ID", L"Duplicate binary ID " + id, path, L"id", id);
					binaryPaths.emplace(std::wstring(id.GetString()), path);
				}
				const CString contentType = Attribute(node, L"content-type");
				if (contentType.IsEmpty()) AddAt(Severity::Error, L"Q-BINARY-MISSING-MIME", L"Binary has no content-type", path, L"content-type");
				else if (contentType.Find(L'/') <= 0 || contentType.Right(1) == L"/")
					AddAt(Severity::Error, L"Q-BINARY-INVALID-MIME", L"Invalid binary content-type", path, L"content-type", contentType);
				const Base64Result base64 = CheckBase64(node, cancelRequested);
				if (!base64.hasData) AddAt(Severity::Error, L"Q-BINARY-EMPTY", L"Binary content is empty", path);
				else if (!base64.valid) AddAt(Severity::Error, L"Q-BINARY-INVALID-BASE64", L"Invalid Base64 data in binary", path);
				else if (ImageMimeMismatch(contentType, base64))
					AddAt(Severity::Error, L"Q-BINARY-MIME-MISMATCH", L"Binary format does not match content-type", path, L"content-type", contentType);
			}
			if (name == L"a" || name == L"image") {
				const HrefAttribute href = XLinkHref(node);
				const bool note = name == L"a" && Attribute(node, L"type").CompareNoCase(L"note") == 0;
				if (href.present && !href.value.IsEmpty() && href.value[0] == L'#') {
					if (ValidLocalHref(href.value)) {
						links.push_back({ href.value.Mid(1), href.value, href.name, sourceNoteId, path, note, name == L"image" });
						if (name == L"image") usedBinaries.insert(std::wstring(href.value.Mid(1).GetString()));
					} else AddAt(Severity::Error, name == L"image" ? L"Q-IMAGE-INVALID-HREF" : L"Q-LINK-INVALID-HREF",
						L"Invalid internal link", path, href.name, href.value);
				} else if (name == L"image")
					AddAt(Severity::Error, L"Q-IMAGE-NONLOCAL", L"Image must link to an internal binary", path, href.name, href.value);
				else if (note)
					AddAt(Severity::Error, L"Q-NOTE-NONLOCAL", L"Note link must be internal", path, href.name, href.value);
			}
			if ((name == L"p" || name == L"subtitle" || name == L"title" || name == L"cite") && !MeaningfulContent(node)) {
				if (name != L"p" || (parentName != L"title" && parentName != L"cite")) {
					const wchar_t* code = name == L"p" ? L"Q-STRUCTURE-EMPTY-P" : name == L"subtitle" ? L"Q-STRUCTURE-EMPTY-SUBTITLE" :
						name == L"title" ? L"Q-STRUCTURE-EMPTY-TITLE" : L"Q-STRUCTURE-EMPTY-CITE";
					AddAt(Severity::Warning, code, L"Suspicious empty element " + name, path);
				}
			}
			if (name == L"section" && !HasDirectChild(node, L"section") && !MeaningfulContent(node))
				AddAt(Severity::Warning, L"Q-STRUCTURE-EMPTY-SECTION", L"Empty section", path);
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
					pending.push_back({ child, inNotes, inTitleInfo, inMainBody, sourceNoteId, std::move(childPath) });
				}
			}
		}
	}

	void AddNoteCycles()
	{
		CheckCancellation(cancelRequested);
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
			CheckCancellation(cancelRequested);
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
			CheckCancellation(cancelRequested);
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
				AddAt(Severity::Warning, L"Q-NOTE-SELF-REFERENCE", L"Note refers to itself", link.path, link.attributeName, link.sourceValue);
			else if (component[source->second] == component[target->second] && sizes[component[source->second]] > 1)
				AddAt(Severity::Warning, L"Q-NOTE-CYCLE", L"Cycle between notes", link.path, link.attributeName, link.sourceValue);
		}
	}

	Report Finish()
	{
		CheckCancellation(cancelRequested);
		const std::vector<int> rootPath{ 0 };
		if (!description) AddAt(Severity::Error, L"Q-STRUCTURE-DESCRIPTION", L"Missing description element", rootPath);
		if (!body) AddAt(Severity::Error, L"Q-STRUCTURE-BODY", L"Missing body element", rootPath);
		else if (!bodySection) AddAt(Severity::Error, L"Q-STRUCTURE-BODY-SECTION", L"Main body has no section", bodyPath.empty() ? rootPath : bodyPath);
		if (!titleInfo) AddAt(Severity::Error, L"Q-METADATA-TITLE-INFO", L"Missing title-info", descriptionPath.empty() ? rootPath : descriptionPath);
		const std::vector<int>& metadataPath = titleInfoPath.empty() ? (descriptionPath.empty() ? rootPath : descriptionPath) : titleInfoPath;
		if (!bookTitle) AddAt(Severity::Warning, L"Q-METADATA-BOOK-TITLE", L"Book title is missing", metadataPath);
		if (!language) AddAt(Severity::Warning, L"Q-METADATA-LANGUAGE", L"Document language is missing", metadataPath);
		if (!author) AddAt(Severity::Warning, L"Q-METADATA-AUTHOR", L"Author is missing", metadataPath);
		for (const Link& link : links) {
			CheckCancellation(cancelRequested);
			const std::wstring target(link.href.GetString());
			if (link.note && noteIds.find(target) == noteIds.end()) {
				const bool missing = ids.find(target) == ids.end();
				CString message;
				message.Format(missing ? L"Note target #%s was not found" : L"Note link #%s points outside a notes section",
					link.href.GetString());
				AddAt(Severity::Error, missing ? L"Q-NOTE-MISSING" : L"Q-NOTE-WRONG-TARGET",
					message, link.path, link.attributeName, link.sourceValue);
			} else if (!link.image && ids.find(target) == ids.end()) {
				CString message; message.Format(L"Link target #%s was not found", link.href.GetString());
				AddAt(Severity::Error, L"Q-LINK-MISSING", message, link.path, link.attributeName, link.sourceValue);
			} else if (link.image && binaries.find(target) == binaries.end()) {
				CString message; message.Format(L"Image binary #%s is missing", link.href.GetString());
				AddAt(Severity::Error, L"Q-IMAGE-MISSING-BINARY", message, link.path, link.attributeName, link.sourceValue);
			}
		}
		AddNoteCycles();
		for (const std::wstring& id : binaries) if (usedBinaries.find(id) == usedBinaries.end()) {
			CheckCancellation(cancelRequested);
			CString message; message.Format(L"Binary %s is not used", id.c_str());
			AddAt(Severity::Warning, L"Q-BINARY-UNUSED", message, binaryPaths[id], L"id", id.c_str());
		}
		if (!index.valid) for (Issue& issue : report.issues) issue.start = issue.end = -1;
		return report;
	}
};

CString IssueCategoryText(const Issue& issue)
{
	const wchar_t* key = L"fbe.quality.category.xml";
	const wchar_t* fallback = L"XML";
	switch (issue.category) {
	case Category::Links: key = L"fbe.quality.category.links"; fallback = L"Links"; break;
	case Category::Notes: key = L"fbe.quality.category.notes"; fallback = L"Notes"; break;
	case Category::Images: key = L"fbe.quality.category.images"; fallback = L"Images"; break;
	case Category::Structure: key = L"fbe.quality.category.structure"; fallback = L"Structure"; break;
	case Category::Metadata: key = L"fbe.quality.category.metadata"; fallback = L"Metadata"; break;
	default: break;
	}
	return FbeLoadRuntimeStringByKey(key, fallback);
}

CString IssueSeverityText(Severity severity)
{
	if (severity == Severity::Error) return FbeLoadRuntimeStringByKey(L"fbe.quality.severity.error", L"Error");
	if (severity == Severity::Warning) return FbeLoadRuntimeStringByKey(L"fbe.quality.severity.warning", L"Warning");
	return FbeLoadRuntimeStringByKey(L"fbe.quality.severity.info", L"Information");
}

bool SaveUtf8Report(const CString& path, const CString& content, bool bom, DWORD& error)
{
	error = ERROR_SUCCESS;
	const int length = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, content, content.GetLength(), NULL, 0, NULL, NULL);
	if (length <= 0) { error = ::GetLastError(); return false; }
	std::vector<char> bytes(static_cast<size_t>(length));
	if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, content, content.GetLength(), bytes.data(), length, NULL, NULL) != length) {
		error = ::GetLastError(); return false;
	}
	const auto extendedPath = [](const CString& value) -> CString {
		if(value.GetLength() < MAX_PATH || value.Left(4) == L"\\\\?\\") return value;
		CString absolute(value);
		if(value.Left(2) != L"\\\\" && (value.GetLength() < 2 || value[1] != L':')) {
			const DWORD needed = ::GetFullPathNameW(value, 0, NULL, NULL);
			if(needed == 0) return value;
			std::vector<wchar_t> buffer(static_cast<size_t>(needed) + 1);
			if(::GetFullPathNameW(value, static_cast<DWORD>(buffer.size()), buffer.data(), NULL) == 0) return value;
			absolute = buffer.data();
		}
		if(absolute.Left(2) == L"\\\\") return CString(L"\\\\?\\UNC\\") + absolute.Mid(2);
		return CString(L"\\\\?\\") + absolute;
	};
	const CString destination = extendedPath(path);
	const int slash = max(path.ReverseFind(L'\\'), path.ReverseFind(L'/'));
	const CString directory = slash >= 0 ? path.Left(slash + 1) : CString(L".\\");
	static std::atomic<unsigned> nextTemporary{ 0 };
	CString temporary;
	HANDLE output = INVALID_HANDLE_VALUE;
	for(int attempt = 0; attempt < 16; ++attempt) {
		CString name;
		name.Format(L".fbe-report-%08lx-%08lx-%08x.tmp", ::GetCurrentProcessId(), ::GetTickCount(), ++nextTemporary);
		temporary = extendedPath(directory + name);
		output = ::CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		if(output != INVALID_HANDLE_VALUE) break;
		if(::GetLastError() != ERROR_FILE_EXISTS) { error = ::GetLastError(); return false; }
	}
	if(output == INVALID_HANDLE_VALUE) { error = ERROR_FILE_EXISTS; return false; }
	DWORD written = 0;
	const char marker[] = "\xEF\xBB\xBF";
	const bool ok = (!bom || (::WriteFile(output, marker, 3, &written, NULL) && written == 3)) &&
		::WriteFile(output, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL) && written == bytes.size() &&
		::FlushFileBuffers(output);
	if (!ok) { error = ::GetLastError(); if (error == ERROR_SUCCESS) error = ERROR_WRITE_FAULT; }
	::CloseHandle(output);
	if (ok && ::MoveFileExW(temporary, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
	if (ok) error = ::GetLastError();
	::DeleteFileW(temporary);
	return false;
}

class ReportSaveEvents : public IFileDialogEvents {
    std::atomic<ULONG> m_references{ 1 };
public:
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
		if (!object) return E_POINTER;
		*object = NULL;
		if (iid == IID_IUnknown || iid == IID_IFileDialogEvents) { *object = static_cast<IFileDialogEvents*>(this); AddRef(); return S_OK; }
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return ++m_references; }
	ULONG STDMETHODCALLTYPE Release() override { return --m_references; }
	HRESULT STDMETHODCALLTYPE OnFileOk(IFileDialog*) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnFolderChanging(IFileDialog*, IShellItem*) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnFolderChange(IFileDialog*) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnSelectionChange(IFileDialog*) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnShareViolation(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE* response) override {
		if (response) *response = FDESVR_DEFAULT;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE OnTypeChange(IFileDialog* dialog) override {
		UINT index = 1;
		return SUCCEEDED(dialog->GetFileTypeIndex(&index)) ? dialog->SetDefaultExtension(index == 2 ? L"html" : L"txt") : S_OK;
	}
	HRESULT STDMETHODCALLTYPE OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE* response) override {
		if (response) *response = FDEOR_DEFAULT;
		return S_OK;
	}
};

class ResultsDialog : public CDialogImpl<ResultsDialog> {
public:
	enum { IDD = IDD_FB2_QUALITY_RESULTS };
	explicit ResultsDialog(const Report& report, UINT initialSaveFilter = 1) : m_report(report), m_initialSaveFilter(initialSaveFilter) {}
	int selected = -1;
	BEGIN_MSG_MAP(ResultsDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInit)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_GETMINMAXINFO, OnMinMax)
		MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_NCDESTROY, OnNcDestroy)
		COMMAND_ID_HANDLER(IDC_FB2_QUALITY_GOTO, OnGoTo)
		COMMAND_ID_HANDLER(IDC_FB2_QUALITY_COPY, OnCopy)
		COMMAND_ID_HANDLER(IDC_FB2_QUALITY_SAVE, OnSave)
		COMMAND_ID_HANDLER(IDCANCEL, OnClose)
		NOTIFY_HANDLER(IDC_FB2_QUALITY_LIST, NM_DBLCLK, OnActivate)
		NOTIFY_HANDLER(IDC_FB2_QUALITY_LIST, LVN_ITEMCHANGED, OnSelectionChanged)
		NOTIFY_HANDLER(IDC_FB2_QUALITY_LIST, LVN_COLUMNCLICK, OnColumnClick)
	END_MSG_MAP()
private:
	const Report& m_report;
	UINT m_initialSaveFilter = 1;
	int m_sortColumn = -1;
	bool m_sortDescending = false;
	HFONT m_font = NULL;
	HWND m_emptyMessage = NULL;
	UINT m_columnDpi = 96;
	void FitInitialColumns() {
		CListViewCtrl list = GetDlgItem(IDC_FB2_QUALITY_LIST);
		RECT client = {}; list.GetClientRect(&client);
		HDC dc = ::GetDC(list);
		HFONT font = reinterpret_cast<HFONT>(::SendMessageW(list, WM_GETFONT, 0, 0));
		HFONT previous = dc && font ? reinterpret_cast<HFONT>(::SelectObject(dc, font)) : NULL;
		const auto textWidth = [dc](const CString& value) -> int {
			SIZE size = {};
			if(dc) ::GetTextExtentPoint32W(dc, value, value.GetLength(), &size);
			return size.cx;
		};
		int typeWidth = textWidth(FbeLoadRuntimeStringByKey(L"fbe.quality.column.type", L"Type"));
		for(Severity severity : { Severity::Error, Severity::Warning, Severity::Info })
			typeWidth = max(typeWidth, textWidth(SeverityText(severity)));
		int codeWidth = textWidth(FbeLoadRuntimeStringByKey(L"fbe.quality.column.code", L"Code"));
		int categoryWidth = textWidth(FbeLoadRuntimeStringByKey(L"fbe.quality.column.category", L"Category"));
		for(const Issue& issue : m_report.issues) {
			codeWidth = max(codeWidth, textWidth(issue.code));
			categoryWidth = max(categoryWidth, textWidth(CategoryText(issue)));
		}
		const int locationWidth = textWidth(FbeLoadRuntimeStringByKey(L"fbe.quality.column.location", L"Location"));
		if(previous) ::SelectObject(dc, previous);
		if(dc) ::ReleaseDC(list, dc);
		const int padding = UiMetrics::ScaleForDpi(24, m_columnDpi);
		const int type = max(UiMetrics::ScaleForDpi(90, m_columnDpi), min(typeWidth + padding, UiMetrics::ScaleForDpi(190, m_columnDpi)));
		const int code = max(UiMetrics::ScaleForDpi(170, m_columnDpi), min(codeWidth + padding, UiMetrics::ScaleForDpi(260, m_columnDpi)));
		const int category = max(UiMetrics::ScaleForDpi(100, m_columnDpi), min(categoryWidth + padding, UiMetrics::ScaleForDpi(170, m_columnDpi)));
		const int location = max(UiMetrics::ScaleForDpi(90, m_columnDpi), min(locationWidth + padding, UiMetrics::ScaleForDpi(150, m_columnDpi)));
		const int description = max(UiMetrics::ScaleForDpi(330, m_columnDpi),
			client.right - type - code - category - location - UiMetrics::ScaleForDpi(20, m_columnDpi));
		list.SetColumnWidth(0, type); list.SetColumnWidth(1, code); list.SetColumnWidth(2, category);
		list.SetColumnWidth(3, description); list.SetColumnWidth(4, location);
	}
	void ButtonWidths(int (&widths)[4]) const {
		const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
		const int ids[] = { IDC_FB2_QUALITY_GOTO, IDC_FB2_QUALITY_COPY, IDC_FB2_QUALITY_SAVE, IDCANCEL };
		HDC dc = ::GetDC(m_hWnd);
		HFONT oldFont = dc ? reinterpret_cast<HFONT>(::SelectObject(dc, m_font ? m_font : reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)))) : NULL;
		for (int i = 0; i < 4; ++i) {
			wchar_t label[256] = {};
			::GetWindowTextW(GetDlgItem(ids[i]), label, _countof(label));
			SIZE textSize = {};
			if (dc) ::GetTextExtentPoint32W(dc, label, static_cast<int>(wcslen(label)), &textSize);
			widths[i] = max(UiMetrics::ScaleForDpi(62, dpi), textSize.cx + UiMetrics::ScaleForDpi(28, dpi));
		}
		if (dc) { ::SelectObject(dc, oldFont); ::ReleaseDC(m_hWnd, dc); }
	}
	int MinimumWindowWidth() const {
		const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
		int widths[4]; ButtonWidths(widths);
		RECT window = {}, client = {}; ::GetWindowRect(m_hWnd, &window); ::GetClientRect(m_hWnd, &client);
		const int nonClient = window.right - window.left - (client.right - client.left);
		return max(UiMetrics::ScaleForDpi(550, dpi), widths[0] + widths[1] + widths[2] + widths[3] +
			UiMetrics::ScaleForDpi(3 * 5 + 2 * 8, dpi) + nonClient);
	}
	void ApplyDpiFont(UINT dpi) {
		HFONT font = UiMetrics::CreateDialogFontForDpi(dpi);
		if (!font) return;
		::SendMessageW(m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
		const int ids[] = { IDC_FB2_QUALITY_SUMMARY, IDC_FB2_QUALITY_LIST, IDC_FB2_QUALITY_DETAILS,
			IDC_FB2_QUALITY_GOTO, IDC_FB2_QUALITY_COPY, IDC_FB2_QUALITY_SAVE, IDCANCEL };
		for (int id : ids) ::SendMessageW(GetDlgItem(id), WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
		if (m_font) ::DeleteObject(m_font);
		m_font = font;
	}
	CString GeometryPath() const { return U::GetSettingsDir() + L"QualityChecker.ini"; }
	void RestoreGeometry() {
		const CString path = GeometryPath();
		int width = ::GetPrivateProfileIntW(L"Results", L"Width", 0, path);
		int height = ::GetPrivateProfileIntW(L"Results", L"Height", 0, path);
		if (width <= 0 || height <= 0) return;
		RECT current = {}; GetWindowRect(&current);
		const int left = ::GetPrivateProfileIntW(L"Results", L"Left", current.left, path);
		const int top = ::GetPrivateProfileIntW(L"Results", L"Top", current.top, path);
		RECT wanted = { left, top, left + width, top + height };
		MONITORINFO monitor = { sizeof(monitor) };
		if (!::GetMonitorInfoW(::MonitorFromRect(&wanted, MONITOR_DEFAULTTONEAREST), &monitor)) return;
		const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
		const int minimumWidth = MinimumWindowWidth();
		const int minimumHeight = UiMetrics::ScaleForDpi(310, dpi);
		width = min(max(width, minimumWidth), monitor.rcWork.right - monitor.rcWork.left);
		height = min(max(height, minimumHeight), monitor.rcWork.bottom - monitor.rcWork.top);
		const int x = min(max(left, monitor.rcWork.left), monitor.rcWork.right - width);
		const int y = min(max(top, monitor.rcWork.top), monitor.rcWork.bottom - height);
		SetWindowPos(NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	}
	void SaveGeometry() {
		RECT rect = {}; GetWindowRect(&rect);
		const CString path = GeometryPath();
		const struct { LPCWSTR key; int value; } fields[] = {
			{ L"Left", rect.left }, { L"Top", rect.top },
			{ L"Width", rect.right - rect.left }, { L"Height", rect.bottom - rect.top }
		};
		for (const auto& field : fields) {
			CString value; value.Format(L"%d", field.value);
			::WritePrivateProfileStringW(L"Results", field.key, value, path);
		}
	}
	static CString SeverityText(Severity severity) {
		return IssueSeverityText(severity);
	}
	static CString CategoryText(const Issue& issue) {
		return IssueCategoryText(issue);
	}
	CString LocationText(const Issue& issue) const {
		if (issue.line < 0) return FbeLoadRuntimeStringByKey(L"fbe.quality.location.unknown", L"Unknown");
		CString value; value.Format(L"%d:%d", issue.line, issue.column); return value;
	}
	void Layout() {
		if (!GetDlgItem(IDC_FB2_QUALITY_LIST)) return;
		RECT client = {}; GetClientRect(&client);
		const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
		const int margin = UiMetrics::ScaleForDpi(8, dpi), gap = UiMetrics::ScaleForDpi(5, dpi);
		const int summaryHeight = UiMetrics::ScaleForDpi(19, dpi), detailsHeight = UiMetrics::ScaleForDpi(69, dpi);
		const int buttonHeight = UiMetrics::ScaleForDpi(25, dpi);
		int buttonWidths[4]; ButtonWidths(buttonWidths);
		const int width = max(0, client.right - 2 * margin);
		const int buttonY = client.bottom - margin - buttonHeight;
		const int detailsY = buttonY - gap - detailsHeight;
		const int listY = margin + summaryHeight + gap;
		::SetWindowPos(GetDlgItem(IDC_FB2_QUALITY_SUMMARY), NULL, margin, margin, width, summaryHeight, SWP_NOZORDER | SWP_NOACTIVATE);
		::SetWindowPos(GetDlgItem(IDC_FB2_QUALITY_LIST), NULL, margin, listY, width, max(0, detailsY - gap - listY), SWP_NOZORDER | SWP_NOACTIVATE);
		if (m_emptyMessage) ::SetWindowPos(m_emptyMessage, HWND_TOP, margin + gap, listY + UiMetrics::ScaleForDpi(32, dpi),
			max(0, width - 2 * gap), UiMetrics::ScaleForDpi(30, dpi), SWP_NOACTIVATE);
		::SetWindowPos(GetDlgItem(IDC_FB2_QUALITY_DETAILS), NULL, margin, detailsY, width, detailsHeight, SWP_NOZORDER | SWP_NOACTIVATE);
		const int ids[] = { IDC_FB2_QUALITY_GOTO, IDC_FB2_QUALITY_COPY, IDC_FB2_QUALITY_SAVE, IDCANCEL };
		int x = client.right - margin;
		for (int i = 3; i >= 0; --i) {
			x -= UiMetrics::ScaleForDpi(buttonWidths[i], dpi);
			::SetWindowPos(GetDlgItem(ids[i]), NULL, x, buttonY, UiMetrics::ScaleForDpi(buttonWidths[i], dpi), buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
			x -= gap;
		}
	}
	LRESULT OnInit(UINT, WPARAM, LPARAM, BOOL&) {
		::SetPropW(m_hWnd, L"FBE_SKIP_SYSTEM_DIALOG_LOCALIZATION", reinterpret_cast<HANDLE>(1));
		ApplyDpiFont(UiMetrics::DpiForWindow(m_hWnd));
		SetWindowText(FbeLoadRuntimeStringByKey(L"fbe.quality.caption", L"FB2 quality check"));
		SetDlgItemText(IDC_FB2_QUALITY_GOTO, FbeLoadRuntimeStringByKey(L"fbe.quality.goto", L"Go to"));
		SetDlgItemText(IDC_FB2_QUALITY_COPY, FbeLoadRuntimeStringByKey(L"fbe.quality.copy", L"Copy"));
		SetDlgItemText(IDC_FB2_QUALITY_SAVE, FbeLoadRuntimeStringByKey(L"fbe.quality.save", L"Save report"));
		SetDlgItemText(IDCANCEL, FbeLoadRuntimeStringByKey(L"fbe.quality.close", L"Close"));
		CString summary; summary.Format(FbeLoadRuntimeStringByKey(L"fbe.quality.summary", L"Errors: %d     Warnings: %d     Information: %d"), m_report.ErrorCount(), m_report.WarningCount(), m_report.InfoCount());
		SetDlgItemText(IDC_FB2_QUALITY_SUMMARY, summary);
		CListViewCtrl list = GetDlgItem(IDC_FB2_QUALITY_LIST);
		list.ModifyStyle(0, WS_CLIPSIBLINGS);
		list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
		const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
		m_columnDpi = dpi;
		list.InsertColumn(0, FbeLoadRuntimeStringByKey(L"fbe.quality.column.type", L"Type"), LVCFMT_LEFT, UiMetrics::ScaleForDpi(90, dpi));
		list.InsertColumn(1, FbeLoadRuntimeStringByKey(L"fbe.quality.column.code", L"Code"), LVCFMT_LEFT, UiMetrics::ScaleForDpi(170, dpi));
		list.InsertColumn(2, FbeLoadRuntimeStringByKey(L"fbe.quality.column.category", L"Category"), LVCFMT_LEFT, UiMetrics::ScaleForDpi(100, dpi));
		list.InsertColumn(3, FbeLoadRuntimeStringByKey(L"fbe.quality.column.description", L"Description"), LVCFMT_LEFT, UiMetrics::ScaleForDpi(330, dpi));
		list.InsertColumn(4, FbeLoadRuntimeStringByKey(L"fbe.quality.column.location", L"Location"), LVCFMT_LEFT, UiMetrics::ScaleForDpi(90, dpi));
		for (size_t i = 0; i < m_report.issues.size(); ++i) {
			const Issue& issue = m_report.issues[i];
			const int row = list.InsertItem(static_cast<int>(i), SeverityText(issue.severity));
			list.SetItemData(row, static_cast<DWORD_PTR>(i));
			list.SetItemText(row, 1, issue.code);
			list.SetItemText(row, 2, CategoryText(issue));
			list.SetItemText(row, 3, issue.message);
			list.SetItemText(row, 4, LocationText(issue));
		}
		if (!m_report.issues.empty()) list.SelectItem(0);
		else {
			m_emptyMessage = ::CreateWindowExW(0, L"STATIC",
				FbeLoadRuntimeStringByKey(L"fbe.quality.report.clean", L"No issues found."),
				WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 0, 0, m_hWnd,
				reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FB2_QUALITY_EMPTY)), _Module.GetResourceInstance(), NULL);
			if (m_emptyMessage && m_font) ::SendMessageW(m_emptyMessage, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), FALSE);
		}
		UpdateGoTo();
		RestoreGeometry();
		Layout();
		FitInitialColumns();
		return TRUE;
	}
	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&) { Layout(); return 0; }
	LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&) { SaveGeometry(); ::RemovePropW(m_hWnd, L"FBE_SKIP_SYSTEM_DIALOG_LOCALIZATION"); return 0; }
	LRESULT OnNcDestroy(UINT, WPARAM, LPARAM, BOOL&) { if (m_font) { ::DeleteObject(m_font); m_font = NULL; } return 0; }
	LRESULT OnDpiChanged(UINT, WPARAM wParam, LPARAM lParam, BOOL&) {
		const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
		SetWindowPos(NULL, suggested->left, suggested->top, suggested->right - suggested->left,
			suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
		ApplyDpiFont(LOWORD(wParam));
		if (m_emptyMessage && m_font) ::SendMessageW(m_emptyMessage, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
		CListViewCtrl list = GetDlgItem(IDC_FB2_QUALITY_LIST);
		for(int column = 0; column < 5; ++column)
			list.SetColumnWidth(column, ::MulDiv(list.GetColumnWidth(column), LOWORD(wParam), m_columnDpi));
		m_columnDpi = LOWORD(wParam);
		Layout();
		return 0;
	}
	LRESULT OnMinMax(UINT, WPARAM, LPARAM parameter, BOOL&) {
		MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(parameter);
		const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
		info->ptMinTrackSize.x = MinimumWindowWidth();
		info->ptMinTrackSize.y = UiMetrics::ScaleForDpi(310, dpi);
		return 0;
	}
	void UpdateGoTo() {
		CListViewCtrl list = GetDlgItem(IDC_FB2_QUALITY_LIST);
		const int row = list.GetNextItem(-1, LVNI_SELECTED);
		const int index = row >= 0 ? static_cast<int>(list.GetItemData(row)) : -1;
		const bool available = index >= 0 && static_cast<size_t>(index) < m_report.issues.size() && m_report.issues[index].start >= 0;
		::EnableWindow(GetDlgItem(IDC_FB2_QUALITY_GOTO), available ? TRUE : FALSE);
		::EnableWindow(GetDlgItem(IDC_FB2_QUALITY_COPY), index >= 0 ? TRUE : FALSE);
		CString details;
		if (index >= 0 && static_cast<size_t>(index) < m_report.issues.size()) {
			const Issue& issue = m_report.issues[index];
			details = issue.message;
			if (!issue.details.IsEmpty()) details += L"\r\n" + issue.details;
			if (!issue.recommendation.IsEmpty()) details += L"\r\n" + issue.recommendation;
		}
		SetDlgItemText(IDC_FB2_QUALITY_DETAILS, details);
	}
	LRESULT OnSelectionChanged(int, LPNMHDR, BOOL&) { UpdateGoTo(); return 0; }
	static int CALLBACK CompareRows(LPARAM left, LPARAM right, LPARAM context) {
		const ResultsDialog* dialog = reinterpret_cast<const ResultsDialog*>(context);
		const Issue& a = dialog->m_report.issues[static_cast<size_t>(left)];
		const Issue& b = dialog->m_report.issues[static_cast<size_t>(right)];
		int result = 0;
		if (dialog->m_sortColumn == 0) result = static_cast<int>(a.severity) - static_cast<int>(b.severity);
		else if (dialog->m_sortColumn == 2) result = CategoryText(a).CompareNoCase(CategoryText(b));
		else if (dialog->m_sortColumn == 4) result = a.start == b.start ? 0 : a.start < b.start ? -1 : 1;
		if (result == 0) result = left == right ? 0 : left < right ? -1 : 1;
		return dialog->m_sortDescending ? -result : result;
	}
	LRESULT OnColumnClick(int, LPNMHDR header, BOOL&) {
		const int column = reinterpret_cast<NMLISTVIEW*>(header)->iSubItem;
		if (column != 0 && column != 2 && column != 4) return 0;
		m_sortDescending = m_sortColumn == column && !m_sortDescending;
		m_sortColumn = column;
		CListViewCtrl list = GetDlgItem(IDC_FB2_QUALITY_LIST);
		list.SortItems(CompareRows, reinterpret_cast<LPARAM>(this));
		return 0;
	}
	LRESULT OnGoTo(WORD, WORD, HWND, BOOL&) {
		CListViewCtrl list = GetDlgItem(IDC_FB2_QUALITY_LIST);
		const int row = list.GetNextItem(-1, LVNI_SELECTED);
		const int index = row >= 0 ? static_cast<int>(list.GetItemData(row)) : -1;
		if (index >= 0 && static_cast<size_t>(index) < m_report.issues.size()) selected = index;
		if (selected >= 0) EndDialog(IDOK);
		return 0;
	}
	LRESULT OnActivate(int, LPNMHDR, BOOL& handled) { BOOL ignored = FALSE; handled = TRUE; return OnGoTo(0, 0, NULL, ignored); }
	LRESULT OnCopy(WORD, WORD, HWND, BOOL&) {
		CListViewCtrl list = GetDlgItem(IDC_FB2_QUALITY_LIST);
		const int row = list.GetNextItem(-1, LVNI_SELECTED);
		if (row < 0) return 0;
		const Issue& issue = m_report.issues[static_cast<size_t>(list.GetItemData(row))];
		CString text = issue.code + L": " + issue.message;
		if (!issue.details.IsEmpty()) text += L"\r\n" + issue.details;
		if (!issue.recommendation.IsEmpty()) text += L"\r\n" + issue.recommendation;
		const SIZE_T bytes = static_cast<SIZE_T>(text.GetLength() + 1) * sizeof(wchar_t);
		HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (!memory) return 0;
		void* target = ::GlobalLock(memory);
		if (!target) { ::GlobalFree(memory); return 0; }
		memcpy(target, text.GetString(), bytes);
		::GlobalUnlock(memory);
		if (!::OpenClipboard(m_hWnd)) { ::GlobalFree(memory); return 0; }
		::EmptyClipboard();
		if (!::SetClipboardData(CF_UNICODETEXT, memory)) ::GlobalFree(memory);
		::CloseClipboard();
		return 0;
	}
	LRESULT OnSave(WORD, WORD, HWND, BOOL&) {
		const CString textLabel = FbeLoadRuntimeStringByKey(L"fbe.quality.filter.text", L"Text report (*.txt)");
		const CString htmlLabel = FbeLoadRuntimeStringByKey(L"fbe.quality.filter.html", L"HTML report (*.html)");
		const COMDLG_FILTERSPEC filters[] = { { textLabel, L"*.txt" }, { htmlLabel, L"*.html" } };
		ReportSaveEvents events;
		ModernFileDialog::Request request;
		request.save = true;
		request.pathMustExist = true;
		request.overwritePrompt = true;
		request.title = FbeLoadRuntimeStringByKey(L"fbe.quality.save", L"Save report").GetString();
		request.initialFileName = L"fb2-quality-report";
		request.defaultExtension = m_initialSaveFilter == 2 ? L"html" : L"txt";
		request.filters = filters;
		request.filterCount = _countof(filters);
		request.filterIndex = m_initialSaveFilter;
		request.events = &events;
		const ModernFileDialog::Result chosen = ModernFileDialog::Show(m_hWnd, request);
		if (chosen.outcome != ModernFileDialog::Outcome::Accepted) return 0;
		const CString destination(chosen.paths.front().c_str());
		const bool html = chosen.filterIndex == 2;
		DWORD error = ERROR_SUCCESS;
		if (!SaveReport(m_report, destination, html, error)) {
			CString message; message.Format(FbeLoadRuntimeStringByKey(L"fbe.quality.save.error", L"Unable to save report (Windows error %lu)."), error);
			LPWSTR reason = NULL;
			if (::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, error, 0, reinterpret_cast<LPWSTR>(&reason), 0, NULL) && reason) {
				message += L"\r\n"; message += reason; ::LocalFree(reason);
			}
			::MessageBoxW(m_hWnd, message, FbeLoadRuntimeStringByKey(L"fbe.quality.caption", L"FB2 quality check"), MB_ICONERROR);
		}
		return 0;
	}
	LRESULT OnClose(WORD, WORD, HWND, BOOL&) { EndDialog(IDCANCEL); return 0; }
};

struct AnalysisProgressState {
	const CString& xml;
	Report report;
	std::atomic_bool cancelRequested{ false };
	std::atomic_bool finished{ false };
	std::thread worker;
	CString cancellingText;
	bool startFailed = false;
	bool cancelledByUser = false;
	bool simulateCancel = false;
};

HRESULT CALLBACK AnalysisProgressCallback(HWND window, UINT notification, WPARAM button, LPARAM, LONG_PTR reference)
{
	AnalysisProgressState* state = reinterpret_cast<AnalysisProgressState*>(reference);
	if (notification == TDN_CREATED) {
		::SendMessageW(window, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 0);
		if (state->simulateCancel) ::PostMessageW(window, TDM_CLICK_BUTTON, IDCANCEL, 0);
		try {
			state->worker = std::thread([state, window]() {
				const HRESULT initialized = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
				try {
					if (SUCCEEDED(initialized)) state->report = Check(state->xml, &state->cancelRequested);
					else state->startFailed = true;
				} catch (...) { state->startFailed = true; }
				if (SUCCEEDED(initialized)) ::CoUninitialize();
				state->finished.store(true, std::memory_order_release);
				::PostMessageW(window, TDM_CLICK_BUTTON, IDCANCEL, 0);
			});
		} catch (...) {
			state->startFailed = true;
			state->finished.store(true, std::memory_order_release);
			::PostMessageW(window, TDM_CLICK_BUTTON, IDCANCEL, 0);
		}
	} else if (notification == TDN_BUTTON_CLICKED && button == IDCANCEL) {
		if (state->simulateCancel || !state->finished.load(std::memory_order_acquire)) {
			state->cancelledByUser = true;
			state->cancelRequested.store(true, std::memory_order_relaxed);
			if (!state->finished.load(std::memory_order_acquire)) {
				::SendMessageW(window, TDM_SET_ELEMENT_TEXT, TDE_CONTENT,
					reinterpret_cast<LPARAM>(state->cancellingText.GetString()));
				return S_FALSE;
			}
		}
	}
	return S_OK;
}
}

int Report::ErrorCount() const { int result = 0; for (const Issue& issue : issues) if (issue.severity == Severity::Error) ++result; return result; }
int Report::WarningCount() const { int result = 0; for (const Issue& issue : issues) if (issue.severity == Severity::Warning) ++result; return result; }
int Report::InfoCount() const { int result = 0; for (const Issue& issue : issues) if (issue.severity == Severity::Info) ++result; return result; }

bool AnalyzeWithProgressImpl(HWND parent, const CString& xml, Report& report, bool simulateCancel)
{
	const CString title = FbeLoadRuntimeStringByKey(L"fbe.quality.caption", L"FB2 quality check");
	const CString progress = FbeLoadRuntimeStringByKey(L"fbe.quality.analysis.progress", L"Analyzing the current document...");
	const CString cancel = FbeLoadRuntimeStringByKey(L"fbe.quality.analysis.cancel", L"Cancel");
	AnalysisProgressState state{ xml };
	state.simulateCancel = simulateCancel;
	state.cancellingText = FbeLoadRuntimeStringByKey(L"fbe.quality.analysis.cancelling", L"Cancelling analysis...");
	TASKDIALOG_BUTTON button = { IDCANCEL, cancel };
	TASKDIALOGCONFIG config = { sizeof(config) };
	config.hwndParent = parent;
	config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SHOW_MARQUEE_PROGRESS_BAR;
	config.pszWindowTitle = title;
	config.pszContent = progress;
	config.cButtons = 1;
	config.pButtons = &button;
	config.pfCallback = AnalysisProgressCallback;
	config.lpCallbackData = reinterpret_cast<LONG_PTR>(&state);
	int selected = IDCANCEL;
	const HRESULT result = ThemeManager::TaskDialogIndirect(config, &selected, NULL, NULL);
	if (state.worker.joinable()) state.worker.join();
	if (FAILED(result)) {
		report = Check(xml);
		return !report.cancelled;
	}
	if (state.cancelledByUser || state.report.cancelled) {
		report = Report();
		report.cancelled = true;
		return false;
	}
	if (state.startFailed) {
		report = Check(xml);
		return !report.cancelled;
	}
	report = std::move(state.report);
	return true;
}

bool AnalyzeWithProgress(HWND parent, const CString& xml, Report& report)
{
	return AnalyzeWithProgressImpl(parent, xml, report, false);
}

bool ProbeAnalysisCancellation(HWND parent, const CString& xml)
{
	Report report;
	return !AnalyzeWithProgressImpl(parent, xml, report, true) && report.cancelled && report.issues.empty();
}

Report Check(const CString& xml, const std::atomic_bool* cancelRequested)
{
	Report report;
	SYSTEMTIME checked = {}; ::GetLocalTime(&checked);
	report.checkedAt.Format(L"%04u-%02u-%02u %02u:%02u:%02u", checked.wYear, checked.wMonth,
		checked.wDay, checked.wHour, checked.wMinute, checked.wSecond);
	try {
	CheckCancellation(cancelRequested);
	MSXML2::IXMLDOMDocument2Ptr document;
	if (FAILED(document.CreateInstance(L"Msxml2.DOMDocument.6.0"))) { Add(report, Severity::Error, L"Unable to create XML analyzer", L"Q-XML-ANALYZER"); return report; }
	document->async = VARIANT_FALSE;
	document->validateOnParse = VARIANT_FALSE;
	document->resolveExternals = VARIANT_FALSE;
	document->setProperty(L"ProhibitDTD", _variant_t(VARIANT_TRUE));
	if (document->loadXML(_bstr_t(xml)) == VARIANT_FALSE) {
		CheckCancellation(cancelRequested);
		MSXML2::IXMLDOMParseErrorPtr error = document->parseError;
		CString message; message.Format(FbeLoadRuntimeStringByKey(L"fbe.quality.rule.xml-parse", L"Invalid XML at line %ld: %s"),
			error->line, static_cast<const wchar_t*>(_bstr_t(error->reason)));
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
		PopulateLocations(report, xml, cancelRequested);
		return report;
	}
	CheckCancellation(cancelRequested);
	Node root = document->documentElement;
	if (!root || Name(root) != L"FictionBook") { Add(report, Severity::Error, L"Root element must be FictionBook", L"Q-XML-ROOT"); return report; }
	if (CString(static_cast<const wchar_t*>(_bstr_t(root->namespaceURI))) != L"http://www.gribuser.ru/xml/fictionbook/2.0")
		Add(report, Severity::Error, L"Incorrect FictionBook namespace", L"Q-XML-NAMESPACE");
	Scan scan(xml, cancelRequested); scan.Visit(root, false, false, { 0 });
	Report findings = scan.Finish();
	report.issues.insert(report.issues.end(), findings.issues.begin(), findings.issues.end());
	report.title = findings.title;
	PopulateLocations(report, xml, cancelRequested);
	return report;
	} catch (const AnalysisCancelled&) {
		report.issues.clear();
		report.cancelled = true;
		return report;
	} catch (const _com_error&) {
		Add(report, Severity::Error, L"Unable to complete XML analysis", L"Q-XML-ANALYSIS");
		return report;
	}
}

CString FormatReport(const Report& report)
{
	CString text = FbeLoadRuntimeStringByKey(L"fbe.quality.caption", L"FB2 quality check") + L"\r\n";
	text += FbeLoadRuntimeStringByKey(L"fbe.quality.report.document", L"Document") + L": " + report.title + L"\r\n";
	text += FbeLoadRuntimeStringByKey(L"fbe.quality.report.checked", L"Checked") + L": " + report.checkedAt + L"\r\n";
	text += FbeLoadRuntimeStringByKey(L"fbe.quality.report.version", L"FBE Next version") + L": " FBE_VERSION_WSTRING L"\r\n";
	CString summary; summary.Format(FbeLoadRuntimeStringByKey(L"fbe.quality.summary", L"Errors: %d     Warnings: %d     Information: %d"),
		report.ErrorCount(), report.WarningCount(), report.InfoCount());
	text += summary + L"\r\n\r\n";
	for (const Issue& issue : report.issues) {
		text += L"[" + IssueSeverityText(issue.severity) + L"] [" + issue.code + L"] [" + IssueCategoryText(issue) + L"] " + issue.message;
		if (issue.line > 0) { CString location; location.Format(L" (%d:%d)", issue.line, issue.column); text += location; }
		text += L"\r\n";
		if (!issue.details.IsEmpty()) text += issue.details + L"\r\n";
		if (!issue.recommendation.IsEmpty()) text += issue.recommendation + L"\r\n";
	}
	if (report.issues.empty()) text += FbeLoadRuntimeStringByKey(L"fbe.quality.report.clean", L"No issues found.") + L"\r\n";
	return text;
}

CString FormatHtmlReport(const Report& report)
{
	const auto escape = [](const CString& value) {
		CString result;
		for (int i = 0; i < value.GetLength(); ++i) {
			switch (value[i]) {
			case L'&': result += L"&amp;"; break;
			case L'<': result += L"&lt;"; break;
			case L'>': result += L"&gt;"; break;
			case L'"': result += L"&quot;"; break;
			case L'\'': result += L"&#39;"; break;
			default: result += value[i]; break;
			}
		}
		return result;
	};
	CString html = L"<!doctype html><html><head><meta charset=\"utf-8\"><title>";
	html += escape(FbeLoadRuntimeStringByKey(L"fbe.quality.caption", L"FB2 quality check"));
	html += L"</title><style>body{font:16px system-ui,sans-serif;max-width:1000px;margin:2em auto;padding:0 1em;color:#222;background:#fff}table{border-collapse:collapse;width:100%;margin-bottom:2em}th,td{border:1px solid #aaa;padding:.4em;text-align:left;vertical-align:top}th{background:#eee}tr:nth-child(even){background:#f7f7f7}@media(prefers-color-scheme:dark){body{color:#eee;background:#202020}th{background:#333}tr:nth-child(even){background:#292929}}</style></head><body><h1>";
	html += escape(FbeLoadRuntimeStringByKey(L"fbe.quality.caption", L"FB2 quality check")) + L"</h1><p>";
	html += escape(FbeLoadRuntimeStringByKey(L"fbe.quality.report.document", L"Document")) + L": " + escape(report.title) + L"<br>";
	html += escape(FbeLoadRuntimeStringByKey(L"fbe.quality.report.checked", L"Checked")) + L": " + escape(report.checkedAt) + L"<br>";
	html += escape(FbeLoadRuntimeStringByKey(L"fbe.quality.report.version", L"FBE Next version")) + L": " FBE_VERSION_WSTRING L"</p><p>";
	CString summary; summary.Format(FbeLoadRuntimeStringByKey(L"fbe.quality.summary", L"Errors: %d     Warnings: %d     Information: %d"),
		report.ErrorCount(), report.WarningCount(), report.InfoCount());
	html += escape(summary) + L"</p>";
	const Category categories[] = { Category::Xml, Category::Links, Category::Images, Category::Notes, Category::Structure, Category::Metadata };
	for (Category category : categories) {
		const Issue* first = nullptr;
		for (const Issue& issue : report.issues) if (issue.category == category) { first = &issue; break; }
		if (!first) continue;
		html += L"<h2>" + escape(IssueCategoryText(*first)) + L"</h2><table><thead><tr>";
		const wchar_t* columns[] = { L"fbe.quality.column.type", L"fbe.quality.column.code", L"fbe.quality.column.description", L"fbe.quality.column.location" };
		const wchar_t* fallbacks[] = { L"Type", L"Code", L"Description", L"Location" };
		for (int i = 0; i < 4; ++i) html += L"<th>" + escape(FbeLoadRuntimeStringByKey(columns[i], fallbacks[i])) + L"</th>";
		html += L"</tr></thead><tbody>";
		for (const Issue& issue : report.issues) {
			if (issue.category != category) continue;
			html += L"<tr><td>" + escape(IssueSeverityText(issue.severity)) + L"</td><td>" + escape(issue.code) + L"</td><td>" + escape(issue.message);
			if (!issue.details.IsEmpty()) html += L"<p>" + escape(issue.details) + L"</p>";
			if (!issue.recommendation.IsEmpty()) html += L"<p>" + escape(issue.recommendation) + L"</p>";
			CString location; if (issue.line > 0) location.Format(L"%d:%d", issue.line, issue.column);
			html += L"</td><td>" + escape(location) + L"</td></tr>";
		}
		html += L"</tbody></table>";
	}
	if (report.issues.empty()) html += L"<p>" + escape(FbeLoadRuntimeStringByKey(L"fbe.quality.report.clean", L"No issues found.")) + L"</p>";
	html += L"</body></html>";
	return html;
}

bool SaveReport(const Report& report, const CString& path, bool html, DWORD& error)
{
	return SaveUtf8Report(path, html ? FormatHtmlReport(report) : FormatReport(report), !html, error);
}

int ShowReport(HWND parent, const Report& report)
{
	ResultsDialog dialog(report);
	dialog.DoModal(parent);
	return dialog.selected;
}

bool ProbeResultsDialogLayout(HWND parent, const Report& report, CString* diagnostics)
{
	ResultsDialog dialog(report);
	HWND window = dialog.Create(parent);
	if (!window) { if (diagnostics) *diagnostics = L"dialog-create-failed"; return false; }
	const HWND list = ::GetDlgItem(window, IDC_FB2_QUALITY_LIST);
	const HWND details = ::GetDlgItem(window, IDC_FB2_QUALITY_DETAILS);
	const HWND button = ::GetDlgItem(window, IDC_FB2_QUALITY_SAVE);
	RECT beforeWindow = {}, beforeList = {}, beforeDetails = {}, beforeButton = {};
	RECT initialWindow = {};
	::GetWindowRect(window, &initialWindow);
	::SetWindowPos(window, NULL, 0, 0, initialWindow.right - initialWindow.left + 200,
		initialWindow.bottom - initialWindow.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	::GetWindowRect(window, &beforeWindow);
	::GetWindowRect(list, &beforeList);
	::GetWindowRect(details, &beforeDetails);
	::GetWindowRect(button, &beforeButton);
	::SetWindowPos(window, NULL, 0, 0, beforeWindow.right - beforeWindow.left - 100,
		beforeWindow.bottom - beforeWindow.top + 100, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	RECT afterWindow = {}, afterList = {}, afterDetails = {}, afterButton = {};
	::GetWindowRect(window, &afterWindow);
	::GetWindowRect(list, &afterList);
	::GetWindowRect(details, &afterDetails);
	::GetWindowRect(button, &afterButton);
	const HWND header = reinterpret_cast<HWND>(::SendMessageW(list, LVM_GETHEADER, 0, 0));
	const bool widthChanged = afterWindow.right - afterWindow.left <= beforeWindow.right - beforeWindow.left - 70 &&
		afterList.right - afterList.left <= beforeList.right - beforeList.left - 70;
	const bool heightGrew = afterList.bottom - afterList.top >= beforeList.bottom - beforeList.top + 70;
	const bool separated = afterDetails.top > afterList.bottom && afterButton.top > afterDetails.bottom;
	const bool buttonMoved = afterButton.top >= beforeButton.top + 70;
	const int columns = header ? static_cast<int>(::SendMessageW(header, HDM_GETITEMCOUNT, 0, 0)) : -1;
	const DWORD listStyles = static_cast<DWORD>(::SendMessageW(list, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0));
	const bool cleanListStyle = (listStyles & LVS_EX_GRIDLINES) == 0 && (listStyles & LVS_EX_DOUBLEBUFFER) != 0;
	RECT buttonRects[4] = {};
	const int buttonIds[] = { IDC_FB2_QUALITY_GOTO, IDC_FB2_QUALITY_COPY, IDC_FB2_QUALITY_SAVE, IDCANCEL };
	bool buttonsFit = true;
	for(int index = 0; index < 4; ++index) {
		const HWND control = ::GetDlgItem(window, buttonIds[index]);
		::GetWindowRect(control, &buttonRects[index]);
		wchar_t label[256] = {};
		::GetWindowTextW(control, label, _countof(label));
		HDC dc = ::GetDC(control);
		HFONT font = reinterpret_cast<HFONT>(::SendMessageW(control, WM_GETFONT, 0, 0));
		HFONT old = dc && font ? reinterpret_cast<HFONT>(::SelectObject(dc, font)) : NULL;
		SIZE measured = {};
		if(dc) ::GetTextExtentPoint32W(dc, label, static_cast<int>(wcslen(label)), &measured);
		if(dc && old) ::SelectObject(dc, old);
		if(dc) ::ReleaseDC(control, dc);
		buttonsFit = buttonsFit && buttonRects[index].right - buttonRects[index].left >= measured.cx + 8;
		if(index > 0) buttonsFit = buttonsFit && buttonRects[index - 1].right < buttonRects[index].left;
	}
	wchar_t closeText[256] = {};
	::GetWindowTextW(::GetDlgItem(window, IDCANCEL), closeText, _countof(closeText));
	const bool closeCorrect = CString(closeText) == FbeLoadRuntimeStringByKey(L"fbe.quality.close", L"Close");
	const bool geometry = widthChanged && heightGrew && separated && buttonMoved && columns == 5 &&
		cleanListStyle && buttonsFit && closeCorrect;
	NMLISTVIEW click = {};
	click.hdr.hwndFrom = list;
	click.hdr.idFrom = IDC_FB2_QUALITY_LIST;
	click.hdr.code = LVN_COLUMNCLICK;
	click.iSubItem = 0;
	::SendMessageW(window, WM_NOTIFY, click.hdr.idFrom, reinterpret_cast<LPARAM>(&click));
	LVITEMW first = {}; first.mask = LVIF_PARAM; first.iItem = 0;
	const bool sortedFirst = ::SendMessageW(list, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&first)) != 0 &&
		static_cast<size_t>(first.lParam) < report.issues.size() && report.issues[first.lParam].severity == Severity::Error;
	::SendMessageW(window, WM_NOTIFY, click.hdr.idFrom, reinterpret_cast<LPARAM>(&click));
	first = {}; first.mask = LVIF_PARAM; first.iItem = 0;
	const bool sortedReverse = ::SendMessageW(list, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&first)) != 0 &&
		static_cast<size_t>(first.lParam) < report.issues.size() && report.issues[first.lParam].severity == Severity::Warning;
	dialog.DestroyWindow();
	Report cleanReport;
	ResultsDialog cleanDialog(cleanReport);
	const HWND cleanWindow = cleanDialog.Create(parent);
	const HWND emptyMessage = cleanWindow ? ::GetDlgItem(cleanWindow, IDC_FB2_QUALITY_EMPTY) : NULL;
	wchar_t emptyText[256] = {};
	if(emptyMessage) ::GetWindowTextW(emptyMessage, emptyText, _countof(emptyText));
	const bool cleanMessage = emptyMessage && CString(emptyText) == FbeLoadRuntimeStringByKey(L"fbe.quality.report.clean", L"No issues found.");
	if(cleanWindow) cleanDialog.DestroyWindow();
	if (diagnostics) diagnostics->Format(L"width=%d height=%d separated=%d button=%d columns=%d styles=%d labels=%d close=%d empty=%d ascending=%d descending=%d",
		widthChanged, heightGrew, separated, buttonMoved, columns, cleanListStyle, buttonsFit, closeCorrect, cleanMessage, sortedFirst, sortedReverse);
	return geometry && cleanMessage && sortedFirst && sortedReverse;
}

bool ProbeResultsDialogVisual(HWND parent, const Report& report, const CString& screenshotPath, CString* diagnostics)
{
	ResultsDialog dialog(report);
	const HWND window = dialog.Create(parent);
	if(!window) { if(diagnostics) *diagnostics = L"dialog-create-failed"; return false; }
	::ShowWindow(window, SW_SHOWNOACTIVATE);
	::UpdateWindow(window);
	const HWND list = ::GetDlgItem(window, IDC_FB2_QUALITY_LIST);
	int initialWidths[5] = {};
	for(int column = 0; column < 5; ++column)
		initialWidths[column] = static_cast<int>(::SendMessageW(list, LVM_GETCOLUMNWIDTH, column, 0));
	for(int iteration = 0; iteration < 10; ++iteration) {
		const int column = iteration % 5;
		const int width = UiMetrics::ScaleForDpi(85 + (iteration % 3) * 45, UiMetrics::DpiForWindow(window));
		::SendMessageW(list, LVM_SETCOLUMNWIDTH, column, width);
	}
	for(int column = 0; column < 5; ++column)
		::SendMessageW(list, LVM_SETCOLUMNWIDTH, column, initialWidths[column]);
	::RedrawWindow(list, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
	if(HWND empty = ::GetDlgItem(window, IDC_FB2_QUALITY_EMPTY))
		::RedrawWindow(empty, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	RECT bounds = {}; ::GetWindowRect(window, &bounds);
	const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
	BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
	info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
	void* bits = NULL;
	HBITMAP bitmap = ::CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
	HDC memory = bitmap ? ::CreateCompatibleDC(NULL) : NULL;
	HGDIOBJ previous = memory ? ::SelectObject(memory, bitmap) : NULL;
	const bool captured = bits && memory && previous && ::PrintWindow(window, memory, 0) != FALSE;
	bool saved = false;
	if(captured) {
		BITMAPFILEHEADER file = {}; file.bfType = 0x4d42;
		file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
		file.bfSize = file.bfOffBits + width * height * 4;
		HANDLE output = ::CreateFileW(screenshotPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if(output != INVALID_HANDLE_VALUE) {
			DWORD written = 0;
			saved = ::WriteFile(output, &file, sizeof(file), &written, NULL) && written == sizeof(file) &&
				::WriteFile(output, &info.bmiHeader, sizeof(info.bmiHeader), &written, NULL) && written == sizeof(info.bmiHeader) &&
				::WriteFile(output, bits, width * height * 4, &written, NULL) && written == static_cast<DWORD>(width * height * 4);
			::CloseHandle(output);
		}
	}
	if(previous) ::SelectObject(memory, previous);
	if(memory) ::DeleteDC(memory);
	if(bitmap) ::DeleteObject(bitmap);
	dialog.DestroyWindow();
	if(diagnostics) diagnostics->Format(L"captured=%d saved=%d width=%d height=%d drags=10", captured, saved, width, height);
	return captured && saved;
}

bool ProbeReportSaveDialog(HWND parent, const Report& report, bool htmlFilter)
{
	ResultsDialog dialog(report, htmlFilter ? 2 : 1);
	const HWND window = dialog.Create(parent);
	if(!window) return false;
	::ShowWindow(window, SW_SHOWNOACTIVATE);
	::SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_FB2_QUALITY_SAVE, BN_CLICKED),
		reinterpret_cast<LPARAM>(::GetDlgItem(window, IDC_FB2_QUALITY_SAVE)));
	dialog.DestroyWindow();
	return true;
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
