#pragma once
#include <windows.h>
#include <objidl.h>
#include <oleauto.h>
#include <cstring>

namespace FbeImageData {
// SAFEARRAY storage is borrowed COM memory, not an HGLOBAL. The stream owns a
// separate movable allocation and supports the same IStream contract on Win7.
inline HRESULT CreateStream(SAFEARRAY* data, IStream** stream) {
    if(!stream) return E_POINTER;
    *stream = NULL;
    VARTYPE type = VT_EMPTY;
    if(!data || ::SafeArrayGetDim(data) != 1 || ::SafeArrayGetElemsize(data) != 1 ||
        FAILED(::SafeArrayGetVartype(data, &type)) || type != VT_UI1 || data->rgsabound[0].cElements == 0) return E_INVALIDARG;
    const ULONG length = data->rgsabound[0].cElements;
    void* bytes = NULL;
    HRESULT result = ::SafeArrayAccessData(data, &bytes);
    if(FAILED(result)) return result;
    HGLOBAL owned = ::GlobalAlloc(GMEM_MOVEABLE, length);
    if(!owned) { ::SafeArrayUnaccessData(data); return E_OUTOFMEMORY; }
    void* target = ::GlobalLock(owned);
    if(!target) { ::GlobalFree(owned); ::SafeArrayUnaccessData(data); return E_OUTOFMEMORY; }
    std::memcpy(target, bytes, length);
    ::GlobalUnlock(owned);
    result = ::SafeArrayUnaccessData(data);
    if(SUCCEEDED(result)) result = ::CreateStreamOnHGlobal(owned, TRUE, stream);
    if(FAILED(result)) ::GlobalFree(owned);
    return result;
}
}
