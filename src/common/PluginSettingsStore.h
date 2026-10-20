#pragma once

#include "DeploymentContext.h"
#include "ExportFileTransaction.h"
#include "RuntimeLocalizationCommon.h"
#include <atlbase.h>
#include <atlstr.h>
#include <map>
#include <string>

// Value-only settings shared by the four bundled plug-ins. Opening a store
// never creates a registry key or a file; writes require an explicit Commit.
namespace FbePluginSettings {
inline bool RuntimeDialogProbeRequested() {
    wchar_t mode[2] = {}, probe[2] = {};
    return ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", mode, _countof(mode)) == 1 && mode[0] == L'1' &&
        ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_PLUGIN_SETTINGS", probe, _countof(probe)) == 1 && probe[0] == L'1';
}
inline CStringW PortableFilePath(const CStringW& value, bool forStorage) {
    if(DeploymentContext::RegistryPersistenceAllowed() || value.IsEmpty()) return value;
    const std::wstring root = DeploymentContext::ExecutableDirectory();
    const std::wstring path(value.GetString());
    if(forStorage) {
        if(path.size() > root.size() && ::CompareStringOrdinal(path.c_str(), static_cast<int>(root.size()), root.c_str(), static_cast<int>(root.size()), TRUE) == CSTR_EQUAL)
            return CStringW(path.substr(root.size()).c_str());
    } else if(path.find(L':') == std::wstring::npos && path[0] != L'\\' && path[0] != L'/') {
        return CStringW((root + path).c_str());
    }
    return value;
}
template<class Key> inline DWORD ReadDword(Key& key, LPCWSTR name, DWORD fallback) {
    DWORD value = fallback;
    return key.QueryDWORDValue(name, value) == ERROR_SUCCESS ? value : fallback;
}
template<class Key> inline CStringW ReadString(Key& key, LPCWSTR name, LPCWSTR fallback = L"") {
    ULONG length = 0;
    if(key.QueryStringValue(name, NULL, &length) != ERROR_SUCCESS || !length) return CStringW(fallback);
    CStringW value; wchar_t* buffer = value.GetBuffer(static_cast<int>(length));
    const LONG result = key.QueryStringValue(name, buffer, &length);
    value.ReleaseBuffer();
    return result == ERROR_SUCCESS ? value : CStringW(fallback);
}

class Store {
    struct Value { bool string = false; DWORD number = 0; std::wstring text; };
    struct NameLess {
        bool operator()(const std::wstring& left, const std::wstring& right) const {
            return ::CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()), right.c_str(), static_cast<int>(right.size()), TRUE) == CSTR_LESS_THAN;
        }
    };
    bool portable;
    bool registryWritable = false;
    std::wstring path, registryPath;
    std::string original;
    std::map<std::wstring, Value, NameLess> values;
    CRegKey registry;
    HRESULT error = S_OK;
    static HRESULT LastError() { const DWORD code = ::GetLastError(); return HRESULT_FROM_WIN32(code ? code : ERROR_WRITE_FAULT); }
    LONG Fail(LONG code) { if(SUCCEEDED(error)) error = HRESULT_FROM_WIN32(code); return code; }
    static bool Take(const std::wstring& text, size_t& pos, wchar_t ch) {
        FbeRuntimeLocalization::JsonSkipWhitespace(text, pos);
        if(pos == text.size() || text[pos] != ch) return false;
        ++pos; return true;
    }
    static bool Number(const std::wstring& text, size_t& pos, DWORD& number) {
        FbeRuntimeLocalization::JsonSkipWhitespace(text, pos);
        const size_t start = pos; ULONGLONG value = 0;
        while(pos < text.size() && text[pos] >= L'0' && text[pos] <= L'9') {
            value = value * 10 + static_cast<unsigned>(text[pos++] - L'0');
            if(value > MAXDWORD) return false;
        }
        if(pos == start || (pos - start > 1 && text[start] == L'0')) return false;
        number = static_cast<DWORD>(value); return true;
    }
    static std::wstring Quote(const std::wstring& value) {
        std::wstring result(L"\"");
        for(wchar_t ch : value) {
            if(ch == L'"' || ch == L'\\') { result += L'\\'; result += ch; }
            else if(ch < 0x20) { wchar_t escaped[7] = {}; swprintf_s(escaped, L"\\u%04x", static_cast<unsigned>(ch)); result += escaped; }
            else result += ch;
        }
        return result + L'"';
    }
    bool Parse(const std::wstring& text) {
        size_t pos = 0; bool version = false, settings = false;
        if(!Take(text, pos, L'{')) return false;
        do {
            std::wstring name;
            if(!FbeRuntimeLocalization::JsonParseString(text, pos, name) || !Take(text, pos, L':')) return false;
            if(name == L"schemaVersion" && !version) {
                DWORD value = 0; if(!Number(text, pos, value) || value != 1) return false; version = true;
            } else if(name == L"values" && !settings) {
                settings = true;
                if(!Take(text, pos, L'{')) return false;
                if(!Take(text, pos, L'}')) {
                    do {
                        Value value;
                        if(!FbeRuntimeLocalization::JsonParseString(text, pos, name) || name.empty() || name.find(L'\0') != std::wstring::npos ||
                            !Take(text, pos, L':') || values.count(name)) return false;
                        FbeRuntimeLocalization::JsonSkipWhitespace(text, pos);
                        value.string = pos < text.size() && text[pos] == L'"';
                        if(value.string) {
                            if(!FbeRuntimeLocalization::JsonParseString(text, pos, value.text) || value.text.find(L'\0') != std::wstring::npos) return false;
                        } else if(!Number(text, pos, value.number)) return false;
                        values.emplace(name, value);
                    } while(Take(text, pos, L','));
                    if(!Take(text, pos, L'}')) return false;
                }
            } else return false;
        } while(Take(text, pos, L','));
        if(!Take(text, pos, L'}')) return false;
        FbeRuntimeLocalization::JsonSkipWhitespace(text, pos);
        return pos == text.size() && version && settings;
    }
    static HRESULT ReadFile(const std::wstring& file, std::string& bytes) {
        bytes.clear();
        HANDLE handle = ::CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if(handle == INVALID_HANDLE_VALUE) { const DWORD code = ::GetLastError(); return code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND ? S_FALSE : HRESULT_FROM_WIN32(code); }
        LARGE_INTEGER size = {}; HRESULT hr = S_OK;
        if(!::GetFileSizeEx(handle, &size)) hr = LastError();
        else if(size.QuadPart <= 0 || size.QuadPart > 1024 * 1024) hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        if(SUCCEEDED(hr)) {
            bytes.resize(static_cast<size_t>(size.QuadPart)); DWORD read = 0;
            if(!::ReadFile(handle, &bytes[0], static_cast<DWORD>(bytes.size()), &read, NULL) || read != bytes.size()) hr = HRESULT_FROM_WIN32(ERROR_READ_FAULT);
        }
        ::CloseHandle(handle); return hr;
    }
    LONG Writable() {
        if(FAILED(error)) return HRESULT_CODE(error);
        if(!portable && !registryWritable) {
            registry.Close();
            const LONG result = registry.Create(HKEY_CURRENT_USER, registryPath.c_str());
            registryWritable = result == ERROR_SUCCESS;
            return result;
        }
        return ERROR_SUCCESS;
    }
public:
    Store(LPCWSTR fileName, LPCWSTR key) : portable(!DeploymentContext::RegistryPersistenceAllowed()), registryPath(key) {
        if(!portable) { registry.Open(HKEY_CURRENT_USER, key, KEY_READ); return; }
        path = DeploymentContext::SettingsDirectory() + L"Plugins\\" + fileName + L".json";
        const HRESULT read = ReadFile(path, original);
        if(read == S_FALSE) return;
        if(FAILED(read)) { error = read; return; }
        const int count = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, original.data(), static_cast<int>(original.size()), NULL, 0);
        if(!count) { error = HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION); return; }
        std::wstring text(static_cast<size_t>(count), L'\0');
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, original.data(), static_cast<int>(original.size()), &text[0], count);
        if(!text.empty() && text[0] == 0xfeff) text.erase(0, 1);
        if(!Parse(text)) error = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    HRESULT Error() const { return error; }
    const std::wstring& Path() const { return path; }
    LONG QueryDWORDValue(LPCWSTR name, DWORD& value) {
        if(!portable) return registry.QueryDWORDValue(name, value);
        const auto entry = values.find(name); if(entry == values.end()) return ERROR_FILE_NOT_FOUND;
        if(entry->second.string) return Fail(ERROR_INVALID_DATA);
        value = entry->second.number; return ERROR_SUCCESS;
    }
    LONG QueryStringValue(LPCWSTR name, LPWSTR buffer, ULONG* count) {
        if(!portable) return registry.QueryStringValue(name, buffer, count);
        const auto entry = values.find(name); if(entry == values.end()) return ERROR_FILE_NOT_FOUND;
        if(!entry->second.string) return Fail(ERROR_INVALID_DATA);
        const ULONG needed = static_cast<ULONG>(entry->second.text.size() + 1);
        const ULONG capacity = *count; *count = needed;
        if(!buffer) return ERROR_SUCCESS;
        if(capacity < needed) return ERROR_MORE_DATA;
        memcpy(buffer, entry->second.text.c_str(), needed * sizeof(wchar_t)); return ERROR_SUCCESS;
    }
    LONG SetDWORDValue(LPCWSTR name, DWORD value) {
        const LONG ready = Writable(); if(ready != ERROR_SUCCESS) return Fail(ready);
        if(!portable) return Fail(registry.SetDWORDValue(name, value));
        Value entry; entry.number = value; values[name] = entry; return ERROR_SUCCESS;
    }
    LONG SetStringValue(LPCWSTR name, LPCWSTR value) {
        const LONG ready = Writable(); if(ready != ERROR_SUCCESS) return Fail(ready);
        if(!portable) return Fail(registry.SetStringValue(name, value));
        Value entry; entry.string = true; entry.text = value; values[name] = entry; return ERROR_SUCCESS;
    }
    LONG DeleteValue(LPCWSTR name) {
        if(FAILED(error)) return HRESULT_CODE(error);
        if(portable) { values.erase(name); return ERROR_SUCCESS; }
        const LONG opened = Writable();
        if(opened) return Fail(opened);
        const LONG result = registry.DeleteValue(name); return result == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : Fail(result);
    }
    HRESULT Commit() {
        if(FAILED(error) || !portable) return error;
        std::wstring json = L"{\"schemaVersion\":1,\"values\":{"; bool first = true;
        for(const auto& entry : values) {
            if(!first) json += L',';
            first = false; json += Quote(entry.first) + L":";
            json += entry.second.string ? Quote(entry.second.text) : std::to_wstring(entry.second.number);
        }
        json += L"}}\n";
        const int count = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, json.data(), static_cast<int>(json.size()), NULL, 0, NULL, NULL);
        if(!count) return HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION);
        if(count > 1024 * 1024) return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        std::string bytes(static_cast<size_t>(count), '\0');
        ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, json.data(), static_cast<int>(json.size()), &bytes[0], count, NULL, NULL);
        // Serialize commits to the same portable profile, then reject stale
        // writers instead of silently overwriting another window's settings.
        DWORD hash = 2166136261u; for(wchar_t ch : path) { hash ^= static_cast<DWORD>(towlower(ch)); hash *= 16777619u; }
        const std::wstring mutexName = L"Local\\FBE.PluginSettings." + std::to_wstring(hash);
        CHandle mutex(::CreateMutexW(NULL, FALSE, mutexName.c_str())); if(!mutex) return LastError();
        const DWORD wait = ::WaitForSingleObject(mutex, 5000);
        if(wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) return HRESULT_FROM_WIN32(ERROR_BUSY);
        struct Unlock { HANDLE handle; ~Unlock() { ::ReleaseMutex(handle); } } unlock = {mutex};
        std::string current; HRESULT hr = ReadFile(path, current);
        if(FAILED(hr)) return hr;
        if(current != original) return HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH);
        const std::wstring directory = path.substr(0, path.find_last_of(L'\\'));
        const int created = ::SHCreateDirectoryExW(NULL, directory.c_str(), NULL);
        if(created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS && created != ERROR_FILE_EXISTS) return HRESULT_FROM_WIN32(created);
        FbeExport::FileTransaction transaction; std::wstring temporary;
        hr = transaction.Stage(path, temporary); if(FAILED(hr)) return hr;
        CHandle output(::CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
        if(output == INVALID_HANDLE_VALUE) return LastError();
        DWORD written = 0;
        if(!::WriteFile(output, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL) || written != bytes.size()) return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
        if(!::FlushFileBuffers(output)) return LastError();
        output.Close(); hr = transaction.Commit();
        if(SUCCEEDED(hr)) original = bytes;
        return hr;
    }
};

inline bool ShowSaveError(HWND owner, HRESULT hr) {
    if(SUCCEEDED(hr)) return false;
    wchar_t message[512] = {};
    ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, static_cast<DWORD>(hr), 0, message, _countof(message), NULL);
    CStringW text; text.Format(L"Could not save plug-in settings (0x%08lX).\n%s", static_cast<unsigned long>(hr), message);
    ::MessageBoxW(owner, text, L"FBE Next", MB_OK | MB_ICONERROR);
    return true;
}
}
