#pragma once

#include <atlstr.h>
#include <msxml6.h>
#include "../view/EditorViewController.h"
#include "SourceDocumentTransfer.h"

class SourceEditorControl;
class EditorSelectionState;
namespace FB { class Doc; }
namespace FbeSourceDiagnostics { class SourceViewPhaseProfiler; }

class IBodySourceSelectionMapper
{
public:
	virtual ~IBodySourceSelectionMapper() = default;
	virtual void MapSourceSelectionToBody(FB::Doc& document,
		MSXML2::IXMLDOMDocumentPtr xml, const SourceDocumentText& source,
		EditorSelectionState& selection) = 0;
	virtual void MapBodySelectionToSource(FB::Doc& document,
		MSXML2::IXMLDOMDocumentPtr xml, SourceEditorControl& source,
		const CString& serializedSource, EditorSelectionState& selection,
		FbeSourceDiagnostics::SourceViewPhaseProfiler* profiler) = 0;
};

// Source-specific state and conversion boundary.  Presentation, document
// identity and validation UI remain with the application frame.
class SourceViewSession : public IEditorSourceExchange
{
public:
	SourceViewSession(SourceEditorControl& source, FB::Doc*& document,
		EditorSelectionState& selection, IBodySourceSelectionMapper& selectionMapper);

	EditorSourceOperationResult CommitSourceDocument() override;
	EditorSourceOperationResult PrepareSourceDocument(EditorView previous) override;

	void SetInterfaceLanguage(const CString& language);
	void SetSourceEncoding(const CString& encoding);
	void SetMemoryProfilingEnabled(bool enabled);
	bool DocumentChanged() const;
	const SourceDocumentApplyResult& LastApplyResult() const;

private:
	SourceEditorControl& m_source;
	FB::Doc*& m_document;
	EditorSelectionState& m_selection;
	IBodySourceSelectionMapper& m_selectionMapper;
	MSXML2::IXMLDOMDocumentPtr m_savedXml;
	CString m_interfaceLanguage;
	CString m_sourceEncoding;
	bool m_memoryProfilingEnabled;
	bool m_documentChanged;
	SourceDocumentApplyResult m_lastApplyResult;
};
