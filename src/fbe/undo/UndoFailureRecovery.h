#pragma once
#include <vector>
#include <algorithm>

namespace FbeUndo {
inline const GUID& UnitClass() {
    static const GUID value = {0x5025683c,0x9a42,0x4621,{0x86,0x9b,0x0e,0xd7,0x90,0xba,0x64,0x2e}};
    return value; // private editor unit type; not the plug-in COM contract
}
inline HRESULT Inject(LPCWSTR point) {
    wchar_t mode[4] = {}, fault[80] = {};
    if (::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", mode, _countof(mode)) != 1 || mode[0] != L'1' ||
        !::GetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", fault, _countof(fault)) || wcscmp(fault, point)) return S_OK;
    ::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", nullptr);
    return E_FAIL; // one failing call; compensation can run
}
inline unsigned long long DocumentState(MSHTML::IHTMLDocument2Ptr document) {
    unsigned long long hash = 14695981039346656037ULL;
    auto bytes = [&](const void* data, size_t size) {
        const BYTE* first = static_cast<const BYTE*>(data);
        for (size_t i = 0; i < size; ++i) { hash ^= first[i]; hash *= 1099511628211ULL; }
    };
    auto string = [&](const _bstr_t& value) { bytes(static_cast<LPCWSTR>(value), value.length() * sizeof(wchar_t)); const BYTE end = 0; bytes(&end, 1); };
    // MSHTML may materialize the default OPTION selection while rebuilding
    // cover lists. That flag is not book content; current SELECT values are
    // compared separately below, including explicit cover references.
    CString markup(static_cast<LPCWSTR>(document->body->outerHTML));
    markup.Replace(L"<OPTION selected ", L"<OPTION ");
    markup.Replace(L"<OPTION selected>", L"<OPTION>");
    string(_bstr_t(markup));
    MSHTML::IHTMLElementCollectionPtr all(document->all);
    for (long i = 0; i < all->length; ++i) {
        IDispatchPtr element(all->item(i));
        CComQIPtr<MSHTML::IHTMLInputElement> input(element);
        CComQIPtr<MSHTML::IHTMLSelectElement> select(element);
        if (input) string(input->value);
        if (select) string(select->value);
    }
    MSHTML::IHTMLElementPtr binary(document->all->item(L"binobj"));
    MSHTML::IHTMLElementCollectionPtr nodes(MSHTML::IHTMLElement2Ptr(binary)->getElementsByTagName(L"DIV"));
    for (long i = 0; i < nodes->length; ++i) {
        _variant_t data(MSHTML::IHTMLElementPtr(nodes->item(i))->getAttribute(L"base64data", 2));
        if (data.vt != (VT_ARRAY | VT_UI1) || !data.parray) continue;
        long low = 0, high = -1; CheckError(::SafeArrayGetLBound(data.parray, 1, &low)); CheckError(::SafeArrayGetUBound(data.parray, 1, &high));
        BYTE* raw = nullptr; CheckError(::SafeArrayAccessData(data.parray, reinterpret_cast<void**>(&raw)));
        bytes(raw, static_cast<size_t>(high - low + 1));
        CheckError(::SafeArrayUnaccessData(data.parray));
    }
    return hash;
}

// Reinstall retained redo entries through the standard manager's undo state.
// No retained unit is executed and no document mutation is made here.
class RestoreRedo : public CComObjectRootEx<CComSingleThreadModel>, public IOleUndoUnit {
public:
    std::vector<CComPtr<IOleUndoUnit>> units;
    BEGIN_COM_MAP(RestoreRedo)
        COM_INTERFACE_ENTRY(IOleUndoUnit)
    END_COM_MAP()
    STDMETHOD(Do)(IOleUndoManager* manager) {
        // NULL means execution without recording an inverse. This unit only
        // records retained history, so no work is needed without a manager.
        if (manager) for (const auto& unit : units) { const HRESULT hr = manager->Add(unit); if (FAILED(hr)) return hr; }
        return S_OK;
    }
    STDMETHOD(GetDescription)(BSTR* value) { if (!value) return E_POINTER; *value = ::SysAllocString(L"Restore editor history"); return *value ? S_OK : E_OUTOFMEMORY; }
    STDMETHOD(GetUnitType)(CLSID* clsid, LONG* id) { if (!clsid || !id) return E_POINTER; *clsid = CLSID_NULL; *id = 0; return S_OK; }
    STDMETHOD(OnNextAdd)() { return S_OK; }
};
class HistoryCheckpoint {
    std::vector<CComPtr<IOleUndoUnit>> m_undo, m_redo;
    static std::vector<CComPtr<IOleUndoUnit>> Read(IOleUndoManager* manager, bool redo) {
        CComPtr<IEnumOleUndoUnits> list;
        CheckError(redo ? manager->EnumRedoable(&list) : manager->EnumUndoable(&list));
        std::vector<CComPtr<IOleUndoUnit>> result;
        for (;;) { CComPtr<IOleUndoUnit> unit; const HRESULT hr = list->Next(1, &unit, nullptr); if (hr == S_FALSE) break; CheckError(hr); result.push_back(unit); }
        return result;
    }
public:
    explicit HistoryCheckpoint(IOleUndoManager* manager) : m_undo(Read(manager, false)), m_redo(Read(manager, true)) {}
    // A canceled MSHTML paste can publish one empty native unit after its
    // event handler returns. Never confuse another edit with that unit.
    std::vector<CComPtr<IOleUndoUnit>> AddedUndoUnits(IOleUndoManager* manager, size_t maximum) const {
        const auto undo = Read(manager, false);
        if (undo.size() < m_undo.size() || undo.size() - m_undo.size() > maximum ||
            !std::equal(m_undo.begin(), m_undo.end(), undo.begin())) CheckError(E_UNEXPECTED);
        std::vector<CComPtr<IOleUndoUnit>> added(undo.begin() + m_undo.size(), undo.end());
        for (const auto& unit : added) {
            GUID clsid = {}; LONG id = 0;
            CheckError(unit->GetUnitType(&clsid, &id));
            if (clsid == UnitClass()) CheckError(E_UNEXPECTED);
        }
        return added;
    }
    CComPtr<IOleUndoUnit> AddedUndoUnit(IOleUndoManager* manager) const {
        const auto added = AddedUndoUnits(manager, 1);
        return added.empty() ? CComPtr<IOleUndoUnit>() : added.back();
    }
    bool Owned(bool redo) const {
        const auto& units = redo ? m_redo : m_undo;
        if (units.empty()) return false;
        GUID clsid = {}; LONG id = 0;
        return SUCCEEDED(units.back()->GetUnitType(&clsid, &id)) && clsid == UnitClass();
    }
    HRESULT Restore(IOleUndoManager* manager) {
        try {
            const auto undo = Read(manager, false), redo = Read(manager, true);
            if (undo == m_undo && redo == m_redo) return S_OK;
            // Never clear surviving native history. OLE clears both stacks
            // after a failed UndoTo/RedoTo and its compensating operation.
            if ((!undo.empty() && undo != m_undo) || !redo.empty()) return E_UNEXPECTED;
            DWORD state = 0; if (manager->GetOpenParentState(&state) != S_FALSE) return E_UNEXPECTED;
            CComObject<RestoreRedo>* raw = nullptr; CComPtr<IOleUndoUnit> replay;
            if (!m_redo.empty()) { CheckError(CComObject<RestoreRedo>::CreateInstance(&raw)); replay = raw; raw->units = m_redo; }
            if (undo.empty()) for (const auto& unit : m_undo) CheckError(manager->Add(unit));
            if (replay) { CheckError(manager->Add(replay)); CheckError(manager->UndoTo(replay)); }
            return Read(manager, false) == m_undo && Read(manager, true) == m_redo ? S_OK : E_UNEXPECTED;
        } catch (const _com_error& error) { return error.Error(); }
        catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    }
};
}
