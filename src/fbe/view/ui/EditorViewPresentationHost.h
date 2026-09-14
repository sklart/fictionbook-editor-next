#pragma once

#include "../EditorViewController.h"
#include <atlsplit.h>

class CContainerWnd;
class SourceEditorControl;
class EditorSelectionState;
namespace FB { class Doc; }

// Keeps view transitions independent from the concrete WTL splitter class.
// The main frame supplies this small presentation-only adapter so it can use
// a themed splitter without leaking a concrete control type into this layer.
class IEditorViewSplitter
{
public:
	virtual ~IEditorViewSplitter() {}
	virtual void SetPresentationSinglePane(int pane) = 0;
};

struct EditorViewPresentationContext
{
	CContainerWnd& view;
	IEditorViewSplitter& splitter;
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
