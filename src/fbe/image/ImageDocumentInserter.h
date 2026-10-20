#pragma once
#include <atlcomcli.h>
#include <atlstr.h>
#include <functional>
namespace FbeImage {
enum class ImagePlacement { Block, Inline };
struct ImageInsertionResult {
  HRESULT result;
  _variant_t binaryId;
  IDispatchPtr insertedElement;
  MSHTML::IHTMLDOMNodePtr createdBinary;
  ImageInsertionResult() : result(E_FAIL) {}
};
HRESULT AddImportedBinary(const IDispatchPtr &script, const BYTE *bytes,
                          size_t size, const CString &logicalFileName,
                          const CString &mimeType,
                          ImageInsertionResult *result = NULL);
HRESULT CommitBinaryEdit(const IDispatchPtr& script, IOleUndoManager* manager,
                         const std::function<void(bool)>& applyState);
HRESULT InsertImportedImage(const IDispatchPtr &script, const BYTE *bytes,
                            size_t size, const CString &logicalFileName,
                            const CString &mimeType, ImagePlacement placement,
                            ImageInsertionResult *result = NULL,
                            IOleUndoManager *undoManager = NULL);
} // namespace FbeImage
