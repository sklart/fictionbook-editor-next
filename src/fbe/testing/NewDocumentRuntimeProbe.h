#pragma once
#include <functional>
#include "../Speller.h"

// No retained COM pointers: observation must not keep an old document alive.
struct NewDocumentRuntimeProbe {
    static unsigned References(const CSpeller* speller) {
        if(!speller) return 0;
        return !!speller->m_browser+!!speller->m_doc2+!!speller->m_doc3+!!speller->m_doc4+
            !!speller->m_fbw_body+!!speller->m_scrollElement+!!speller->m_irs+!!speller->m_irsYo+
            !!speller->m_mkc+!!speller->m_ims+!!speller->m_ids+!!speller->m_ihrs+
            !!speller->m_prevSelRange+!!speller->m_selRange+!!speller->m_impStart+
            !!speller->m_impEnd+!!speller->m_undoSrv+!speller->m_ElementHighlights.empty();
    }
    static bool Attached(const CSpeller* speller, MSHTML::IHTMLDocument2* document) {
        return speller&&speller->m_doc2.GetInterfacePtr()==document;
    }
    static std::function<void(bool)>& Observer() { static thread_local std::function<void(bool)> observer; return observer; }
    static void Observe(bool detached) { if(Observer()) Observer()(detached); }
};
