#pragma once

#include <atlstr.h>
#include <windows.h>

namespace FbeClipboard {

enum class ClipboardBitmapOutput {
  Png,
  Jpeg
};

// Values are supplied by the editor layer; this platform boundary must not
// reach into the application-wide Settings object.
struct ClipboardPasteOptions {
  CString nbspReplacement;
  ClipboardBitmapOutput bitmapOutput;
  int jpegQuality;

  ClipboardPasteOptions()
      : nbspReplacement(L"\u00a0"), bitmapOutput(ClipboardBitmapOutput::Png),
        jpegQuality(75) {}
};

// The optional file is input for the editor layer.  It contains no document,
// DOM or paste-command state.
struct ClipboardPastePreparationResult {
  CString temporaryImagePath;

  ClipboardPastePreparationResult() = default;
  ~ClipboardPastePreparationResult();
  ClipboardPastePreparationResult(const ClipboardPastePreparationResult&) = delete;
  ClipboardPastePreparationResult& operator=(const ClipboardPastePreparationResult&) = delete;
  ClipboardPastePreparationResult(ClipboardPastePreparationResult&& other) noexcept;
  ClipboardPastePreparationResult& operator=(ClipboardPastePreparationResult&& other) noexcept;

  bool HasPreparedBitmap() const { return !temporaryImagePath.IsEmpty(); }

private:
  void RemovePreparedBitmap();
};

class ClipboardPastePreparer {
public:
  static ClipboardPastePreparationResult Prepare(
      HWND clipboardOwner, const ClipboardPasteOptions& options);

  // Kept public so Unicode/NBSP behaviour has a small non-GUI test seam.
  static CString ReplaceStandardNbsp(const CString& text,
                                     const CString& replacement);
};

} // namespace FbeClipboard
