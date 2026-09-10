#include "stdafx.h"
#include "DocumentSession.h"

const DocumentLocation& DocumentSession::Location() const { return m_state.Location(); }
bool DocumentSession::IsArchive() const { return m_state.Location().IsArchive(); }
void DocumentSession::NewDocument() { m_state.NewDocument(); }
void DocumentSession::OpenNormal(const CString& path, FictionBookFileType documentType) { m_state.OpenNormal(path, documentType); }
void DocumentSession::OpenArchive(const DocumentLocation& location) { m_state.OpenArchive(location); }
void DocumentSession::SaveAsNormal(const CString& path, FictionBookFileType documentType) { m_state.SaveAsNormal(path, documentType); }
void DocumentSession::Saved() { m_state.Saved(); }
void DocumentSession::SavedArchive(const DocumentLocation& location) { m_state.OpenArchive(location); }
void DocumentSession::ReloadedNormal(const CString& path, FictionBookFileType documentType) { m_state.ReloadedNormal(path, documentType); }
void DocumentSession::RestoreArchive(const DocumentLocation& location) { m_state.RestoreArchive(location); }
void DocumentSession::AcceptExternalVersion() { m_state.AcceptExternalVersion(); }
