#include <atlstr.h>
#include <ole2.h>
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "clipboard/HtmlPasteSanitizer.h"
static int owner(const wchar_t* ready, const wchar_t* done) {
  if (FAILED(OleInitialize(nullptr))) return 10;
  HWND w = CreateWindowExW(0, L"STATIC", L"fixture", 0, 0, 0, 0, 0, nullptr, nullptr,
                           GetModuleHandleW(nullptr), nullptr);
  const char* fragment = "<p>external <strong>clipboard</strong></p>";
  const std::string prefix =
      "Version:0.9\r\nStartHTML:00000000\r\nEndHTML:00000000\r\nStartFragment:";
  char header[128] = {};
  size_t start = prefix.size() + 8 + strlen("\r\nEndFragment:") + 8 + strlen("\r\n");
  sprintf_s(header, "%08zu\r\nEndFragment:%08zu\r\n", start, start + strlen(fragment));
  const std::string payload = prefix + header + fragment;
  HGLOBAL block = GlobalAlloc(GMEM_MOVEABLE, payload.size() + 1);
  if (!w || !block) return 11;
  void* data = GlobalLock(block);
  if (!data) return 12;
  memcpy(data, payload.c_str(), payload.size() + 1);
  GlobalUnlock(block);
  if (!OpenClipboard(w)) return 13;
  if (!EmptyClipboard()) return 14;
  UINT html = RegisterClipboardFormatW(L"HTML Format");
  if (!SetClipboardData(html, block)) return 15;
  CloseClipboard();
  HANDLE file = CreateFileW(ready, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return 16;
  CloseHandle(file);
  ULONGLONG until = GetTickCount64() + 60000;
  MSG m = {};
  while (GetFileAttributesW(done) == INVALID_FILE_ATTRIBUTES && GetTickCount64() < until) {
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&m);
      DispatchMessageW(&m);
    }
    Sleep(10);
  }
  DestroyWindow(w);
  OleUninitialize();
  return 0;
}
static int probe() {
  if (FAILED(OleInitialize(nullptr))) return 20;
  HWND w = CreateWindowExW(0, L"STATIC", L"probe", 0, 0, 0, 0, 0, nullptr, nullptr,
                           GetModuleHandleW(nullptr), nullptr);
  if (!w) return 21;
  DWORD pid = 0;
  GetWindowThreadProcessId(GetClipboardOwner(), &pid);
  printf("external_owner=%d\n", pid != 0 && pid != GetCurrentProcessId());
  fflush(stdout);
  if (!pid || pid == GetCurrentProcessId()) return 22;
  UINT fmt = RegisterClipboardFormatW(L"HTML Format");
  CString before;
  bool pre = FbeClipboard::HtmlPasteSanitizer::ReadClipboardFragment(w, before);
  IDataObject* object = nullptr;
  HRESULT hr = OleGetClipboard(&object);
  printf("available=%d pre_read=%d pre_match=%d ole_hr=0x%08lx object=%d\n",
         IsClipboardFormatAvailable(fmt), pre,
         before == L"<p>external <strong>clipboard</strong></p>", static_cast<unsigned long>(hr),
         object != nullptr);
  fflush(stdout);
  if (object) object->Release();
  GUID id = {};
  if (FAILED(CoCreateGuid(&id))) return 23;
  DWORD seq = GetClipboardSequenceNumber();
  bool marked = FbeClipboard::HtmlPasteSanitizer::MarkInternalCopy(w, id);
  DWORD seqAfter = GetClipboardSequenceNumber();
  DWORD afterPid = 0;
  GetWindowThreadProcessId(GetClipboardOwner(), &afterPid);
  printf("marked=%d sequence_changed=%d owner_changed=%d\n", marked, seq != seqAfter,
         pid != afterPid);
  fflush(stdout);
  CString html;
  bool read = FbeClipboard::HtmlPasteSanitizer::ReadClipboardFragment(w, html);
  printf("read=%d content=%d\n", read, html == L"<p>external <strong>clipboard</strong></p>");
  fflush(stdout);
  GUID found = {};
  bool marker = FbeClipboard::HtmlPasteSanitizer::GetInternalCopyOrigin(w, found);
  printf("marker=%d origin=%d\n", marker, IsEqualGUID(id, found));
  fflush(stdout);
  bool ok = !marked && seq == seqAfter && pid == afterPid && read &&
            html == L"<p>external <strong>clipboard</strong></p>" && !marker;
  DestroyWindow(w);
  OleUninitialize();
  return ok ? 0 : 25;
}
int wmain(int argc, wchar_t** argv) {
  if (argc == 4 && wcscmp(argv[1], L"owner") == 0) return owner(argv[2], argv[3]);
  if (argc == 2 && wcscmp(argv[1], L"probe") == 0) return probe();
  return 2;
}
