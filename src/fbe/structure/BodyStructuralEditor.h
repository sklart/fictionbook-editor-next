#pragma once

#include <mshtml.h>

namespace FbeStructure { class StructuralTrace; }

namespace FbeStructure {

enum class StructuralOperationStatus
{
	NotApplicable,
	Applied,
	Failed,
};

// A failed DOM call is materially different from a command that simply is not
// applicable at the current selection.  In particular, callers must not
// assume that a failure after an undo unit has changed nothing.
struct StructuralOperationResult
{
	StructuralOperationStatus status;
	HRESULT error;
	bool documentChanged;

	static StructuralOperationResult NotApplicable() { return { StructuralOperationStatus::NotApplicable, S_OK, false }; }
	static StructuralOperationResult Applied(HRESULT warning = S_OK) { return { StructuralOperationStatus::Applied, warning, true }; }
	static StructuralOperationResult Failed(HRESULT error, bool documentChanged = false) { return { StructuralOperationStatus::Failed, error, documentChanged }; }
	bool IsApplicable() const { return status != StructuralOperationStatus::NotApplicable; }
	bool IsApplied() const { return status == StructuralOperationStatus::Applied; }
	bool HasTechnicalFailure() const { return status == StructuralOperationStatus::Failed; }
};

class BodyStructuralEditor
{
public:
	BodyStructuralEditor(MSHTML::IHTMLDocument2Ptr document, MSHTML::IMarkupServices2Ptr markupServices, StructuralTrace* trace = nullptr);
	StructuralOperationResult InsertCite(bool checkOnly);
	StructuralOperationResult InsertPoem(bool checkOnly);
	StructuralOperationResult SplitContainer(bool checkOnly);

private:
	static MSHTML::IHTMLElementPtr FindParentDiv(MSHTML::IHTMLElementPtr element);
	bool ExpandRangeToParagraphs(MSHTML::IHTMLTxtRangePtr& range, MSHTML::IHTMLElementPtr& begin, MSHTML::IHTMLElementPtr& end) const;
	void Before(const wchar_t* phase) const;
	void After(const wchar_t* phase) const;
	void Hr(const wchar_t* phase, HRESULT hr) const;
	MSHTML::IHTMLDocument2Ptr m_document;
	MSHTML::IMarkupServices2Ptr m_markupServices;
	StructuralTrace* m_trace;
};

} // namespace FbeStructure
