#include "../../src/fbe/clipboard/HtmlPasteSanitizer.h"
#include <cstdio>
#include <cstdlib>
#include <string>

static void Check(bool ok, const char* message) {
  if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

static CString Clean(const wchar_t* source) {
  CString output;
  Check(FbeClipboard::HtmlPasteSanitizer::Sanitize(source, L"\u00a0", output) == FbeClipboard::HtmlPasteOutcome::Sanitized, "sanitize failed");
  return output;
}

static void TestFormatting() {
  const CString html = Clean(L"<p class='MsoNormal' style='margin:1in'><span style='font-weight:700;font-style:italic;color:red'>Bold italic</span> <s>gone</s> <span style='vertical-align:super'>2</span></p>");
  Check(html == L"<p><strong><em>Bold italic</em></strong> <strike>gone</strike> <sup>2</sup></p>", "formatting");
  Check(Clean(L"<p><b><b>x</b></b></p>") == L"<p><strong>x</strong></p>", "duplicate strong");
  Check(Clean(L"<div>One</div><div><br></div><p>Three</p>") == L"<p>One</p><p><br></p><p>Three</p>", "paragraphs");
  Check(Clean(L"<div><section><p>First</p><p>Second</p></section></div>") == L"<p>First</p><p>Second</p>", "nested paragraphs");
  Check(Clean(L"<div><section><p>First</p><p></p><p>Third</p></section></div>") == L"<p>First</p><p></p><p>Third</p>", "nested empty paragraph");
  Check(Clean(L"<div style='font-weight:bold'><p>Bold <span style='font-weight:normal'>plain</span> bold</p></div>") ==
    L"<p><strong>Bold </strong>plain<strong> bold</strong></p>", "inherited bold and reset");
  Check(Clean(L"<section style='font-style:italic'><div><p>Italic</p></div></section>") ==
    L"<p><em>Italic</em></p>", "deeply inherited italic");
  Check(Clean(L"<div style='vertical-align:super'><p>up <span style='vertical-align:baseline'>base</span></p></div>") ==
    L"<p><sup>up </sup>base</p>", "inherited vertical alignment reset");
  Check(Clean(L"<p>\u041f\u0440\u0438\u0432\u0435\u0442&nbsp;\u2014 &ldquo;\u0442\u0435\u0441\u0442&rdquo;</p>") == L"<p>\u041f\u0440\u0438\u0432\u0435\u0442\u00a0\u2014 \u201c\u0442\u0435\u0441\u0442\u201d</p>", "unicode");
  Check(Clean(L"<p>&laquo;Price&raquo; &copy; &euro; &#x1f642; &unknown;</p>") ==
    L"<p>\u00abPrice\u00bb \u00a9 \u20ac \U0001f642 &amp;unknown;</p>", "named, numeric and unknown entities");
  Check(Clean(L"<a href='javascript:alert(1)' onclick='x()'>Link</a>") == L"Link", "unsafe link");
  Check(Clean(L"<a href='https://example.org/?a=1&amp;b=2'>Link</a>") == L"<a href=\"https://example.org/?a=1&amp;b=2\">Link</a>", "safe link");
  Check(Clean(L"<p>Keep<script>alert(1)</script>end</p>") == L"<p>Keepend</p>", "script");
  CString output;
  Check(FbeClipboard::HtmlPasteSanitizer::Sanitize(L"<table><tr><td>Keep</td></tr></table>", L"\u00a0", output) == FbeClipboard::HtmlPasteOutcome::Legacy, "complex fallback");
  Check(Clean(L"<div class='section'><p>untrusted</p></div>") == L"<p>untrusted</p>", "html attributes are not provenance");
  Check(Clean(L"<p>bad &unknown;</p>") == L"<p>bad &amp;unknown;</p>", "unknown entity preserved");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<p><span class=code>Code</span><a href='#note'>note</a></p>"), "safe internal structure");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<div class=section><div class=title><p>Title</p></div><p>Body</p></div>"), "safe internal section and title");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<div class=annotation><p>Note</p></div><p class=subtitle>Subtitle</p>"), "safe internal annotation and subtitle");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<table><tr><td>Cell</td></tr></table><ul><li>Item</li></ul>"), "safe internal table and list");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<div class=image href='#img'><img src='fbw-internal:#img'></div>"), "safe internal image");
  Check(!FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<div class=section><script>alert(1)</script></div>"), "internal script rejected");
  Check(!FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<img src='https://example.org/track.png'>"), "external image rejected");
  Check(!FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<p onclick='alert(1)'>x</p>"), "internal event rejected");
  Check(FbeClipboard::HtmlPasteSanitizer::PlainTextToHtml(L"one\r\n\r\nthree\u00a0", L"_") == L"<p>one</p><p></p><p>three_</p>", "plain paragraphs");
  Check(FbeClipboard::HtmlPasteSanitizer::PlainTextToHtml(L"one & two \u00a0", L"_", true) == L"one &amp; two _", "inline plain text");
}

static void TestClipboard() {
  HWND owner = ::CreateWindowExW(0, L"STATIC", L"paste test", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, NULL, ::GetModuleHandleW(NULL), NULL);
  Check(owner != NULL, "clipboard owner window");
  const char* fragment = "<p>\xd0\x9f<strong>ok</strong></p>";
  const std::string prefix = "Version:0.9\r\nStartHTML:00000000\r\nEndHTML:00000000\r\nStartFragment:";
  char header[128] = {};
  const size_t start = prefix.size() + 8 + strlen("\r\nEndFragment:") + 8 + strlen("\r\n");
  std::sprintf(header, "%08zu\r\nEndFragment:%08zu\r\n", start, start + strlen(fragment));
  const std::string payload = prefix + header + fragment;
  HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, payload.size() + 1);
  Check(memory != NULL, "GlobalAlloc");
  char* bytes = static_cast<char*>(::GlobalLock(memory));
  memcpy(bytes, payload.c_str(), payload.size() + 1);
  ::GlobalUnlock(memory);
  Check(::OpenClipboard(owner) != FALSE, "OpenClipboard");
  Check(::EmptyClipboard() != FALSE, "EmptyClipboard");
  const UINT format = ::RegisterClipboardFormat(L"HTML Format");
  Check(::SetClipboardData(format, memory) != NULL, "SetClipboardData");
  ::CloseClipboard();
  const DWORD sequence = ::GetClipboardSequenceNumber();
  CString extracted;
  Check(FbeClipboard::HtmlPasteSanitizer::ReadClipboardFragment(owner, extracted), "read CF_HTML");
  Check(extracted == L"<p>\u041f<strong>ok</strong></p>", "UTF-8 byte offsets");
  Check(::GetClipboardSequenceNumber() == sequence, "clipboard changed");
  Check(!FbeClipboard::HtmlPasteSanitizer::HasInternalCopyMarker(owner), "external html has no internal marker");
  Check(FbeClipboard::HtmlPasteSanitizer::MarkInternalCopy(owner), "mark internal copy");
  Check(FbeClipboard::HtmlPasteSanitizer::HasInternalCopyMarker(owner), "read internal marker");
  wchar_t executable[MAX_PATH] = {};
  Check(::GetModuleFileNameW(NULL, executable, _countof(executable)) != 0, "test executable path");
  std::wstring command = L"\"" + std::wstring(executable) + L"\" --check-marker";
  STARTUPINFOW startup = {}; startup.cb = sizeof(startup);
  PROCESS_INFORMATION process = {};
  Check(::CreateProcessW(executable, &command[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process) != FALSE, "start second process");
  Check(::WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0, "wait for second process");
  DWORD childExit = 1;
  Check(::GetExitCodeProcess(process.hProcess, &childExit) != FALSE && childExit == 0, "marker readable across processes");
  ::CloseHandle(process.hThread); ::CloseHandle(process.hProcess);
  Check(::OpenClipboard(owner) != FALSE, "OpenClipboard cleanup");
  ::EmptyClipboard(); ::CloseClipboard();
  ::DestroyWindow(owner);
}

int wmain(int argc, wchar_t** argv) {
  if (argc == 2 && wcscmp(argv[1], L"--check-marker") == 0)
    return FbeClipboard::HtmlPasteSanitizer::HasInternalCopyMarker(NULL) ? 0 : 1;
  TestFormatting(); TestClipboard(); return 0;
}
