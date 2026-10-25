#include <atlbase.h>
#include <atlcom.h>
#include <mshtml.h>
#include <dispex.h>
#include <cstdio>
#include "binary/BinaryStore.h"

CComModule module;
namespace {
void CheckAt(HRESULT hr, int line) { if (FAILED(hr)) { printf("line=%d\n", line); throw hr; } }
#define Check(expression) CheckAt((expression), __LINE__)
void Require(bool condition) { if (!condition) throw E_FAIL; }
CComPtr<IHTMLElement> Element(IHTMLDocument2* doc, const wchar_t* id) {
    CComQIPtr<IHTMLDocument3> dom(doc); CComPtr<IHTMLElement> result;
    Check(dom->getElementById(CComBSTR(id), &result)); Require(!!result); return result;
}
CComVariant Bytes(BYTE first) {
    CComVariant result;
    result.vt = VT_ARRAY | VT_UI1; result.parray = ::SafeArrayCreateVector(VT_UI1, 0, 65536);
    Require(!!result.parray); void* data = nullptr;
    Check(::SafeArrayAccessData(result.parray, &data));
    memset(data, first, 65536); Check(::SafeArrayUnaccessData(result.parray)); return result;
}
void Equal(VARIANT& value, BYTE first) {
    Require(value.vt == (VT_ARRAY | VT_UI1)); void* data = nullptr;
    Check(::SafeArrayAccessData(value.parray, &data));
    const bool equal = static_cast<BYTE*>(data)[0] == first && static_cast<BYTE*>(data)[65535] == first;
    Check(::SafeArrayUnaccessData(value.parray)); Require(equal);
}
}
int wmain() {
    Check(::CoInitialize(nullptr));
    try {
        CComPtr<IHTMLDocument2> doc; Check(doc.CoCreateInstance(CLSID_HTMLDocument));
        CComQIPtr<IPersistStreamInit> persist(doc); Check(persist->InitNew());
        CComPtr<IHTMLElement> body; Check(doc->get_body(&body));
        Check(body->put_innerHTML(CComBSTR(L"<div id='a'></div><div id='b'></div>")));
        auto a = Element(doc, L"a"), b = Element(doc, L"b");
        CComDispatchDriver first(a), second(b);
        FbeBinary::BinaryStore store, other;
        Check(store.payloads.Attach(a)); Check(store.payloads.Attach(b));
        Require(store.payloads.Count() == 2 && other.payloads.Count() == 0);
        auto bytes = Bytes(41); Check(first.PutPropertyByName(L"base64data", &bytes));
        bytes.Clear(); // caller's transient array is gone
        store.Reconciled(); Require(!store.Dirty());
        CComVariant read; Check(first.GetPropertyByName(L"base64data", &read)); Equal(read, 41);
        Require(!store.Dirty()); // reads do not change generation/dirty state
        // Returned arrays are transient copies; altering one cannot alter owner.
        void* data = nullptr; Check(::SafeArrayAccessData(read.parray, &data));
        memset(data, 99, 65536); Check(::SafeArrayUnaccessData(read.parray));
        read.Clear(); Check(first.GetPropertyByName(L"base64data", &read)); Equal(read, 41);
        auto replacement = Bytes(42); Check(first.PutPropertyByName(L"base64data", &replacement));
        Require(store.Dirty()); read.Clear(); Check(first.GetPropertyByName(L"base64data", &read)); Equal(read, 42);
        CComVariant invalid(L"invalid"); Check(second.PutPropertyByName(L"base64data", &invalid));
        read.Clear(); Check(second.GetPropertyByName(L"base64data", &read)); Require(read.vt == VT_BSTR);
        // Reset releases canonical arrays even while old DOM nodes survive.
        const auto retained = store.payloads.Retain(store.payloads.Snapshot(a));
        const std::weak_ptr<const CComVariant> previous = retained->Get();
        Require(!previous.expired());
        store.payloads.Reset(); Require(store.payloads.Count() == 0);
        Require(previous.expired() && !retained->Get());
        read.Clear(); Require(first.GetPropertyByName(L"base64data", &read) == CO_E_OBJNOTCONNECTED);
        {
            FbeBinary::BinaryPayloadStore temporary;
            auto fresh = Element(doc, L"b");
            // A fresh node avoids installing a second facade on an old element.
            Check(doc->createElement(CComBSTR(L"DIV"), &fresh));
            Check(temporary.Attach(fresh)); second = CComDispatchDriver(fresh);
            Check(second.PutPropertyByName(L"base64data", &replacement));
        }
        read.Clear(); Require(second.GetPropertyByName(L"base64data", &read) == CO_E_OBJNOTCONNECTED);
        CComPtr<IHTMLElement> legacy; Check(doc->createElement(CComBSTR(L"DIV"), &legacy));
        CComDispatchDriver old(legacy);
        CComQIPtr<IDispatchEx> expando(legacy); DISPID property = DISPID_UNKNOWN;
        Check(expando->GetDispID(CComBSTR(L"base64data"), fdexNameEnsure, &property));
        DISPID put = DISPID_PROPERTYPUT;
        DISPPARAMS arguments = {&replacement, &put, 1, 1};
        Check(expando->InvokeEx(property, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &arguments, nullptr, nullptr, nullptr));
        Check(store.payloads.AdoptLegacy(legacy));
        Check(old.GetPropertyByName(L"base64data", &read)); Equal(read, 42);
        auto shared = store.payloads.Snapshot(legacy);
        Require(shared && shared->vt == (VT_ARRAY | VT_UI1));
        Check(store.payloads.Assign(legacy, shared));
        Require(store.payloads.Snapshot(legacy).get() == shared.get());
        const auto generation = store.payloads.Revision();
        auto latest = Bytes(77); Check(old.PutPropertyByName(L"base64data", &latest));
        Require(store.payloads.Revision() == generation + 1);
        const auto observed = store.payloads.Snapshot(legacy);
        Require(observed && observed.get() != shared.get());
        CComVariant copied; Check(::VariantCopy(&copied, const_cast<CComVariant*>(observed.get()))); Equal(copied, 77);
        puts("C3 native MSHTML ownership, read/write, isolation and lifetime: PASS");
    } catch (HRESULT hr) { printf("C3 native FAIL: %08lx\n", hr); return 1; }
    ::CoUninitialize(); return 0;
}
