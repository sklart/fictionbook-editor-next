#include "../stdafx.h"
#include "EditorViewState.h"

EditorViewState::EditorViewState()
{
	Reset();
}

void EditorViewState::CommitTransition(EditorView target)
{
	m_previous = m_current;
	m_current = target;
}

void EditorViewState::Reset(EditorView current, EditorView previous)
{
	m_current = current;
	m_previous = previous;
	m_lastCtrlTab = EditorView::Description;
	m_ctrlTabActive = false;
}
