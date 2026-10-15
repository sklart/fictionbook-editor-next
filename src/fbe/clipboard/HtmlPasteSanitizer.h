#pragma once

#include <atlstr.h>
#include <windows.h>
#include <functional>

namespace FbeClipboard {

enum class HtmlPasteOutcome { Sanitized, Legacy, PlainText };

// Complex/internal fragments keep the legacy route; malformed external HTML
// uses the clipboard's plain text so its content cannot disappear silently.
class HtmlPasteSanitizer {
public:
  static bool MarkInternalCopy(HWND owner);
  static bool HasInternalCopyMarker(HWND owner);
  static bool IsSafeInternalFragment(const CString& html,
                                    const std::function<bool(const CString&, bool image)>& referenceExists = {});
  static CString NormalizeInternalLinks(const CString& html, const CString& documentUrl);
  static bool ReadClipboardFragment(HWND owner, CString& html);
  static HtmlPasteOutcome Sanitize(const CString& input,
                                   const CString& nbspReplacement,
                                   CString& output);
  static bool ReadPlainText(HWND owner, CString& text);
  static CString PlainTextToHtml(const CString& text,
                                 const CString& nbspReplacement,
                                 bool insideParagraph = false);
};

} // namespace FbeClipboard
