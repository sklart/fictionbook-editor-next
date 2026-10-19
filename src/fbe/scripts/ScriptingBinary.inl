namespace FbeScripts {
inline HRESULT InvokeBinary(HWND owner, DISPID method, WORD flags, DISPPARAMS* parameters, VARIANT* result, UINT* argumentError)
{
    if (!(flags & DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
    if (parameters && parameters->cNamedArgs) return DISP_E_NONAMEDARGS;
    const UINT count = parameters ? parameters->cArgs : 0;
    if (count < 1 || count > (method == 152 ? 2U : 1U)) return DISP_E_BADPARAMCOUNT;
    if (!parameters->rgvarg) return E_POINTER;
    XmlScriptApiRequest request(XmlScriptApiOperation::ScriptingDocumentOperation);
    request.scriptingMethod = method; request.arguments.resize(count);
    for (UINT index = 0; index < count; ++index) {
        CheckError(::VariantCopyInd(&request.arguments[index], &parameters->rgvarg[count - index - 1]));
        if (request.arguments[index].vt != VT_BSTR) {
            if (argumentError) *argumentError = count - index - 1;
            return DISP_E_TYPEMISMATCH;
        }
        Stage2String(request.arguments[index]);
    }
    if (method == 152) request.arguments[0] = ApiFilePath(request.arguments[0]);
    if (!SendXmlScriptRequest(owner, request)) return E_UNEXPECTED;
    if (FAILED(request.error)) return request.error;
    return result ? request.returnValue.Detach(result) : S_OK;
}
}
