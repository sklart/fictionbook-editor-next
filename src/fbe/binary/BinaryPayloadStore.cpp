#include "BinaryPayloadStore.h"
#include "BinaryStore.h"
#include <mshtml.h>
#include <cstring>

namespace FbeBinary {
namespace {
bool SameByteArray(const CComVariant& left, const CComVariant& right) {
    if (left.vt != (VT_ARRAY | VT_UI1) || right.vt != left.vt || !left.parray || !right.parray ||
        left.parray->cDims != 1 || right.parray->cDims != 1 ||
        left.parray->cbElements != 1 || right.parray->cbElements != 1 ||
        left.parray->rgsabound[0].lLbound != right.parray->rgsabound[0].lLbound ||
        left.parray->rgsabound[0].cElements != right.parray->rgsabound[0].cElements) return false;
    void* a = nullptr; void* b = nullptr;
    if (FAILED(::SafeArrayAccessData(left.parray, &a))) return false;
    if (FAILED(::SafeArrayAccessData(right.parray, &b))) { ::SafeArrayUnaccessData(left.parray); return false; }
    const auto count = left.parray->rgsabound[0].cElements;
    const bool same = !count || std::memcmp(a, b, count) == 0;
    ::SafeArrayUnaccessData(right.parray); ::SafeArrayUnaccessData(left.parray);
    return same;
}
class ATL_NO_VTABLE PayloadBehavior : public CComObjectRootEx<CComSingleThreadModel>,
    public IElementBehavior, public IElementBehaviorFactory, public IDispatch {
public:
    BEGIN_COM_MAP(PayloadBehavior)
        COM_INTERFACE_ENTRY(IElementBehavior)
        COM_INTERFACE_ENTRY(IElementBehaviorFactory)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()
    std::weak_ptr<BinaryPayloadStore::State> state;
    unsigned long long key = 0;
    IUnknown* identity = nullptr; // identity only; no reference back to the DOM
    ~PayloadBehavior() {
        if (auto owner = state.lock()) { owner->values.erase(key); owner->identities.erase(identity); owner->revisions.erase(key); }
    }
    STDMETHOD(Init)(IElementBehaviorSite*) { return S_OK; }
    STDMETHOD(Notify)(LONG, VARIANT*) { return S_OK; }
    STDMETHOD(Detach)() { return S_OK; }
    STDMETHOD(FindBehavior)(BSTR, BSTR, IElementBehaviorSite*, IElementBehavior** result) {
        return QueryInterface(__uuidof(IElementBehavior), reinterpret_cast<void**>(result));
    }
    STDMETHOD(GetTypeInfoCount)(UINT* count) {
        if (!count) return E_POINTER;
        *count = 0; return S_OK;
    }
    STDMETHOD(GetTypeInfo)(UINT, LCID, ITypeInfo**) { return E_NOTIMPL; }
    STDMETHOD(GetIDsOfNames)(REFIID, LPOLESTR* names, UINT count, LCID, DISPID* ids) {
        if (!names || !ids) return E_POINTER;
        if (count != 1 || !names[0] || _wcsicmp(names[0], L"base64data")) return DISP_E_UNKNOWNNAME;
        *ids = 1; return S_OK;
    }
    STDMETHOD(Invoke)(DISPID id, REFIID, LCID, WORD flags, DISPPARAMS* args,
        VARIANT* result, EXCEPINFO*, UINT*) {
        if (id != 1) return DISP_E_MEMBERNOTFOUND;
        auto owner = state.lock();
        if (!owner) return CO_E_OBJNOTCONNECTED;
        const auto found = owner->values.find(key);
        if (found == owner->values.end()) return CO_E_OBJNOTCONNECTED;
        if (flags & DISPATCH_PROPERTYGET) {
            FBE_BINARY_DIAG_ADD(facadeGets, 1);
#ifdef FBE_BINARY_STORE_DIAGNOSTICS
            FBE_BINARY_DIAG_ADD(facadeGetBytes, DiagnosticVariantBytes(*found->second));
#endif
            if (!result) return E_POINTER;
            return ::VariantCopy(result, const_cast<CComVariant*>(found->second.get()));
        }
        if (flags & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) {
            FBE_BINARY_DIAG_SCOPE(Population); FBE_BINARY_DIAG_ADD(facadePuts, 1);
            if (!args || args->cArgs != 1 || !args->rgvarg) return DISP_E_BADPARAMCOUNT;
            try {
                // Preserve the legacy expando's value types, including invalid
                // values: validation belongs to the operation consuming bytes.
                auto value = std::make_shared<CComVariant>();
                const HRESULT hr = ::VariantCopyInd(value.get(), &args->rgvarg[0]);
                if (FAILED(hr)) return hr;
#ifdef FBE_BINARY_STORE_DIAGNOSTICS
                FBE_BINARY_DIAG_ADD(putCopyBytes, DiagnosticVariantBytes(*value));
#endif
                // BuildBinaryControls writes its already assigned bytes back
                // through the facade. Keep the canonical Undo snapshot in that
                // case; the materialized incoming copy is only transient.
                if (SameByteArray(*found->second, *value)) { FBE_BINARY_DIAG_ADD(sameValuePuts, 1); return S_OK; }
                found->second = std::move(value);
                owner->revisions.at(key) = ++owner->revision;
                return S_OK;
            } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
        }
        return DISP_E_MEMBERNOTFOUND;
    }
};
HRESULT AttachBehavior(const std::shared_ptr<BinaryPayloadStore::State>& state, IDispatch* element, long* attachedCookie) {
    FBE_BINARY_DIAG_SCOPE(BehaviorAttach); FBE_BINARY_DIAG_ADD(attachCalls, 1);
    if (!element) return E_POINTER;
    CComQIPtr<IHTMLElement2> node(element);
    if (!node) return E_NOINTERFACE;
    CComPtr<IUnknown> identity;
    HRESULT hr = element->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity));
    if (FAILED(hr)) return hr;
    if (state->identities.count(identity)) return S_FALSE;
    CComObject<PayloadBehavior>* raw = nullptr;
    hr = CComObject<PayloadBehavior>::CreateInstance(&raw);
    if (FAILED(hr)) return hr;
    CComPtr<IElementBehaviorFactory> factory(raw);
    try {
        const auto key = ++state->next;
        state->values.emplace(key, std::make_shared<CComVariant>());
        raw->state = state; raw->key = key; raw->identity = identity;
        state->revisions.emplace(key, state->revision);
        state->identities.emplace(identity, key);
        CComVariant argument(static_cast<IUnknown*>(factory));
        long cookie = 0;
        // A supplied factory requires neither registration nor an external HTC.
        hr = node->addBehavior(CComBSTR(L"#default#fbe-binary"), &argument, &cookie);
        if (SUCCEEDED(hr) && attachedCookie) *attachedCookie = cookie;
#ifdef FBE_BINARY_STORE_DIAGNOSTICS
        if (SUCCEEDED(hr)) { state->behaviorCookies.emplace(key,cookie); FBE_BINARY_DIAG_ADD(behaviorAttachments,1); }
#endif
        return hr;
    } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
}
}
HRESULT BinaryPayloadStore::Attach(IDispatch* element) { return AttachBehavior(m_state, element, nullptr); }
HRESULT BinaryPayloadStore::AdoptLegacy(IDispatch* element) {
    FBE_BINARY_DIAG_ADD(adoptCalls, 1);
    if (Snapshot(element)) return S_FALSE;
    CComDispatchDriver driver(element); CComVariant value;
    HRESULT hr = driver.GetPropertyByName(L"base64data", &value);
    if (FAILED(hr)) return hr;
    CComQIPtr<IHTMLElement> node(element);
    if (!node) return E_NOINTERFACE;
    try {
        auto payload = std::make_shared<CComVariant>();
        hr = payload->Attach(&value); if (FAILED(hr)) return hr;
        // Install and initialize first. A failed allocation/attachment leaves
        // the original expando untouched, instead of deleting the only value.
        long cookie = 0;
        hr = AttachBehavior(m_state, element, &cookie); if (FAILED(hr)) return hr;
        hr = Assign(element, payload);
        VARIANT_BOOL removed = VARIANT_FALSE;
        if (SUCCEEDED(hr)) hr = node->removeAttribute(CComBSTR(L"base64data"), 0, &removed);
        if (FAILED(hr)) {
            CComQIPtr<IHTMLElement2> behaviorNode(element); VARIANT_BOOL detached = VARIANT_FALSE;
            if (behaviorNode) behaviorNode->removeBehavior(cookie, &detached);
        }
        return hr;
    } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
}
std::shared_ptr<const CComVariant> BinaryPayloadStore::Snapshot(IDispatch* element) const {
    CComPtr<IUnknown> identity;
    if (!element || FAILED(element->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity)))) return {};
    const auto token = m_state->identities.find(identity);
    if (token == m_state->identities.end()) return {};
    const auto value = m_state->values.find(token->second);
    return value == m_state->values.end() ? nullptr : value->second;
}
HRESULT BinaryPayloadStore::Assign(IDispatch* element, const std::shared_ptr<const CComVariant>& value) {
    FBE_BINARY_DIAG_ADD(assignCalls, 1);
    if (!value) return E_INVALIDARG;
    CComPtr<IUnknown> identity;
    if (!element) return E_POINTER;
    HRESULT hr = element->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity));
    if (FAILED(hr)) return hr;
    const auto token = m_state->identities.find(identity);
    if (token == m_state->identities.end()) return CO_E_OBJNOTCONNECTED;
    m_state->values.at(token->second) = value;
    m_state->revisions.at(token->second) = ++m_state->revision;
    return S_OK;
}
bool BinaryPayloadStore::ChangedSince(IDispatch* element, unsigned long long revision) const {
    CComPtr<IUnknown> identity;
    if (!element || FAILED(element->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity)))) return true;
    const auto token = m_state->identities.find(identity);
    if (token == m_state->identities.end()) return true;
    const auto changed = m_state->revisions.find(token->second);
    return changed == m_state->revisions.end() || changed->second > revision;
}
void BinaryStore::RefreshPayloadMetadata() {
    // A facade write changes bytes only. Preserve the ID catalog and metadata
    // of all other entries; rebuilding it would decode every unchanged image.
    for (auto& entry : m_entries) {
        if (!payloads.ChangedSince(entry.second.element, m_payloadRevision)) continue;
        auto& metadata = entry.second.metadata;
        FBE_BINARY_DIAG_ADD(metadataInvalidations, 1);
        metadata.sizeKnown = metadata.dimensionsKnown = metadata.dimensionsAttempted = false;
        metadata.width = metadata.height = 0;
        metadata.generation = ++m_generation;
    }
    Reconciled();
}
}
#ifdef FBE_BINARY_STORE_DIAGNOSTICS
namespace FbeBinary {
size_t BinaryPayloadStore::DiagnosticBehaviorObjectSize() { return sizeof(CComObject<PayloadBehavior>); }
PayloadDiagnosticStats BinaryPayloadStore::DiagnosticStatistics() const {
    PayloadDiagnosticStats result;
    std::unordered_set<const CComVariant*> current, history, all;
    std::unordered_set<const void*> currentBlocks, allBlocks;
    result.currentEntries=m_state->values.size(); result.historyEntries=m_state->history.size();
    for(const auto& entry:m_state->values){const auto* value=entry.second.get(); if(!value) continue;
        if(value->vt==(VT_ARRAY|VT_UI1)&&value->parray&&value->parray->pvData){currentBlocks.insert(value->parray->pvData);allBlocks.insert(value->parray->pvData);}
        if(current.insert(value).second) result.canonicalBytes+=DiagnosticVariantBytes(*value);
        if(all.insert(value).second){result.uniqueBytes+=DiagnosticVariantBytes(*value);result.sharedReferences+=static_cast<unsigned long long>(entry.second.use_count());}
    }
    for(const auto& entry:m_state->history){const auto* value=entry.second.get();if(!value) continue;
        if(value->vt==(VT_ARRAY|VT_UI1)&&value->parray&&value->parray->pvData)allBlocks.insert(value->parray->pvData);
        if(history.insert(value).second){result.historyReferencedBytes+=DiagnosticVariantBytes(*value);if(!current.count(value))result.historyOnlyBytes+=DiagnosticVariantBytes(*value);}
        if(all.insert(value).second){result.uniqueBytes+=DiagnosticVariantBytes(*value);result.sharedReferences+=static_cast<unsigned long long>(entry.second.use_count());}
    }
    result.uniquePayloadBlocks=allBlocks.size();result.canonicalPayloadBlocks=currentBlocks.size();
    result.uniqueCurrentObjects=current.size();result.uniqueHistoryObjects=history.size();result.uniqueObjects=all.size();return result;
}
bool BinaryPayloadStore::DiagnosticHistoryReferences(const CComVariant* value) const { for(const auto& entry:m_state->history)if(entry.second.get()==value)return true;return false; }
HRESULT BinaryPayloadStore::DiagnosticDetach(IDispatch* element) {
    CComPtr<IUnknown> identity; if(!element) return E_POINTER;
    HRESULT hr=element->QueryInterface(IID_IUnknown,reinterpret_cast<void**>(&identity));if(FAILED(hr))return hr;
    const auto token=m_state->identities.find(identity);if(token==m_state->identities.end())return E_INVALIDARG;
    const auto cookie=m_state->behaviorCookies.find(token->second);if(cookie==m_state->behaviorCookies.end())return E_INVALIDARG;
    CComQIPtr<IHTMLElement2> node(element);if(!node)return E_NOINTERFACE;
    VARIANT_BOOL removed=VARIANT_FALSE;hr=node->removeBehavior(cookie->second,&removed);
    return FAILED(hr)?hr:(removed==VARIANT_TRUE?S_OK:S_FALSE);
}
}
#endif
