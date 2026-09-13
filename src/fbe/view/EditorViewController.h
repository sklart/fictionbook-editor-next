#pragma once

#include "EditorViewTransition.h"

// Coordinator boundary for a requested editor representation change.  The
// frame remains responsible for presentation effects; transition policy and
// its outcome are explicit at this boundary.
struct EditorViewChangeResult
{
	EditorView previous = EditorView::Body;
	EditorView current = EditorView::Body;
	EditorViewTransitionPlan plan;
	bool accepted = false;

	bool Succeeded() const { return accepted; }
};

class EditorViewController
{
public:
	EditorViewChangeResult Request(EditorView current, EditorView target) const;
};
