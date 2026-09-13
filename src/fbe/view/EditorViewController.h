#pragma once

#include "EditorViewTransition.h"

enum class EditorViewChangeStatus
{
	Success,
	Rejected,
	Failed
};

enum class EditorViewChangeFailure
{
	None,
	HtmlUnavailable,
	InvalidSource,
	SourceCommitFailed,
	SourcePrepareFailed
};

enum class EditorSourceOperationResult
{
	Success,
	InvalidSource,
	Failed
};

// Narrow presentation port.  It deliberately contains only the effects that
// cannot leave the editor window, while lifecycle ordering remains in the
// controller.
class IEditorViewHost
{
public:
	virtual ~IEditorViewHost() = default;
	virtual bool IsHtmlDocumentAvailable() const = 0;
	virtual void SaveEditorViewSelection(EditorView view) = 0;
	virtual EditorSourceOperationResult CommitSourceDocument() = 0;
	virtual EditorSourceOperationResult PrepareSourceDocument(EditorView previous) = 0;
	virtual void PrepareEditorViewPresentation(EditorView previous, EditorView target,
		const EditorViewTransitionPlan& plan) = 0;
	virtual void RestoreEditorViewSelection(EditorView view) = 0;
	virtual void CompleteEditorViewPresentation(EditorView previous, EditorView target) = 0;
};

struct EditorViewChangeResult
{
	EditorView previous = EditorView::Body;
	EditorView current = EditorView::Body;
	EditorViewChangeStatus status = EditorViewChangeStatus::Rejected;
	EditorViewChangeFailure failure = EditorViewChangeFailure::None;

	bool Succeeded() const { return status == EditorViewChangeStatus::Success; }
};

class EditorViewController
{
public:
	EditorViewChangeResult ChangeView(EditorViewState& state, IEditorViewHost& host,
		EditorView target) const;
};
