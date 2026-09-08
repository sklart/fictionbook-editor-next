#pragma once

namespace Scintilla {
	class ILexer5;
}

bool LoadEditor();
Scintilla::ILexer5* CreateEditorLexer(const char* name);

enum EditorIndicator {
	// 22..25 were verified unused in FBE.  Keep 28 available for downstream
	// Scintilla integrations instead of assigning it here.
	EDITOR_INDICATOR_XML_TAG_INVALID = 22,
	EDITOR_INDICATOR_XML_TAG_MISMATCHED = 23,
	EDITOR_INDICATOR_XML_TAG_MISSING_OPENING = 24,
	EDITOR_INDICATOR_XML_TAG_MISSING_CLOSING = 25,
	EDITOR_INDICATOR_TAG_ATTRIBUTE = 26,
	EDITOR_INDICATOR_TAG_MATCH = 27
};
