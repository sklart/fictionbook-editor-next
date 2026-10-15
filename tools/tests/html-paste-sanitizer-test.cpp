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
  Check(Clean(L"<p>\u041f\u0440\u0438\u0432\u0435\u0442&nbsp;\u2014 &ldquo;\u0442\u0435\u0441\u0442&rdquo;</p>") == L"<p>\u041f\u0440\u0438\u0432\u0435\u0442\u00a0\u2014 \u201c\u0442\u0435\u0441\u0442\u201d</p>", "unicode");
  Check(Clean(L"<a href='javascript:alert(1)' onclick='x()'>Link</a>") == L"Link", "unsafe link");
  Check(Clean(L"<a href='https://example.org/?a=1&amp;b=2'>Link</a>") == L"<a href=\"https://example.org/?a=1&amp;b=2\">Link</a>", "safe link");
  Check(Clean(L"<p>Keep<script>alert(1)</script>end</p>") == L"<p>Keepend</p>", "script");
  CString output;
  Check(FbeClipboard::HtmlPasteSanitizer::Sanitize(L"<table><tr><td>Keep</td></tr></table>", L"\u00a0", output) == FbeClipboard::HtmlPasteOutcome::Legacy, "complex fallback");
  Check(FbeClipboard::HtmlPasteSanitizer::Sanitize(L"<div class='section'><p>internal</p></div>", L"\u00a0", output) == FbeClipboard::HtmlPasteOutcome::Legacy, "internal fallback");
  Check(FbeClipboard::HtmlPasteSanitizer::Sanitize(L"<p>bad &unknown;</p>", L"\u00a0", output) == FbeClipboard::HtmlPasteOutcome::PlainText, "malformed fallback");
  Check(FbeClipboard::HtmlPasteSanitizer::PlainTextToHtml(L"one\r\n\r\nthree\u00a0", L"_") == L"<p>one</p><p></p><p>three_</p>", "plain paragraphs");
}

static void TestClipboard() {
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
  Check(::OpenClipboard(NULL) != FALSE, "OpenClipboard");
  Check(::EmptyClipboard() != FALSE, "EmptyClipboard");
  const UINT format = ::RegisterClipboardFormat(L"HTML Format");
  Check(::SetClipboardData(format, memory) != NULL, "SetClipboardData");
  ::CloseClipboard();
  const DWORD sequence = ::GetClipboardSequenceNumber();
  CString extracted;
  Check(FbeClipboard::HtmlPasteSanitizer::ReadClipboardFragment(NULL, extracted), "read CF_HTML");
  Check(extracted == L"<p>\u041f<strong>ok</strong></p>", "UTF-8 byte offsets");
  Check(::GetClipboardSequenceNumber() == sequence, "clipboard changed");
  Check(::OpenClipboard(NULL) != FALSE, "OpenClipboard cleanup");
  ::EmptyClipboard(); ::CloseClipboard();
}

int main() { TestFormatting(); TestClipboard(); return 0; }
