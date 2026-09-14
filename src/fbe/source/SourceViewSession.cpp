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
#include <memory>

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

EditorSourceOperationResult SourceViewSession::PrepareSourceDocument(EditorView previous)
{
	std::unique_ptr<FbeSourceDiagnostics::SourceViewPhaseProfiler> phaseProfiler;
	if(m_memoryProfilingEnabled)
		phaseProfiler.reset(new FbeSourceDiagnostics::SourceViewPhaseProfiler(true));
	m_documentChanged = false;
	m_selection.BodySource().bodyToSourceTransferred = false;
	const bool transferBodySelection = previous == EditorView::Body;
	CString sourceText;
	if(SourceDocumentTransfer::PrepareSerializedSource(*m_document, m_savedXml,
		m_sourceEncoding, sourceText) != SourceTransitionResult::Success)
		return EditorSourceOperationResult::Failed;
	if(phaseProfiler) phaseProfiler->Mark("serialized source preparation");
	if(phaseProfiler) phaseProfiler->Mark("Unicode newline normalization");
	if(m_document->DocRelChanged())
	{
		const DWORD byteCount = ::WideCharToMultiByte(CP_UTF8, 0, sourceText,
			sourceText.GetLength(), NULL, 0, NULL, NULL);
		if(phaseProfiler) phaseProfiler->Mark("UTF-8 size calculation");
		m_source.SendMessage(SCI_CLEARALL);
		if(phaseProfiler) phaseProfiler->Mark("SCI_CLEARALL");
		int lineCount = 1;
		for (int index = 0; index < sourceText.GetLength(); ++index)
			if (sourceText[index] == L'\n') ++lineCount;
		m_source.SendMessage(SCI_ALLOCATELINES, lineCount);
		if(phaseProfiler) phaseProfiler->Mark("line count estimation and SCI_ALLOCATELINES");
		std::vector<char> buffer(byteCount);
		if(!buffer.empty())
		{
			::WideCharToMultiByte(CP_UTF8, 0, sourceText, sourceText.GetLength(),
				buffer.data(), byteCount, NULL, NULL);
			if(phaseProfiler) phaseProfiler->Mark("UTF-16 to UTF-8 conversion");
			m_source.SendMessage(SCI_APPENDTEXT, byteCount, reinterpret_cast<LPARAM>(buffer.data()));
			if(phaseProfiler) phaseProfiler->Mark("SCI_APPENDTEXT");
		}
	}
	if (transferBodySelection)
	{
		m_selectionMapper.MapBodySelectionToSource(*m_document, m_savedXml, m_source,
			sourceText, m_selection, phaseProfiler.get());
	}
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	if(phaseProfiler) phaseProfiler->Mark("SCI_EMPTYUNDOBUFFER");
	m_document->MarkDocCP();
	return EditorSourceOperationResult::Success;
}
