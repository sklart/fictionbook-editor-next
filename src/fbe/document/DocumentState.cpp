#include "stdafx.h"
#include "DocumentState.h"

DocumentState::DocumentState() {}
const DocumentLocation& DocumentState::Location() const { return m_location; }
void DocumentState::NewDocument() { ResetDocumentLocation(m_location); }
void DocumentState::OpenNormal(const CString& path, FictionBookFileType documentType) { m_location = CreateNormalDocumentLocation(path, documentType); }
void DocumentState::OpenArchive(const DocumentLocation& location) { m_location = location; }
void DocumentState::SaveAsNormal(const CString& path, FictionBookFileType documentType) { OpenNormal(path, documentType); }
void DocumentState::Saved() { UpdateDocumentLocationFingerprint(m_location); }
void DocumentState::ReloadedNormal(const CString& path, FictionBookFileType documentType) { OpenNormal(path, documentType); }
void DocumentState::RestoreArchive(const DocumentLocation& location) { OpenArchive(location); }
void DocumentState::AcceptExternalVersion() { UpdateDocumentLocationFingerprint(m_location); }
