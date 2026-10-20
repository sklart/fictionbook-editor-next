namespace FbeScripts {
inline HRESULT InvokeBinary(HWND owner, DISPID method, WORD flags, DISPPARAMS* parameters, VARIANT* result, UINT* argumentError)
{
    if (!(flags & DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
    if (parameters && parameters->cNamedArgs) return DISP_E_NONAMEDARGS;
    const UINT count = parameters ? parameters->cArgs : 0;
    const UINT minimum = method == 149 || method == 150 ? 2U : 1U;
    const UINT maximum = method == 149 || method == 150 ? 3U : (method == 151 || method == 152 ? 2U : 1U);
    if (count < minimum || count > maximum) return DISP_E_BADPARAMCOUNT;
    if (!parameters->rgvarg) return E_POINTER;
    XmlScriptApiRequest request(XmlScriptApiOperation::ScriptingDocumentOperation);
    request.scriptingMethod = method; request.arguments.resize(count);
    for (UINT index = 0; index < count; ++index) {
        CheckError(::VariantCopyInd(&request.arguments[index], &parameters->rgvarg[count - index - 1]));
        const bool boolean = method == 151 && index == 1;
        if (request.arguments[index].vt != (boolean ? VT_BOOL : VT_BSTR)) {
            if (argumentError) *argumentError = count - index - 1;
            return DISP_E_TYPEMISMATCH;
        }
        if (!boolean) Stage2String(request.arguments[index]);
    }
    if (method == 152) request.arguments[0] = ApiFilePath(request.arguments[0]);
    if (method == 149 || method == 150) {
        const size_t index = method == 149 ? 0 : 1;
        request.arguments[index] = ApiFilePath(request.arguments[index]);
    }
    if (!SendXmlScriptRequest(owner, request)) return E_UNEXPECTED;
    if (FAILED(request.error)) return request.error;
    return result ? request.returnValue.Detach(result) : S_OK;
}
}
