namespace FbeScripts { namespace Dialogs {
struct Options {
    CString title, prompt, value, initialDirectory, initialFileName, defaultExtension;
    bool multiSelect = false;
    std::vector<std::pair<std::wstring, std::wstring>> filters;
};
inline bool SafeString(const std::wstring& json, size_t& cursor, CString& value)
{
    std::wstring parsed;
    if (!FbeRuntimeLocalization::JsonParseString(json, cursor, parsed) || parsed.size() > 32767 || parsed.find(L'\0') != std::wstring::npos) return false;
    value = parsed.c_str(); return true;
}
inline bool Filters(const std::wstring& json, size_t& cursor, Options& options)
{
    using namespace FbeRuntimeLocalization;
    JsonSkipWhitespace(json, cursor);
    if (cursor >= json.size() || json[cursor++] != L'[') return false;
    JsonSkipWhitespace(json, cursor);
    if (cursor < json.size() && json[cursor] == L']') { ++cursor; return true; }
    for (;;) {
        if (options.filters.size() >= 32 || cursor >= json.size() || json[cursor++] != L'{') return false;
        CString name, pattern; std::set<std::wstring> keys;
        for (;;) {
            std::wstring key; CString text;
            if (!JsonParseString(json, cursor, key) || !keys.insert(key).second) return false;
            JsonSkipWhitespace(json, cursor);
            if (cursor >= json.size() || json[cursor++] != L':' || !SafeString(json, cursor, text) || text.IsEmpty() || text.GetLength() > 256) return false;
            if (key == L"name") name = text; else if (key == L"pattern") pattern = text; else return false;
            JsonSkipWhitespace(json, cursor);
            if (cursor >= json.size()) return false;
            const wchar_t delimiter = json[cursor++];
            if (delimiter == L'}') break;
            if (delimiter != L',') return false;
        }
        if (name.IsEmpty() || pattern.IsEmpty()) return false;
        options.filters.emplace_back(static_cast<LPCWSTR>(name), static_cast<LPCWSTR>(pattern));
        JsonSkipWhitespace(json, cursor);
        if (cursor >= json.size()) return false;
        const wchar_t delimiter = json[cursor++];
        if (delimiter == L']') return true;
        if (delimiter != L',') return false;
        JsonSkipWhitespace(json, cursor);
    }
}
inline bool Parse(BSTR source, bool input, Options& options)
{
    using namespace FbeRuntimeLocalization;
    if (::SysStringLen(source) > 65536) return false;
    const std::wstring json(source ? source : L"", ::SysStringLen(source));
    size_t cursor = 0; JsonSkipWhitespace(json, cursor);
    if (cursor >= json.size() || json[cursor++] != L'{') return false;
    JsonSkipWhitespace(json, cursor); std::set<std::wstring> keys;
    if (cursor < json.size() && json[cursor] != L'}') {
        for (;;) {
            std::wstring key;
            if (!JsonParseString(json, cursor, key) || !keys.insert(key).second) return false;
            JsonSkipWhitespace(json, cursor);
            if (cursor >= json.size() || json[cursor++] != L':') return false;
            JsonSkipWhitespace(json, cursor);
            if (!input && key == L"filters") { if (!Filters(json, cursor, options)) return false; }
            else if (!input && key == L"multiSelect") {
                if (json.compare(cursor, 4, L"true") == 0) { options.multiSelect = true; cursor += 4; }
                else if (json.compare(cursor, 5, L"false") == 0) cursor += 5; else return false;
            } else {
                CString text; if (!SafeString(json, cursor, text)) return false;
                if (key == L"title") options.title = text;
                else if (input && key == L"prompt") options.prompt = text;
                else if (input && key == L"value") options.value = text;
                else if (!input && key == L"initialDirectory") options.initialDirectory = text;
                else if (!input && key == L"initialFileName") options.initialFileName = text;
                else if (!input && key == L"defaultExtension") options.defaultExtension = text;
                else return false;
            }
            JsonSkipWhitespace(json, cursor);
            if (cursor >= json.size()) return false;
            if (json[cursor] == L'}') break;
            if (json[cursor++] != L',') return false;
        }
    }
    if (cursor >= json.size() || json[cursor++] != L'}') return false;
    JsonSkipWhitespace(json, cursor); return cursor == json.size();
}
inline CString Error(LPCWSTR code, HRESULT error, bool input)
{
    CString json; const CString message(_com_error(error).ErrorMessage());
    json.Format(L"{%s\"status\":\"error\",\"code\":\"%s\",\"hresult\":\"0x%08lX\",\"message\":\"%s\"}",
        input ? L"\"accepted\":false," : L"", code, static_cast<unsigned long>(error), static_cast<LPCWSTR>(JsonEscape(message)));
    return json;
}
inline CString Show(HWND owner, DISPID method, BSTR source)
{
    const bool input = method == 118;
    Options options;
    if (!Parse(source, input, options) || (method == 116 && options.multiSelect) || (method == 117 && !options.filters.empty()))
        return Error(L"invalid-options", E_INVALIDARG, input);
    if (!owner || !::IsWindow(owner)) return Error(L"owner-unavailable", E_UNEXPECTED, input);
    if (input) {
        CString value(options.value);
        const int response = AU::InputBoxOwned(value, options.title, options.prompt, owner);
        if (response == -1) return Error(L"dialog-failed", E_FAIL, true);
        CString json;
        json.Format(L"{\"accepted\":%s,\"value\":\"%s\"}", response == IDYES ? L"true" : L"false",
            static_cast<LPCWSTR>(JsonEscape(response == IDYES ? value : CString())));
        return json;
    }
    ModernFileDialog::Request request;
    request.save = method == 116; request.pickFolders = method == 117;
    request.allowMultiSelect = options.multiSelect;
    request.fileMustExist = method == 115; request.pathMustExist = true;
    request.overwritePrompt = request.save; request.strictInitialFolder = true;
    request.title = options.title; request.initialFolder = options.initialDirectory;
    request.initialFileName = options.initialFileName; request.defaultExtension = options.defaultExtension;
    std::vector<COMDLG_FILTERSPEC> filters;
    for (const auto& filter : options.filters) filters.push_back({ filter.first.c_str(), filter.second.c_str() });
    request.filters = filters.empty() ? NULL : filters.data(); request.filterCount = static_cast<UINT>(filters.size());
    const ModernFileDialog::Result selected = ModernFileDialog::Show(owner, request);
    if (selected.outcome == ModernFileDialog::Outcome::Cancelled) return L"{\"status\":\"cancelled\"}";
    if (selected.outcome != ModernFileDialog::Outcome::Accepted) return Error(L"dialog-failed", selected.error, false);
    CString json(L"{\"status\":\"ok\",");
    if (options.multiSelect) {
        json += L"\"paths\":[";
        for (size_t index = 0; index < selected.paths.size(); ++index) {
            if (index) json += L",";
            json += L"\"" + JsonEscape(CString(selected.paths[index].c_str())) + L"\"";
        }
        json += L"]}";
    } else json += L"\"path\":\"" + JsonEscape(CString(selected.paths.front().c_str())) + L"\"}";
    return json;
}
} }
