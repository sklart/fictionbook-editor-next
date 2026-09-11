#include "../stdafx.h"
#include "EditorViewTransition.h"

EditorViewTransitionPlan MakeEditorViewTransitionPlan(EditorView from, EditorView to)
{
	EditorViewTransitionPlan plan;
	plan.commitSourceToDocument = from == EditorView::Source && to != EditorView::Source;
	plan.prepareDocumentSource = from != EditorView::Source && to == EditorView::Source;
	plan.restoreTargetSelection = !(from == EditorView::Source && to == EditorView::Body);
	plan.leaveDescriptionMode = from == EditorView::Description && to != EditorView::Description;
	plan.enterDescriptionMode = from != EditorView::Description && to == EditorView::Description;
	return plan;
}
