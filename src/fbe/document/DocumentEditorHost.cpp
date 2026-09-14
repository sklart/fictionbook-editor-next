#include "stdafx.h"
#include "DocumentEditorHost.h"

void DocumentEditorHost::SetDocumentFilePathSource(const CString* filename, const bool* namevalid) { m_editor.SetDocumentFilePathSource(filename, namevalid); }
bool DocumentEditorHost::IsWindow() const { return m_editor.IsWindow() != FALSE; }
void DocumentEditorHost::DestroyWindow() { m_editor.DestroyWindow(); }
HWND DocumentEditorHost::Create(HWND parent, const CRect& bounds, LPCTSTR classId)
{
	CRect mutableBounds(bounds);
	return m_editor.Create(parent, mutableBounds, classId);
}
SHD::IWebBrowser2Ptr DocumentEditorHost::Browser() const { return m_editor.Browser(); }
MSHTML::IHTMLDocument2Ptr DocumentEditorHost::Document() const { return m_editor.Document(); }
IDispatchPtr DocumentEditorHost::Script() const { return m_editor.Script(); }
bool DocumentEditorHost::HasDoc() const { return m_editor.HasDoc(); }
bool DocumentEditorHost::Loaded() { return m_editor.Loaded(); }
bool DocumentEditorHost::Init() { return m_editor.Init(); }

long DocumentEditorHost::GetVersionNumber() const { return m_editor.GetVersionNumber(); }
bool DocumentEditorHost::IsFormChanged() const { return m_editor.IsFormChanged(); }
void DocumentEditorHost::ResetFormChanged() { m_editor.ResetFormChanged(); }
bool DocumentEditorHost::IsFormCP() const { return m_editor.IsFormCP(); }
void DocumentEditorHost::ResetFormCP() { m_editor.ResetFormCP(); }

void DocumentEditorHost::BeginNavigationTrace() { m_editor.BeginNavigationTrace(); }
void DocumentEditorHost::OnNavigateError(SHD::IWebBrowser2Ptr browser, CComVariant* url,
	CComVariant* frame, CComVariant* status, VARIANT_BOOL* cancel) { m_editor.OnNavigateError(browser, url, frame, status, cancel); }
bool DocumentEditorHost::NavigationFailed() const { return m_editor.NavigationFailed(); }
long DocumentEditorHost::NavigationStatus() const { return m_editor.NavigationStatus(); }
CString DocumentEditorHost::LastBrowserEvent() const { return m_editor.LastBrowserEvent(); }
CString DocumentEditorHost::NavURL() const { return m_editor.NavURL(); }
IDispatchPtr DocumentEditorHost::CreateHelper() { return m_editor.CreateHelper(); }
HRESULT DocumentEditorHost::SetExternalDispatch(IDispatchPtr dispatch) { return m_editor.SetExternalDispatch(dispatch); }

void DocumentEditorHost::Normalize(MSHTML::IHTMLDOMNodePtr node) { m_editor.Normalize(node); }
HRESULT DocumentEditorHost::AddImportedBinary(const BYTE* data, size_t size, const CString& logicalFileName,
	const CString& mimeType) { return m_editor.AddImportedBinary(data, size, logicalFileName, mimeType); }
MSHTML::IHTMLElementPtr DocumentEditorHost::SelectionStructCon() { return m_editor.SelectionStructCon(); }
void DocumentEditorHost::LoadTransformedHtml(IStream* stream)
{
	IPersistStreamInitPtr persist(Browser()->Document);
	persist->InitNew();
	persist->Load(stream);
}
