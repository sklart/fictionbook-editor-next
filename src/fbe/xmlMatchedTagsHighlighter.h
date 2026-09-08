//this file is part of notepad++
//Copyright (C)2003 Don HO <donho@altern.org>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef XMLMATCHEDTAGSHIGHLIGHTER_H
#define XMLMATCHEDTAGSHIGHLIGHTER_H

#pragma once

#include "Scintilla.h"
#include "XmlTagMatcher.h"

using namespace std;

class ScintillaDirectCall {
public:
	ScintillaDirectCall(CWindow* source) : m_source(source), m_directFunction(nullptr), m_directPointer(0) {
		if (m_source) {
			m_directFunction = reinterpret_cast<SciFnDirect>(m_source->SendMessage(SCI_GETDIRECTFUNCTION));
			m_directPointer = static_cast<sptr_t>(m_source->SendMessage(SCI_GETDIRECTPOINTER));
		}
	}

	LRESULT Call(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) const {
		if (m_directFunction && m_directPointer) {
			return static_cast<LRESULT>(m_directFunction(m_directPointer, message,
				static_cast<uptr_t>(wParam), static_cast<sptr_t>(lParam)));
		}
		return m_source->SendMessage(message, wParam, lParam);
	}

private:
	CWindow* m_source;
	SciFnDirect m_directFunction;
	sptr_t m_directPointer;
};

// wrapper class
class ScintillaEditView {
public:
	ScintillaEditView(CWindow* source) : m_directCall(source) {};

	LRESULT execute(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) {
		return m_directCall.Call(message, wParam, lParam);
	}

    int getCurrentDocLen() {
        return int(execute(SCI_GETLENGTH));
    };

	std::string getText() {
		const int length = getCurrentDocLen();
		std::string text(static_cast<size_t>(length) + 1, '\0');
		execute(SCI_GETTEXT, static_cast<WPARAM>(text.size()), reinterpret_cast<LPARAM>(&text[0]));
		text.resize(static_cast<size_t>(length));
		return text;
	}

	void clearIndicator(int indicatorNumber) { execute(SCI_SETINDICATORCURRENT, indicatorNumber); execute(SCI_INDICATORCLEARRANGE, 0, getCurrentDocLen()); }
	void getText(char *dest, int start, int end) { Sci_TextRange tr = {}; tr.chrg.cpMin = start; tr.chrg.cpMax = end; tr.lpstrText = dest; execute(SCI_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr)); }

	bool isShownIndentGuide()const {
		return false;
	}

private:
	ScintillaDirectCall m_directCall;
};

struct XmlMatchedTagsState {
	XmlMatchedTagsState() = default;
	struct IndicatorRange { int indicator; int start; int end; };
	vector<pair<int, int> > tagRanges;
	vector<pair<int, int> > attributeRanges;
	vector<IndicatorRange> diagnosticRanges;
	// Bumped only for SC_MOD_INSERTTEXT/SC_MOD_DELETETEXT.  UI notifications
	// must not cause a document read or a tokenizer rebuild.
	unsigned long long documentRevision = 1;
	unsigned long long matcherRevision = 0;
	int cachedCaret = -1;
	bool cachedMatch = false;
	bool cachedHighlightEnabled = false;
	int cachedHighlightMode = -1;
	bool cachedHighlightAttributes = false;
	bool cachedShowErrors = false;
	XmlTagMatcher* cachedMatcher = nullptr;
	~XmlMatchedTagsState() { delete cachedMatcher; }
	XmlMatchedTagsState(const XmlMatchedTagsState&) = delete;
	XmlMatchedTagsState& operator=(const XmlMatchedTagsState&) = delete;
	void Invalidate() { ++documentRevision; cachedCaret = -1; cachedMatch = false; }
};

enum class XmlTagHighlightMode { NameOnly, FullTag };
struct XmlTagHighlightOptions { bool enabled = false; XmlTagHighlightMode mode = XmlTagHighlightMode::NameOnly; bool highlightAttributes = false; bool showErrors = true; };

class XmlSourceTagHighlighter {
public:
	XmlSourceTagHighlighter(CWindow* source, XmlMatchedTagsState* state) : _state(state) { _pEditView = new ScintillaEditView(source); }
	~XmlSourceTagHighlighter() { delete _pEditView; }
	bool UpdateHighlight(const XmlTagHighlightOptions& options);
	bool GotoMatchingTag();
	void GotoWrongTag();
private:
	ScintillaEditView* _pEditView;
	XmlMatchedTagsState* _state;
	void ClearCurrentRanges();
	void ClearDiagnosticRanges();
	void FillRange(int indicator, const XmlByteRange& range, vector<pair<int, int> >& ranges);
	XmlTagMatcher& Matcher();
	void RefreshDiagnostics(const XmlTagHighlightOptions& options, XmlTagMatcher& matcher);
};

#endif //XMLMATCHEDTAGSHIGHLIGHTER_H
