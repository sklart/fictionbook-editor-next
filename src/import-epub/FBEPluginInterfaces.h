#pragma once

// Minimal copy of the FBE import plugin COM interface.
//
// FBE itself declares the same interface in fbe/FBE.h, generated from fbe.idl:
//   IID_IFBEImportPlugin = 8094bc55-99c0-4adf-bd55-71e206dfd403
//
// Keeping this tiny declaration locally makes the plugin independent from the
// exact location of generated FBE.h while remaining binary-compatible with FBE.

#include <unknwn.h>
#include <oaidl.h>

#ifndef __IFBEImportPlugin_INTERFACE_DEFINED__
#define __IFBEImportPlugin_INTERFACE_DEFINED__

MIDL_INTERFACE("8094bc55-99c0-4adf-bd55-71e206dfd403")
IFBEImportPlugin : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Import(
        /* [in] */ long hWnd,
        /* [out] */ BSTR* filename,
        /* [out] */ IDispatch** document) = 0;
};

#endif // __IFBEImportPlugin_INTERFACE_DEFINED__
