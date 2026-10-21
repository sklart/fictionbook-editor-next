namespace FbeScripts {
inline HRESULT InvokeStage3D(HWND owner, DISPID method, WORD flags, DISPPARAMS* parameters, VARIANT* result, UINT* argumentError)
{
    if (!(flags & DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
    if (parameters && parameters->cNamedArgs) return DISP_E_NONAMEDARGS;
    const UINT count = parameters ? parameters->cArgs : 0;
    if (count != (method == 164 || method == 167 ? 0U : 2U)) return DISP_E_BADPARAMCOUNT;
    if (count && !parameters->rgvarg) return E_POINTER;
    XmlScriptApiRequest request(XmlScriptApiOperation::ScriptingDocumentOperation);request.scriptingMethod = method;
    request.arguments.resize(count);
    for (UINT i = 0; i < count; ++i) {
        CheckError(::VariantCopyInd(&request.arguments[i], &parameters->rgvarg[count-i-1]));
        const VARTYPE type = static_cast<VARTYPE>(i == 1 && method != 168 ? VT_BOOL : VT_BSTR);
        if (request.arguments[i].vt != type) {if (argumentError) *argumentError = count-i-1;return DISP_E_TYPEMISMATCH;}
    }
    if (!SendXmlScriptRequest(owner, request)) return E_UNEXPECTED;
    if (FAILED(request.error)) return request.error;
    return result ? request.returnValue.Detach(result) : S_OK;
}
}
