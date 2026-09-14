#include "../../src/fbe/stdafx.h"
#include "../../src/fbe/clipboard/ClipboardPastePreparer.h"

#include <cstdlib>
#include <iostream>
#include <utility>

ATL::CImage::CInitGDIPlus ATL::CImage::s_initGDIPlus;

static void Require(bool value, const char* message) {
  if (!value) {
    std::cerr << message << std::endl;
    std::exit(1);
  }
}

static void EmptyTestClipboard() {
  Require(::OpenClipboard(NULL) != FALSE, "OpenClipboard");
  Require(::EmptyClipboard() != FALSE, "EmptyClipboard");
  ::CloseClipboard();
}

static void SetUnicodeText(const wchar_t* text) {
  const size_t length = wcslen(text) + 1;
  HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, length * sizeof(wchar_t));
  Require(memory != NULL, "GlobalAlloc text");
  wchar_t* target = static_cast<wchar_t*>(::GlobalLock(memory));
  Require(target != NULL, "GlobalLock text");
  wcscpy_s(target, length, text);
  ::GlobalUnlock(memory);
  Require(::OpenClipboard(NULL) != FALSE, "OpenClipboard text");
  Require(::EmptyClipboard() != FALSE, "EmptyClipboard text");
  if (!::SetClipboardData(CF_UNICODETEXT, memory)) {
    ::CloseClipboard();
    ::GlobalFree(memory);
    Require(false, "SetClipboardData text");
  }
  ::CloseClipboard();
}

static CString ReadUnicodeText() {
  Require(::OpenClipboard(NULL) != FALSE, "OpenClipboard read");
  HGLOBAL memory = static_cast<HGLOBAL>(::GetClipboardData(CF_UNICODETEXT));
  const wchar_t* text = memory ? static_cast<const wchar_t*>(::GlobalLock(memory)) : NULL;
  CString result(text ? text : L"");
  if (text) ::GlobalUnlock(memory);
  ::CloseClipboard();
  return result;
}

static void TestTextPreparation() {
  FbeClipboard::ClipboardPasteOptions options;
  options.nbspReplacement = L"\u00a0";
  SetUnicodeText(L"\u0422\u0435\u0441\u0442 \u03b1\u03b2 \u4e2d\u6587");
  FbeClipboard::ClipboardPastePreparer::Prepare(NULL, options);
  Require(ReadUnicodeText() == L"\u0422\u0435\u0441\u0442 \u03b1\u03b2 \u4e2d\u6587", "Unicode text changed");

  SetUnicodeText(L"alpha\u00a0beta");
  FbeClipboard::ClipboardPastePreparer::Prepare(NULL, options);
  Require(ReadUnicodeText() == L"alpha\u00a0beta", "Default NBSP changed");

  options.nbspReplacement = L"_";
  SetUnicodeText(L"alpha\u00a0beta");
  FbeClipboard::ClipboardPastePreparer::Prepare(NULL, options);
  Require(ReadUnicodeText() == L"alpha_beta", "NBSP replacement changed");
  EmptyTestClipboard();
}

static void TestBitmapPreparation(FbeClipboard::ClipboardBitmapOutput output,
                                  const wchar_t* extension) {
  const unsigned char pixels[] = {0, 0, 255, 0};
  HBITMAP bitmap = ::CreateBitmap(1, 1, 1, 32, pixels);
  Require(bitmap != NULL, "CreateBitmap");
  Require(::OpenClipboard(NULL) != FALSE, "OpenClipboard bitmap");
  Require(::EmptyClipboard() != FALSE, "EmptyClipboard bitmap");
  if (!::SetClipboardData(CF_BITMAP, bitmap)) {
    ::CloseClipboard();
    ::DeleteObject(bitmap);
    Require(false, "SetClipboardData bitmap");
  }
  ::CloseClipboard();

  FbeClipboard::ClipboardPasteOptions options;
  options.bitmapOutput = output;
  options.jpegQuality = 81;
  CString preparedPath;
  {
    FbeClipboard::ClipboardPastePreparationResult result =
        FbeClipboard::ClipboardPastePreparer::Prepare(NULL, options);
    Require(result.HasPreparedBitmap(), "Bitmap was not prepared");
    preparedPath = result.temporaryImagePath;
    Require(::GetFileAttributes(preparedPath) != INVALID_FILE_ATTRIBUTES,
            "Prepared bitmap file missing");
    Require(preparedPath.Right(4).CompareNoCase(extension) == 0,
            "Prepared bitmap extension");
  }
  Require(::GetFileAttributes(preparedPath) == INVALID_FILE_ATTRIBUTES,
          "Prepared bitmap file was not removed by result ownership");
  EmptyTestClipboard();
}

static void TestBitmapMoveOwnership() {
  const unsigned char pixels[] = {0, 0, 255, 0};
  HBITMAP bitmap = ::CreateBitmap(1, 1, 1, 32, pixels);
  Require(bitmap != NULL, "CreateBitmap move");
  Require(::OpenClipboard(NULL) != FALSE, "OpenClipboard move");
  Require(::EmptyClipboard() != FALSE, "EmptyClipboard move");
  if (!::SetClipboardData(CF_BITMAP, bitmap)) {
    ::CloseClipboard();
    ::DeleteObject(bitmap);
    Require(false, "SetClipboardData move");
  }
  ::CloseClipboard();

  CString preparedPath;
  {
    FbeClipboard::ClipboardPastePreparationResult first =
        FbeClipboard::ClipboardPastePreparer::Prepare(NULL, FbeClipboard::ClipboardPasteOptions());
    Require(first.HasPreparedBitmap(), "Move source bitmap was not prepared");
    preparedPath = first.temporaryImagePath;
    {
      FbeClipboard::ClipboardPastePreparationResult second(std::move(first));
      Require(!first.HasPreparedBitmap(), "Move source retained bitmap ownership");
      Require(second.HasPreparedBitmap(), "Move destination lost bitmap ownership");
      Require(::GetFileAttributes(preparedPath) != INVALID_FILE_ATTRIBUTES,
              "Move destination lost prepared bitmap file");
    }
    Require(::GetFileAttributes(preparedPath) == INVALID_FILE_ATTRIBUTES,
            "Move destination did not remove prepared bitmap file");
  }
  EmptyTestClipboard();
}

int main() {
  TestTextPreparation();
  TestBitmapPreparation(FbeClipboard::ClipboardBitmapOutput::Png, L".png");
  TestBitmapPreparation(FbeClipboard::ClipboardBitmapOutput::Jpeg, L".jpg");
  TestBitmapMoveOwnership();
  return 0;
}
