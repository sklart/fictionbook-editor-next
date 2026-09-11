#pragma once

enum class EditorView
{
	Body,
	Description,
	Source
};

// Compatibility names are values of the independent state type.  NEXT is
// deliberately absent: Ctrl+Tab is a command, not an editor representation.
constexpr EditorView BODY = EditorView::Body;
constexpr EditorView DESC = EditorView::Description;
constexpr EditorView SOURCE = EditorView::Source;

class EditorViewState
{
public:
	EditorViewState();
	EditorView Current() const { return m_current; }
	EditorView Previous() const { return m_previous; }
	EditorView& CurrentRef() { return m_current; }
	EditorView& PreviousRef() { return m_previous; }
	EditorView LastCtrlTabView() const { return m_lastCtrlTab; }
	bool CtrlTabActive() const { return m_ctrlTabActive; }
	EditorView& LastCtrlTabViewRef() { return m_lastCtrlTab; }
	bool& CtrlTabActiveRef() { return m_ctrlTabActive; }
	void CommitTransition(EditorView target);
	void SetLastCtrlTabView(EditorView view) { m_lastCtrlTab = view; }
	void SetCtrlTabActive(bool active) { m_ctrlTabActive = active; }
	void Reset(EditorView current = EditorView::Body, EditorView previous = EditorView::Description);

private:
	EditorView m_current;
	EditorView m_previous;
	EditorView m_lastCtrlTab;
	bool m_ctrlTabActive;
};
