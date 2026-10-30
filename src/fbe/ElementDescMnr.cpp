#include "stdafx.h"

#include "ElementDescMnr.h"

#include "utils.h"
#include "apputils.h"
#include "Settings.h"
#include "mainfrm.h"
#include "StartupTrace.h"

extern CSettings _Settings;

const int IMG_ID_BODY = 0;
const int IMG_ID_SECTION = 0;
const int IMG_ID_IMAGE = 3;
const int IMG_ID_POEM = 3;

CElementDescMnr _EDMnr;

bool IsDiv(const MSHTML::IHTMLElementPtr& elem, const CString& className)
{
	if(U::scmp(elem->tagName, L"DIV") == 0 && U::scmp(elem->className, className) == 0)
		return true;

	return false;
}

bool IsP(const MSHTML::IHTMLElementPtr& elem, const CString& className)
{
	if(U::scmp(elem->tagName, L"P") == 0 && U::scmp(elem->className, className) == 0)
		return true;

	return false;
}

// added by SeNS
bool IsStylesheet(const MSHTML::IHTMLElementPtr& elem)
{
	return (U::scmp(elem->className, L"stylesheet") == 0);
}

bool IsStyle(const MSHTML::IHTMLElementPtr& elem)
{
	// Only SPAN markup can match the prefix below.  Avoid serializing the
	// entire subtree for every BODY, DIV and paragraph while building the tree.
	if(U::scmp(elem->tagName, L"SPAN") != 0)
		return false;
	CString outerHTML(static_cast<const wchar_t*>(elem->outerHTML));
	outerHTML.MakeLower();
	return (outerHTML.Find(L"<span class=") == 0);
}

CString GetStylesheetTitle(const MSHTML::IHTMLElementPtr& /* unused: elem */)
{
	return L"";
}

bool IsSection(const MSHTML::IHTMLElementPtr& elem)
{
	return IsDiv(elem, L"section");
}

CString GetSectionTitle(const MSHTML::IHTMLElementPtr& elem)
{
	return U::FindTitle(elem);
}

bool IsBody(const MSHTML::IHTMLElementPtr& elem)
{
	return IsDiv(elem, L"body");
}

CString GetBodyTitle(const MSHTML::IHTMLElementPtr& elem)
{
	CString caption;
	caption = AU::GetAttrCS(elem, L"fbname");
	U::NormalizeInplace(caption);
	if (caption.IsEmpty())
		caption = U::FindTitle(elem);
	return caption;
}

bool IsImage(const MSHTML::IHTMLElementPtr& elem)
{
//	return IsDiv(elem, L"image");
	return (U::scmp(elem->className, L"image") == 0);
}

CString GetImageTitle(const MSHTML::IHTMLElementPtr& elem)
{
	return U::GetImageFileName(elem);
}

bool IsAnnotation(const MSHTML::IHTMLElementPtr& elem)
{
	return IsDiv(elem, L"annotation");
}

CString GetAnnotationTitle(const MSHTML::IHTMLElementPtr& /* unused: elem */)
{
	return L"";
}

bool IsHistory(const MSHTML::IHTMLElementPtr& elem)
{
	return IsDiv(elem, L"history");
}

CString GetHistoryTitle(const MSHTML::IHTMLElementPtr& /* unused: elem */)
{
	return L"";
}

bool IsPoem(const MSHTML::IHTMLElementPtr& elem)
{
	return IsDiv(elem, L"poem");
}

CString GetPoemTitle(const MSHTML::IHTMLElementPtr& elem)
{
	return U::FindTitle(elem);
}

bool IsStanza(const MSHTML::IHTMLElementPtr& elem)
{
	return IsDiv(elem, L"stanza");
}

CString GetStanzaTitle(const MSHTML::IHTMLElementPtr& elem)
{
	return U::FindTitle(elem);
}

bool IsTitle(const MSHTML::IHTMLElementPtr& elem)
{
	return IsDiv(elem, L"title");
}

CString GetTitleTitle(const MSHTML::IHTMLElementPtr& elem)
{
	return U::FindTitle(elem);
}

bool IsEpigraph(const MSHTML::IHTMLElementPtr& elem)
{
	return IsDiv(elem, L"epigraph");
}

CString GetEpigraphTitle(const MSHTML::IHTMLElementPtr& elem)
{
	return U::FindTitle(elem);
}

bool IsCite(const MSHTML::IHTMLElementPtr& elem)
{
	return IsDiv(elem, L"cite");
}

CString GetCiteTitle(const MSHTML::IHTMLElementPtr& elem)
{
	return U::FindTitle(elem);
}

bool IsCode(const MSHTML::IHTMLElementPtr& elem)
{
	return IsDiv(elem, L"code");
}

CString GetCodeTitle(const MSHTML::IHTMLElementPtr& elem)
{
	return U::FindTitle(elem);
}

bool IsSubtitle(const MSHTML::IHTMLElementPtr& elem)
{
	return IsP(elem, L"subtitle");
}

CString GetSubtitleTitle(const MSHTML::IHTMLElementPtr& elem)
{
	return U::FindTitle(elem);
}

bool IsTable(const MSHTML::IHTMLElementPtr& elem)
{
	return IsDiv(elem, L"table");
}

bool IsTH(const MSHTML::IHTMLElementPtr& elem)
{
	return IsP(elem, L"th");
}

bool IsTD(const MSHTML::IHTMLElementPtr& elem)
{
	return IsP(elem, L"td");
}

bool IsTR(const MSHTML::IHTMLElementPtr& elem)
{
	return IsP(elem, L"tr");
}

CString GetTableTitle(const MSHTML::IHTMLElementPtr& elem)
{
	return U::FindTitle(elem);
}

CElementDescMnr::CElementDescMnr() : m_initedStEDs(false)
{
}

// Init uses Settings to get information about each item in its list,
// but until Settings are deserialized, that information is reflected
// by CSettings::GetDocTreeItemState via variable which is set to
// CElementDescriptor constructor. On the other hand information about
// serialized fields and default values is collected after initiation, but not before.
// Though we have to assign really deserialized values during creation of CTreeWithToolBar
// explicitly calling information on CSettings::TREEITEMSHOWINFO instance via the same
// CSettings::GetDocTreeItemState. That is non-obvious but works!
bool CElementDescMnr::InitStandartEDs()
{
	if(!m_initedStEDs)
	{
		// SeNS
		CElementDescriptor* stylesheet = new CElementDescriptor;
		stylesheet->Init(IsStylesheet, GetStylesheetTitle, 0, false, L"Stylesheet");
		CElementDescriptor* style = new CElementDescriptor;
		style->Init(IsStyle, GetStylesheetTitle, 0, false, L"Style");

		CElementDescriptor* section = new CElementDescriptor;
		section->Init(IsSection, GetSectionTitle, 0, true, L"Section");
		CElementDescriptor* body = new CElementDescriptor;
		body->Init(IsBody, GetBodyTitle, 0, true, L"Body");
		CElementDescriptor* image = new CElementDescriptor;
		image->Init(IsImage, GetImageTitle, 21, true, L"Image");
		CElementDescriptor* annotation = new CElementDescriptor;
		annotation->Init(IsAnnotation, GetAnnotationTitle, 12, true, L"Annotation");
		CElementDescriptor* history = new CElementDescriptor;
		history->Init(IsHistory, GetHistoryTitle, 15, true, L"History");
		CElementDescriptor* poem = new CElementDescriptor;
		poem->Init(IsPoem, GetPoemTitle, 3, true, L"Poem");
		CElementDescriptor* stanza = new CElementDescriptor;
		stanza->Init(IsStanza, GetStanzaTitle, 3, true, L"Stanza");
		CElementDescriptor* title = new CElementDescriptor;
		title->Init(IsTitle, GetTitleTitle, 27, false, L"Title");
		CElementDescriptor* epigraph = new CElementDescriptor;
		epigraph->Init(IsEpigraph, GetEpigraphTitle, 18, false, L"Epigraph");
		CElementDescriptor* cite = new CElementDescriptor;
		cite->Init(IsCite, GetCiteTitle, 9, false, L"Cite");
		// issue #83
/*		CElementDescriptor* code = new CElementDescriptor;
		code->Init(IsCode, GetCodeTitle, 24, false, L"Code"); */
		CElementDescriptor* subtitle = new CElementDescriptor;
		subtitle->Init(IsSubtitle, GetSubtitleTitle, 6, true, L"Subtitle");
		CElementDescriptor* table = new CElementDescriptor;
		table->Init(IsTable, GetTableTitle, 30, true, L"Table");
		CElementDescriptor* th = new CElementDescriptor;
		th->Init(IsTH, GetTableTitle, 27, true, L"th");
		CElementDescriptor* td = new CElementDescriptor;
		td->Init(IsTD, GetTableTitle, 27, true, L"td");
		CElementDescriptor* tr = new CElementDescriptor;
		tr->Init(IsTR, GetTableTitle, 27, true, L"tr");

		m_stEDs.Add(stylesheet);	// SeNS
		m_stEDs.Add(style);

		m_stEDs.Add(section);
		m_stEDs.Add(body);
		m_stEDs.Add(image);
		m_stEDs.Add(annotation);
		m_stEDs.Add(history);
		m_stEDs.Add(poem);
		m_stEDs.Add(stanza);
		m_stEDs.Add(title);
		m_stEDs.Add(epigraph);
		m_stEDs.Add(cite);
//		m_stEDs.Add(code);	// issue #83
		m_stEDs.Add(subtitle);
		m_stEDs.Add(table);
		m_stEDs.Add(th);
		m_stEDs.Add(td);
		m_stEDs.Add(tr);

		m_initedStEDs = true;
	}

	return true;
}

bool CElementDescMnr::InitScriptEDs()
{
	CElementDescriptor* ED;

	WIN32_FIND_DATA fd;
	HANDLE found = FindFirstFile(U::GetDocTReeScriptsDir() + L"\\*.js", &fd);
	if(INVALID_HANDLE_VALUE != found)
	{
		do
		{
			ED = new CElementDescriptor;
			if(ED->Load(U::GetDocTReeScriptsDir() + L"\\" + fd.cFileName))
			{
				m_EDs.Add(ED);
			}
			else
			{
				delete ED;
			}
		}
		while(FindNextFile(found, &fd));
		FindClose(found);
	}
	return true;
}

bool CElementDescMnr::GetElementDescriptor(const MSHTML::IHTMLElementPtr& elem, CElementDescriptor **desc) const
{
	// Standard descriptors inspect the same two COM properties repeatedly.
	// Cache them for this lookup only; unknown matchers and script descriptors
	// continue to use their original virtual IsMe implementation.
	const _bstr_t tag(elem->tagName);
	const _bstr_t cls(elem->className);
	const wchar_t* tagName = tag;
	const wchar_t* className = cls;
	for(int i = 0; i < m_stEDs.GetSize(); i++)
	{
		const FN_IsMe matcher = m_stEDs[i]->NativeMatcher();
		bool matched = false;
		if (matcher == IsStylesheet) matched = U::scmp(className, L"stylesheet") == 0;
		else if (matcher == IsStyle) matched = U::scmp(tagName, L"SPAN") == 0 && IsStyle(elem);
		else if (matcher == IsImage) matched = U::scmp(className, L"image") == 0;
		else {
			const wchar_t* expectedTag = L"DIV";
			const wchar_t* expectedClass = NULL;
			if (matcher == IsSection) expectedClass = L"section";
			else if (matcher == IsBody) expectedClass = L"body";
			else if (matcher == IsAnnotation) expectedClass = L"annotation";
			else if (matcher == IsHistory) expectedClass = L"history";
			else if (matcher == IsPoem) expectedClass = L"poem";
			else if (matcher == IsStanza) expectedClass = L"stanza";
			else if (matcher == IsTitle) expectedClass = L"title";
			else if (matcher == IsEpigraph) expectedClass = L"epigraph";
			else if (matcher == IsCite) expectedClass = L"cite";
			else if (matcher == IsTable) expectedClass = L"table";
			else {
				expectedTag = L"P";
				if (matcher == IsSubtitle) expectedClass = L"subtitle";
				else if (matcher == IsTH) expectedClass = L"th";
				else if (matcher == IsTD) expectedClass = L"td";
				else if (matcher == IsTR) expectedClass = L"tr";
			}
			matched = expectedClass ? U::scmp(tagName, expectedTag) == 0 && U::scmp(className, expectedClass) == 0
				: m_stEDs[i]->IsMe(elem);
		}
		if(matched)
		{
			*desc = m_stEDs[i];
			return true;
		}
	}

	for(int i = 0; i < m_EDs.GetSize(); i++)
	{
		if(m_EDs[i]->IsMe(elem))
		{
			*desc = m_EDs[i];
			return true;
		}
	}
	return false;
}

void CElementDescMnr::AddElementDescriptor(CElementDescriptor *desc)
{
	m_EDs.Add(desc);
}

CElementDescriptor* CElementDescMnr::GetStED(int index)
{
	return m_stEDs[index];
}

CElementDescriptor* CElementDescMnr::GetED(int index)
{
	return m_EDs[index];
}

int CElementDescMnr::GetEDsCount()
{
	return m_EDs.GetSize();
}

int CElementDescMnr::GetStEDsCount()
{
	return m_stEDs.GetSize();
}

void CElementDescMnr::CleanUpAll(bool continueOnError)
{
	for(int i = 0; i < m_EDs.GetSize(); i++)
	{
		if (!continueOnError) { m_EDs[i]->CleanUp(); continue; }
		// A failing extension must not interrupt release of editor-owned COM
		// references or prevent the remaining descriptors from being cleaned.
		try { m_EDs[i]->CleanUp(); }
		catch (const _com_error& error) {
			StartupTrace::HResult(L"document", L"D210", error.Error(), L"tree apiCleanUp failed; continuing detach");
		}
		catch (...) {
			StartupTrace::Error(L"document", L"D211", L"tree apiCleanUp exception; continuing detach");
		}
	}
}

void CElementDescMnr::CleanTree()
{
	for(int i = 0; i < m_EDs.GetSize(); i++)
	{
		m_EDs[i]->CleanUp();
		m_EDs[i]->SetViewInTree(false);
	}
	for(int i = 0; i < m_stEDs.GetSize(); i++)
	{
		m_stEDs[i]->SetViewInTree(false);
	}
}
