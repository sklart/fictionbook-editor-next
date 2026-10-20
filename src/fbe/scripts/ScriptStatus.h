#pragma once
#include "ScriptExecutionContext.h"
#include "ScriptExtendedDialogs.h"

namespace FbeScripts { namespace Status {
inline void Notify(HWND owner) noexcept {
    if(::IsWindow(owner)&&::GetWindowThreadProcessId(owner,nullptr)==::GetCurrentThreadId())
        ::SendMessageW(owner,AU::WM_SCRIPT_STATUS_CHANGED,0,0);
}
struct Entry { CString token,text; };
struct Resources : ScriptResourceLifetime {
    HWND owner;std::vector<Entry> entries;
    explicit Resources(HWND window):owner(window){}
    ~Resources(){entries.clear();Notify(owner);}
};
inline const CString* Active(HWND owner) {
    const auto& contexts=ExecutionContexts();
    for(auto context=contexts.rbegin();context!=contexts.rend();++context) {
        const auto* resource=static_cast<const Resources*>(context->statusResources.get());
        if(resource&&resource->owner==owner&&!resource->entries.empty())return &resource->entries.back().text;
    } return nullptr;
}
inline void Invalidate(HWND owner) {
    for(auto& context:ExecutionContexts()) {
        auto* resource=static_cast<Resources*>(context.statusResources.get());
        if(resource&&resource->owner==owner)resource->entries.clear();
    } Notify(owner);
}
inline void Operation(HWND owner,DISPID method,const std::vector<CComVariant>& args,CComVariant& result) {
    auto& contexts=ExecutionContexts();
    if(contexts.empty()||!::IsWindow(owner))CheckError(E_UNEXPECTED);
    auto& current=contexts.back().statusResources;
    auto* resource=static_cast<Resources*>(current.get());
    if(method==161) {
        const CString text(args[0].bstrVal,::SysStringLen(args[0].bstrVal));
        if(!ExtendedDialogs::ValidText(text,4096))CheckError(E_INVALIDARG);
        size_t total=0;for(const auto& context:contexts)if(context.statusResources)total+=static_cast<Resources*>(context.statusResources.get())->entries.size();
        if(total>=256||(resource&&resource->entries.size()>=64))CheckError(E_OUTOFMEMORY);
        if(resource&&resource->owner!=owner)CheckError(E_UNEXPECTED);
        if(!resource){auto created=std::make_shared<Resources>(owner);current=created;resource=created.get();}
        GUID guid={};CheckError(::CoCreateGuid(&guid));wchar_t token[40]={};::StringFromGUID2(guid,token,_countof(token));
        // Allocate the result before changing the stack.
        result=CComVariant(token);resource->entries.push_back({CString(token),text});
    } else {
        if(method==162) {
            const CString token(args[0].bstrVal,::SysStringLen(args[0].bstrVal));
            if(!ExtendedDialogs::ValidText(token,64)||!resource||resource->owner!=owner||resource->entries.empty()||resource->entries.back().token!=token)CheckError(E_INVALIDARG);
            result=true;resource->entries.pop_back();
        } else {
            result=true;if(resource){if(resource->owner!=owner)CheckError(E_UNEXPECTED);resource->entries.clear();}
        }
    } Notify(owner);
}
} }
