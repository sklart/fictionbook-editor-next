#include "stdafx.h"
#include "Fb2QualityChecker.h"
#include "resource.h"
#include <cwctype>
#include <map>
#include <set>
#include <string>

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

CString Href(const Node& node)
{
	MSXML2::IXMLDOMNamedNodeMapPtr attributes = node->attributes;
	if (!attributes) return CString();
	for (long i = 0; i < attributes->length; ++i) {
		Node attribute = attributes->item[i];
		if (Name(attribute) == L"href") return CString(static_cast<const wchar_t*>(_bstr_t(attribute->text)));
	}
	return CString();
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
	std::set<std::wstring> binaries;
	std::set<std::wstring> usedBinaries;
	std::set<std::wstring> noteIds;
	std::map<std::wstring, std::vector<int>> binaryPaths;
	struct Link { CString href; CString sourceValue; CString attributeName; std::vector<int> path; bool note; bool image; };
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

	void Visit(const Node& node, bool inNotes, bool inTitleInfo, const std::vector<int>& path)
	{
		if (node->nodeType != MSXML2::NODE_ELEMENT) return;
		SourceIndexer::TagSpan span;
		if (index.Next(CString(static_cast<const wchar_t*>(_bstr_t(node->nodeName))), span)) spans[path] = span;
		const CString name = Name(node);
		const CString id = Attribute(node, L"id");
		if (!id.IsEmpty()) {
			if (!ids.insert(std::wstring(id.GetString())).second) {
				CString message; message.Format(L"Повторяется идентификатор %s", id.GetString());
				AddAt(Severity::Error, L"Q-LINK-DUPLICATE-ID", message, path, L"id", id);
			}
			if (inNotes && name == L"section") noteIds.insert(std::wstring(id.GetString()));
		}
		if (name == L"description") { description = true; if (descriptionPath.empty()) descriptionPath = path; }
		if (name == L"body") {
			body = true;
			inNotes = Attribute(node, L"name").CompareNoCase(L"notes") == 0;
			if (!inNotes && bodyPath.empty()) bodyPath = path;
		}
		if (name == L"section" && !inNotes) bodySection = true;
		if (name == L"title-info") { titleInfo = true; inTitleInfo = true; if (titleInfoPath.empty()) titleInfoPath = path; }
		if (inTitleInfo && (name == L"book-title" || name == L"lang")) {
			CString content(static_cast<const wchar_t*>(_bstr_t(node->text)));
			content.Trim();
			if (name == L"book-title" && !content.IsEmpty()) bookTitle = true;
			if (name == L"lang" && !content.IsEmpty()) language = true;
		}
		if (inTitleInfo && name == L"author") author = true;
		if (name == L"binary") {
			if (id.IsEmpty()) AddAt(Severity::Error, L"Q-BINARY-MISSING-ID", L"У binary не указан id", path, L"id");
			else {
				binaries.insert(std::wstring(id.GetString()));
				binaryPaths.emplace(std::wstring(id.GetString()), path);
			}
		}
		if (name == L"a" || name == L"image") {
			CString href = Href(node);
			CString hrefName;
			MSXML2::IXMLDOMNamedNodeMapPtr attributes = node->attributes;
			for (long i = 0; attributes && i < attributes->length; ++i) {
				Node attribute = attributes->item[i];
				if (Name(attribute) == L"href") { hrefName = static_cast<const wchar_t*>(_bstr_t(attribute->nodeName)); break; }
			}
			if (!href.IsEmpty() && href[0] == L'#') {
				links.push_back({ href.Mid(1), href, hrefName, path, name == L"a" && Attribute(node, L"type").CompareNoCase(L"note") == 0, name == L"image" });
				if (name == L"image") usedBinaries.insert(std::wstring(href.Mid(1).GetString()));
			} else if (name == L"image") AddAt(Severity::Error, L"Q-IMAGE-NONLOCAL", L"У изображения отсутствует внутренняя ссылка на binary", path, hrefName, href);
		}
		if (name == L"p" || name == L"subtitle") {
			CString content(static_cast<const wchar_t*>(_bstr_t(node->text)));
			content.Trim();
			if (content.IsEmpty() && node->childNodes->length == 0)
				AddAt(Severity::Warning, L"Q-STRUCTURE-EMPTY", L"Подозрительный пустой элемент " + name, path);
		}
		MSXML2::IXMLDOMNodeListPtr children = node->childNodes;
		int elementIndex = 0;
		for (long i = 0; i < children->length; ++i) {
			Node child = children->item[i];
			if (child->nodeType == MSXML2::NODE_ELEMENT) {
				std::vector<int> childPath(path);
				childPath.push_back(elementIndex++);
				Visit(child, inNotes, inTitleInfo, childPath);
			}
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
				CString message; message.Format(L"Ссылка на примечание #%s не найдена", link.href.GetString());
				AddAt(Severity::Error, L"Q-NOTE-MISSING", message, link.path, link.attributeName, link.sourceValue);
			} else if (!link.image && ids.find(target) == ids.end()) {
				CString message; message.Format(L"Ссылка #%s не найдена", link.href.GetString());
				AddAt(Severity::Error, L"Q-LINK-MISSING", message, link.path, link.attributeName, link.sourceValue);
			} else if (link.image && binaries.find(target) == binaries.end()) {
				CString message; message.Format(L"Binary %s отсутствует", link.href.GetString());
				AddAt(Severity::Error, L"Q-IMAGE-MISSING-BINARY", message, link.path, link.attributeName, link.sourceValue);
			}
		}
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
