#include "stdafx.h"
#include "ImageDocumentInserter.h"
#include "../utils/utils.h"
namespace FbeImage {
static HRESULT Finish(ImageInsertionResult *out, HRESULT result,
                      const _variant_t *binaryId = NULL) {
  if (out) {
    out->result = result;
    if (binaryId)
      out->binaryId = *binaryId;
  }
  return result;
}

static BSTR MakeId(const CString &name) {
  CString input = U::Transliterate(name), id;
  int start = input.ReverseFind(_T('\\'));
  start = start < 0 ? 0 : start + 1;
  for (int i = start; i < input.GetLength(); ++i) {
    TCHAR c = input[i];
    if ((c >= _T('0') && c <= _T('9')) || (c >= _T('A') && c <= _T('Z')) ||
        (c >= _T('a') && c <= _T('z') || c == _T('_') || c == _T('-') ||
         c == _T('.')))
      id.AppendChar(c);
  }
  if (!id.IsEmpty() &&
      !((id[0] >= _T('A') && id[0] <= _T('Z')) ||
        (id[0] >= _T('a') && id[0] <= _T('z')) || id[0] == _T('_')))
    id.Insert(0, _T('_'));
  return id.AllocSysString();
}
HRESULT AddImportedBinary(const IDispatchPtr &script, const BYTE *bytes,
                          size_t size, const CString &name, const CString &mime,
                          ImageInsertionResult *out) {
  if (out)
    *out = ImageInsertionResult();
  if (!script || !bytes || !size || size > ULONG_MAX)
    return Finish(out, E_INVALIDARG);
  _variant_t args[4];
  SAFEARRAY *data = SafeArrayCreateVector(VT_UI1, 0, (ULONG)size);
  if (!data)
    return Finish(out, E_OUTOFMEMORY);
  void *raw = NULL;
  HRESULT hr = SafeArrayAccessData(data, &raw);
  if (FAILED(hr)) {
    SafeArrayDestroy(data);
    return Finish(out, hr);
  }
  memcpy(raw, bytes, size);
  SafeArrayUnaccessData(data);
  V_ARRAY(&args[0]) = data;
  V_VT(&args[0]) = VT_ARRAY | VT_UI1;
  V_BSTR(&args[1]) = mime.AllocSysString();
  V_VT(&args[1]) = VT_BSTR;
  V_BSTR(&args[2]) = MakeId(name);
  V_VT(&args[2]) = VT_BSTR;
  V_BSTR(&args[3]) = ::SysAllocString(L"");
  V_VT(&args[3]) = VT_BSTR;
  CComDispatchDriver body(script);
  _variant_t id;
  hr = body.InvokeN(L"apiAddBinary", args, 4, &id);
  if (SUCCEEDED(hr))
    hr = body.Invoke0(L"FillCoverList");
  return Finish(out, hr, &id);
}
HRESULT InsertImportedImage(const IDispatchPtr &script, const BYTE *bytes,
                            size_t size, const CString &name,
                            const CString &mime, ImagePlacement placement,
                            ImageInsertionResult *out) {
  ImageInsertionResult result;
  HRESULT hr = AddImportedBinary(script, bytes, size, name, mime, &result);
  if (SUCCEEDED(hr)) {
    CComDispatchDriver body(script);
    _variant_t check(false);
    _variant_t inserted;
    hr = body.Invoke2(placement == ImagePlacement::Inline ? L"InsInlineImage"
                                                          : L"InsImage",
                      &check, &result.binaryId, &inserted);
    if (SUCCEEDED(hr) && V_VT(&inserted) == VT_DISPATCH)
      result.insertedElement = V_DISPATCH(&inserted);
    result.result = hr;
  }
  return Finish(out, hr, &result.binaryId);
}
} // namespace FbeImage
