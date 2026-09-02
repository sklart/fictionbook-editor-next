#pragma once

#include <shobjidl.h>
#include <shlobj.h>
#include <atlbase.h>
#include <functional>
#include <string>
#include <vector>

namespace ModernFileDialog {
enum class Outcome { Accepted, Cancelled, Failed };

struct Request {
    bool save = false, allowMultiSelect = false, fileMustExist = false, pathMustExist = false, overwritePrompt = false;
    std::wstring title, okButtonLabel, fileNameLabel, defaultExtension;
    std::wstring initialFileName, initialFolder;
    const COMDLG_FILTERSPEC* filters = nullptr; UINT filterCount = 0, filterIndex = 0;
    IFileDialogEvents* events = nullptr;
    std::function<HRESULT(IFileDialogCustomize*)> customize;
    std::function<void(IFileDialogCustomize*)> readCustomization;
};
struct Result { Outcome outcome = Outcome::Failed; HRESULT error = E_FAIL; UINT filterIndex = 0; std::vector<std::wstring> paths; };

inline void TraceFailure(const wchar_t* operation, HRESULT error) {
    wchar_t message[256] = {};
    _snwprintf_s(message, _countof(message), _TRUNCATE,
        L"ModernFileDialog failed: %s; hr=0x%08lX\n", operation ? operation : L"file dialog",
        static_cast<unsigned long>(error));
    ::OutputDebugStringW(message);
}

inline void AppendPath(IShellItem* item, std::vector<std::wstring>& paths) {
    PWSTR path = nullptr;
    if (item && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) { paths.push_back(path); CoTaskMemFree(path); }
}

inline HRESULT CreateShellItemFromPath(LPCWSTR path, IShellItem** item) {
    typedef HRESULT (WINAPI *CreateItemFromParsingName)(PCWSTR, IBindCtx*, REFIID, void**);
    HMODULE shell32 = GetModuleHandleW(L"shell32.dll");
    const CreateItemFromParsingName createItem = shell32
        ? reinterpret_cast<CreateItemFromParsingName>(GetProcAddress(shell32, "SHCreateItemFromParsingName")) : nullptr;
    return createItem ? createItem(path, nullptr, IID_IShellItem, reinterpret_cast<void**>(item)) : E_NOTIMPL;
}

inline Result Show(HWND owner, const Request& request) {
    Result result;
    if (request.save && request.allowMultiSelect) { result.error = E_INVALIDARG; return result; }
    CComPtr<IFileDialog> dialog; HRESULT hr = E_FAIL;
    if (request.save) { CComPtr<IFileSaveDialog> save; hr = save.CoCreateInstance(CLSID_FileSaveDialog); dialog = save; }
    else { CComPtr<IFileOpenDialog> open; hr = open.CoCreateInstance(CLSID_FileOpenDialog); dialog = open; }
    if (FAILED(hr)) { result.error = hr; return result; }
    DWORD options = 0; hr = dialog->GetOptions(&options);
    if (FAILED(hr)) { result.error = hr; return result; }
    options |= FOS_FORCEFILESYSTEM;
    if (request.allowMultiSelect) options |= FOS_ALLOWMULTISELECT;
    if (request.fileMustExist) options |= FOS_FILEMUSTEXIST;
    if (request.pathMustExist) options |= FOS_PATHMUSTEXIST;
    if (request.overwritePrompt) options |= FOS_OVERWRITEPROMPT;
    if (FAILED(hr = dialog->SetOptions(options))) { result.error = hr; return result; }
    if (!request.title.empty()) dialog->SetTitle(request.title.c_str());
    if (!request.okButtonLabel.empty()) dialog->SetOkButtonLabel(request.okButtonLabel.c_str());
    if (!request.fileNameLabel.empty()) dialog->SetFileNameLabel(request.fileNameLabel.c_str());
    if (!request.defaultExtension.empty()) dialog->SetDefaultExtension(request.defaultExtension.c_str());
    if (!request.initialFileName.empty()) dialog->SetFileName(request.initialFileName.c_str());
    if (request.filters && request.filterCount && FAILED(hr = dialog->SetFileTypes(request.filterCount, request.filters))) { result.error = hr; return result; }
    if (request.filterIndex && FAILED(hr = dialog->SetFileTypeIndex(request.filterIndex))) { result.error = hr; return result; }
    if (!request.initialFolder.empty()) {
        IShellItem* rawFolder = nullptr;
        if (SUCCEEDED(CreateShellItemFromPath(request.initialFolder.c_str(), &rawFolder))) {
            CComPtr<IShellItem> folder;
            folder.Attach(rawFolder);
            dialog->SetFolder(folder);
        }
    }
    CComPtr<IFileDialogCustomize> customize;
    if (request.customize || request.readCustomization) {
        if (FAILED(hr = dialog->QueryInterface(IID_PPV_ARGS(&customize)))) { result.error = hr; return result; }
        if (request.customize && FAILED(hr = request.customize(customize))) { result.error = hr; return result; }
    }
    DWORD cookie = 0;
    if (request.events && FAILED(hr = dialog->Advise(request.events, &cookie))) { result.error = hr; return result; }
    hr = dialog->Show(owner); if (cookie) dialog->Unadvise(cookie);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) { result.outcome = Outcome::Cancelled; result.error = hr; return result; }
    if (FAILED(hr)) { result.error = hr; return result; }
    dialog->GetFileTypeIndex(&result.filterIndex);
    if (request.readCustomization) request.readCustomization(customize);
    if (request.allowMultiSelect) {
        CComPtr<IFileOpenDialog> open; CComPtr<IShellItemArray> items;
        if (FAILED(hr = dialog->QueryInterface(IID_PPV_ARGS(&open))) || FAILED(hr = open->GetResults(&items)) || !items) { result.error = FAILED(hr) ? hr : E_FAIL; return result; }
        DWORD count = 0; items->GetCount(&count);
        for (DWORD i = 0; i < count; ++i) { CComPtr<IShellItem> item; if (SUCCEEDED(items->GetItemAt(i, &item))) AppendPath(item, result.paths); }
    } else {
        CComPtr<IShellItem> item; if (FAILED(hr = dialog->GetResult(&item))) { result.error = hr; return result; } AppendPath(item, result.paths);
    }
    if (result.paths.empty()) { result.error = E_FAIL; return result; }
    result.outcome = Outcome::Accepted; result.error = S_OK; return result;
}
}
