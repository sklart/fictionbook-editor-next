#include "../stdafx.h"
#include "EditorViewController.h"

EditorViewChangeResult EditorViewController::Request(EditorView current, EditorView target) const
{
	EditorViewChangeResult result;
	result.previous = current;
	result.current = target;
	result.plan = MakeEditorViewTransitionPlan(current, target);
	result.accepted = true;
	return result;
}
