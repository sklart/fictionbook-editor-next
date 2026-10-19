#pragma once
#include "ScriptExecutionContext.h"
#include <map>
#include "../../common/RuntimeLocalizationCommon.h"
#include "Scintilla.h"

namespace FbeScripts {
inline CString ResourceToken()
{
    GUID guid = {}; CheckError(::CoCreateGuid(&guid));
    wchar_t value[40] = {}; ::StringFromGUID2(guid, value, _countof(value)); return CString(value);
}
struct RedrawState { unsigned count = 0; bool visible = false; };
inline std::map<HWND, RedrawState>& RedrawStates()
{ static thread_local std::map<HWND, RedrawState> states; return states; }
class DocumentUpdate {
    MSHTML::IMarkupServices2Ptr m_markup;
    HWND m_window;
    bool m_source, m_redraw = false, m_active = false;
public:
    CString token;
    DocumentUpdate(MSHTML::IMarkupServices2Ptr markup, HWND window, bool source, LPCWSTR action, bool redraw) :
        m_markup(markup), m_window(window), m_source(source), token(ResourceToken())
    {
        if (source) ::SendMessageW(window, SCI_BEGINUNDOACTION, 0, 0);
        else CheckError(m_markup->raw_BeginUndoUnit(const_cast<LPWSTR>(action)));
        m_active = true;
        try { if (redraw) {
            auto& state = RedrawStates()[window];
            if (state.count++ == 0) {
                state.visible = ::IsWindowVisible(window) != FALSE;
                if (state.visible) ::SendMessageW(window, WM_SETREDRAW, FALSE, 0);
            }
            m_redraw = true;
        } } catch (...) { Close(); throw; }
    }
    void Close()
    {
        if (!m_active) return;
        m_active = false;
        HRESULT error = S_OK;
        if (m_source) { if (::IsWindow(m_window)) ::SendMessageW(m_window, SCI_ENDUNDOACTION, 0, 0); }
        else error = m_markup->raw_EndUndoUnit();
        if (m_redraw) {
            auto& states = RedrawStates(); auto found = states.find(m_window);
            if (found != states.end() && --found->second.count == 0) {
                if (found->second.visible && ::IsWindow(m_window)) {
                    ::SendMessageW(m_window, WM_SETREDRAW, TRUE, 0);
                    ::RedrawWindow(m_window, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
                }
                states.erase(found);
            }
        }
        CheckError(error);
    }
    ~DocumentUpdate() { try { Close(); } catch (const _com_error&) {} }
};
struct SelectionBookmark {
    CString token, view;
    CComPtr<IUnknown> document, root;
    MSHTML::IHTMLTxtRangePtr range;
    MSHTML::IHTMLControlRangePtr controls;
    long revision = -1;
    sptr_t anchor = 0, caret = 0;
    std::string source;
};
struct ScriptDocumentResources : ScriptResourceLifetime {
    std::vector<SelectionBookmark> bookmarks;
    std::vector<std::unique_ptr<DocumentUpdate>> updates;
    ~ScriptDocumentResources() { while (!updates.empty()) updates.pop_back(); }
};
inline bool HasDocumentUpdates()
{
    for (const auto& context : ExecutionContexts()) {
        if (context.documentResources && !static_cast<ScriptDocumentResources*>(context.documentResources.get())->updates.empty()) return true;
    }
    return false;
}
inline ScriptDocumentResources& DocumentResources()
{
    auto& contexts = ExecutionContexts(); if (contexts.empty()) CheckError(E_UNEXPECTED);
    auto& state = contexts.back().documentResources;
    if (!state) state = std::make_shared<ScriptDocumentResources>();
    return *static_cast<ScriptDocumentResources*>(state.get());
}
inline std::string SourceBytes(HWND window)
{
    const auto length = ::SendMessageW(window, SCI_GETLENGTH, 0, 0);
    if (length < 0 || length > 64 * 1024 * 1024) CheckError(E_OUTOFMEMORY);
    std::vector<char> bytes(static_cast<size_t>(length) + 1);
    ::SendMessageW(window, SCI_GETTEXT, bytes.size(), reinterpret_cast<LPARAM>(bytes.data()));
    return std::string(bytes.data(), static_cast<size_t>(length));
}
inline CString FromUtf8(const std::string& bytes)
{
    if (bytes.empty()) return CString();
    const int length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), NULL, 0);
    if (!length) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
    CString text; ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), text.GetBuffer(length), length);
    text.ReleaseBuffer(length); return text;
}
inline CString EscapeHtml(CString text)
{ text.Replace(L"&", L"&amp;"); text.Replace(L"<", L"&lt;"); text.Replace(L">", L"&gt;"); text.Replace(L"\"", L"&quot;"); return text; }
}
