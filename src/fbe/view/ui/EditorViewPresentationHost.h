#pragma once

#include "../EditorViewController.h"
#include <atlsplit.h>

class CContainerWnd;
class SourceEditorControl;
class EditorSelectionState;
namespace FB { class Doc; }

struct EditorViewPresentationContext
{
	CContainerWnd& view;
	WTL::CSplitterWindow& splitter;
	SourceEditorControl& source;
	FB::Doc*& document;
	EditorSelectionState& selection;
	bool showDocumentTree;
};

// Presentation-only implementation: no document identity, persistence,
// persistence, extension or command-menu ownership belongs here.
class EditorViewPresentationHost : public IEditorViewPresentationHost
{
public:
	explicit EditorViewPresentationHost(const EditorViewPresentationContext& context);
	bool IsHtmlDocumentAvailable() const override;
	void SaveSelection(EditorView view) override;
	void PrepareEditorViewPresentation(EditorView previous, EditorView target,
		const EditorViewTransitionPlan& plan) override;
	void RestoreSelection(EditorView target) override;
	void CompleteEditorViewPresentation(EditorView previous, EditorView target) override;

private:
	EditorViewPresentationContext m_context;
};
