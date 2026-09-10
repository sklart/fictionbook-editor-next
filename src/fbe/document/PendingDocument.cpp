#include "stdafx.h"
#include "PendingDocument.h"
#include "..\mainfrm.h"
#include "..\FBDoc.h"

PendingDocument::PendingDocument(CMainFrame& frame, FB::Doc* previous)
	: m_previous(previous), m_document(new FB::Doc(frame))
{
	FB::Doc::m_active_doc = m_document.get();
}

PendingDocument::~PendingDocument()
{
	FB::Doc::m_active_doc = m_previous;
}

FB::Doc& PendingDocument::Document() const
{
	return *m_document;
}

void PendingDocument::Rollback()
{
	m_document.reset();
	FB::Doc::m_active_doc = m_previous;
}

FB::Doc* PendingDocument::Commit()
{
	FB::Doc* committed = m_document.release();
	FB::Doc::m_active_doc = committed;
	return committed;
}
