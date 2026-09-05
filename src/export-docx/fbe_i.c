

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


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



#ifdef __cplusplus
extern "C"{
#endif 


#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#else // !_MIDL_USE_GUIDDEF_

#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        EXTERN_C __declspec(selectany) const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif // !_MIDL_USE_GUIDDEF_

MIDL_DEFINE_GUID(IID, LIBID_FBELib,0x37B16C7D,0x4400,0x4d7d,0xAA,0x35,0x14,0xC7,0x4E,0x26,0x5E,0xA4);


MIDL_DEFINE_GUID(IID, IID_IFBEImportPlugin,0x8094bc55,0x99c0,0x4adf,0xbd,0x55,0x71,0xe2,0x06,0xdf,0xd4,0x03);


MIDL_DEFINE_GUID(IID, IID_IFBEExportPlugin,0x1afaab7f,0x6f66,0x4ef6,0xb1,0x99,0x16,0xfa,0x49,0xcc,0x5b,0x52);


MIDL_DEFINE_GUID(IID, IID_IFBEPluginInfo2,0x8B261B99,0x2842,0x42B3,0x9C,0xE2,0x0E,0x74,0x58,0x97,0xB7,0x14);


MIDL_DEFINE_GUID(IID, IID_IFBEProgressSink,0x8A1C1E23,0x1B04,0x4A03,0xA6,0x6D,0x5C,0x04,0x3D,0x64,0xA9,0x18);


MIDL_DEFINE_GUID(IID, IID_IFBECancellationToken,0x53C61B95,0x63C2,0x449B,0xAF,0x03,0x6A,0x7F,0x85,0xC5,0x7D,0x8A);


MIDL_DEFINE_GUID(IID, IID_IFBEDocumentSnapshot,0x5A624ED2,0x8418,0x448E,0x94,0xE8,0x12,0xD0,0xBC,0xA6,0xF3,0xE3);


MIDL_DEFINE_GUID(IID, IID_IFBEPluginHost,0xCE40BDDD,0x5A69,0x4439,0xA4,0x63,0x33,0xB8,0x81,0x64,0xB0,0xD0);


MIDL_DEFINE_GUID(IID, IID_IFBEImportPlugin2,0x387B8B64,0x28D3,0x4C52,0x8C,0x2F,0x5F,0x8E,0xCF,0x31,0xA8,0xC1);


MIDL_DEFINE_GUID(IID, IID_IFBEExportPlugin2,0xB65F97B9,0xD9CD,0x430E,0x80,0xE2,0x2E,0x01,0x0C,0xB2,0xC7,0xD8);


MIDL_DEFINE_GUID(IID, IID_IExternalHelper,0x7269066E,0x2089,0x4408,0xB3,0xF3,0xE8,0xD7,0x59,0x84,0xD5,0xA6);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



