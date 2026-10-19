#pragma once

#include <atlstr.h>
#include <functional>

namespace FB { class Doc; }
class SourceEditorControl;
class XmlScriptUndoUnit;

struct XmlScriptDiagnostic
{
	bool valid;
	int line;
	int column;
	CString message;
	HRESULT error = S_OK;
	XmlScriptDiagnostic() : valid(false), line(0), column(0) {}
};

// Editor-owned backend with no COM or MSHTML dispatch surface. The scripting
// adapter exchanges requests without exposing this backend in the public ABI.
class XmlScriptBackend
{
public:
	typedef std::function<void(const CString&)> SynchronizeCallback;
	XmlScriptBackend(FB::Doc*& document, SourceEditorControl& source,
		const std::function<bool()>& sourceIsActive, const SynchronizeCallback& synchronize);
	bool GetSourceText(CString& text) const;
	XmlScriptDiagnostic ValidateSourceText(const CString& text) const;
	XmlScriptDiagnostic ValidateCurrentDocument() const;
	XmlScriptDiagnostic ApplySourceText(const CString& text, const CString& operationName);
private:
	friend class XmlScriptUndoUnit;
	XmlScriptDiagnostic ApplySnapshot(const CString& text, bool markDocumentDirty);
	FB::Doc*& m_document;
	SourceEditorControl& m_source;
	std::function<bool()> m_sourceIsActive;
	SynchronizeCallback m_synchronize;
};
