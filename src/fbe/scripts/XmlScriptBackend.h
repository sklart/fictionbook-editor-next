#pragma once

#include <atlstr.h>
#include <functional>
#include <vector>

namespace FB { class Doc; }
class SourceEditorControl;

struct XmlScriptDiagnostic
{
	bool valid;
	int line;
	int column;
	CString message;
	XmlScriptDiagnostic() : valid(false), line(0), column(0) {}
};

// Internal scripting boundary. It deliberately has no COM or MSHTML dispatch
// surface: publishing it through window.external is a separate IDL change.
class XmlScriptBackend
{
public:
	typedef std::function<void(const CString&)> SynchronizeCallback;
	XmlScriptBackend(FB::Doc*& document, SourceEditorControl& source,
		const std::function<bool()>& sourceIsActive, const SynchronizeCallback& synchronize);
	bool GetSourceText(CString& text) const;
	XmlScriptDiagnostic ValidateSourceText(const CString& text) const;
	XmlScriptDiagnostic ApplySourceText(const CString& text, const CString& operationName);
	bool CanUndo() const;
	XmlScriptDiagnostic UndoLastApply();
private:
	struct UndoSnapshot
	{
		CString text;
		bool documentWasDirty;
	};

	XmlScriptDiagnostic ApplyValidatedText(const CString& text, bool recordUndo, bool markDocumentDirty);
	FB::Doc*& m_document;
	SourceEditorControl& m_source;
	std::function<bool()> m_sourceIsActive;
	SynchronizeCallback m_synchronize;
	std::vector<UndoSnapshot> m_undoSnapshots;
};
