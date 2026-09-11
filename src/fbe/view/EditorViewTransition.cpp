#include "../stdafx.h"
#include "EditorViewTransition.h"

EditorViewTransitionPlan MakeEditorViewTransitionPlan(EditorView from, EditorView to)
{
	EditorViewTransitionPlan plan;
	plan.commitSourceToDocument = from == EditorView::Source && to != EditorView::Source;
	plan.prepareDocumentSource = from != EditorView::Source && to == EditorView::Source;
	plan.restoreTargetSelection = !(from == EditorView::Source && to == EditorView::Body);
	plan.leaveDescriptionMode = to == EditorView::Body;
	plan.enterDescriptionMode = to == EditorView::Description;
	return plan;
}

EditorView NextCtrlTabEditorView(EditorView current, EditorView previous,
	EditorView lastCtrlTab, bool ctrlTabActive)
{
	if(!ctrlTabActive && current != lastCtrlTab) return lastCtrlTab;
	if((previous == EditorView::Body && current == EditorView::Description) ||
		(previous == EditorView::Description && current == EditorView::Body)) return EditorView::Source;
	if((previous == EditorView::Body && current == EditorView::Source) ||
		(previous == EditorView::Source && current == EditorView::Body)) return EditorView::Description;
	if((previous == EditorView::Source && current == EditorView::Description) ||
		(previous == EditorView::Description && current == EditorView::Source)) return EditorView::Body;
	return current;
}
