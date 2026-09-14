#pragma once

#include "../apputils.h"
#include "../FBEView.h"

// The document-facing editor boundary. FB::Doc owns the editor lifetime, but
// accesses its content, lifecycle and semantic dirty state through this small
// explicit contract rather than through arbitrary CFBEView operations.
class DocumentEditorHost
{
public:
	explicit DocumentEditorHost(CFBEView& editor) : m_editor(editor) {}

	void SetDocumentFilePathSource(const CString* filename, const bool* namevalid);
	bool IsWindow() const;
	void DestroyWindow();
	HWND Create(HWND parent, const CRect& bounds, LPCTSTR classId);
	SHD::IWebBrowser2Ptr Browser() const;
	MSHTML::IHTMLDocument2Ptr Document() const;
	IDispatchPtr Script() const;
	bool HasDoc() const;
	bool Loaded();
	bool Init();

	long GetVersionNumber() const;
	bool IsFormChanged() const;
	void ResetFormChanged();
	bool IsFormCP() const;
	void ResetFormCP();

	void BeginNavigationTrace();
	void OnNavigateError(SHD::IWebBrowser2Ptr browser, CComVariant* url,
		CComVariant* frame, CComVariant* status, VARIANT_BOOL* cancel);
	bool NavigationFailed() const;
	long NavigationStatus() const;
	CString LastBrowserEvent() const;
	CString NavURL() const;
	IDispatchPtr CreateHelper();
	HRESULT SetExternalDispatch(IDispatchPtr dispatch);

	void Normalize(MSHTML::IHTMLDOMNodePtr node);
	HRESULT AddImportedBinary(const BYTE* data, size_t size, const CString& logicalFileName,
		const CString& mimeType);
	MSHTML::IHTMLElementPtr SelectionStructCon();
	void LoadTransformedHtml(IStream* stream);

private:
	CFBEView& m_editor;
};
