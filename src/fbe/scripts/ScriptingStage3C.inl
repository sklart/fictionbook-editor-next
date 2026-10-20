namespace FbeScripts {
inline HRESULT InvokeStage3C(HWND owner, DISPID method, WORD flags, DISPPARAMS* parameters, VARIANT* result, UINT* argumentError)
{
    if(!(flags&DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
    if(parameters&&parameters->cNamedArgs) return DISP_E_NONAMEDARGS;
    const UINT count=parameters?parameters->cArgs:0;
    const UINT expected=method==153?4U:method==157||method==159||method==163?0U:method==160?2U:1U;
    if(count!=expected)return DISP_E_BADPARAMCOUNT;
    if(count&&!parameters->rgvarg)return E_POINTER;
    XmlScriptApiRequest request(XmlScriptApiOperation::ScriptingDocumentOperation); request.scriptingMethod=method;
    request.arguments.resize(count);
    for(UINT i=0;i<count;++i) {
        CheckError(::VariantCopyInd(&request.arguments[i],&parameters->rgvarg[count-i-1]));
        if(request.arguments[i].vt!=VT_BSTR){if(argumentError)*argumentError=count-i-1;return DISP_E_TYPEMISMATCH;}
    }
    if(!SendXmlScriptRequest(owner,request))return E_UNEXPECTED;
    if(FAILED(request.error))return request.error;
    return result?request.returnValue.Detach(result):S_OK;
}
}
