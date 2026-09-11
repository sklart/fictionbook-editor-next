#pragma once
#include <atlcomcli.h>
#include <atlstr.h>
namespace FbeImage {
enum class ImagePlacement { Block, Inline };
struct ImageInsertionResult {
  HRESULT result;
  _variant_t binaryId;
  IDispatchPtr insertedElement;
  ImageInsertionResult() : result(E_FAIL) {}
};
HRESULT AddImportedBinary(const IDispatchPtr &script, const BYTE *bytes,
                          size_t size, const CString &logicalFileName,
                          const CString &mimeType,
                          ImageInsertionResult *result = NULL);
HRESULT InsertImportedImage(const IDispatchPtr &script, const BYTE *bytes,
                            size_t size, const CString &logicalFileName,
                            const CString &mimeType, ImagePlacement placement,
                            ImageInsertionResult *result = NULL);
} // namespace FbeImage
