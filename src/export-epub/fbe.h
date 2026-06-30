

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Tue Jan 19 06:14:07 2038
 */
/* Compiler settings for ..\fbe\fbe.idl:
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


