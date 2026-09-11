#pragma once

#include "EditorViewState.h"

struct EditorViewTransitionPlan
{
	bool commitSourceToDocument = false;
	bool prepareDocumentSource = false;
	bool saveCurrentSelection = true;
	bool restoreTargetSelection = true;
	bool leaveDescriptionMode = false;
	bool enterDescriptionMode = false;
};

EditorViewTransitionPlan MakeEditorViewTransitionPlan(EditorView from, EditorView to);
EditorView NextCtrlTabEditorView(EditorView current, EditorView previous,
	EditorView lastCtrlTab, bool ctrlTabActive);
