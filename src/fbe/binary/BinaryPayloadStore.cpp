#include "BinaryPayloadStore.h"
#include <mshtml.h>

namespace FbeBinary {
namespace {
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
        if (auto owner = state.lock()) { owner->values.erase(key); owner->identities.erase(identity); }
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
            if (!result) return E_POINTER;
            return ::VariantCopy(result, const_cast<CComVariant*>(found->second.get()));
        }
        if (flags & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) {
            if (!args || args->cArgs != 1 || !args->rgvarg) return DISP_E_BADPARAMCOUNT;
            try {
                // Preserve the legacy expando's value types, including invalid
                // values: validation belongs to the operation consuming bytes.
                auto value = std::make_shared<CComVariant>();
                const HRESULT hr = ::VariantCopyInd(value.get(), &args->rgvarg[0]);
                if (FAILED(hr)) return hr;
                found->second = std::move(value);
                ++owner->revision;
                return S_OK;
            } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
        }
        return DISP_E_MEMBERNOTFOUND;
    }
};
HRESULT AttachBehavior(const std::shared_ptr<BinaryPayloadStore::State>& state, IDispatch* element, long* attachedCookie) {
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
        state->identities.emplace(identity, key);
        CComVariant argument(static_cast<IUnknown*>(factory));
        long cookie = 0;
        // A supplied factory requires neither registration nor an external HTC.
        hr = node->addBehavior(CComBSTR(L"#default#fbe-binary"), &argument, &cookie);
        if (SUCCEEDED(hr) && attachedCookie) *attachedCookie = cookie;
        return hr;
    } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
}
}
HRESULT BinaryPayloadStore::Attach(IDispatch* element) { return AttachBehavior(m_state, element, nullptr); }
HRESULT BinaryPayloadStore::AdoptLegacy(IDispatch* element) {
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
    if (!value) return E_INVALIDARG;
    CComPtr<IUnknown> identity;
    if (!element) return E_POINTER;
    HRESULT hr = element->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity));
    if (FAILED(hr)) return hr;
    const auto token = m_state->identities.find(identity);
    if (token == m_state->identities.end()) return CO_E_OBJNOTCONNECTED;
    m_state->values.at(token->second) = value;
    ++m_state->revision;
    return S_OK;
}
}
