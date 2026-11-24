#include <windows.h>
#include <mshtml.h>
#include <atlbase.h>
#include <atlcom.h>
#include <cstdio>

struct Sink final : IHTMLChangeSink {
    unsigned notifications = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (id != IID_IUnknown && id != __uuidof(IHTMLChangeSink)) return E_NOINTERFACE;
        *out = static_cast<IHTMLChangeSink*>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE Notify() override { ++notifications; return S_OK; }
};

static bool check(HRESULT hr, const char* what) {
    if (SUCCEEDED(hr)) return true;
    std::printf("error=%s hr=0x%08lx\n", what, static_cast<unsigned long>(hr));
    return false;
}

static void scope(IMarkupPointer* p, char* name, size_t capacity) {
    CComPtr<IHTMLElement> element;
    const HRESULT hr = p->CurrentScope(&element);
    CComBSTR tag;
    if (FAILED(hr) || !element || FAILED(element->get_tagName(&tag)) || !tag) {
        sprintf_s(name, capacity, "unavailable:%08lx", static_cast<unsigned long>(hr)); return;
    }
    WideCharToMultiByte(CP_UTF8, 0, tag, -1, name, static_cast<int>(capacity), nullptr, nullptr);
}

static bool run() {
    CComPtr<IHTMLDocument2> doc;
    if (!check(doc.CoCreateInstance(CLSID_HTMLDocument), "create-document")) return false;
    CComQIPtr<IPersistStreamInit> init(doc);
    if (!init || !check(init->InitNew(), "init-document")) return false;
    SAFEARRAY* text = SafeArrayCreateVector(VT_VARIANT, 0, 1);
    if (!text) return false;
    CComVariant html(L"<html><head><script id='userCmd'></script></head><body><div id='book'>first</div><input id='field' value='first'></body></html>");
    LONG index = 0; HRESULT hr = SafeArrayPutElement(text, &index, &html);
    if (SUCCEEDED(hr)) hr = doc->write(text);
    SafeArrayDestroy(text);
    if (!check(hr, "write-document") || !check(doc->close(), "close-document")) return false;
    CComQIPtr<IHTMLDocument3> doc3(doc);
    CComQIPtr<IMarkupContainer2> container(doc);
    CComQIPtr<IMarkupServices> services(doc);
    if (!doc3 || !container || !services) return false;
    CComPtr<IHTMLElement> book, field;
    if (!check(doc3->getElementById(CComBSTR(L"book"), &book), "find-book") || !book ||
        !check(doc3->getElementById(CComBSTR(L"field"), &field), "find-field") || !field) return false;
    for (int mode = 0; mode != 4; ++mode) {
        const char* label[] = {"head", "body", "head-plus-body", "input-value"};
        CComPtr<IHTMLElement> previous, fresh;
        if (!check(doc3->getElementById(CComBSTR(L"userCmd"), &previous), "find-script") || !previous ||
            !check(doc->createElement(CComBSTR(L"script"), &fresh), "create-script") ||
            !check(fresh->put_id(CComBSTR(L"userCmd")), "set-id")) return false;
        CComQIPtr<IHTMLDOMNode> oldNode(previous), newNode(fresh);
        CComPtr<IHTMLDOMNode> parent;
        if (!oldNode || !newNode || !check(oldNode->get_parentNode(&parent), "parent") || !parent) return false;
        Sink first, second; DWORD cookieA = 0, cookieB = 0;
        if (!check(container->RegisterForDirtyRange(&first, &cookieA), "register-a")) return false;
        if (!check(container->RegisterForDirtyRange(&second, &cookieB), "register-b")) {
            container->UnRegisterForDirtyRange(cookieA); return false;
        }
        const long before = container->GetVersionNumber();
        if (mode == 0 || mode == 2) {
            CComPtr<IHTMLDOMNode> removed;
            hr = parent->replaceChild(newNode, oldNode, &removed);
        }
        if (SUCCEEDED(hr) && (mode == 1 || mode == 2)) hr = book->put_innerText(CComBSTR(mode == 1 ? L"second" : L"third"));
        if (SUCCEEDED(hr) && mode == 3) {
            CComQIPtr<IHTMLInputElement> input(field);
            hr = input ? input->put_value(CComBSTR(L"changed")) : E_NOINTERFACE;
        }
        CComPtr<IMarkupPointer> a1,a2,b1,b2;
        if (SUCCEEDED(hr)) hr = services->CreateMarkupPointer(&a1);
        if (SUCCEEDED(hr)) hr = services->CreateMarkupPointer(&a2);
        if (SUCCEEDED(hr)) hr = services->CreateMarkupPointer(&b1);
        if (SUCCEEDED(hr)) hr = services->CreateMarkupPointer(&b2);
        const long after = container->GetVersionNumber();
        HRESULT rangeA = FAILED(hr) ? hr : container->GetAndClearDirtyRange(cookieA, a1, a2);
        HRESULT rangeB = FAILED(hr) ? hr : container->GetAndClearDirtyRange(cookieB, b1, b2);
        char leftA[80] = "none", rightA[80] = "none", leftB[80] = "none", rightB[80] = "none";
        if (SUCCEEDED(rangeA) && a1 && a2) { scope(a1, leftA, sizeof leftA); scope(a2, rightA, sizeof rightA); }
        if (SUCCEEDED(rangeB) && b1 && b2) { scope(b1, leftB, sizeof leftB); scope(b2, rightB, sizeof rightB); }
        std::printf("case=%s version=%ld,%ld notify=%u,%u range=%08lx,%s,%s other=%08lx,%s,%s\n", label[mode], before, after,
            first.notifications, second.notifications, static_cast<unsigned long>(rangeA), leftA, rightA,
            static_cast<unsigned long>(rangeB), leftB, rightB);
        container->UnRegisterForDirtyRange(cookieB); container->UnRegisterForDirtyRange(cookieA);
        if (!check(hr, "operation")) return false;
    }
    return true;
}

int main() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (!check(hr, "coinitialize")) return 1;
    const bool ok = run();
    CoUninitialize();
    return ok ? 0 : 1;
}
