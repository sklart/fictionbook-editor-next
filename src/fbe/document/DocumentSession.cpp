#include "stdafx.h"
#include "DocumentSession.h"

DocumentState& DocumentSession::State() { return m_state; }
const DocumentState& DocumentSession::State() const { return m_state; }
void DocumentSession::NewDocument() { m_state.NewDocument(); }
void DocumentSession::OpenNormal(const CString& path, FictionBookFileType documentType) { m_state.OpenNormal(path, documentType); }
void DocumentSession::OpenArchive(const DocumentLocation& location) { m_state.OpenArchive(location); }
void DocumentSession::SaveAsNormal(const CString& path, FictionBookFileType documentType) { m_state.SaveAsNormal(path, documentType); }
void DocumentSession::Saved() { m_state.Saved(); }
void DocumentSession::ReloadedNormal(const CString& path, FictionBookFileType documentType) { m_state.ReloadedNormal(path, documentType); }
void DocumentSession::RestoreArchive(const DocumentLocation& location) { m_state.RestoreArchive(location); }
void DocumentSession::AcceptExternalVersion() { m_state.AcceptExternalVersion(); }
