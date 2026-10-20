#pragma once

#include <atlstr.h>
#include <windows.h>
#include <functional>

namespace FbeClipboard {

enum class InternalReferenceKind { Link, Note, Image };

enum class HtmlPasteOutcome { Sanitized, Legacy, PlainText };

// Complex/internal fragments keep the legacy route; malformed external HTML
// uses the clipboard's plain text so its content cannot disappear silently.
class HtmlPasteSanitizer {
public:
  static bool MarkInternalCopy(HWND owner, const GUID& origin);
  static bool GetInternalCopyOrigin(HWND owner, GUID& origin);
  static bool HasInternalCopyMarker(HWND owner);
  static bool IsSafeInternalFragment(const CString& html,
                                    bool sameDocument = true,
                                    const std::function<bool(const CString&, InternalReferenceKind)>& referenceExists = {},
                                    const std::function<bool(const CString&)>& idExists = {});
  static CString NormalizeInternalLinks(const CString& html, const CString& documentUrl);
  static bool ReadClipboardFragment(HWND owner, CString& html);
  // Shared CF_HTML decoder; strict mode is used by the scripting API without
  // changing the established paste fallback policy.
  static bool ReadClipboardFragment(HGLOBAL data, CString& html, bool strict);
  static HtmlPasteOutcome Sanitize(const CString& input,
                                   const CString& nbspReplacement,
                                   CString& output);
  static bool ReadPlainText(HWND owner, CString& text);
  static CString PlainTextToHtml(const CString& text,
                                 const CString& nbspReplacement,
                                 bool insideParagraph = false);
};

} // namespace FbeClipboard
