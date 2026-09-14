#include "stdafx.h"
#include "ClipboardPastePreparer.h"

namespace FbeClipboard {
namespace {

class ClipboardScope {
public:
  explicit ClipboardScope(HWND owner) : m_open(::OpenClipboard(owner) != FALSE) {}
  ~ClipboardScope() { if (m_open) ::CloseClipboard(); }
  bool IsOpen() const { return m_open; }

private:
  bool m_open;
};

class GlobalMemoryLock {
public:
  explicit GlobalMemoryLock(HGLOBAL memory)
      : m_memory(memory), m_data(memory ? ::GlobalLock(memory) : NULL) {}
  ~GlobalMemoryLock() { if (m_data) ::GlobalUnlock(m_memory); }
  void* Data() const { return m_data; }

private:
  HGLOBAL m_memory;
  void* m_data;
};

void PrepareUnicodeText(const ClipboardPasteOptions& options) {
  if (options.nbspReplacement.Compare(L"\u00a0") == 0)
    return;

  HGLOBAL source = static_cast<HGLOBAL>(::GetClipboardData(CF_UNICODETEXT));
  GlobalMemoryLock sourceLock(source);
  const wchar_t* text = static_cast<const wchar_t*>(sourceLock.Data());
  if (!text)
    return;

  const CString replacement = ClipboardPastePreparer::ReplaceStandardNbsp(
      CString(text), options.nbspReplacement);
  HGLOBAL target = ::GlobalAlloc(GMEM_DDESHARE,
                                 (replacement.GetLength() + 1) * sizeof(wchar_t));
  if (!target)
    return;
  bool copied = false;
  {
    GlobalMemoryLock targetLock(target);
    wchar_t* targetText = static_cast<wchar_t*>(targetLock.Data());
    if (targetText) {
      wcscpy_s(targetText, replacement.GetLength() + 1, replacement);
      copied = true;
    }
  }
  if (!copied) {
    ::GlobalFree(target);
    return;
  }
  // Clipboard ownership transfers only on success.  The previous Unicode
  // handle is released by the clipboard when it is replaced.
  if (!::SetClipboardData(CF_UNICODETEXT, target))
    ::GlobalFree(target);
}

CString PrepareBitmapFile(const ClipboardPasteOptions& options) {
  HBITMAP bitmap = static_cast<HBITMAP>(::GetClipboardData(CF_BITMAP));
  if (!bitmap)
    return CString();

  wchar_t temporaryDirectory[MAX_PATH] = {};
  wchar_t temporaryName[MAX_PATH] = {};
  if (!::GetTempPath(_countof(temporaryDirectory), temporaryDirectory) ||
      !::GetTempFileName(temporaryDirectory, L"img", ::GetTickCount(),
                         temporaryName))
    return CString();

  CString imagePath(temporaryName);
  imagePath.Replace(L".tmp", options.bitmapOutput == ClipboardBitmapOutput::Png
                                 ? L".png" : L".jpg");
  // GetTempFileName creates the .tmp file.  It is not the file later passed to
  // the editor and must not be leaked beside the prepared image.
  ::DeleteFile(temporaryName);

  CImage image;
  image.Attach(bitmap);
  HRESULT saved = E_FAIL;
  if (options.bitmapOutput == ClipboardBitmapOutput::Png) {
    saved = image.Save(imagePath, Gdiplus::ImageFormatPNG);
  } else {
    Gdiplus::EncoderParameters parameters = {};
    parameters.Count = 1;
    parameters.Parameter[0].Guid = Gdiplus::EncoderQuality;
    parameters.Parameter[0].NumberOfValues = 1;
    parameters.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    ULONG quality = options.jpegQuality;
    parameters.Parameter[0].Value = &quality;
    saved = image.Save(imagePath, Gdiplus::ImageFormatJPEG, &parameters);
  }
  // CF_BITMAP remains owned by the clipboard, not by this temporary CImage.
  image.Detach();
  if (FAILED(saved)) {
    ::DeleteFile(imagePath);
    return CString();
  }
  return imagePath;
}

} // namespace

void ClipboardPastePreparationResult::RemovePreparedBitmap() const {
  if (!temporaryImagePath.IsEmpty())
    ::DeleteFile(temporaryImagePath);
}

CString ClipboardPastePreparer::ReplaceStandardNbsp(const CString& text,
                                                     const CString& replacement) {
  CString result(text);
  if (replacement.Compare(L"\u00a0") != 0)
    result.Replace(L"\u00a0", replacement);
  return result;
}

ClipboardPastePreparationResult ClipboardPastePreparer::Prepare(
    HWND clipboardOwner, const ClipboardPasteOptions& options) {
  ClipboardPastePreparationResult result;
  ClipboardScope clipboard(clipboardOwner);
  if (!clipboard.IsOpen())
    return result;

  // Preserve the established priority: text is prepared before bitmap.
  if (::IsClipboardFormatAvailable(CF_TEXT) ||
      ::IsClipboardFormatAvailable(CF_UNICODETEXT)) {
    PrepareUnicodeText(options);
  } else if (::IsClipboardFormatAvailable(CF_BITMAP)) {
    result.temporaryImagePath = PrepareBitmapFile(options);
  }
  return result;
}

} // namespace FbeClipboard
