#pragma once

namespace Scintilla {
	class ILexer5;
}

bool LoadEditor();
Scintilla::ILexer5* CreateEditorLexer(const char* name);

enum EditorIndicator {
	EDITOR_INDICATOR_TAG_ATTRIBUTE = 26,
	EDITOR_INDICATOR_TAG_MATCH = 27
};
