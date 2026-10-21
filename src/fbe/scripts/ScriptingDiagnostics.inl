namespace FbeScripts {
inline CString ApiErrorMessage(HRESULT error)
{
    CString message(_com_error(error).ErrorMessage());
    if (message.GetLength() > 512) {
        message = message.Left(512);
        if (message[511] >= 0xd800 && message[511] <= 0xdbff) message = message.Left(511);
    }
    return message;
}
inline LPCWSTR ApiErrorCode(HRESULT error)
{
    if (error == E_ACCESSDENIED) return L"access-denied";
    if (error == E_INVALIDARG) return L"invalid-argument";
    if (error == DISP_E_TYPEMISMATCH) return L"type-mismatch";
    if (error == DISP_E_BADPARAMCOUNT) return L"bad-parameter-count";
    if (error == DISP_E_NONAMEDARGS) return L"named-arguments-not-supported";
    if (error == DISP_E_MEMBERNOTFOUND) return L"method-not-supported";
    if (error == E_UNEXPECTED) return L"context-unavailable";
    if (error == E_OUTOFMEMORY) return L"out-of-memory";
    if (error == HRESULT_FROM_WIN32(ERROR_INVALID_NAME)) return L"save-as-required";
    if (error == HRESULT_FROM_WIN32(ERROR_INVALID_STATE)) return L"source-changes-must-be-applied";
    if (error == HRESULT_FROM_WIN32(ERROR_FILE_INVALID)) return L"document-modified-externally";
    if (error == HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)) return L"save-not-supported";
    if (error == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW)) return L"message-too-long";
    if (error == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) return L"file-not-found";
    if (error == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) return L"path-not-found";
    if (error == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS) || error == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) return L"already-exists";
    if (error == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION) || error == HRESULT_FROM_WIN32(ERROR_LOCK_VIOLATION)) return L"file-locked";
    return L"operation-failed";
}
void RecordApiFailure(LPCWSTR method, HRESULT error) noexcept
{
    try {
        auto& stack = ExecutionContexts();
        if (stack.empty()) return;
        CString json;
        json.Format(L"{\"schemaVersion\":1,\"hasError\":true,\"method\":\"%s\",\"hresult\":\"0x%08lX\",\"code\":\"%s\",\"message\":\"%s\"}",
            static_cast<LPCWSTR>(JsonEscape(method)), static_cast<unsigned long>(error), ApiErrorCode(error),
            static_cast<LPCWSTR>(JsonEscape(ApiErrorMessage(error))));
        stack.back().lastScriptingError = json;
    } catch (...) { }
}
inline bool EmptyValidationOptions(const CString& value)
{
    if (value.GetLength() > 1024) return false;
    const std::wstring json(value.GetString(), value.GetLength()); size_t cursor = 0;
    FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
    if (cursor == json.size() || json[cursor++] != L'{') return false;
    FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
    if (cursor == json.size() || json[cursor++] != L'}') return false;
    FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
    return cursor == json.size();
}
inline CString CurrentValidationJson(HWND owner)
{
    XmlScriptApiRequest request(XmlScriptApiOperation::ValidateCurrentDocument);
    if (!SendXmlScriptRequest(owner, request)) request.error = E_UNEXPECTED;
    CString json;
    if (FAILED(request.error) || !request.succeeded) {
        if (SUCCEEDED(request.error)) request.error = E_FAIL;
        RecordApiFailure(L"ValidateCurrentDocument", request.error);
        json.Format(L"{\"schemaVersion\":1,\"status\":\"error\",\"valid\":null,\"diagnosticsComplete\":false,\"errors\":[{\"code\":\"%s\",\"severity\":\"error\",\"hresult\":\"0x%08lX\",\"message\":\"%s\",\"line\":0,\"column\":0}],\"warnings\":[]}",
            request.error == E_UNEXPECTED ? L"no-document" : L"validation-unavailable", static_cast<unsigned long>(request.error),
            static_cast<LPCWSTR>(JsonEscape(ApiErrorMessage(request.error))));
    } else if (request.valid) {
        json = L"{\"schemaVersion\":1,\"status\":\"ok\",\"valid\":true,\"diagnosticsComplete\":true,\"errors\":[],\"warnings\":[]}";
    } else {
        json.Format(L"{\"schemaVersion\":1,\"status\":\"ok\",\"valid\":false,\"diagnosticsComplete\":false,\"errors\":[{\"code\":\"xml-invalid\",\"severity\":\"error\",\"message\":\"%s\",\"line\":%d,\"column\":%d}],\"warnings\":[]}",
            static_cast<LPCWSTR>(JsonEscape(request.message)), request.line, request.column);
    }
    return json;
}
inline bool LogApiMessage(const CString& level, const CString& message)
{
    if (level != L"debug" && level != L"info" && level != L"warning" && level != L"error") CheckError(E_INVALIDARG);
    if (message.GetLength() > 512) CheckError(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW));
    if (!message.IsEmpty() && !::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, message, message.GetLength(), NULL, 0, NULL, NULL)) CheckError(E_INVALIDARG);
    if (!StartupTrace::Enabled()) return false;
    auto& context = ExecutionContexts().back();
    static thread_local ULONGLONG window = 0;
    static thread_local unsigned count = 0;
    const ULONGLONG now = ::GetTickCount64();
    if (now - window >= 1000) { window = now; count = 0; }
    if (count >= 32 || context.loggedMessages >= 256) return false;
    ++count; ++context.loggedMessages;
    CString details;
    details.Format(L"number=%ld; message-present=%d; size=%d", context.token, message.IsEmpty() ? 0 : 1, message.GetLength());
    if (level == L"error") StartupTrace::Error(L"script-api", L"SA144", details);
    else if (level == L"warning") StartupTrace::Warning(L"script-api", L"SA144", details);
    else if (level == L"debug") StartupTrace::Debug(L"script-api", L"SA144", details);
    else StartupTrace::Event(L"script-api", L"SA144", details);
    return true;
}
inline HRESULT InvokeStage3A(HWND owner, DISPID method, WORD flags, DISPPARAMS* parameters, VARIANT* result, UINT* argumentError)
{
    if (!(flags & DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
    if (parameters && parameters->cNamedArgs) return DISP_E_NONAMEDARGS;
    const UINT count = parameters ? parameters->cArgs : 0;
    const UINT expected = method == 143 ? 1U : (method == 144 ? 2U : 0U);
    if (count != expected) return DISP_E_BADPARAMCOUNT;
    if (count && !parameters->rgvarg) return E_POINTER;
    std::vector<CComVariant> args(count);
    for (UINT index = 0; index < count; ++index) {
        CheckError(::VariantCopyInd(&args[index], &parameters->rgvarg[count - index - 1]));
        if (args[index].vt != VT_BSTR) { if (argumentError) *argumentError = count - index - 1; return DISP_E_TYPEMISMATCH; }
    }
    if (ExecutionContexts().empty()) return E_UNEXPECTED;
    CComVariant value;
    if (method == 143) {
        if (::SysStringLen(args[0].bstrVal) > 1024) return E_INVALIDARG;
        if (!EmptyValidationOptions(Stage2String(args[0]))) return E_INVALIDARG;
        value = FileString(CurrentValidationJson(owner));
    } else if (method == 144) {
        if (::SysStringLen(args[1].bstrVal) > 512) return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        value = LogApiMessage(Stage2String(args[0]), Stage2String(args[1], true));
    } else if (method == 145) {
        const CString& error = ExecutionContexts().back().lastScriptingError;
        value = FileString(error.IsEmpty() ? CString(L"{\"schemaVersion\":1,\"hasError\":false}") : error);
    } else {
        ExecutionContexts().back().lastScriptingError.Empty(); value = true;
    }
    return result ? value.Detach(result) : S_OK;
}
}
