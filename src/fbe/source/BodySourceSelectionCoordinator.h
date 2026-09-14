#pragma once

#include "SourceViewSession.h"

// Coordinates the existing DOM/XML selection mapping.  Persistent ranges and
// transfer flags remain owned by EditorSelectionState.
class BodySourceSelectionCoordinator : public IBodySourceSelectionMapper
{
public:
	void MapSourceSelectionToBody(FB::Doc& document,
		MSXML2::IXMLDOMDocumentPtr xml, const SourceDocumentText& source,
		EditorSelectionState& selection) override;
	void MapBodySelectionToSource(FB::Doc& document,
		MSXML2::IXMLDOMDocumentPtr xml, SourceEditorControl& source,
		const CString& serializedSource, EditorSelectionState& selection,
		FbeSourceDiagnostics::SourceViewPhaseProfiler* profiler) override;
};
