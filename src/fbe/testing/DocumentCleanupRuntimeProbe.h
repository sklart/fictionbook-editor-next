#pragma once
#include <functional>
#include "../ElementDescMnr.h"

namespace FB { class Doc; }

// Empty outside runtime tests; callbacks hold no document COM interfaces.
struct DocumentCleanupRuntimeProbe {
    static std::function<void(FB::Doc*, FB::Doc*, bool)>& OnDetach() {
        static thread_local std::function<void(FB::Doc*, FB::Doc*, bool)> callback; return callback;
    }
    static std::function<void(LPCWSTR, bool, HRESULT)>& OnScript() {
        static thread_local std::function<void(LPCWSTR, bool, HRESULT)> callback; return callback;
    }
    static void Detach(FB::Doc* old, FB::Doc* next, bool released) {
        if (OnDetach()) OnDetach()(old, next, released);
    }
    static void Script(LPCWSTR className, bool completed, HRESULT result) {
        if (OnScript()) OnScript()(className, completed, result);
    }
    static void Remove(CElementDescMnr& manager, CElementDescriptor* descriptor) {
        manager.m_EDs.Remove(descriptor);
    }
};
