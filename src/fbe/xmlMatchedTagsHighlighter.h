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

	void clearIndicator(int indicatorNumber) {
		int docStart = 0;
		int docEnd = getCurrentDocLen();
		execute(SCI_SETINDICATORCURRENT, indicatorNumber);
		execute(SCI_INDICATORCLEARRANGE, docStart, docEnd-docStart);
	};

	void getText(char *dest, int start, int end) {
		Sci_TextRange tr;
		tr.chrg.cpMin = start;
		tr.chrg.cpMax = end;
		tr.lpstrText = dest;
		execute(SCI_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr));
	}

	bool isShownIndentGuide()const {
		return false;
	}

private:
	ScintillaDirectCall m_directCall;
};

enum TagCateg {tagOpen, tagClose, inSingleTag, outOfTag, invalidTag, unknownPb};

typedef pair<CString, int> TAG;
typedef vector<TAG>::iterator tagIterator;

struct XmlMatchedTagsState {
	vector<pair<int, int> > tagRanges;
	vector<pair<int, int> > attributeRanges;
};


class XmlMatchedTagsHighlighter {
public:
	XmlMatchedTagsHighlighter(CWindow* source, XmlMatchedTagsState* state) : _state(state) {
	  _pEditView = new ScintillaEditView(source);
	};
	~XmlMatchedTagsHighlighter() { delete _pEditView; }
	XmlMatchedTagsHighlighter(const XmlMatchedTagsHighlighter&) = delete;
	XmlMatchedTagsHighlighter& operator=(const XmlMatchedTagsHighlighter&) = delete;
	bool tagMatch(bool doHilite, bool doHiliteAttr, bool gotoTag);
	void gotoWrongTag();
	
private:
	struct XmlMatchedTagsPos {
		int tagOpenStart;
		int tagNameEnd;
		int tagOpenEnd;

		int tagCloseStart;
		int tagCloseEnd;
	};
	
	ScintillaEditView *_pEditView;
	XmlMatchedTagsState* _state;

	int getFirstTokenPosFrom(int targetStart, int targetEnd, const char *token, std::pair<int, int> & foundPos);
	TagCateg getTagCategory(XmlMatchedTagsPos & tagsPos, int curPos);
	bool getMatchedTagPos(int searchStart, int searchEnd, const char *tag2find, const char *oppositeTag2find, vector<int> oppositeTagFound, XmlMatchedTagsPos & tagsPos);
	bool getXmlMatchedTagsPos(XmlMatchedTagsPos & tagsPos);
	vector< pair<int, int> > getAttributesPos(int start, int end);
	bool isInList(int element, vector<int> elementList) {
		for (size_t i = 0 ; i < elementList.size() ; i++)
			if (element == elementList[i])
				return true;
		return false;
	};
	vector< pair<CString, int> > lookupTags();
};

#endif //XMLMATCHEDTAGSHIGHLIGHTER_H
