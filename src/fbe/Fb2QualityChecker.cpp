#include "stdafx.h"
#include "Fb2QualityChecker.h"
#include "resource.h"
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

void Add(Report& report, Severity severity, const CString& message, const CString& locator = CString())
{
	report.issues.push_back({ severity, message, locator });
}

struct Scan {
	Report report;
	std::set<std::wstring> ids;
	std::set<std::wstring> binaries;
	std::set<std::wstring> usedBinaries;
	std::set<std::wstring> noteIds;
	struct Link { CString href; CString locator; bool note; bool image; };
	std::vector<Link> links;
	bool description = false;
	bool body = false;
	bool bodySection = false;
	bool titleInfo = false;
	bool bookTitle = false;
	bool language = false;
	bool author = false;

	void Visit(const Node& node, bool inNotes, bool inTitleInfo)
	{
		if (node->nodeType != MSXML2::NODE_ELEMENT) return;
		const CString name = Name(node);
		const CString id = Attribute(node, L"id");
		if (!id.IsEmpty()) {
			if (!ids.insert(std::wstring(id.GetString())).second) {
				CString message; message.Format(L"Повторяется идентификатор %s", id.GetString());
				Add(report, Severity::Error, message, id);
			}
			if (inNotes && name == L"section") noteIds.insert(std::wstring(id.GetString()));
		}
		if (name == L"description") description = true;
		if (name == L"body") { body = true; inNotes = Attribute(node, L"name").CompareNoCase(L"notes") == 0; }
		if (name == L"section" && !inNotes) bodySection = true;
		if (name == L"title-info") { titleInfo = true; inTitleInfo = true; }
		if (inTitleInfo && (name == L"book-title" || name == L"lang")) {
			CString content(static_cast<const wchar_t*>(_bstr_t(node->text)));
			content.Trim();
			if (name == L"book-title" && !content.IsEmpty()) bookTitle = true;
			if (name == L"lang" && !content.IsEmpty()) language = true;
		}
		if (inTitleInfo && name == L"author") author = true;
		if (name == L"binary") {
			if (id.IsEmpty()) Add(report, Severity::Error, L"У binary не указан id");
			else binaries.insert(std::wstring(id.GetString()));
		}
		if (name == L"a" || name == L"image") {
			CString href = Href(node);
			if (!href.IsEmpty() && href[0] == L'#') {
				links.push_back({ href.Mid(1), href, name == L"a" && Attribute(node, L"type").CompareNoCase(L"note") == 0, name == L"image" });
				if (name == L"image") usedBinaries.insert(std::wstring(href.Mid(1).GetString()));
			} else if (name == L"image") Add(report, Severity::Error, L"У изображения отсутствует внутренняя ссылка на binary");
		}
		if (name == L"p" || name == L"subtitle") {
			CString content(static_cast<const wchar_t*>(_bstr_t(node->text)));
			content.Trim();
			if (content.IsEmpty() && node->childNodes->length == 0)
				Add(report, Severity::Warning, L"Подозрительный пустой элемент " + name, id);
		}
		MSXML2::IXMLDOMNodeListPtr children = node->childNodes;
		for (long i = 0; i < children->length; ++i) Visit(children->item[i], inNotes, inTitleInfo);
	}

	Report Finish()
	{
		if (!description) Add(report, Severity::Error, L"Отсутствует description");
		if (!body) Add(report, Severity::Error, L"Отсутствует body");
		else if (!bodySection) Add(report, Severity::Error, L"В основном body нет разделов section");
		if (!titleInfo) Add(report, Severity::Error, L"Отсутствует title-info");
		if (!bookTitle) Add(report, Severity::Warning, L"Не указано название книги");
		if (!language) Add(report, Severity::Warning, L"Не указан язык документа");
		if (!author) Add(report, Severity::Warning, L"Не указан автор");
		for (const Link& link : links) {
			const std::wstring target(link.href.GetString());
			if (link.note && noteIds.find(target) == noteIds.end()) {
				CString message; message.Format(L"Ссылка на примечание #%s не найдена", link.href.GetString());
				Add(report, Severity::Error, message, link.locator);
			} else if (!link.image && ids.find(target) == ids.end()) {
				CString message; message.Format(L"Ссылка #%s не найдена", link.href.GetString());
				Add(report, Severity::Error, message, link.locator);
			}
		}
		for (const std::wstring& id : usedBinaries) if (binaries.find(id) == binaries.end()) {
			CString message; message.Format(L"Binary %s отсутствует", id.c_str());
			Add(report, Severity::Error, message, CString(L"#") + id.c_str());
		}
		for (const std::wstring& id : binaries) if (usedBinaries.find(id) == usedBinaries.end()) {
			CString message; message.Format(L"Binary %s не используется", id.c_str());
			Add(report, Severity::Warning, message, CString(L"id=\"") + id.c_str() + L"\"");
		}
		return report;
	}
};

class ResultsDialog : public CDialogImpl<ResultsDialog> {
public:
	enum { IDD = IDD_FB2_QUALITY_RESULTS };
	explicit ResultsDialog(const Report& report) : m_report(report) {}
	CString selected;
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
		const bool available = index >= 0 && static_cast<size_t>(index) < m_report.issues.size() && !m_report.issues[index].locator.IsEmpty();
		::EnableWindow(GetDlgItem(IDC_FB2_QUALITY_GOTO), available ? TRUE : FALSE);
	}
	LRESULT OnSelectionChanged(int, LPNMHDR, BOOL&) { UpdateGoTo(); return 0; }
	LRESULT OnGoTo(WORD, WORD, HWND, BOOL&) {
		CListViewCtrl list = GetDlgItem(IDC_FB2_QUALITY_LIST);
		const int index = list.GetNextItem(-1, LVNI_SELECTED);
		if (index >= 0 && static_cast<size_t>(index) < m_report.issues.size()) selected = m_report.issues[index].locator;
		if (!selected.IsEmpty()) EndDialog(IDOK);
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
	if (FAILED(document.CreateInstance(L"Msxml2.DOMDocument.6.0"))) { Add(report, Severity::Error, L"Не удалось создать XML-анализатор."); return report; }
	document->async = VARIANT_FALSE;
	document->validateOnParse = VARIANT_FALSE;
	document->resolveExternals = VARIANT_FALSE;
	document->setProperty(L"ProhibitDTD", _variant_t(VARIANT_TRUE));
	if (document->loadXML(_bstr_t(xml)) == VARIANT_FALSE) {
		MSXML2::IXMLDOMParseErrorPtr error = document->parseError;
		CString message; message.Format(L"Некорректный XML, строка %ld: %s", error->line, static_cast<const wchar_t*>(_bstr_t(error->reason)));
		CString locator; locator.Format(L"line:%ld", error->line);
		Add(report, Severity::Error, message, locator);
		return report;
	}
	Node root = document->documentElement;
	if (!root || Name(root) != L"FictionBook") { Add(report, Severity::Error, L"Корневой элемент должен быть FictionBook."); return report; }
	if (CString(static_cast<const wchar_t*>(_bstr_t(root->namespaceURI))) != L"http://www.gribuser.ru/xml/fictionbook/2.0")
		Add(report, Severity::Error, L"Неверное пространство имён FictionBook.");
	Scan scan; scan.Visit(root, false, false);
	Report findings = scan.Finish();
	report.issues.insert(report.issues.end(), findings.issues.begin(), findings.issues.end());
	return report;
	} catch (const _com_error&) {
		Add(report, Severity::Error, L"Не удалось завершить анализ XML.");
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

CString ShowReport(HWND parent, const Report& report)
{
	ResultsDialog dialog(report);
	dialog.DoModal(parent);
	return dialog.selected;
}
}
