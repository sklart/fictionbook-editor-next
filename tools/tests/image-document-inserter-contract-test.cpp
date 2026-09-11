#include "../../src/fbe/stdafx.h"
#include "../../src/fbe/utils/utils.h"

#include <cstdlib>
#include <iostream>

namespace U {
CString Transliterate(CString src) { return src; }
void NormalizeInplace(CString &) {}
} // namespace U

#include "../../src/fbe/image/ImageDocumentInserter.cpp"

static void Require(bool value, const char *message) {
  if (!value) {
    std::cerr << message << std::endl;
    std::exit(1);
  }
}

class TestDispatch final : public IDispatch {
public:
  TestDispatch() : refs_(1) {}
  STDMETHOD(QueryInterface)(REFIID iid, void **out) override {
    if (!out)
      return E_POINTER;
    *out = NULL;
    if (iid == IID_IUnknown || iid == IID_IDispatch) {
      *out = static_cast<IDispatch *>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  STDMETHOD_(ULONG, AddRef)() override { return ++refs_; }
  STDMETHOD_(ULONG, Release)() override {
    const ULONG refs = --refs_;
    if (!refs)
      delete this;
    return refs;
  }
  STDMETHOD(GetTypeInfoCount)(UINT *) override { return E_NOTIMPL; }
  STDMETHOD(GetTypeInfo)(UINT, LCID, ITypeInfo **) override { return E_NOTIMPL; }
  STDMETHOD(GetIDsOfNames)(REFIID, LPOLESTR *, UINT, LCID, DISPID *) override { return E_NOTIMPL; }
  STDMETHOD(Invoke)(DISPID, REFIID, LCID, WORD, DISPPARAMS *, VARIANT *, EXCEPINFO *, UINT *) override { return E_NOTIMPL; }
private:
  ULONG refs_;
};

static void CheckInvalid(const IDispatchPtr &script, const BYTE *bytes,
                         size_t size, const char *name) {
  FbeImage::ImageInsertionResult result;
  const HRESULT hr = FbeImage::AddImportedBinary(script, bytes, size, L"x",
                                                 L"image/jpeg", &result);
  Require(hr == E_INVALIDARG, name);
  Require(result.result == E_INVALIDARG, "result HRESULT");
  Require(!result.insertedElement, "result inserted element");
}

int main() {
  const BYTE byte = 0;
  CheckInvalid(IDispatchPtr(), &byte, 1, "null script");
  CheckInvalid(IDispatchPtr(new TestDispatch()), NULL, 1, "null bytes");
  CheckInvalid(IDispatchPtr(new TestDispatch()), &byte, 0,
               "zero bytes");
  return 0;
}
