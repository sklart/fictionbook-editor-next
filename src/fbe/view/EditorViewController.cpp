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
	IEditorSourceExchange& source, IEditorViewPresentationHost& presentation,
	EditorView target) const
{
	EditorViewChangeResult result;
	result.previous = state.Current();
	result.current = result.previous;
	const EditorViewTransitionPlan plan =
		MakeEditorViewTransitionPlan(result.previous, target);

	if (plan.saveCurrentSelection)
		presentation.SaveSelection(result.previous);
	if (plan.commitSourceToDocument)
	{
		const EditorSourceOperationResult sourceResult = source.CommitSourceDocument();
		if (sourceResult != EditorSourceOperationResult::Success)
		{
			result.status = EditorViewChangeStatus::Rejected;
			result.failure = SourceFailure(sourceResult,
				EditorViewChangeFailure::SourceCommitFailed);
			return result;
		}
	}
	if ((target == EditorView::Body || target == EditorView::Description) &&
		!presentation.IsHtmlDocumentAvailable())
	{
		result.failure = EditorViewChangeFailure::HtmlUnavailable;
		return result;
	}
	if (plan.prepareDocumentSource)
	{
		const EditorSourceOperationResult sourceResult =
			source.PrepareSourceDocument(result.previous);
		if (sourceResult != EditorSourceOperationResult::Success)
		{
			result.status = EditorViewChangeStatus::Failed;
			result.failure = SourceFailure(sourceResult,
				EditorViewChangeFailure::SourcePrepareFailed);
			return result;
		}
	}

	presentation.PrepareEditorViewPresentation(result.previous, target, plan);
	state.CommitTransition(target);
	if (plan.restoreTargetSelection)
		presentation.RestoreSelection(target);
	presentation.CompleteEditorViewPresentation(result.previous, target);
	result.current = target;
	result.status = EditorViewChangeStatus::Success;
	return result;
}
