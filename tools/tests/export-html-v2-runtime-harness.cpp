#include "../../src/fbe/FBE.h"
#include <iostream>
#include <oleauto.h>
#include <string>
#include <windows.h>
typedef HRESULT(STDAPICALLTYPE *GetClassObject)(REFCLSID, REFIID, void **);
#ifdef FBE_TEST_EXPORT_EPUB
#define FBE_TEST_CLSID L"{36FCFB2D-C3D8-4B81-ABC1-5A09CA846515}"
#define FBE_TEST_PLUGIN_ID L"export-epub"
#define FBE_TEST_SCENARIO L"export-epub"
#define FBE_TEST_PATH L"FBE_NEXT_TEST_EXPORT_EPUB_PATH"
#define FBE_TEST_CANCEL L"FBE_NEXT_TEST_EXPORT_EPUB_CANCEL"
#define FBE_TEST_FAIL L"FBE_NEXT_TEST_EXPORT_EPUB_FAIL"
#elif defined(FBE_TEST_EXPORT_DOCX)
#define FBE_TEST_CLSID L"{09B5ABFF-177E-4C03-98D0-9EF4E1C9DB56}"
#define FBE_TEST_PLUGIN_ID L"export-docx"
#define FBE_TEST_SCENARIO L"export-docx"
#define FBE_TEST_PATH L"FBE_NEXT_TEST_EXPORT_DOCX_PATH"
#define FBE_TEST_CANCEL L"FBE_NEXT_TEST_EXPORT_DOCX_CANCEL"
#define FBE_TEST_FAIL L"FBE_NEXT_TEST_EXPORT_DOCX_FAIL"
#else
#define FBE_TEST_CLSID L"{C3098839-EF69-4DE5-B27D-1E80051CA843}"
#define FBE_TEST_PLUGIN_ID L"export-html"
#define FBE_TEST_SCENARIO L"export-html"
#define FBE_TEST_PATH L"FBE_NEXT_TEST_EXPORT_HTML_PATH"
#define FBE_TEST_CANCEL L"FBE_NEXT_TEST_EXPORT_HTML_CANCEL"
#define FBE_TEST_FAIL L"FBE_NEXT_TEST_EXPORT_HTML_FAIL"
#endif
static int Die(const wchar_t *s, HRESULT h) {
  std::wcerr << s << L" 0x" << std::hex << h << L"\n";
  return 1;
}
class Progress : public IFBEProgressSink {
  LONG r = 1;

public:
  int start = 0, done = 0;
  STDMETHOD(QueryInterface)(REFIID i, void **p) {
    if (!p)
      return E_POINTER;
    *p = 0;
    if (i != IID_IUnknown && i != IID_IFBEProgressSink)
      return E_NOINTERFACE;
    *p = this;
    AddRef();
    return S_OK;
  }
  STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&r); }
  STDMETHOD_(ULONG, Release)() {
    LONG n = InterlockedDecrement(&r);
    if (!n)
      delete this;
    return n;
  }
  STDMETHOD(Report)(ULONG a, ULONG b, BSTR) {
    if (a == 0 && b == 1)
      ++start;
    if (a == 1 && b == 1)
      ++done;
    return S_OK;
  }
};
class Cancel : public IFBECancellationToken {
  LONG r = 1;

public:
  STDMETHOD(QueryInterface)(REFIID i, void **p) {
    if (!p)
      return E_POINTER;
    *p = 0;
    if (i != IID_IUnknown && i != IID_IFBECancellationToken)
      return E_NOINTERFACE;
    *p = this;
    AddRef();
    return S_OK;
  }
  STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&r); }
  STDMETHOD_(ULONG, Release)() {
    LONG n = InterlockedDecrement(&r);
    if (!n)
      delete this;
    return n;
  }
  STDMETHOD(IsCancellationRequested)(BOOL *x) {
    if (!x)
      return E_POINTER;
    *x = FALSE;
    return S_OK;
  }
};
class Host : public IFBEPluginHost {
  LONG r = 1;
  Progress *p = new Progress;
  Cancel *c = new Cancel;

public:
  int messages = 0;
  ~Host() {
    p->Release();
    c->Release();
  }
  bool ok() { return p->start && p->done; }
  bool noDone() { return !p->done; }
  STDMETHOD(QueryInterface)(REFIID i, void **x) {
    if (!x)
      return E_POINTER;
    *x = 0;
    if (i != IID_IUnknown && i != IID_IFBEPluginHost)
      return E_NOINTERFACE;
    *x = this;
    AddRef();
    return S_OK;
  }
  STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&r); }
  STDMETHOD_(ULONG, Release)() {
    LONG n = InterlockedDecrement(&r);
    if (!n)
      delete this;
    return n;
  }
  STDMETHOD(GetHostVersion)(BSTR *x) {
    if (!x)
      return E_POINTER;
    *x = SysAllocString(L"test");
    return *x ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHOD(GetUiLocale)(BSTR *x) {
    if (!x)
      return E_POINTER;
    *x = SysAllocString(L"ru-RU");
    return *x ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHOD(GetOwnerWindow)(LONGLONG *x) {
    if (!x)
      return E_POINTER;
    *x = 0;
    return S_OK;
  }
  STDMETHOD(GetProgressSink)(IFBEProgressSink **x) {
    if (!x)
      return E_POINTER;
    *x = p;
    p->AddRef();
    return S_OK;
  }
  STDMETHOD(GetCancellationToken)(IFBECancellationToken **x) {
    if (!x)
      return E_POINTER;
    *x = c;
    c->AddRef();
    return S_OK;
  }
  STDMETHOD(ReportMessage)(LONG, BSTR code, BSTR) {
    if (code && !wcscmp(code, L"export-failed"))
      ++messages;
    return S_OK;
  }
  STDMETHOD(Trace)(BSTR, BSTR) { return S_OK; }
};
class Snapshot : public IFBEDocumentSnapshot {
  LONG r = 1;

public:
  STDMETHOD(QueryInterface)(REFIID i, void **x) {
    if (!x)
      return E_POINTER;
    *x = 0;
    if (i != IID_IUnknown && i != IID_IFBEDocumentSnapshot)
      return E_NOINTERFACE;
    *x = this;
    AddRef();
    return S_OK;
  }
  STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&r); }
  STDMETHOD_(ULONG, Release)() {
    LONG n = InterlockedDecrement(&r);
    if (!n)
      delete this;
    return n;
  }
  STDMETHOD(OpenXmlStream)(IStream **x) {
    if (!x)
      return E_POINTER;
    *x = 0;
    IStream *s = 0;
    HRESULT h = CreateStreamOnHGlobal(0, TRUE, &s);
    if (FAILED(h))
      return h;
    const char xml[] =
        u8"<?xml version=\"1.0\" encoding=\"utf-8\"?><FictionBook "
        u8"xmlns=\"http://www.gribuser.ru/xml/fictionbook/"
        u8"2.0\"><description><title-info><genre>prose</"
        u8"genre><author><first-name>Тест</first-name><last-name>Автор</"
        u8"last-name></author><book-title>Кириллица</book-title><lang>ru</"
        u8"lang></title-info></description><body><section><p>Привет, "
        u8"мир</p></section></body></FictionBook>";
    ULONG n = 0;
    h = s->Write(xml, sizeof(xml) - 1, &n);
    LARGE_INTEGER z = {};
    s->Seek(z, STREAM_SEEK_SET, 0);
    *x = s;
    return h;
  }
  STDMETHOD(GetSourceFilePath)(BSTR *x) {
    if (!x)
      return E_POINTER;
    *x = SysAllocString(L"test.fb2");
    return S_OK;
  }
  STDMETHOD(GetEncoding)(BSTR *x) {
    if (!x)
      return E_POINTER;
    *x = SysAllocString(L"utf-8");
    return S_OK;
  }
};
static HRESULT Call(IFBEExportPlugin2 *e, Host *h, Snapshot *s) {
  BSTR n = SysAllocString(L"book");
  HRESULT r = e->Export(h, n, s);
  SysFreeString(n);
  return r;
}
int wmain(int argc, wchar_t **argv) {
  if (argc != 2)
    return 2;
  HRESULT initializeResult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(initializeResult))
    return Die(L"CoInitializeEx", initializeResult);
  HMODULE m = LoadLibraryExW(argv[1], 0,
                             LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                 LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!m)
    return 3;
  GetClassObject g = (GetClassObject)GetProcAddress(m, "DllGetClassObject");
  if (!g)
    return 4;
  CLSID c = {};
  if (FAILED(CLSIDFromString(FBE_TEST_CLSID, &c)))
    return 5;
  IClassFactory *f = 0;
  HRESULT h = g(c, IID_IClassFactory, (void **)&f);
  if (FAILED(h))
    return Die(L"factory", h);
  IUnknown *u = 0;
  h = f->CreateInstance(0, IID_IUnknown, (void **)&u);
  f->Release();
  if (FAILED(h))
    return Die(L"create", h);
  IFBEPluginInfo2 *i = 0;
  IFBEExportPlugin2 *e = 0;
  IFBEExportPlugin *v1 = 0;
  h = u->QueryInterface(IID_IFBEPluginInfo2, (void **)&i);
  if (FAILED(h))
    return Die(L"info", h);
  BSTR id = 0;
  ULONG api = 0;
  i->GetPluginId(&id);
  i->GetApiVersion(&api);
  if (!id || wcscmp(id, FBE_TEST_PLUGIN_ID) || api != 2)
    return 6;
  SysFreeString(id);
  i->Release();
  if (FAILED(u->QueryInterface(IID_IFBEExportPlugin2, (void **)&e)))
    return 7;
  if (FAILED(u->QueryInterface(IID_IFBEExportPlugin, (void **)&v1)))
    return 8;
  wchar_t dir[MAX_PATH] = {};
  GetTempPathW(MAX_PATH, dir);
  std::wstring root = std::wstring(dir) + L"fbe-export-v2-" +
                      std::to_wstring(GetCurrentProcessId());
  CreateDirectoryW(root.c_str(), 0);
  std::wstring good = root + L"\\ok";
#if defined(FBE_TEST_EXPORT_EPUB)
  good += L".epub";
#elif defined(FBE_TEST_EXPORT_DOCX)
  good += L".docx";
#else
  good += L".html";
#endif
  SetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", L"1");
  SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO", FBE_TEST_SCENARIO);
#ifdef FBE_TEST_EXPORT_EPUB
  SetEnvironmentVariableW(L"FBE_NEXT_TEST_EXPORT_EPUB_VERSION", L"3");
#endif
  SetEnvironmentVariableW(L"FBE_NEXT_TEST_EXPORT_HTML_MODE", L"4");
  SetEnvironmentVariableW(FBE_TEST_CANCEL, 0);
  SetEnvironmentVariableW(FBE_TEST_FAIL, 0);
  SetEnvironmentVariableW(FBE_TEST_PATH, good.c_str());
  Host *host = new Host;
  Snapshot *snap = new Snapshot;
  h = Call(e, host, snap);
  if (FAILED(h) || !host->ok())
    return Die(L"success", h);
  HANDLE file = CreateFileW(good.c_str(), GENERIC_READ, FILE_SHARE_READ, 0,
                            OPEN_EXISTING, 0, 0);
  if (file == INVALID_HANDLE_VALUE)
    return 9;
  char out[8193] = {};
  DWORD n = 0;
  ReadFile(file, out, sizeof(out) - 1, &n, 0);
  CloseHandle(file);
  out[n] = 0;
#if defined(FBE_TEST_EXPORT_EPUB) || defined(FBE_TEST_EXPORT_DOCX)
  if (n < 4 || memcmp(out, "PK\x03\x04", 4))
#else
  if (!strstr(out, "Привет, мир") && !strstr(out, "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82\x2C\x20\xD0\xBC\xD0\xB8\xD1\x80"))
#endif
    return 10;
#ifdef FBE_TEST_EXPORT_EPUB
  std::wstring epub2 = root + L"\\ok-epub2.epub";
  SetEnvironmentVariableW(L"FBE_NEXT_TEST_EXPORT_EPUB_VERSION", L"2");
  SetEnvironmentVariableW(FBE_TEST_PATH, epub2.c_str());
  Host *epub2Host = new Host;
  Snapshot *epub2Snapshot = new Snapshot;
  h = Call(e, epub2Host, epub2Snapshot);
  if (FAILED(h) || !epub2Host->ok()) return Die(L"epub2", h);
  HANDLE epub2File = CreateFileW(epub2.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if (epub2File == INVALID_HANDLE_VALUE) return 14;
  DWORD epub2Bytes = 0; ReadFile(epub2File, out, sizeof(out) - 1, &epub2Bytes, 0); CloseHandle(epub2File);
  if (epub2Bytes < 4 || memcmp(out, "PK\x03\x04", 4)) return 15;
  epub2Snapshot->Release(); epub2Host->Release(); SetEnvironmentVariableW(L"FBE_NEXT_TEST_EXPORT_EPUB_VERSION", L"3"); SetEnvironmentVariableW(FBE_TEST_PATH, good.c_str());
#endif
  snap->Release();
  host->Release();
#ifdef FBE_TEST_EXPORT_DOCX
  std::wstring blocked = root + L"\\missing-output-directory\\blocked.docx";
  SetEnvironmentVariableW(FBE_TEST_PATH, blocked.c_str());
  host = new Host;
  snap = new Snapshot;
  h = Call(e, host, snap);
  if (!FAILED(h) || h == S_OK || !host->messages || !host->noDone())
    return 16;
  snap->Release();
  host->Release();
  SetEnvironmentVariableW(FBE_TEST_PATH, good.c_str());
#endif
  SetEnvironmentVariableW(FBE_TEST_FAIL, L"1");
  host = new Host;
  snap = new Snapshot;
  h = Call(e, host, snap);
  if (!FAILED(h) || !host->messages || !host->noDone())
    return 11;
  snap->Release();
  host->Release();
  SetEnvironmentVariableW(FBE_TEST_FAIL, 0);
  SetEnvironmentVariableW(FBE_TEST_CANCEL, L"1");
  host = new Host;
  snap = new Snapshot;
  h = Call(e, host, snap);
  if (h != HRESULT_FROM_WIN32(ERROR_CANCELLED) || host->messages ||
      !host->noDone())
    return 12;
  BSTR name = SysAllocString(L"book");
  IDispatch *dom = 0;
  h = v1->Export(0, name, dom);
  SysFreeString(name);
  if (h != S_FALSE)
    return 13;
  snap->Release();
  host->Release();
  e->Release();
  v1->Release();
  u->Release();
  SetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", 0);
  SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO", 0);
  SetEnvironmentVariableW(FBE_TEST_PATH, 0);
  SetEnvironmentVariableW(FBE_TEST_CANCEL, 0);
  SetEnvironmentVariableW(FBE_TEST_FAIL, 0);
#ifdef FBE_TEST_EXPORT_EPUB
  SetEnvironmentVariableW(L"FBE_NEXT_TEST_EXPORT_EPUB_VERSION", 0);
  std::wcout << L"EPUB3=" << good << L"\nEPUB2=" << (root + L"\\ok-epub2.epub") << L"\n";
#elif defined(FBE_TEST_EXPORT_DOCX)
  std::wcout << L"DOCX=" << good << L"\n";
#else
  DeleteFileW(good.c_str());
  RemoveDirectoryW(root.c_str());
#endif
  FreeLibrary(m);
  std::wcout << L"Export v2 runtime passed\n";
  return 0;
}
