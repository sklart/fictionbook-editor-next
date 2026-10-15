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
  CString output;
  Check(FbeClipboard::HtmlPasteSanitizer::Sanitize(L"<a href='javascript:alert(1)' onclick='x()'>Link</a>", L"\u00a0", output) ==
    FbeClipboard::HtmlPasteOutcome::Legacy, "unsafe link must not be silently discarded");
  Check(Clean(L"<a href='https://example.org/?a=1&amp;b=2'>Link</a>") == L"<a href=\"https://example.org/?a=1&amp;b=2\">Link</a>", "safe link");
  Check(Clean(L"<p>Keep<script>alert(1)</script>end</p>") == L"<p>Keepend</p>", "script");
  Check(FbeClipboard::HtmlPasteSanitizer::Sanitize(L"<table><tr><td>Keep</td></tr></table>", L"\u00a0", output) == FbeClipboard::HtmlPasteOutcome::Legacy, "complex fallback");
  Check(Clean(L"<p>bad &unknown;</p>") == L"<p>bad &amp;unknown;</p>", "unknown entity preserved");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<p><span class=code>Code</span><a href='#note'>note</a></p>"), "safe internal structure");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<div class=section><div class=title><p>Title</p></div><p>Body</p></div>"), "safe internal section and title");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<div class=annotation><p>Note</p></div><p class=subtitle>Subtitle</p>"), "safe internal annotation and subtitle");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<table><tr><td>Cell</td></tr></table><ul><li>Item</li></ul>"), "safe internal table and list");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<table class='table'><tbody><tr class='tr'><td class='td'>Cell</td></tr></tbody></table>"), "actual FBE table tbody");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<div class=image href='#img'><img src='fbw-internal:#img'></div>"), "safe internal image");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<span onresizestart='return false' contentEditable='false' class='image' href='#img'><img src='fbw-internal:#img'></span>"), "actual FBE inline image");
  const auto referencesInBook = [](const CString& id, FbeClipboard::InternalReferenceKind kind) {
    return kind == FbeClipboard::InternalReferenceKind::Image ? id == L"present-image" : id == L"present-note";
  };
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(
    L"<p><a class='note' href='#present-note'>note</a></p>", true, referencesInBook), "existing note accepted");
  Check(!FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(
    L"<p><a class='note' href='#missing-note'>note</a></p>", true, referencesInBook), "missing note rejected");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(
    L"<p id='local'><a href='#local'>self link</a></p>", false, referencesInBook), "fragment-local target accepted");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(
    L"<div class='image' href='#present-image'><img src='fbw-internal:#present-image'></div>", true, referencesInBook), "existing block image accepted");
  Check(!FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(
    L"<span class='image' href='#missing-image'><img src='fbw-internal:#missing-image'></span>", true, referencesInBook), "missing inline image rejected");
  Check(!FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(
    L"<p><a href='#present-note'>link</a></p>", false, referencesInBook), "cross-book matching link ID rejected");
  Check(!FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(
    L"<div class='image' href='#present-image'><img src='fbw-internal:#present-image'></div>", false, referencesInBook),
    "cross-book image ID rejected even when destination has a binary");
  Check(!FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(
    L"<p id='present-note'><a href='#present-note'>link</a></p>", false, referencesInBook,
    [](const CString& id) { return id == L"present-note"; }), "fragment-local ID collision rejected");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(
    L"<p><strong>Text</strong> <em>format</em></p>", false, referencesInBook), "cross-book independent formatting accepted");
  Check(!FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<span contentEditable='true'>editable</span>"), "unexpected editable fragment rejected");
  Check(FbeClipboard::HtmlPasteSanitizer::Sanitize(L"<div class='section'><p>Legacy</p></div>", L"\u00a0", output) ==
    FbeClipboard::HtmlPasteOutcome::Legacy, "unmarked FBE structure must not lose structure");
  Check(FbeClipboard::HtmlPasteSanitizer::Sanitize(L"<a class='note' href='file:///D:/FBE/main.html#note'>note</a>", L"\u00a0", output) ==
    FbeClipboard::HtmlPasteOutcome::Legacy, "unmarked FBE link must not lose its target");
  const CString base(L"file:///D:/FBE/main.html");
  Check(FbeClipboard::HtmlPasteSanitizer::NormalizeInternalLinks(L"<P><A href=\"file:///D:/FBE/main.html#note\">note</A></P>", base) ==
    L"<P><A href=\"#note\">note</A></P>", "MSHTML absolute internal link");
  Check(FbeClipboard::HtmlPasteSanitizer::NormalizeInternalLinks(
    L"<p>href=file:///C:/FBE/main.html#note1</p>", L"file:///C:/FBE/main.html") ==
    L"<p>href=file:///C:/FBE/main.html#note1</p>", "literal href text preserved");
  Check(FbeClipboard::HtmlPasteSanitizer::NormalizeInternalLinks(
    L"<p>href=file:///D:/FBE/main.html#note1</p><!-- href=file:///D:/FBE/main.html#note2 -->"
    L"<a data-url='href=file:///D:/FBE/main.html#note3' href='file:///D:/Other/main.html#note4'>Other</a>", base) ==
    L"<p>href=file:///D:/FBE/main.html#note1</p><!-- href=file:///D:/FBE/main.html#note2 -->"
    L"<a data-url='href=file:///D:/FBE/main.html#note3' href='file:///D:/Other/main.html#note4'>Other</a>",
    "normalization changes only matching anchor href attributes");
  Check(FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<P id=paste-copy-source><SPAN class=code>Internal code</SPAN> <A \r\nhref=\"#paste-neighbor\">internal \r\nlink</A></P>"), "MSHTML copied FBE paragraph");
  Check(!FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<div class=section><script>alert(1)</script></div>"), "internal script rejected");
  Check(!FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<img src='https://example.org/track.png'>"), "external image rejected");
  Check(!FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(L"<p onclick='alert(1)'>x</p>"), "internal event rejected");
  Check(FbeClipboard::HtmlPasteSanitizer::Sanitize(L"<p>Formula <math><mi>x</mi><mo>+</mo><mn>1</mn></math></p>", L"\u00a0", output) ==
    FbeClipboard::HtmlPasteOutcome::Legacy, "complex formula cannot be flattened silently");
  Check(FbeClipboard::HtmlPasteSanitizer::PlainTextToHtml(L"one\r\n\r\nthree\u00a0", L"_") == L"<p>one</p><p></p><p>three_</p>", "plain paragraphs");
  Check(FbeClipboard::HtmlPasteSanitizer::PlainTextToHtml(L"one & two \u00a0", L"_", true) == L"one &amp; two _", "inline plain text");
}

static void TestClipboard() {
  HWND owner = ::CreateWindowExW(0, L"STATIC", L"paste test", 0, 0, 0, 0, 0,
                                NULL, NULL, ::GetModuleHandleW(NULL), NULL);
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
  GUID sourceIdentity = GUID_NULL;
  Check(SUCCEEDED(::CoCreateGuid(&sourceIdentity)), "source identity");
  Check(FbeClipboard::HtmlPasteSanitizer::MarkInternalCopy(owner, sourceIdentity), "mark internal copy");
  Check(FbeClipboard::HtmlPasteSanitizer::HasInternalCopyMarker(owner), "read internal marker");
  GUID copiedIdentity = GUID_NULL;
  Check(FbeClipboard::HtmlPasteSanitizer::GetInternalCopyOrigin(owner, copiedIdentity) &&
    ::IsEqualGUID(sourceIdentity, copiedIdentity), "origin survives clipboard round-trip");
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
  Check(SUCCEEDED(::OleInitialize(nullptr)), "OleInitialize");
  if (argc == 2 && wcscmp(argv[1], L"--check-marker") == 0) {
    const bool marked = FbeClipboard::HtmlPasteSanitizer::HasInternalCopyMarker(NULL);
    ::OleUninitialize();
    return marked ? 0 : 1;
  }
  TestFormatting(); TestClipboard();
  ::OleUninitialize();
  return 0;
}
