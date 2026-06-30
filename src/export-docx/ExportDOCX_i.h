

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Tue Jan 19 06:14:07 2038
 */
/* Compiler settings for ExportDOCX.idl:
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


#ifndef __ExportDOCX_i_h__
#define __ExportDOCX_i_h__

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

#ifndef __ExportDOCXPlugin_FWD_DEFINED__
#define __ExportDOCXPlugin_FWD_DEFINED__

#ifdef __cplusplus
typedef class ExportDOCXPlugin ExportDOCXPlugin;
#else
typedef struct ExportDOCXPlugin ExportDOCXPlugin;
#endif /* __cplusplus */

#endif 	/* __ExportDOCXPlugin_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "fbe.h"

#ifdef __cplusplus
extern "C"{
#endif 



#ifndef __ExportDOCXLib_LIBRARY_DEFINED__
#define __ExportDOCXLib_LIBRARY_DEFINED__

/* library ExportDOCXLib */
/* [uuid] */ 


EXTERN_C const IID LIBID_ExportDOCXLib;

EXTERN_C const CLSID CLSID_ExportDOCXPlugin;

#ifdef __cplusplus

class DECLSPEC_UUID("41494D79-3346-4E8C-A432-51BCD3742FC1")
ExportDOCXPlugin;
#endif
#endif /* __ExportDOCXLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


