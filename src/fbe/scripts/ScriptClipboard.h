#pragma once
#include "../clipboard/HtmlPasteSanitizer.h"
#include <shlobj.h>
#include <vector>

namespace FbeScripts { namespace Clipboard {
constexpr size_t MaximumBytes=2*1024*1024;
inline CString Text(const CComVariant& value) {
    const UINT length=::SysStringLen(value.bstrVal);
    if(length>MaximumBytes/sizeof(wchar_t)||(value.bstrVal&&wcslen(value.bstrVal)!=length))CheckError(E_INVALIDARG);
    if(length&&!::WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.bstrVal,length,nullptr,0,nullptr,nullptr))CheckError(E_INVALIDARG);
    return CString(value.bstrVal,length);
}
struct Memory {
    HGLOBAL value=nullptr;
    ~Memory(){if(value)::GlobalFree(value);}
    Memory()=default; Memory(const Memory&)=delete;Memory& operator=(const Memory&)=delete;
};
struct Locked {
    HGLOBAL memory; void* data;
    explicit Locked(HGLOBAL value):memory(value),data(::GlobalLock(value)){if(!data)CheckError(E_OUTOFMEMORY);}
    ~Locked(){::GlobalUnlock(memory);}
};
struct Open {
    explicit Open(HWND owner){if(!::OpenClipboard(owner))CheckError(CLIPBRD_E_CANT_OPEN);}
    ~Open(){::CloseClipboard();}
};
struct Medium {
    STGMEDIUM value={};
    ~Medium(){if(value.tymed)::ReleaseStgMedium(&value);}
};
struct Format { CLIPFORMAT id;std::vector<BYTE> bytes; };
class DataObject final : public IDataObject {
    volatile LONG references=1;
    std::vector<Format> formats;
public:
    explicit DataObject(std::vector<Format>&& values):formats(std::move(values)){}
    STDMETHOD(QueryInterface)(REFIID iid,void** value) override {
        if(!value)return E_POINTER;*value=nullptr;
        if(iid!=IID_IUnknown&&iid!=IID_IDataObject)return E_NOINTERFACE;
        *value=static_cast<IDataObject*>(this);AddRef();return S_OK;
    }
    STDMETHOD_(ULONG,AddRef)() override{return static_cast<ULONG>(::InterlockedIncrement(&references));}
    STDMETHOD_(ULONG,Release)() override{const ULONG count=static_cast<ULONG>(::InterlockedDecrement(&references));if(!count)delete this;return count;}
    STDMETHOD(QueryGetData)(FORMATETC* request) override {
        if(!request)return E_POINTER;
        if(request->ptd)return DV_E_DVTARGETDEVICE;
        if(request->dwAspect!=DVASPECT_CONTENT||request->lindex!=-1||!(request->tymed&TYMED_HGLOBAL))return DV_E_FORMATETC;
        for(const auto& format:formats)if(format.id==request->cfFormat)return S_OK;
        return DV_E_FORMATETC;
    }
    STDMETHOD(GetData)(FORMATETC* request,STGMEDIUM* medium) override {
        if(!medium)return E_POINTER;*medium={};const HRESULT hr=QueryGetData(request);if(FAILED(hr))return hr;
        for(const auto& format:formats)if(format.id==request->cfFormat) {
            HGLOBAL memory=::GlobalAlloc(GMEM_MOVEABLE,format.bytes.size());if(!memory)return E_OUTOFMEMORY;
            void* data=::GlobalLock(memory);if(!data){::GlobalFree(memory);return E_OUTOFMEMORY;}
            memcpy(data,format.bytes.data(),format.bytes.size());::GlobalUnlock(memory);
            medium->tymed=TYMED_HGLOBAL;medium->hGlobal=memory;return S_OK;
        } return DV_E_FORMATETC;
    }
    STDMETHOD(GetDataHere)(FORMATETC*,STGMEDIUM*) override{return DATA_E_FORMATETC;}
    STDMETHOD(GetCanonicalFormatEtc)(FORMATETC*,FORMATETC* result) override {if(!result)return E_POINTER;result->ptd=nullptr;return E_NOTIMPL;}
    STDMETHOD(SetData)(FORMATETC*,STGMEDIUM*,BOOL) override{return E_NOTIMPL;}
    STDMETHOD(EnumFormatEtc)(DWORD direction,IEnumFORMATETC** result) override {
        if(!result)return E_POINTER;*result=nullptr;if(direction!=DATADIR_GET)return E_NOTIMPL;
        std::vector<FORMATETC> descriptors;
        try {for(const auto& format:formats)descriptors.push_back({format.id,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL});}
        catch(...){return E_OUTOFMEMORY;}
        return ::SHCreateStdEnumFmtEtc(static_cast<UINT>(descriptors.size()),descriptors.data(),result);
    }
    STDMETHOD(DAdvise)(FORMATETC*,DWORD,IAdviseSink*,DWORD*) override{return OLE_E_ADVISENOTSUPPORTED;}
    STDMETHOD(DUnadvise)(DWORD) override{return OLE_E_ADVISENOTSUPPORTED;}
    STDMETHOD(EnumDAdvise)(IEnumSTATDATA**) override{return OLE_E_ADVISENOTSUPPORTED;}
};
// The isolated runtime harness substitutes storage only. All validation,
// CF_HTML encoding/decoding and IDataObject ownership remain production code.
struct TestStorage { CComPtr<IDataObject> object; HRESULT failure=S_OK; };
inline thread_local TestStorage* testStorage=nullptr;
inline CString Clean(const CString& html) {
    if(html.IsEmpty())return CString();
    CString output;
    if(FbeClipboard::HtmlPasteSanitizer::Sanitize(html,L"\u00a0",output)!=FbeClipboard::HtmlPasteOutcome::Sanitized)CheckError(E_INVALIDARG);
    return output;
}
inline CLIPFORMAT HtmlFormat() {
    const UINT format=::RegisterClipboardFormatW(L"HTML Format");if(!format)CheckError(E_FAIL);return static_cast<CLIPFORMAT>(format);
}
inline std::vector<BYTE> HtmlBytes(const CString& html) {
    const int length=html.IsEmpty()?0: ::WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,html,html.GetLength(),nullptr,0,nullptr,nullptr);
    if(!html.IsEmpty()&&!length)CheckError(E_INVALIDARG);
    const CStringA prefix="<html><body><!--StartFragment-->",suffix="<!--EndFragment--></body></html>";
    CStringA header;header.Format("Version:1.0\r\nStartHTML:%010u\r\nEndHTML:%010u\r\nStartFragment:%010u\r\nEndFragment:%010u\r\n",0U,0U,0U,0U);
    const unsigned start=header.GetLength(),fragment=start+prefix.GetLength(),end=fragment+length,finish=end+suffix.GetLength();
    if(finish+1>MaximumBytes)CheckError(E_INVALIDARG);
    header.Format("Version:1.0\r\nStartHTML:%010u\r\nEndHTML:%010u\r\nStartFragment:%010u\r\nEndFragment:%010u\r\n",start,finish,fragment,end);
    std::vector<BYTE> bytes(finish+1,0);memcpy(bytes.data(),header.GetString(),header.GetLength());
    memcpy(bytes.data()+start,prefix.GetString(),prefix.GetLength());
    if(length)::WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,html,html.GetLength(),reinterpret_cast<char*>(bytes.data()+fragment),length,nullptr,nullptr);
    memcpy(bytes.data()+end,suffix.GetString(),suffix.GetLength());return bytes;
}
inline CComPtr<IDataObject> Prepare(const CString& text,const CString* html=nullptr) {
    std::vector<Format> formats;Format plain={CF_UNICODETEXT,{}};
    plain.bytes.resize((static_cast<size_t>(text.GetLength())+1)*sizeof(wchar_t));
    memcpy(plain.bytes.data(),text.GetString(),plain.bytes.size());formats.push_back(std::move(plain));
    if(html)formats.push_back({HtmlFormat(),HtmlBytes(Clean(*html))});
    CComPtr<IDataObject> object;object.Attach(new DataObject(std::move(formats)));return object;
}
inline CString Decode(HGLOBAL memory,bool html) {
    if(!memory)CheckError(E_UNEXPECTED);
    const SIZE_T size=::GlobalSize(memory);
    if(!size||size>MaximumBytes+(html?0:sizeof(wchar_t)))CheckError(E_INVALIDARG);
    if(html) {
        CString fragment;
        if(!FbeClipboard::HtmlPasteSanitizer::DecodeClipboardFragment(memory,fragment,true))CheckError(E_INVALIDARG);
        CComVariant value(fragment);return Clean(Text(value));
    }
    if(size%sizeof(wchar_t))CheckError(E_INVALIDARG);
    Locked lock(memory);const auto* text=static_cast<const wchar_t*>(lock.data);
    const size_t count=wcsnlen_s(text,size/sizeof(wchar_t));
    if(count==size/sizeof(wchar_t))CheckError(E_INVALIDARG);
    CComVariant value(CString(text,static_cast<int>(count)));return Text(value);
}
inline CComVariant Operation(HWND owner,DISPID method,const std::vector<CComVariant>& args) {
    if(method==158||method==160) {
        const CString text=Text(args[method==160?1:0]);CString html;
        if(method==160)html=Text(args[0]);
        CComPtr<IDataObject> object=Prepare(text,method==160?&html:nullptr);
        if(testStorage){CheckError(testStorage->failure);testStorage->object=object;}
        else CheckError(::OleSetClipboard(object));
        return CComVariant(true);
    }
    const bool html=method==159;const CLIPFORMAT format=html?HtmlFormat():CF_UNICODETEXT;
    if(testStorage) {
        CheckError(testStorage->failure);if(!testStorage->object)return CComVariant(L"");
        FORMATETC request={format,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
        if(testStorage->object->QueryGetData(&request)==DV_E_FORMATETC)return CComVariant(L"");
        Medium medium;CheckError(testStorage->object->GetData(&request,&medium.value));
        if(medium.value.tymed!=TYMED_HGLOBAL)CheckError(E_UNEXPECTED);
        return CComVariant(Decode(medium.value.hGlobal,html));
    }
    Open clipboard(owner);if(!::IsClipboardFormatAvailable(format))return CComVariant(L"");
    return CComVariant(Decode(static_cast<HGLOBAL>(::GetClipboardData(format)),html));
}
} }
