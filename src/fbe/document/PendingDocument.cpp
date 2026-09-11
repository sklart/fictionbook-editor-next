#include "stdafx.h"
#include "PendingDocument.h"
#include "..\mainfrm.h"
#include "..\FBDoc.h"

PendingDocument::PendingDocument(CMainFrame& frame, FB::Doc* previous)
	: m_previous(previous), m_document(new FB::Doc(frame)), m_state(Pending)
{
	FB::Doc::m_active_doc = m_document.get();
}

PendingDocument::~PendingDocument()
{
	if (m_state == Pending)
	{
		m_document.reset();
		FB::Doc::m_active_doc = m_previous;
	}
}

FB::Doc& PendingDocument::Document() const
{
	ATLASSERT(m_state == Pending && m_document.get() != NULL);
	return *m_document;
}

void PendingDocument::Rollback()
{
	ATLASSERT(m_state == Pending);
	if (m_state != Pending) return;
	m_document.reset();
	FB::Doc::m_active_doc = m_previous;
	m_state = RolledBack;
}

FB::Doc* PendingDocument::Commit()
{
	ATLASSERT(m_state == Pending && m_document.get() != NULL);
	if (m_state != Pending) return NULL;
	FB::Doc* committed = m_document.release();
	FB::Doc::m_active_doc = committed;
	m_state = Committed;
	return committed;
}
