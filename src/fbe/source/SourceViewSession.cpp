#include "../stdafx.h"
#include "SourceViewSession.h"
#include "SourceDocumentTransfer.h"
#include "SourceViewDiagnostics.h"
#include "ui/SourceEditorControl.h"
#include "../apputils.h"
#include "../utils/utils.h"
#include "../FBDoc.h"
#include "Scintilla.h"
#include "../view/EditorSelectionState.h"

SourceViewSession::SourceViewSession(SourceEditorControl& source, FB::Doc*& document,
	EditorSelectionState& selection, IBodySourceSelectionMapper& selectionMapper) :
	m_source(source), m_document(document), m_selection(selection), m_selectionMapper(selectionMapper),
	m_memoryProfilingEnabled(false), m_documentChanged(false)
{
}

void SourceViewSession::SetInterfaceLanguage(const CString& language)
{
	m_interfaceLanguage = language;
}

void SourceViewSession::SetSourceEncoding(const CString& encoding)
{
	m_sourceEncoding = encoding;
}

void SourceViewSession::SetMemoryProfilingEnabled(bool enabled)
{
	m_memoryProfilingEnabled = enabled;
}

bool SourceViewSession::DocumentChanged() const
{
	return m_documentChanged;
}

const SourceDocumentApplyResult& SourceViewSession::LastApplyResult() const
{
	return m_lastApplyResult;
}

EditorSourceOperationResult SourceViewSession::CommitSourceDocument()
{
	m_documentChanged = false;
	m_lastApplyResult = SourceDocumentApplyResult();
	m_selection.BodySource().sourceToBodyTransferred = false;
	SourceDocumentText sourceDocument;
	if(SourceDocumentTransfer::ReadSourceText(m_source, sourceDocument) != SourceTransitionResult::Success)
		return EditorSourceOperationResult::Failed;
	m_lastApplyResult = SourceDocumentTransfer::ApplySourceDocument(
		*m_document, sourceDocument, m_source.SendMessage(SCI_GETMODIFY) != 0,
		m_savedXml, m_interfaceLanguage);
	if(m_lastApplyResult.result == SourceTransitionResult::InvalidSource)
		return EditorSourceOperationResult::InvalidSource;
	if(m_lastApplyResult.result != SourceTransitionResult::Success)
		return EditorSourceOperationResult::Failed;
	m_documentChanged = m_lastApplyResult.documentChanged;
	m_selectionMapper.MapSourceSelectionToBody(*m_document, m_savedXml, sourceDocument, m_selection);
	m_document->MarkDocCP();
	return EditorSourceOperationResult::Success;
}

EditorSourceOperationResult SourceViewSession::PrepareSourceDocument(EditorView)
{
	m_documentChanged = false;
	m_selection.BodySource().bodyToSourceTransferred = false;
	CString sourceText;
	if(SourceDocumentTransfer::PrepareSerializedSource(*m_document, m_savedXml,
		m_sourceEncoding, sourceText) != SourceTransitionResult::Success)
		return EditorSourceOperationResult::Failed;
	if(m_document->DocRelChanged())
	{
		const DWORD byteCount = ::WideCharToMultiByte(CP_UTF8, 0, sourceText,
			sourceText.GetLength(), NULL, 0, NULL, NULL);
		std::vector<char> buffer(byteCount);
		if(!buffer.empty())
			::WideCharToMultiByte(CP_UTF8, 0, sourceText, sourceText.GetLength(),
				buffer.data(), byteCount, NULL, NULL);
		m_source.SendMessage(SCI_CLEARALL);
		if(!buffer.empty()) m_source.SendMessage(SCI_APPENDTEXT, byteCount,
			reinterpret_cast<LPARAM>(buffer.data()));
	}
	m_selectionMapper.MapBodySelectionToSource(*m_document, m_savedXml, m_source,
		sourceText, m_selection);
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	m_document->MarkDocCP();
	return EditorSourceOperationResult::Success;
}
