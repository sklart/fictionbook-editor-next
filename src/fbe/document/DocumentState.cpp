#include "stdafx.h"
#include "DocumentState.h"

DocumentState::DocumentState() {}
const DocumentLocation& DocumentState::Location() const { return m_location; }
void DocumentState::CloseDocument() { ResetDocumentLocation(m_location); m_hasDocument = false; }
void DocumentState::NewDocument() { ResetDocumentLocation(m_location); m_hasDocument = true; }
void DocumentState::OpenNormal(const CString& path, FictionBookFileType documentType) { m_location = CreateNormalDocumentLocation(path, documentType); m_hasDocument = true; }
void DocumentState::OpenArchive(const DocumentLocation& location) { m_location = location; m_hasDocument = true; }
void DocumentState::SaveAsNormal(const CString& path, FictionBookFileType documentType) { OpenNormal(path, documentType); }
void DocumentState::Saved() { UpdateDocumentLocationFingerprint(m_location); }
void DocumentState::ReloadedNormal(const CString& path, FictionBookFileType documentType) { OpenNormal(path, documentType); }
void DocumentState::RestoreArchive(const DocumentLocation& location) { OpenArchive(location); }
void DocumentState::AcceptExternalVersion() { UpdateDocumentLocationFingerprint(m_location); }
