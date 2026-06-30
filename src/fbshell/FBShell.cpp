#include "stdafx.h"
#if defined(FBE_ENABLE_EXPERIMENTAL_PROPERTY_HANDLER)
#include "Fb2PropertyHandler.h"
#endif
#if defined(FBE_ENABLE_EXPERIMENTAL_THUMBNAIL_PROVIDER)
#include "Fb2ThumbnailProvider.h"
#endif

// {69EA815C-7D5E-486e-85D7-433B19127467}
DEFINE_GUID(FMTID_FB,
0x69ea815c, 0x7d5e, 0x486e, 0x85, 0xd7, 0x43, 0x3b, 0x19, 0x12, 0x74, 0x67);

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
#if defined(FBE_ENABLE_EXPERIMENTAL_PROPERTY_HANDLER)
  OBJECT_ENTRY(CLSID_Fb2PropertyHandler, Fb2PropertyHandler)
#endif
#if defined(FBE_ENABLE_EXPERIMENTAL_THUMBNAIL_PROVIDER)
  OBJECT_ENTRY(CLSID_Fb2ThumbnailProvider, Fb2ThumbnailProvider)
#endif
END_OBJECT_MAP()

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
  if (dwReason == DLL_PROCESS_ATTACH)
  {
    _Module.Init(ObjectMap, hInstance);
    DisableThreadLibraryCalls(hInstance);
  }
  else if (dwReason == DLL_PROCESS_DETACH) {
    _Module.Term();
  }
  return TRUE;    // ok
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI DllCanUnloadNow(void)
{
  return (_Module.GetLockCount()==0) ? S_OK : S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
  return _Module.GetClassObject(rclsid, riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI DllRegisterServer(void)
{
  // registers object, typelib and all interfaces in typelib
  return _Module.RegisterServer();
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
  return _Module.UnregisterServer();
}

/////////////////////////////////////////////////////////////////////////////
// misc global utilities
const wchar_t *FBNS=L"http://www.gribuser.ru/xml/fictionbook/2.0";
const wchar_t *XLINKNS=L"http://www.w3.org/1999/xlink";

bool StrEQ(const wchar_t *zstr,const wchar_t *wstr,int wlen) {
  while (*zstr && wlen--)
    if (*zstr++!=*wstr++)
      return false;

  return wlen==0 && !*zstr;
}

void NormalizeInplace(WTL::CString& s) {
  int len=s.GetLength();
  TCHAR *p=s.GetBuffer(len);
  TCHAR *r=p;
  TCHAR *q=p;
  TCHAR *e=p+len;
  int state=0;

  while (p<e) {
    switch (state) {
    case 0:
      if ((unsigned)*p > 32) {
        *q++=*p;
        state=1;
      }
      break;
    case 1:
      if ((unsigned)*p > 32)
        *q++=*p;
      else
        state=2;
      break;
    case 2:
      if ((unsigned)*p > 32) {
        *q++=_T(' ');
        *q++=*p;
        state=1;
      }
      break;
    }
    ++p;
  }
  s.ReleaseBuffer(static_cast<int>(q-r));
}

WTL::CString GetAttr(MSXML2::ISAXAttributes *attr,const wchar_t *name,const wchar_t *ns) {
  int nslen=ns ? lstrlenW(ns) : 0;
  int nlen=lstrlenW(name);

  int vlen;
  wchar_t *val;

  if (FAILED(attr->raw_getValueFromName((wchar_t*)ns,nslen,(wchar_t*)name,nlen,&val,&vlen)))
    return WTL::CString();

  WTL::CString ret(val,vlen);
  NormalizeInplace(ret);
  return ret;
}

void AppendText(WTL::CString& str,const TCHAR *text,int len) {
  int off=str.GetLength();
  int total=off+len;

  TCHAR *buf=str.GetBuffer(total);

  memcpy(buf+off,text,len*sizeof(TCHAR));

  str.ReleaseBuffer(total);
}
