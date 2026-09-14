#include "../../stdafx.h"
#include "EditorViewPresentationHost.h"
#include "../../ContainerWnd.h"
#include "../../apputils.h"
#include "../../utils/utils.h"
#include "../../FBDoc.h"
#include "../../source/ui/SourceEditorControl.h"
#include "../EditorSelectionState.h"
#include "../../StartupTrace.h"

EditorViewPresentationHost::EditorViewPresentationHost(const EditorViewPresentationContext& context) : m_context(context) {}

bool EditorViewPresentationHost::IsHtmlDocumentAvailable() const
{
	return m_context.document && m_context.document->m_body.HasDoc();
}

void EditorViewPresentationHost::SaveSelection(EditorView view)
{
	if (!IsHtmlDocumentAvailable() && (view == BODY || view == DESC))
	{
		StartupTrace::Warning(L"selection", L"E301", L"SaveSelection ignored: HTML document is unavailable");
		return;
	}
	if (view == BODY) m_context.selection.BodyRange() = m_context.document->m_body.Document()->selection->createRange();
	if (view == DESC) m_context.selection.DescriptionRange() = m_context.document->m_body.Document()->selection->createRange();
}

void EditorViewPresentationHost::PrepareEditorViewPresentation(EditorView previous, EditorView target,
	const EditorViewTransitionPlan& plan)
{
	if (plan.commitSourceToDocument) m_context.source.SendMessage(SCI_SETSAVEPOINT);
	if (previous != target && target != SOURCE)
		m_context.splitter.SetSinglePaneMode(m_context.showDocumentTree ? SPLIT_PANE_NONE : SPLIT_PANE_RIGHT);
	CComDispatchDriver body(m_context.document->m_body.Script()); CComVariant argument;
	if (plan.leaveDescriptionMode) { argument = false; CheckError(body.Invoke1(L"apiShowDesc", &argument)); }
	if (plan.enterDescriptionMode) { argument = true; CheckError(body.Invoke1(L"apiShowDesc", &argument)); }
	if (target == BODY || target == DESC) m_context.view.ActivateWnd(m_context.document->m_body);
	else {
		m_context.source.UpdateLineNumberMargin(false);
		m_context.view.HideActiveWnd(); m_context.splitter.SetSinglePaneMode(SPLIT_PANE_RIGHT);
		m_context.view.ActivateWnd(m_context.source);
		if (m_context.selection.BodySource().bodyToSourceTransferred) {
			m_context.source.SendMessage(SCI_SETSELECTIONSTART, m_context.selection.BodySource().sourceStart);
			m_context.source.SendMessage(SCI_SETSELECTIONEND, m_context.selection.BodySource().sourceEnd);
			m_context.source.SendMessage(SCI_SCROLLCARET);
		}
		if (previous == BODY) body.Invoke0(L"SaveBodyScroll");
	}
}

void EditorViewPresentationHost::RestoreSelection(EditorView target)
{
	if (target == BODY && (bool)m_context.selection.BodyRange()) m_context.selection.BodyRange()->select();
	if (target == DESC && (bool)m_context.selection.DescriptionRange()) m_context.selection.DescriptionRange()->select();
}

void EditorViewPresentationHost::CompleteEditorViewPresentation(EditorView previous, EditorView target)
{
	m_context.view.SetFocus();
	if (target == BODY && previous == SOURCE && m_context.selection.BodySource().sourceToBodyTransferred && (bool)m_context.selection.BodyRange())
		m_context.selection.BodyRange()->select();
	if (target != SOURCE || !m_context.selection.BodySource().bodyToSourceTransferred) return;
	m_context.source.SendMessage(SCI_SETSEL, m_context.selection.BodySource().sourceStart, m_context.selection.BodySource().sourceEnd);
	const int sourceLine = m_context.source.SendMessage(SCI_LINEFROMPOSITION, m_context.selection.BodySource().sourceStart);
	m_context.source.SendMessage(SCI_ENSUREVISIBLEENFORCEPOLICY, sourceLine);
	m_context.source.SendMessage(SCI_GOTOPOS, m_context.selection.BodySource().sourceStart);
	m_context.source.SendMessage(SCI_SETSEL, m_context.selection.BodySource().sourceStart, m_context.selection.BodySource().sourceEnd);
	m_context.source.SendMessage(SCI_SCROLLCARET);
	::PostMessage(m_context.source, SCI_ENSUREVISIBLEENFORCEPOLICY, sourceLine, 0);
	::PostMessage(m_context.source, SCI_GOTOPOS, m_context.selection.BodySource().sourceStart, 0);
	::PostMessage(m_context.source, SCI_SETSEL, m_context.selection.BodySource().sourceStart, m_context.selection.BodySource().sourceEnd);
	::PostMessage(m_context.source, SCI_SCROLLCARET, 0, 0);
}
