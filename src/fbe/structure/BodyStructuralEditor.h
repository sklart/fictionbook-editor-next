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

enum class SplitFailurePoint
{
	None,
	BeforeMutation,
	AfterFirstMutation
};

// Used by the production runtime harness to prove that a failure reports
// whether the live DOM has already changed.  Callers normally use the
// one-argument overloads below.
enum class CitePoemFailurePoint
{
	None,
	BeforeMutation,
	AfterInsert,
	BeforeSelection
};

class BodyStructuralEditor
{
public:
	BodyStructuralEditor(MSHTML::IHTMLDocument2Ptr document, MSHTML::IMarkupServices2Ptr markupServices, StructuralTrace* trace = nullptr);
	StructuralOperationResult InsertCite(bool checkOnly);
	StructuralOperationResult InsertPoem(bool checkOnly);
	StructuralOperationResult InsertCite(bool checkOnly, CitePoemFailurePoint failurePoint);
	StructuralOperationResult InsertPoem(bool checkOnly, CitePoemFailurePoint failurePoint);
	StructuralOperationResult SplitContainer(bool checkOnly);
	StructuralOperationResult SplitContainer(bool checkOnly, SplitFailurePoint failurePoint);

private:
	static MSHTML::IHTMLElementPtr FindParentDiv(MSHTML::IHTMLElementPtr element);
	// S_OK: range expanded; S_FALSE: selection is not structurally applicable;
	// failed HRESULT: MSHTML/markup-services failure that must reach the UI.
	HRESULT ExpandRangeToParagraphs(MSHTML::IHTMLTxtRangePtr& range, MSHTML::IHTMLElementPtr& begin, MSHTML::IHTMLElementPtr& end) const;
	void Before(const wchar_t* phase) const;
	void After(const wchar_t* phase) const;
	void Hr(const wchar_t* phase, HRESULT hr) const;
	MSHTML::IHTMLDocument2Ptr m_document;
	MSHTML::IMarkupServices2Ptr m_markupServices;
	StructuralTrace* m_trace;
};

} // namespace FbeStructure
