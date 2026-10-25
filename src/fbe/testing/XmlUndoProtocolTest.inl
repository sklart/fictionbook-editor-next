// Exercise the production parent unit with observable OLE child units.
#include "XmlUndoLifetimeProbe.h"

HRESULT RuntimeTests::CreateXmlUndoLifetimeProbe(XmlScriptBackend* backend, FB::Doc* document, IOleUndoUnit** result)
{
    if (!result) return E_POINTER;
    *result = nullptr;
    wchar_t mode[4] = {}, scenario[64] = {};
    if (::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", mode, _countof(mode)) != 1 || mode[0] != L'1' ||
        !::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO", scenario, _countof(scenario)) || wcscmp(scenario, L"xml-script-backend-runtime")) return E_ACCESSDENIED;
    if (!backend || !document) return E_POINTER;
    try {
        XmlVisualState state; state.Capture(document);
        CComObject<XmlScriptUndoUnit>* raw = nullptr;
        CheckError(CComObject<XmlScriptUndoUnit>::CreateInstance(&raw));
        CComPtr<IOleUndoUnit> owned(raw);
        raw->Initialize(backend, CString(), CString(), false, L"XML lifetime probe", state, state);
        return owned->QueryInterface(IID_IOleUndoUnit, reinterpret_cast<void**>(result));
    } catch (const _com_error& error) { return error.Error(); }
    catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
}

class XmlUndoProtocolProbe : public CComObjectRootEx<CComSingleThreadModel>, public IOleUndoUnit {
public:
    int nextAdds=0;
    BEGIN_COM_MAP(XmlUndoProtocolProbe)
        COM_INTERFACE_ENTRY(IOleUndoUnit)
    END_COM_MAP()
    STDMETHOD(Do)(IOleUndoManager*) { return S_OK; }
    STDMETHOD(GetDescription)(BSTR* value) { if(!value) return E_POINTER; *value=::SysAllocString(L"XML protocol probe"); return *value ? S_OK : E_OUTOFMEMORY; }
    STDMETHOD(GetUnitType)(CLSID* clsid,LONG* id) { if(!clsid || !id) return E_POINTER; *clsid=CLSID_NULL; *id=0; return S_OK; }
    STDMETHOD(OnNextAdd)() { ++nextAdds; return S_OK; }
};
static bool VerifyXmlUndoParentProtocol()
{
    wchar_t mode[4]={}, scenario[64]={};
    if(::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE",mode,_countof(mode))!=1 || mode[0]!=L'1' ||
        !::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO",scenario,_countof(scenario)) || wcscmp(scenario,L"xml-script-backend-runtime")) return true;
    CComObject<XmlScriptUndoUnit>* outerRaw=nullptr; CComObject<XmlScriptUndoUnit>* nestedRaw=nullptr;
    CComObject<XmlUndoProtocolProbe>* firstRaw=nullptr; CComObject<XmlUndoProtocolProbe>* secondRaw=nullptr;
    CheckError(CComObject<XmlScriptUndoUnit>::CreateInstance(&outerRaw)); CComPtr<IOleParentUndoUnit> outer(outerRaw);
    CheckError(CComObject<XmlScriptUndoUnit>::CreateInstance(&nestedRaw)); CComPtr<IOleParentUndoUnit> nested(nestedRaw);
    CheckError(CComObject<XmlUndoProtocolProbe>::CreateInstance(&firstRaw)); CComPtr<IOleUndoUnit> first(firstRaw);
    CheckError(CComObject<XmlUndoProtocolProbe>::CreateInstance(&secondRaw)); CComPtr<IOleUndoUnit> second(secondRaw);
    CheckError(nested->Add(first)); CheckError(nested->Add(second));
    if(firstRaw->nextAdds!=1 || secondRaw->nextAdds!=0) return false;
    CheckError(outer->Open(nested));
    if(outer->FindUnit(first)!=S_OK) return false; // Search the still-open nested group.
    CheckError(outer->Close(nested,TRUE));
    if(outer->FindUnit(first)!=S_OK || outer->FindUnit(second)!=S_OK || outer->FindUnit(nullptr)!=E_POINTER) return false;
    if(outer->Open(outer)!=E_INVALIDARG || outer->Open(nested)!=E_INVALIDARG || outer->Add(outer)!=E_INVALIDARG) return false;
    CheckError(outer->OnNextAdd());
    return secondRaw->nextAdds==1;
}
