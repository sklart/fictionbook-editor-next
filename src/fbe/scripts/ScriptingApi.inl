// Included by ExternalHelper.cpp; the legacy dual interface stays immutable.
#include "ScriptingDialogs.inl"
#include "ScriptingStage2.inl"
#include "ScriptingDiagnostics.inl"
#include "ScriptingBinary.inl"
#include "ScriptingStage3C.inl"
HRESULT ExternalHelper::InvokeScriptingExtension(DISPID dispid, WORD flags, DISPPARAMS* parameters,
    VARIANT* result, UINT* argumentError)
{
    if (dispid >= 153 && dispid <= 156) return FbeScripts::InvokeStage3C(m_xmlScriptHost, dispid, flags, parameters, result, argumentError);
    if (dispid >= 147 && dispid <= 152) return FbeScripts::InvokeBinary(m_xmlScriptHost, dispid, flags, parameters, result, argumentError);
    if (dispid >= 143 && dispid <= 146) return FbeScripts::InvokeStage3A(m_xmlScriptHost, dispid, flags, parameters, result, argumentError);
    if (dispid >= 119 && dispid <= 142) return FbeScripts::InvokeStage2(m_xmlScriptHost, dispid, flags, parameters, result, argumentError);
    if (!(flags & DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
    if (parameters && parameters->cNamedArgs) return DISP_E_NONAMEDARGS;
    const UINT count = parameters ? parameters->cArgs : 0;
    const UINT expected = dispid == 111 || dispid == 112 ? 2 :
        (dispid == 101 || dispid == 113 || (dispid >= 115 && dispid <= 118) || dispid == 1000 || dispid == 1001 ? 1 : 0);
    if (count != expected) return DISP_E_BADPARAMCOUNT;
    if (count && (!parameters || !parameters->rgvarg)) return E_POINTER;
    if (dispid >= 111 && dispid <= 114) {
        CComVariant name, value;
        if (count) {
            HRESULT hr = ::VariantCopyInd(&name, &parameters->rgvarg[count - 1]);
            if (FAILED(hr)) return hr;
            if (name.vt != VT_BSTR) { if (argumentError) *argumentError = count - 1; return DISP_E_TYPEMISMATCH; }
        }
        HRESULT hr = FbeScripts::SettingsStore::Access(static_cast<int>(dispid - 111),
            count ? CString(name.bstrVal, ::SysStringLen(name.bstrVal)) : CString(),
            count == 2 ? &parameters->rgvarg[0] : NULL, &value);
        if (FAILED(hr)) return hr;
        return result ? value.Detach(result) : S_OK;
    }
    CComVariant argument;
    if (count) {
        HRESULT hr = ::VariantCopyInd(&argument, &parameters->rgvarg[0]);
        if (FAILED(hr)) return hr;
        const VARTYPE type = static_cast<VARTYPE>(dispid == 1001 ? VT_I4 : VT_BSTR);
        if (argument.vt != type) { if (argumentError) *argumentError = 0; return DISP_E_TYPEMISMATCH; }
    }
    CComVariant value;
    if (dispid >= 115 && dispid <= 118) {
        value = FbeScripts::Dialogs::Show(m_xmlScriptHost, dispid, argument.bstrVal);
        return result ? value.Detach(result) : S_OK;
    }
    const FbeScripts::ScriptExecutionContext* context = FbeScripts::CurrentExecutionContext();
    CString text;
    switch (dispid) {
    case 100: value = L"1.3.0"; break;
    case 101: {
        DISPID id = DISPID_UNKNOWN; LPOLESTR name = argument.bstrVal;
        const bool exists = name && name[0] && name[0] != L'_' && wcslen(name) == ::SysStringLen(argument.bstrVal) &&
            SUCCEEDED(GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &id));
        value = exists; break;
    }
    case 102: value = L"{\"schemaVersion\":1,\"apiVersion\":\"1.3.0\",\"capabilities\":{\"scriptContext\":true,\"environment\":true,\"scriptSettings\":true,\"fileDialogs\":true,\"inputBoxEx\":true,\"selection\":true,\"documentUpdate\":true,\"progress\":true,\"textFiles\":true,\"paths\":true,\"documentValidation\":true,\"logging\":true,\"scriptingErrors\":true,\"binaryReadExport\":true,\"binaryObjects\":true}}"; break;
    case 103: value = context ? context->path : CString(); break;
    case 104:
        if (context) { const int slash = context->path.ReverseFind(L'\\'); if (slash >= 0) text = context->path.Left(slash + 1); }
        value = text; break;
    case 105:
        if (context) text = context->path.Mid(context->path.ReverseFind(L'\\') + 1);
        value = text; break;
    case 106: value = context ? context->uid : CString(); break;
    case 107: value = FbeScripts::AbsoluteScriptPath(_Settings.GetScriptsFolder()); break;
    case 108: value = _Settings.GetInterfaceLocaleName(); break;
    case 109: case 110: {
        XmlScriptApiRequest request(XmlScriptApiOperation::GetScriptingEnvironment);
        if (!SendXmlScriptRequest(m_xmlScriptHost, request)) return E_UNEXPECTED;
        if (dispid == 109) value = request.viewMode; else value = request.documentModified;
        break;
    }
    case 1000: {
        const CString path = FbeScripts::AbsoluteScriptPath(CString(argument.bstrVal, ::SysStringLen(argument.bstrVal)));
        if (path.IsEmpty() || FbeScripts::ExecutionContexts().size() >= 256) return E_INVALIDARG;
        XmlScriptApiRequest request(XmlScriptApiOperation::GetScriptingEnvironment); request.scriptPath = path;
        if (!SendXmlScriptRequest(m_xmlScriptHost, request)) return E_UNEXPECTED;
        value = FbeScripts::PushExecutionContext(path, request.scriptUid); break;
    }
    case 1001: if (!FbeScripts::PopExecutionContext(argument.lVal)) return E_INVALIDARG; break;
    default: return DISP_E_MEMBERNOTFOUND;
    }
    return result ? value.Detach(result) : S_OK;
}
