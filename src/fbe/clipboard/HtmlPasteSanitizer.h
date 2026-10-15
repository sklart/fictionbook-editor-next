#pragma once

#include <atlstr.h>
#include <windows.h>

namespace FbeClipboard {

enum class HtmlPasteOutcome { Sanitized, Legacy, PlainText };

// Complex/internal fragments keep the legacy route; malformed external HTML
// uses the clipboard's plain text so its content cannot disappear silently.
class HtmlPasteSanitizer {
public:
  static bool ReadClipboardFragment(HWND owner, CString& html);
  static HtmlPasteOutcome Sanitize(const CString& input,
                                   const CString& nbspReplacement,
                                   CString& output);
  static bool ReadPlainText(HWND owner, CString& text);
  static CString PlainTextToHtml(const CString& text,
                                 const CString& nbspReplacement);
};

} // namespace FbeClipboard
