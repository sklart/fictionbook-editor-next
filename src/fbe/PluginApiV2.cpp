#include "stdafx.h"
#include "PluginApiV2.h"
#include "StartupTrace.h"
#include "..\\..\\version.h"

namespace
{
class ATL_NO_VTABLE PluginProgressSink : public CComObjectRootEx<CComSingleThreadModel>, public IFBEProgressSink
{
public:
    BEGIN_COM_MAP(PluginProgressSink) COM_INTERFACE_ENTRY(IFBEProgressSink) END_COM_MAP()
    STDMETHOD(Report)(ULONG completed, ULONG total, BSTR)
    { CString detail; detail.Format(L"completed=%lu; total=%lu", completed, total); StartupTrace::Event(L"plugin", L"P220", L"plugin-progress; " + detail); return S_OK; }
};
class ATL_NO_VTABLE PluginCancellationToken : public CComObjectRootEx<CComSingleThreadModel>, public IFBECancellationToken
{
public:
    BEGIN_COM_MAP(PluginCancellationToken) COM_INTERFACE_ENTRY(IFBECancellationToken) END_COM_MAP()
    STDMETHOD(IsCancellationRequested)(BOOL* cancelled) { if (!cancelled) return E_POINTER; *cancelled = FALSE; return S_OK; }
};
class ATL_NO_VTABLE PluginHost : public CComObjectRootEx<CComSingleThreadModel>, public IFBEPluginHost
{
public:
    HWND m_owner = NULL; CString m_locale; CComPtr<IFBEProgressSink> m_progress; CComPtr<IFBECancellationToken> m_cancellation;
    BEGIN_COM_MAP(PluginHost) COM_INTERFACE_ENTRY(IFBEPluginHost) END_COM_MAP()
    STDMETHOD(GetHostVersion)(BSTR* version) { if (!version) return E_POINTER; wchar_t value[64] = {}; ::MultiByteToWideChar(CP_ACP, 0, FBE_VERSION_STRING, -1, value, _countof(value)); *version = ::SysAllocString(value); return *version ? S_OK : E_OUTOFMEMORY; }
    STDMETHOD(GetUiLocale)(BSTR* locale) { if (!locale) return E_POINTER; *locale = m_locale.AllocSysString(); return *locale ? S_OK : E_OUTOFMEMORY; }
    STDMETHOD(GetOwnerWindow)(LONGLONG* hwnd) { if (!hwnd) return E_POINTER; *hwnd = static_cast<LONGLONG>(reinterpret_cast<INT_PTR>(m_owner)); return S_OK; }
    STDMETHOD(GetProgressSink)(IFBEProgressSink** sink) { if (!sink) return E_POINTER; *sink = m_progress; if (*sink) (*sink)->AddRef(); return *sink ? S_OK : E_UNEXPECTED; }
    STDMETHOD(GetCancellationToken)(IFBECancellationToken** token) { if (!token) return E_POINTER; *token = m_cancellation; if (*token) (*token)->AddRef(); return *token ? S_OK : E_UNEXPECTED; }
    STDMETHOD(ReportMessage)(LONG severity, BSTR code, BSTR) { CString detail; detail.Format(L"severity=%ld; code=%s", severity, code ? code : L""); StartupTrace::Event(L"plugin", L"P221", L"plugin-message; " + detail); return S_OK; }
    STDMETHOD(Trace)(BSTR eventName, BSTR) { StartupTrace::Event(L"plugin", L"P222", CString(L"plugin-trace; event=") + (eventName ? eventName : L"")); return S_OK; }
};
class ATL_NO_VTABLE DocumentSnapshot : public CComObjectRootEx<CComSingleThreadModel>, public IFBEDocumentSnapshot
{
public:
    CString m_xml, m_path, m_encoding;
    BEGIN_COM_MAP(DocumentSnapshot) COM_INTERFACE_ENTRY(IFBEDocumentSnapshot) END_COM_MAP()
    STDMETHOD(OpenXmlStream)(IStream** stream) {
        if (!stream) return E_POINTER; *stream = NULL; CComPtr<IStream> result; HRESULT hr = ::CreateStreamOnHGlobal(NULL, TRUE, &result); if (FAILED(hr)) return hr;
        const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, m_xml, m_xml.GetLength(), NULL, 0, NULL, NULL); if (bytes <= 0) return HRESULT_FROM_WIN32(::GetLastError());
        std::vector<char> utf8(bytes); ::WideCharToMultiByte(CP_UTF8, 0, m_xml, m_xml.GetLength(), &utf8[0], bytes, NULL, NULL); ULONG written = 0; hr = result->Write(&utf8[0], static_cast<ULONG>(utf8.size()), &written);
        if (FAILED(hr) || written != utf8.size()) return FAILED(hr) ? hr : STG_E_WRITEFAULT; LARGE_INTEGER beginning = {}; result->Seek(beginning, STREAM_SEEK_SET, NULL); *stream = result.Detach(); return S_OK;
    }
    STDMETHOD(GetSourceFilePath)(BSTR* path) { if (!path) return E_POINTER; *path = m_path.AllocSysString(); return *path ? S_OK : E_OUTOFMEMORY; }
    STDMETHOD(GetEncoding)(BSTR* encoding) { if (!encoding) return E_POINTER; *encoding = m_encoding.AllocSysString(); return *encoding ? S_OK : E_OUTOFMEMORY; }
};
}
HRESULT FbePluginApiV2::CreateHost(HWND owner, LPCWSTR locale, IFBEPluginHost** host) {
    if (!host) return E_POINTER; *host = NULL; CComObject<PluginProgressSink>* progress = NULL; HRESULT hr = CComObject<PluginProgressSink>::CreateInstance(&progress); if (FAILED(hr)) return hr; progress->AddRef();
    CComObject<PluginCancellationToken>* cancellation = NULL; hr = CComObject<PluginCancellationToken>::CreateInstance(&cancellation); if (FAILED(hr)) { progress->Release(); return hr; } cancellation->AddRef();
    CComObject<PluginHost>* result = NULL; hr = CComObject<PluginHost>::CreateInstance(&result); if (FAILED(hr)) { cancellation->Release(); progress->Release(); return hr; }
    result->AddRef(); result->m_owner = owner; result->m_locale = locale ? locale : L""; result->m_progress.Attach(progress); result->m_cancellation.Attach(cancellation); *host = result; StartupTrace::Event(L"plugin", L"P210", L"plugin-host-created"); return S_OK;
}
HRESULT FbePluginApiV2::CreateSnapshot(MSXML2::IXMLDOMDocument2* document, LPCWSTR sourcePath, LPCWSTR encoding, IFBEDocumentSnapshot** snapshot) {
    if (!snapshot) return E_POINTER; *snapshot = NULL; if (!document) return E_INVALIDARG; CComObject<DocumentSnapshot>* result = NULL; HRESULT hr = CComObject<DocumentSnapshot>::CreateInstance(&result); if (FAILED(hr)) return hr;
    result->AddRef(); result->m_xml = static_cast<BSTR>(document->xml);
    if (result->m_xml.Left(5).CompareNoCase(L"<?xml") == 0) { const int end = result->m_xml.Find(L"?>"); if (end >= 0) result->m_xml = result->m_xml.Mid(end + 2); }
    result->m_xml = CString(L"<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n") + result->m_xml;
    result->m_path = sourcePath ? sourcePath : L""; result->m_encoding = L"utf-8"; *snapshot = result; StartupTrace::Event(L"plugin", L"P211", L"plugin-snapshot-created"); return S_OK;
}
