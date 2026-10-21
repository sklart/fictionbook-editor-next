#pragma once

#include <shobjidl.h>
#include <atomic>

// Observe the real Shell dialog only in file-close-runtime. Acceptance and
// overwrite confirmation still use the native dialog and its normal handlers.
namespace FileCloseSaveDialogProbe {
struct State {
    bool active = false;
    std::atomic<bool> ready{false};
    CString target, suggestion;
    DWORD options = 0;
    unsigned overwrites = 0;
    IFileDialog* dialog = nullptr;
    UINT_PTR timer = 0;
};
inline State& Current() { static thread_local State state; return state; }
class Events final : public IFileDialogEvents {
public:
    ~Events() { auto& state=Current(); if(state.timer) ::KillTimer(nullptr,state.timer); state.timer=0; state.dialog=nullptr; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** value) override {
        if (!value) return E_POINTER;
        *value = nullptr;
        if (iid != IID_IUnknown && iid != IID_IFileDialogEvents) return E_NOINTERFACE;
        *value = static_cast<IFileDialogEvents*>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_references; }
    ULONG STDMETHODCALLTYPE Release() override { return --m_references; }
    HRESULT STDMETHODCALLTYPE OnFileOk(IFileDialog*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnFolderChanging(IFileDialog*, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnFolderChange(IFileDialog* dialog) override {
        auto& state = Current();
        // Initial folder events precede the Shell's filename initialization.
        if (!state.ready.load()&&!state.timer) {
            state.dialog=dialog;
            state.timer=::SetTimer(nullptr,0,100,ObserveInitializedDialog);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSelectionChange(IFileDialog*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnShareViolation(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE* response) override {
        *response = FDESVR_DEFAULT; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnTypeChange(IFileDialog*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE* response) override {
        ++Current().overwrites; *response = FDEOR_DEFAULT; return S_OK;
    }
private:
    static void CALLBACK ObserveInitializedDialog(HWND,UINT,UINT_PTR timer,DWORD) {
        auto& state=Current();::KillTimer(nullptr,timer);state.timer=0;
        if(!state.active||!state.dialog) return;
        PWSTR name=nullptr;
        if(SUCCEEDED(state.dialog->GetFileName(&name))) {state.suggestion=name;::CoTaskMemFree(name);}
        state.dialog->GetOptions(&state.options);
        if(!state.target.IsEmpty()) state.dialog->SetFileName(state.target);
        state.ready=true;
    }
    ULONG m_references = 1; // Stack lifetime encloses Advise/Show/Unadvise.
};
}
