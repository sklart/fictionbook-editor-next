#include "../stdafx.h"
#include "EditorViewController.h"

namespace
{
EditorViewChangeFailure SourceFailure(EditorSourceOperationResult result,
	EditorViewChangeFailure failed)
{
	return result == EditorSourceOperationResult::InvalidSource
		? EditorViewChangeFailure::InvalidSource : failed;
}
}

EditorViewChangeResult EditorViewController::ChangeView(EditorViewState& state,
	IEditorViewHost& host, EditorView target) const
{
	EditorViewChangeResult result;
	result.previous = state.Current();
	result.current = result.previous;
	const EditorViewTransitionPlan plan =
		MakeEditorViewTransitionPlan(result.previous, target);

	if (plan.saveCurrentSelection)
		host.SaveEditorViewSelection(result.previous);
	if (plan.commitSourceToDocument)
	{
		const EditorSourceOperationResult sourceResult = host.CommitSourceDocument();
		if (sourceResult != EditorSourceOperationResult::Success)
		{
			result.status = EditorViewChangeStatus::Rejected;
			result.failure = SourceFailure(sourceResult,
				EditorViewChangeFailure::SourceCommitFailed);
			return result;
		}
	}
	if ((target == EditorView::Body || target == EditorView::Description) &&
		!host.IsHtmlDocumentAvailable())
	{
		result.failure = EditorViewChangeFailure::HtmlUnavailable;
		return result;
	}
	if (plan.prepareDocumentSource)
	{
		const EditorSourceOperationResult sourceResult =
			host.PrepareSourceDocument(result.previous);
		if (sourceResult != EditorSourceOperationResult::Success)
		{
			result.status = EditorViewChangeStatus::Failed;
			result.failure = SourceFailure(sourceResult,
				EditorViewChangeFailure::SourcePrepareFailed);
			return result;
		}
	}

	host.PrepareEditorViewPresentation(result.previous, target, plan);
	state.CommitTransition(target);
	if (plan.restoreTargetSelection)
		host.RestoreEditorViewSelection(target);
	host.CompleteEditorViewPresentation(result.previous, target);
	result.current = target;
	result.status = EditorViewChangeStatus::Success;
	return result;
}
