

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Tue Jan 19 06:14:07 2038
 */
/* Compiler settings for ..\contracts\fbe.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 8.01.0628 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */


#ifndef __fbe_h__
#define __fbe_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef DECLSPEC_XFGVIRT
#if defined(_CONTROL_FLOW_GUARD_XFG)
#define DECLSPEC_XFGVIRT(base, func) __declspec(xfg_virtual(base, func))
#else
#define DECLSPEC_XFGVIRT(base, func)
#endif
#endif

/* Forward Declarations */ 

#ifndef __IFBEImportPlugin_FWD_DEFINED__
#define __IFBEImportPlugin_FWD_DEFINED__
typedef interface IFBEImportPlugin IFBEImportPlugin;

#endif 	/* __IFBEImportPlugin_FWD_DEFINED__ */


#ifndef __IFBEExportPlugin_FWD_DEFINED__
#define __IFBEExportPlugin_FWD_DEFINED__
typedef interface IFBEExportPlugin IFBEExportPlugin;

#endif 	/* __IFBEExportPlugin_FWD_DEFINED__ */


#ifndef __IFBEPluginInfo2_FWD_DEFINED__
#define __IFBEPluginInfo2_FWD_DEFINED__
typedef interface IFBEPluginInfo2 IFBEPluginInfo2;

#endif 	/* __IFBEPluginInfo2_FWD_DEFINED__ */


#ifndef __IFBEProgressSink_FWD_DEFINED__
#define __IFBEProgressSink_FWD_DEFINED__
typedef interface IFBEProgressSink IFBEProgressSink;

#endif 	/* __IFBEProgressSink_FWD_DEFINED__ */


#ifndef __IFBECancellationToken_FWD_DEFINED__
#define __IFBECancellationToken_FWD_DEFINED__
typedef interface IFBECancellationToken IFBECancellationToken;

#endif 	/* __IFBECancellationToken_FWD_DEFINED__ */


#ifndef __IFBEDocumentSnapshot_FWD_DEFINED__
#define __IFBEDocumentSnapshot_FWD_DEFINED__
typedef interface IFBEDocumentSnapshot IFBEDocumentSnapshot;

#endif 	/* __IFBEDocumentSnapshot_FWD_DEFINED__ */


#ifndef __IFBEPluginHost_FWD_DEFINED__
#define __IFBEPluginHost_FWD_DEFINED__
typedef interface IFBEPluginHost IFBEPluginHost;

#endif 	/* __IFBEPluginHost_FWD_DEFINED__ */


#ifndef __IFBEImportPlugin2_FWD_DEFINED__
#define __IFBEImportPlugin2_FWD_DEFINED__
typedef interface IFBEImportPlugin2 IFBEImportPlugin2;

#endif 	/* __IFBEImportPlugin2_FWD_DEFINED__ */


#ifndef __IFBEExportPlugin2_FWD_DEFINED__
#define __IFBEExportPlugin2_FWD_DEFINED__
typedef interface IFBEExportPlugin2 IFBEExportPlugin2;

#endif 	/* __IFBEExportPlugin2_FWD_DEFINED__ */


#ifndef __IExternalHelper_FWD_DEFINED__
#define __IExternalHelper_FWD_DEFINED__
typedef interface IExternalHelper IExternalHelper;

#endif 	/* __IExternalHelper_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 



#ifndef __FBELib_LIBRARY_DEFINED__
#define __FBELib_LIBRARY_DEFINED__

/* library FBELib */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID LIBID_FBELib;

#ifndef __IFBEImportPlugin_INTERFACE_DEFINED__
#define __IFBEImportPlugin_INTERFACE_DEFINED__

/* interface IFBEImportPlugin */
/* [unique][helpstring][uuid][object] */ 


EXTERN_C const IID IID_IFBEImportPlugin;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8094bc55-99c0-4adf-bd55-71e206dfd403")
    IFBEImportPlugin : public IUnknown
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE Import( 
            /* [in] */ long hWnd,
            /* [out] */ BSTR *filename,
            /* [out] */ IDispatch **document) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IFBEImportPluginVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFBEImportPlugin * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFBEImportPlugin * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFBEImportPlugin * This);
        
        DECLSPEC_XFGVIRT(IFBEImportPlugin, Import)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *Import )( 
            IFBEImportPlugin * This,
            /* [in] */ long hWnd,
            /* [out] */ BSTR *filename,
            /* [out] */ IDispatch **document);
        
        END_INTERFACE
    } IFBEImportPluginVtbl;

    interface IFBEImportPlugin
    {
        CONST_VTBL struct IFBEImportPluginVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFBEImportPlugin_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFBEImportPlugin_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFBEImportPlugin_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFBEImportPlugin_Import(This,hWnd,filename,document)	\
    ( (This)->lpVtbl -> Import(This,hWnd,filename,document) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFBEImportPlugin_INTERFACE_DEFINED__ */


#ifndef __IFBEExportPlugin_INTERFACE_DEFINED__
#define __IFBEExportPlugin_INTERFACE_DEFINED__

/* interface IFBEExportPlugin */
/* [unique][helpstring][uuid][object] */ 


EXTERN_C const IID IID_IFBEExportPlugin;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("1afaab7f-6f66-4ef6-b199-16fa49cc5b52")
    IFBEExportPlugin : public IUnknown
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE Export( 
            /* [in] */ long hWnd,
            /* [in] */ BSTR filename,
            /* [in] */ IDispatch *document) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IFBEExportPluginVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFBEExportPlugin * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFBEExportPlugin * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFBEExportPlugin * This);
        
        DECLSPEC_XFGVIRT(IFBEExportPlugin, Export)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *Export )( 
            IFBEExportPlugin * This,
            /* [in] */ long hWnd,
            /* [in] */ BSTR filename,
            /* [in] */ IDispatch *document);
        
        END_INTERFACE
    } IFBEExportPluginVtbl;

    interface IFBEExportPlugin
    {
        CONST_VTBL struct IFBEExportPluginVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFBEExportPlugin_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFBEExportPlugin_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFBEExportPlugin_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFBEExportPlugin_Export(This,hWnd,filename,document)	\
    ( (This)->lpVtbl -> Export(This,hWnd,filename,document) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFBEExportPlugin_INTERFACE_DEFINED__ */


#ifndef __IFBEPluginInfo2_INTERFACE_DEFINED__
#define __IFBEPluginInfo2_INTERFACE_DEFINED__

/* interface IFBEPluginInfo2 */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IFBEPluginInfo2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8B261B99-2842-42B3-9CE2-0E745897B714")
    IFBEPluginInfo2 : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetPluginId( 
            /* [out] */ BSTR *value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetPluginVersion( 
            /* [out] */ BSTR *value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetApiVersion( 
            /* [out] */ ULONG *value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetCapabilities( 
            /* [out] */ ULONGLONG *value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IFBEPluginInfo2Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFBEPluginInfo2 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFBEPluginInfo2 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFBEPluginInfo2 * This);
        
        DECLSPEC_XFGVIRT(IFBEPluginInfo2, GetPluginId)
        HRESULT ( STDMETHODCALLTYPE *GetPluginId )( 
            IFBEPluginInfo2 * This,
            /* [out] */ BSTR *value);
        
        DECLSPEC_XFGVIRT(IFBEPluginInfo2, GetPluginVersion)
        HRESULT ( STDMETHODCALLTYPE *GetPluginVersion )( 
            IFBEPluginInfo2 * This,
            /* [out] */ BSTR *value);
        
        DECLSPEC_XFGVIRT(IFBEPluginInfo2, GetApiVersion)
        HRESULT ( STDMETHODCALLTYPE *GetApiVersion )( 
            IFBEPluginInfo2 * This,
            /* [out] */ ULONG *value);
        
        DECLSPEC_XFGVIRT(IFBEPluginInfo2, GetCapabilities)
        HRESULT ( STDMETHODCALLTYPE *GetCapabilities )( 
            IFBEPluginInfo2 * This,
            /* [out] */ ULONGLONG *value);
        
        END_INTERFACE
    } IFBEPluginInfo2Vtbl;

    interface IFBEPluginInfo2
    {
        CONST_VTBL struct IFBEPluginInfo2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFBEPluginInfo2_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFBEPluginInfo2_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFBEPluginInfo2_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFBEPluginInfo2_GetPluginId(This,value)	\
    ( (This)->lpVtbl -> GetPluginId(This,value) ) 

#define IFBEPluginInfo2_GetPluginVersion(This,value)	\
    ( (This)->lpVtbl -> GetPluginVersion(This,value) ) 

#define IFBEPluginInfo2_GetApiVersion(This,value)	\
    ( (This)->lpVtbl -> GetApiVersion(This,value) ) 

#define IFBEPluginInfo2_GetCapabilities(This,value)	\
    ( (This)->lpVtbl -> GetCapabilities(This,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFBEPluginInfo2_INTERFACE_DEFINED__ */


#ifndef __IFBEProgressSink_INTERFACE_DEFINED__
#define __IFBEProgressSink_INTERFACE_DEFINED__

/* interface IFBEProgressSink */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IFBEProgressSink;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8A1C1E23-1B04-4A03-A66D-5C043D64A918")
    IFBEProgressSink : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Report( 
            /* [in] */ ULONG completed,
            /* [in] */ ULONG total,
            /* [in] */ BSTR stage) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IFBEProgressSinkVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFBEProgressSink * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFBEProgressSink * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFBEProgressSink * This);
        
        DECLSPEC_XFGVIRT(IFBEProgressSink, Report)
        HRESULT ( STDMETHODCALLTYPE *Report )( 
            IFBEProgressSink * This,
            /* [in] */ ULONG completed,
            /* [in] */ ULONG total,
            /* [in] */ BSTR stage);
        
        END_INTERFACE
    } IFBEProgressSinkVtbl;

    interface IFBEProgressSink
    {
        CONST_VTBL struct IFBEProgressSinkVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFBEProgressSink_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFBEProgressSink_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFBEProgressSink_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFBEProgressSink_Report(This,completed,total,stage)	\
    ( (This)->lpVtbl -> Report(This,completed,total,stage) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFBEProgressSink_INTERFACE_DEFINED__ */


#ifndef __IFBECancellationToken_INTERFACE_DEFINED__
#define __IFBECancellationToken_INTERFACE_DEFINED__

/* interface IFBECancellationToken */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IFBECancellationToken;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("53C61B95-63C2-449B-AF03-6A7F85C57D8A")
    IFBECancellationToken : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE IsCancellationRequested( 
            /* [out] */ BOOL *cancelled) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IFBECancellationTokenVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFBECancellationToken * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFBECancellationToken * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFBECancellationToken * This);
        
        DECLSPEC_XFGVIRT(IFBECancellationToken, IsCancellationRequested)
        HRESULT ( STDMETHODCALLTYPE *IsCancellationRequested )( 
            IFBECancellationToken * This,
            /* [out] */ BOOL *cancelled);
        
        END_INTERFACE
    } IFBECancellationTokenVtbl;

    interface IFBECancellationToken
    {
        CONST_VTBL struct IFBECancellationTokenVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFBECancellationToken_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFBECancellationToken_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFBECancellationToken_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFBECancellationToken_IsCancellationRequested(This,cancelled)	\
    ( (This)->lpVtbl -> IsCancellationRequested(This,cancelled) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFBECancellationToken_INTERFACE_DEFINED__ */


#ifndef __IFBEDocumentSnapshot_INTERFACE_DEFINED__
#define __IFBEDocumentSnapshot_INTERFACE_DEFINED__

/* interface IFBEDocumentSnapshot */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IFBEDocumentSnapshot;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("5A624ED2-8418-448E-94E8-12D0BCA6F3E3")
    IFBEDocumentSnapshot : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE OpenXmlStream( 
            /* [out] */ IStream **stream) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetSourceFilePath( 
            /* [out] */ BSTR *path) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetEncoding( 
            /* [out] */ BSTR *encoding) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IFBEDocumentSnapshotVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFBEDocumentSnapshot * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFBEDocumentSnapshot * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFBEDocumentSnapshot * This);
        
        DECLSPEC_XFGVIRT(IFBEDocumentSnapshot, OpenXmlStream)
        HRESULT ( STDMETHODCALLTYPE *OpenXmlStream )( 
            IFBEDocumentSnapshot * This,
            /* [out] */ IStream **stream);
        
        DECLSPEC_XFGVIRT(IFBEDocumentSnapshot, GetSourceFilePath)
        HRESULT ( STDMETHODCALLTYPE *GetSourceFilePath )( 
            IFBEDocumentSnapshot * This,
            /* [out] */ BSTR *path);
        
        DECLSPEC_XFGVIRT(IFBEDocumentSnapshot, GetEncoding)
        HRESULT ( STDMETHODCALLTYPE *GetEncoding )( 
            IFBEDocumentSnapshot * This,
            /* [out] */ BSTR *encoding);
        
        END_INTERFACE
    } IFBEDocumentSnapshotVtbl;

    interface IFBEDocumentSnapshot
    {
        CONST_VTBL struct IFBEDocumentSnapshotVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFBEDocumentSnapshot_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFBEDocumentSnapshot_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFBEDocumentSnapshot_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFBEDocumentSnapshot_OpenXmlStream(This,stream)	\
    ( (This)->lpVtbl -> OpenXmlStream(This,stream) ) 

#define IFBEDocumentSnapshot_GetSourceFilePath(This,path)	\
    ( (This)->lpVtbl -> GetSourceFilePath(This,path) ) 

#define IFBEDocumentSnapshot_GetEncoding(This,encoding)	\
    ( (This)->lpVtbl -> GetEncoding(This,encoding) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFBEDocumentSnapshot_INTERFACE_DEFINED__ */


#ifndef __IFBEPluginHost_INTERFACE_DEFINED__
#define __IFBEPluginHost_INTERFACE_DEFINED__

/* interface IFBEPluginHost */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IFBEPluginHost;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("CE40BDDD-5A69-4439-A463-33B88164B0D0")
    IFBEPluginHost : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetHostVersion( 
            /* [out] */ BSTR *version) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetUiLocale( 
            /* [out] */ BSTR *locale) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetOwnerWindow( 
            /* [out] */ LONGLONG *hwnd) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetProgressSink( 
            /* [out] */ IFBEProgressSink **sink) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetCancellationToken( 
            /* [out] */ IFBECancellationToken **token) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ReportMessage( 
            /* [in] */ LONG severity,
            /* [in] */ BSTR code,
            /* [in] */ BSTR message) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Trace( 
            /* [in] */ BSTR eventName,
            /* [in] */ BSTR detail) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IFBEPluginHostVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFBEPluginHost * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFBEPluginHost * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFBEPluginHost * This);
        
        DECLSPEC_XFGVIRT(IFBEPluginHost, GetHostVersion)
        HRESULT ( STDMETHODCALLTYPE *GetHostVersion )( 
            IFBEPluginHost * This,
            /* [out] */ BSTR *version);
        
        DECLSPEC_XFGVIRT(IFBEPluginHost, GetUiLocale)
        HRESULT ( STDMETHODCALLTYPE *GetUiLocale )( 
            IFBEPluginHost * This,
            /* [out] */ BSTR *locale);
        
        DECLSPEC_XFGVIRT(IFBEPluginHost, GetOwnerWindow)
        HRESULT ( STDMETHODCALLTYPE *GetOwnerWindow )( 
            IFBEPluginHost * This,
            /* [out] */ LONGLONG *hwnd);
        
        DECLSPEC_XFGVIRT(IFBEPluginHost, GetProgressSink)
        HRESULT ( STDMETHODCALLTYPE *GetProgressSink )( 
            IFBEPluginHost * This,
            /* [out] */ IFBEProgressSink **sink);
        
        DECLSPEC_XFGVIRT(IFBEPluginHost, GetCancellationToken)
        HRESULT ( STDMETHODCALLTYPE *GetCancellationToken )( 
            IFBEPluginHost * This,
            /* [out] */ IFBECancellationToken **token);
        
        DECLSPEC_XFGVIRT(IFBEPluginHost, ReportMessage)
        HRESULT ( STDMETHODCALLTYPE *ReportMessage )( 
            IFBEPluginHost * This,
            /* [in] */ LONG severity,
            /* [in] */ BSTR code,
            /* [in] */ BSTR message);
        
        DECLSPEC_XFGVIRT(IFBEPluginHost, Trace)
        HRESULT ( STDMETHODCALLTYPE *Trace )( 
            IFBEPluginHost * This,
            /* [in] */ BSTR eventName,
            /* [in] */ BSTR detail);
        
        END_INTERFACE
    } IFBEPluginHostVtbl;

    interface IFBEPluginHost
    {
        CONST_VTBL struct IFBEPluginHostVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFBEPluginHost_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFBEPluginHost_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFBEPluginHost_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFBEPluginHost_GetHostVersion(This,version)	\
    ( (This)->lpVtbl -> GetHostVersion(This,version) ) 

#define IFBEPluginHost_GetUiLocale(This,locale)	\
    ( (This)->lpVtbl -> GetUiLocale(This,locale) ) 

#define IFBEPluginHost_GetOwnerWindow(This,hwnd)	\
    ( (This)->lpVtbl -> GetOwnerWindow(This,hwnd) ) 

#define IFBEPluginHost_GetProgressSink(This,sink)	\
    ( (This)->lpVtbl -> GetProgressSink(This,sink) ) 

#define IFBEPluginHost_GetCancellationToken(This,token)	\
    ( (This)->lpVtbl -> GetCancellationToken(This,token) ) 

#define IFBEPluginHost_ReportMessage(This,severity,code,message)	\
    ( (This)->lpVtbl -> ReportMessage(This,severity,code,message) ) 

#define IFBEPluginHost_Trace(This,eventName,detail)	\
    ( (This)->lpVtbl -> Trace(This,eventName,detail) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFBEPluginHost_INTERFACE_DEFINED__ */


#ifndef __IFBEImportPlugin2_INTERFACE_DEFINED__
#define __IFBEImportPlugin2_INTERFACE_DEFINED__

/* interface IFBEImportPlugin2 */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IFBEImportPlugin2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("387B8B64-28D3-4C52-8C2F-5F8ECF31A8C1")
    IFBEImportPlugin2 : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Import( 
            /* [in] */ IFBEPluginHost *host,
            /* [out] */ BSTR *suggestedFileName,
            /* [out] */ IStream **fb2Xml) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IFBEImportPlugin2Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFBEImportPlugin2 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFBEImportPlugin2 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFBEImportPlugin2 * This);
        
        DECLSPEC_XFGVIRT(IFBEImportPlugin2, Import)
        HRESULT ( STDMETHODCALLTYPE *Import )( 
            IFBEImportPlugin2 * This,
            /* [in] */ IFBEPluginHost *host,
            /* [out] */ BSTR *suggestedFileName,
            /* [out] */ IStream **fb2Xml);
        
        END_INTERFACE
    } IFBEImportPlugin2Vtbl;

    interface IFBEImportPlugin2
    {
        CONST_VTBL struct IFBEImportPlugin2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFBEImportPlugin2_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFBEImportPlugin2_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFBEImportPlugin2_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFBEImportPlugin2_Import(This,host,suggestedFileName,fb2Xml)	\
    ( (This)->lpVtbl -> Import(This,host,suggestedFileName,fb2Xml) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFBEImportPlugin2_INTERFACE_DEFINED__ */


#ifndef __IFBEExportPlugin2_INTERFACE_DEFINED__
#define __IFBEExportPlugin2_INTERFACE_DEFINED__

/* interface IFBEExportPlugin2 */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IFBEExportPlugin2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("B65F97B9-D9CD-430E-80E2-2E010CB2C7D8")
    IFBEExportPlugin2 : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Export( 
            /* [in] */ IFBEPluginHost *host,
            /* [in] */ BSTR sourceFileName,
            /* [in] */ IFBEDocumentSnapshot *document) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IFBEExportPlugin2Vtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFBEExportPlugin2 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFBEExportPlugin2 * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFBEExportPlugin2 * This);
        
        DECLSPEC_XFGVIRT(IFBEExportPlugin2, Export)
        HRESULT ( STDMETHODCALLTYPE *Export )( 
            IFBEExportPlugin2 * This,
            /* [in] */ IFBEPluginHost *host,
            /* [in] */ BSTR sourceFileName,
            /* [in] */ IFBEDocumentSnapshot *document);
        
        END_INTERFACE
    } IFBEExportPlugin2Vtbl;

    interface IFBEExportPlugin2
    {
        CONST_VTBL struct IFBEExportPlugin2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFBEExportPlugin2_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFBEExportPlugin2_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFBEExportPlugin2_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFBEExportPlugin2_Export(This,host,sourceFileName,document)	\
    ( (This)->lpVtbl -> Export(This,host,sourceFileName,document) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFBEExportPlugin2_INTERFACE_DEFINED__ */


#ifndef __IExternalHelper_INTERFACE_DEFINED__
#define __IExternalHelper_INTERFACE_DEFINED__

/* interface IExternalHelper */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_IExternalHelper;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("7269066E-2089-4408-B3F3-E8D75984D5A6")
    IExternalHelper : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE BeginUndoUnit( 
            /* [in] */ IDispatch *document,
            /* [in] */ BSTR action) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE EndUndoUnit( 
            /* [in] */ IDispatch *document) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_inflateBlock( 
            /* [in] */ IDispatch *elem,
            /* [retval][out] */ BOOL *pVal) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_inflateBlock( 
            /* [in] */ IDispatch *elem,
            /* [in] */ BOOL newVal) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GenrePopup( 
            /* [in] */ IDispatch *elem,
            /* [in] */ LONG x,
            /* [in] */ LONG y,
            /* [retval][out] */ BSTR *name) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetStylePath( 
            /* [retval][out] */ BSTR *name) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetBinarySize( 
            /* [in] */ BSTR data,
            /* [retval][out] */ int *length) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE InflateParagraphs( 
            /* [in] */ IDispatch *data) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetUUID( 
            /* [retval][out] */ BSTR *name) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE MsgBox( 
            /* [in] */ BSTR message) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE AskYesNo( 
            /* [in] */ BSTR message,
            /* [retval][out] */ BOOL *pVal) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SaveBinary( 
            /* [in] */ BSTR path,
            /* [in] */ BSTR data,
            /* [in] */ BOOL prompt,
            /* [retval][out] */ BOOL *pVal) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetExtendedStyle( 
            /* [in] */ BSTR elem,
            /* [retval][out] */ BOOL *ext) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE DescShowElement( 
            /* [in] */ BSTR elem,
            /* [in] */ BOOL show) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE DescShowMenu( 
            /* [in] */ IDispatch *btn,
            /* [in] */ LONG x,
            /* [in] */ LONG y,
            /* [retval][out] */ BSTR *element_id) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IsFastMode( 
            /* [retval][out] */ BOOL *ext) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SetStyleEx( 
            /* [in] */ IDispatch *doc,
            /* [in] */ IDispatch *elem,
            /* [in] */ BSTR style) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetImageDimsByPath( 
            /* [in] */ BSTR path,
            /* [retval][out] */ BSTR *dims) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetImageDimsByData( 
            /* [in] */ VARIANT *data,
            /* [retval][out] */ BSTR *dims) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetNBSP( 
            /* [retval][out] */ BSTR *name) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetViewWidth( 
            /* [retval][out] */ int *width) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetViewHeight( 
            /* [retval][out] */ int *height) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetProgramVersion( 
            /* [retval][out] */ BSTR *ver) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE InputBox( 
            /* [in] */ BSTR prompt,
            /* [in] */ BSTR title,
            /* [in] */ BSTR value,
            /* [retval][out] */ BSTR *input) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetModalResult( 
            /* [retval][out] */ int *modalResult) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SetStatusBarText( 
            /* [in] */ BSTR text) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetDocumentFilePath( 
            /* [retval][out] */ BSTR *path) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetDocumentFileName( 
            /* [retval][out] */ BSTR *name) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetDocumentDirectory( 
            /* [retval][out] */ BSTR *directory) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IsDiagnosticTraceEnabled( 
            /* [retval][out] */ BOOL *enabled) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE TraceScript( 
            /* [in] */ BSTR code,
            /* [in] */ BSTR message) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetLocalizedString( 
            /* [in] */ BSTR key,
            /* [retval][out] */ BSTR *text) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IExternalHelperVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IExternalHelper * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IExternalHelper * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IExternalHelper * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IExternalHelper * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IExternalHelper * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IExternalHelper * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IExternalHelper * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IExternalHelper, BeginUndoUnit)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *BeginUndoUnit )( 
            IExternalHelper * This,
            /* [in] */ IDispatch *document,
            /* [in] */ BSTR action);
        
        DECLSPEC_XFGVIRT(IExternalHelper, EndUndoUnit)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *EndUndoUnit )( 
            IExternalHelper * This,
            /* [in] */ IDispatch *document);
        
        DECLSPEC_XFGVIRT(IExternalHelper, get_inflateBlock)
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_inflateBlock )( 
            IExternalHelper * This,
            /* [in] */ IDispatch *elem,
            /* [retval][out] */ BOOL *pVal);
        
        DECLSPEC_XFGVIRT(IExternalHelper, put_inflateBlock)
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_inflateBlock )( 
            IExternalHelper * This,
            /* [in] */ IDispatch *elem,
            /* [in] */ BOOL newVal);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GenrePopup)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GenrePopup )( 
            IExternalHelper * This,
            /* [in] */ IDispatch *elem,
            /* [in] */ LONG x,
            /* [in] */ LONG y,
            /* [retval][out] */ BSTR *name);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetStylePath)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetStylePath )( 
            IExternalHelper * This,
            /* [retval][out] */ BSTR *name);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetBinarySize)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetBinarySize )( 
            IExternalHelper * This,
            /* [in] */ BSTR data,
            /* [retval][out] */ int *length);
        
        DECLSPEC_XFGVIRT(IExternalHelper, InflateParagraphs)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *InflateParagraphs )( 
            IExternalHelper * This,
            /* [in] */ IDispatch *data);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetUUID)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetUUID )( 
            IExternalHelper * This,
            /* [retval][out] */ BSTR *name);
        
        DECLSPEC_XFGVIRT(IExternalHelper, MsgBox)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *MsgBox )( 
            IExternalHelper * This,
            /* [in] */ BSTR message);
        
        DECLSPEC_XFGVIRT(IExternalHelper, AskYesNo)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *AskYesNo )( 
            IExternalHelper * This,
            /* [in] */ BSTR message,
            /* [retval][out] */ BOOL *pVal);
        
        DECLSPEC_XFGVIRT(IExternalHelper, SaveBinary)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SaveBinary )( 
            IExternalHelper * This,
            /* [in] */ BSTR path,
            /* [in] */ BSTR data,
            /* [in] */ BOOL prompt,
            /* [retval][out] */ BOOL *pVal);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetExtendedStyle)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetExtendedStyle )( 
            IExternalHelper * This,
            /* [in] */ BSTR elem,
            /* [retval][out] */ BOOL *ext);
        
        DECLSPEC_XFGVIRT(IExternalHelper, DescShowElement)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *DescShowElement )( 
            IExternalHelper * This,
            /* [in] */ BSTR elem,
            /* [in] */ BOOL show);
        
        DECLSPEC_XFGVIRT(IExternalHelper, DescShowMenu)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *DescShowMenu )( 
            IExternalHelper * This,
            /* [in] */ IDispatch *btn,
            /* [in] */ LONG x,
            /* [in] */ LONG y,
            /* [retval][out] */ BSTR *element_id);
        
        DECLSPEC_XFGVIRT(IExternalHelper, IsFastMode)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *IsFastMode )( 
            IExternalHelper * This,
            /* [retval][out] */ BOOL *ext);
        
        DECLSPEC_XFGVIRT(IExternalHelper, SetStyleEx)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SetStyleEx )( 
            IExternalHelper * This,
            /* [in] */ IDispatch *doc,
            /* [in] */ IDispatch *elem,
            /* [in] */ BSTR style);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetImageDimsByPath)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetImageDimsByPath )( 
            IExternalHelper * This,
            /* [in] */ BSTR path,
            /* [retval][out] */ BSTR *dims);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetImageDimsByData)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetImageDimsByData )( 
            IExternalHelper * This,
            /* [in] */ VARIANT *data,
            /* [retval][out] */ BSTR *dims);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetNBSP)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetNBSP )( 
            IExternalHelper * This,
            /* [retval][out] */ BSTR *name);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetViewWidth)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetViewWidth )( 
            IExternalHelper * This,
            /* [retval][out] */ int *width);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetViewHeight)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetViewHeight )( 
            IExternalHelper * This,
            /* [retval][out] */ int *height);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetProgramVersion)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetProgramVersion )( 
            IExternalHelper * This,
            /* [retval][out] */ BSTR *ver);
        
        DECLSPEC_XFGVIRT(IExternalHelper, InputBox)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *InputBox )( 
            IExternalHelper * This,
            /* [in] */ BSTR prompt,
            /* [in] */ BSTR title,
            /* [in] */ BSTR value,
            /* [retval][out] */ BSTR *input);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetModalResult)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetModalResult )( 
            IExternalHelper * This,
            /* [retval][out] */ int *modalResult);
        
        DECLSPEC_XFGVIRT(IExternalHelper, SetStatusBarText)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SetStatusBarText )( 
            IExternalHelper * This,
            /* [in] */ BSTR text);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetDocumentFilePath)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetDocumentFilePath )( 
            IExternalHelper * This,
            /* [retval][out] */ BSTR *path);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetDocumentFileName)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetDocumentFileName )( 
            IExternalHelper * This,
            /* [retval][out] */ BSTR *name);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetDocumentDirectory)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetDocumentDirectory )( 
            IExternalHelper * This,
            /* [retval][out] */ BSTR *directory);
        
        DECLSPEC_XFGVIRT(IExternalHelper, IsDiagnosticTraceEnabled)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *IsDiagnosticTraceEnabled )( 
            IExternalHelper * This,
            /* [retval][out] */ BOOL *enabled);
        
        DECLSPEC_XFGVIRT(IExternalHelper, TraceScript)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *TraceScript )( 
            IExternalHelper * This,
            /* [in] */ BSTR code,
            /* [in] */ BSTR message);
        
        DECLSPEC_XFGVIRT(IExternalHelper, GetLocalizedString)
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetLocalizedString )( 
            IExternalHelper * This,
            /* [in] */ BSTR key,
            /* [retval][out] */ BSTR *text);
        
        END_INTERFACE
    } IExternalHelperVtbl;

    interface IExternalHelper
    {
        CONST_VTBL struct IExternalHelperVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IExternalHelper_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IExternalHelper_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IExternalHelper_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IExternalHelper_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IExternalHelper_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IExternalHelper_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IExternalHelper_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IExternalHelper_BeginUndoUnit(This,document,action)	\
    ( (This)->lpVtbl -> BeginUndoUnit(This,document,action) ) 

#define IExternalHelper_EndUndoUnit(This,document)	\
    ( (This)->lpVtbl -> EndUndoUnit(This,document) ) 

#define IExternalHelper_get_inflateBlock(This,elem,pVal)	\
    ( (This)->lpVtbl -> get_inflateBlock(This,elem,pVal) ) 

#define IExternalHelper_put_inflateBlock(This,elem,newVal)	\
    ( (This)->lpVtbl -> put_inflateBlock(This,elem,newVal) ) 

#define IExternalHelper_GenrePopup(This,elem,x,y,name)	\
    ( (This)->lpVtbl -> GenrePopup(This,elem,x,y,name) ) 

#define IExternalHelper_GetStylePath(This,name)	\
    ( (This)->lpVtbl -> GetStylePath(This,name) ) 

#define IExternalHelper_GetBinarySize(This,data,length)	\
    ( (This)->lpVtbl -> GetBinarySize(This,data,length) ) 

#define IExternalHelper_InflateParagraphs(This,data)	\
    ( (This)->lpVtbl -> InflateParagraphs(This,data) ) 

#define IExternalHelper_GetUUID(This,name)	\
    ( (This)->lpVtbl -> GetUUID(This,name) ) 

#define IExternalHelper_MsgBox(This,message)	\
    ( (This)->lpVtbl -> MsgBox(This,message) ) 

#define IExternalHelper_AskYesNo(This,message,pVal)	\
    ( (This)->lpVtbl -> AskYesNo(This,message,pVal) ) 

#define IExternalHelper_SaveBinary(This,path,data,prompt,pVal)	\
    ( (This)->lpVtbl -> SaveBinary(This,path,data,prompt,pVal) ) 

#define IExternalHelper_GetExtendedStyle(This,elem,ext)	\
    ( (This)->lpVtbl -> GetExtendedStyle(This,elem,ext) ) 

#define IExternalHelper_DescShowElement(This,elem,show)	\
    ( (This)->lpVtbl -> DescShowElement(This,elem,show) ) 

#define IExternalHelper_DescShowMenu(This,btn,x,y,element_id)	\
    ( (This)->lpVtbl -> DescShowMenu(This,btn,x,y,element_id) ) 

#define IExternalHelper_IsFastMode(This,ext)	\
    ( (This)->lpVtbl -> IsFastMode(This,ext) ) 

#define IExternalHelper_SetStyleEx(This,doc,elem,style)	\
    ( (This)->lpVtbl -> SetStyleEx(This,doc,elem,style) ) 

#define IExternalHelper_GetImageDimsByPath(This,path,dims)	\
    ( (This)->lpVtbl -> GetImageDimsByPath(This,path,dims) ) 

#define IExternalHelper_GetImageDimsByData(This,data,dims)	\
    ( (This)->lpVtbl -> GetImageDimsByData(This,data,dims) ) 

#define IExternalHelper_GetNBSP(This,name)	\
    ( (This)->lpVtbl -> GetNBSP(This,name) ) 

#define IExternalHelper_GetViewWidth(This,width)	\
    ( (This)->lpVtbl -> GetViewWidth(This,width) ) 

#define IExternalHelper_GetViewHeight(This,height)	\
    ( (This)->lpVtbl -> GetViewHeight(This,height) ) 

#define IExternalHelper_GetProgramVersion(This,ver)	\
    ( (This)->lpVtbl -> GetProgramVersion(This,ver) ) 

#define IExternalHelper_InputBox(This,prompt,title,value,input)	\
    ( (This)->lpVtbl -> InputBox(This,prompt,title,value,input) ) 

#define IExternalHelper_GetModalResult(This,modalResult)	\
    ( (This)->lpVtbl -> GetModalResult(This,modalResult) ) 

#define IExternalHelper_SetStatusBarText(This,text)	\
    ( (This)->lpVtbl -> SetStatusBarText(This,text) ) 

#define IExternalHelper_GetDocumentFilePath(This,path)	\
    ( (This)->lpVtbl -> GetDocumentFilePath(This,path) ) 

#define IExternalHelper_GetDocumentFileName(This,name)	\
    ( (This)->lpVtbl -> GetDocumentFileName(This,name) ) 

#define IExternalHelper_GetDocumentDirectory(This,directory)	\
    ( (This)->lpVtbl -> GetDocumentDirectory(This,directory) ) 

#define IExternalHelper_IsDiagnosticTraceEnabled(This,enabled)	\
    ( (This)->lpVtbl -> IsDiagnosticTraceEnabled(This,enabled) ) 

#define IExternalHelper_TraceScript(This,code,message)	\
    ( (This)->lpVtbl -> TraceScript(This,code,message) ) 

#define IExternalHelper_GetLocalizedString(This,key,text)	\
    ( (This)->lpVtbl -> GetLocalizedString(This,key,text) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IExternalHelper_INTERFACE_DEFINED__ */

#endif /* __FBELib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


