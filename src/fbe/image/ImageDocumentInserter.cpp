#include "stdafx.h"
#include "ImageDocumentInserter.h"
#include "../BinaryId.h"
#include "../utils/utils.h"
#include "../undo/UndoFailureRecovery.h"
namespace FbeImage {
class BinaryRecordingBlock : public CComObjectRootEx<CComSingleThreadModel>, public IOleParentUndoUnit {
public:
  BEGIN_COM_MAP(BinaryRecordingBlock)
    COM_INTERFACE_ENTRY(IOleUndoUnit)
    COM_INTERFACE_ENTRY(IOleParentUndoUnit)
  END_COM_MAP()
  STDMETHOD(Do)(IOleUndoManager*) { return E_UNEXPECTED; }
  STDMETHOD(GetDescription)(BSTR* text) { if (!text) return E_POINTER; *text = ::SysAllocString(L"Binary recording block"); return *text ? S_OK : E_OUTOFMEMORY; }
  STDMETHOD(GetUnitType)(CLSID* clsid, LONG* id) { if (!clsid || !id) return E_POINTER; *clsid = CLSID_NULL; *id = 0; return S_OK; }
  STDMETHOD(OnNextAdd)() { return S_OK; }
  STDMETHOD(Open)(IOleParentUndoUnit*) { return S_OK; }
  STDMETHOD(Close)(IOleParentUndoUnit* unit, BOOL) { return unit == static_cast<IOleParentUndoUnit*>(this) ? S_FALSE : S_OK; }
  STDMETHOD(Add)(IOleUndoUnit*) { return S_OK; }
  STDMETHOD(FindUnit)(IOleUndoUnit*) { return S_FALSE; }
  STDMETHOD(GetParentState)(DWORD* state) { if (!state) return E_POINTER; *state = UAS_BLOCKED; return S_OK; }
};

class BinaryRecordingScope {
  CComPtr<IOleUndoManager> m_manager;
  CComPtr<IOleParentUndoUnit> m_block;
  bool m_open = false;
public:
  explicit BinaryRecordingScope(IOleUndoManager* manager, bool open = true) : m_manager(manager) {
    CComObject<BinaryRecordingBlock>* raw = nullptr;
    CheckError(CComObject<BinaryRecordingBlock>::CreateInstance(&raw));
    m_block = raw;
    if (open) CheckError(Open());
  }
  bool IsOpen() const { return m_open; }
  HRESULT Open() { if (m_open) return S_OK; const HRESULT hr = m_manager->Open(m_block); if (SUCCEEDED(hr)) m_open = true; return hr; }
  HRESULT Close() {
    if (!m_open) return S_OK;
    const HRESULT hr = m_manager->Close(m_block, FALSE);
    if (SUCCEEDED(hr)) m_open = false;
    return hr;
  }
  ~BinaryRecordingScope() { if (m_open) { const HRESULT hr = Close(); if (FAILED(hr)) ::OutputDebugString(L"FBE: binary recording block Close failed\n"); } }
};

static MSHTML::IHTMLDocument2Ptr ScriptDocument(const IDispatchPtr& script) {
  CComDispatchDriver driver(script);
  _variant_t document;
  CheckError(driver.GetPropertyByName(L"document", &document));
  return MSHTML::IHTMLDocument2Ptr(document);
}

// Retain the original binary node: cloning/serializing HTML loses base64data
// (a SAFEARRAY expando), input values and event handlers in MSHTML.
class ImportedBinaryUndoUnit : public CComObjectRootEx<CComSingleThreadModel>, public IOleUndoUnit {
public:
  BEGIN_COM_MAP(ImportedBinaryUndoUnit)
    COM_INTERFACE_ENTRY(IOleUndoUnit)
  END_COM_MAP()
  IDispatchPtr script;
  MSHTML::IHTMLDOMNodePtr node, parent, next;
  bool attached = true;
  std::function<void(bool)> applyState;
  bool applyBefore = true;
  HRESULT ApplyState(IOleUndoManager* manager) {
    try {
      CComPtr<IOleUndoManager> recordingManager;
      IServiceProviderPtr services(ScriptDocument(script));
      CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&recordingManager)));
      BinaryRecordingScope compensation(recordingManager, false), recording(recordingManager);
      HRESULT failure = S_OK;
      try {
        applyState(applyBefore);
        CheckError(FbeUndo::Inject(L"binary-api-close")); CheckError(recording.Close());
        if (manager) { CheckError(FbeUndo::Inject(L"binary-api-add")); CheckError(manager->Add(this)); }
        applyBefore = !applyBefore; return S_OK;
      } catch (const _com_error& error) { failure = error.Error(); }
      catch (const std::bad_alloc&) { failure = E_OUTOFMEMORY; }
      catch (const CAtlException& error) { failure = error.m_hr; }
      catch (...) { failure = E_UNEXPECTED; }
      try {
        if (!recording.IsOpen()) CheckError(compensation.Open());
        applyState(!applyBefore);
        CheckError(compensation.Close()); CheckError(recording.Close());
      } catch (...) { return E_ABORT; }
      return failure;
    } catch (const _com_error& error) { return error.Error(); }
    catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (const CAtlException& error) { return error.m_hr; }
    catch (...) { return E_UNEXPECTED; }
  }
  STDMETHOD(Do)(IOleUndoManager* manager) {
    if (applyState) return ApplyState(manager);
    try {
      CComPtr<IOleUndoManager> recordingManager;
      IServiceProviderPtr services(ScriptDocument(script));
      CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&recordingManager)));
      MSHTML::IHTMLDocument2Ptr document(ScriptDocument(script));
      const MSHTML::IHTMLDOMNodePtr oldParent(node->parentNode), oldNext(node->nextSibling);
      struct CoverState { MSHTML::IHTMLSelectElementPtr select; _bstr_t value; std::vector<MSHTML::IHTMLDOMNodePtr> options; };
      std::vector<CoverState> covers;
      for (LPCWSTR name : {L"tiCover", L"stiCover"}) {
        MSHTML::IHTMLElementPtr container(document->all->item(name));
        MSHTML::IHTMLElementCollectionPtr selects(MSHTML::IHTMLElement2Ptr(container)->getElementsByTagName(L"SELECT"));
        for (long i = 0; i < selects->length; ++i) {
          CoverState cover; cover.select = MSHTML::IHTMLSelectElementPtr(selects->item(i)); cover.value = cover.select->value;
          for (MSHTML::IHTMLDOMNodePtr option(MSHTML::IHTMLDOMNodePtr(cover.select)->firstChild); option; option = option->nextSibling)
            cover.options.push_back(option->cloneNode(VARIANT_TRUE));
          covers.push_back(std::move(cover));
        }
      }
      // Reserve the compensating block before any mutation (including the
      // Add failure case, when the original block has already been closed).
      BinaryRecordingScope compensation(recordingManager, false);
      BinaryRecordingScope recording(recordingManager);
      bool changed = false;
      HRESULT failure = S_OK;
      try {
      if (attached) {
        if (node->parentNode != parent) return E_UNEXPECTED;
        changed = true;
        parent->removeChild(node);
      } else {
        // MSHTML may retain a removed node in an orphan fragment. A parent
        // pointer alone does not mean it is attached to the user's document.
        MSHTML::IHTMLDOMNodePtr body(ScriptDocument(script)->body);
        for (MSHTML::IHTMLDOMNodePtr ancestor(node->parentNode); ancestor; ancestor = ancestor->parentNode)
          if (ancestor == parent || ancestor == body) return E_UNEXPECTED;
        changed = true;
        if (next && next->parentNode == parent) parent->insertBefore(node, next.GetInterfacePtr());
        else parent->appendChild(node);
      }
      // Exercise exceptions after partial DOM mutation, inside the same
      // compensation boundary as failures returned by COM.
      if (FAILED(FbeUndo::Inject(L"binary-oom"))) throw std::bad_alloc();
      if (FAILED(FbeUndo::Inject(L"binary-com-oom"))) _com_issue_error(E_OUTOFMEMORY);
      if (FAILED(FbeUndo::Inject(L"binary-atl-oom"))) throw CAtlException(E_OUTOFMEMORY);
      if (FAILED(FbeUndo::Inject(L"binary-exception"))) throw E_UNEXPECTED;
      CComDispatchDriver driver(script);
      CheckError(FbeUndo::Inject(L"binary-rebuild")); CheckError(driver.Invoke0(L"RebuildImagesInfo"));
      CheckError(FbeUndo::Inject(L"binary-covers")); CheckError(driver.Invoke0(L"FillCoverList"));
      if (FAILED(FbeUndo::Inject(L"binary-covers-oom"))) throw std::bad_alloc();
      CheckError(FbeUndo::Inject(L"binary-close")); CheckError(recording.Close());
      if (manager) { CheckError(FbeUndo::Inject(L"binary-add")); CheckError(manager->Add(this)); }
      attached = !attached;
      return S_OK;
      } catch (const _com_error& error) { failure = error.Error(); }
      catch (const std::bad_alloc&) { failure = E_OUTOFMEMORY; }
      catch (const CAtlException& error) { failure = error.m_hr; }
      catch (...) { failure = E_UNEXPECTED; }
      if (!changed) return failure;
      try {
          if (!recording.IsOpen()) CheckError(compensation.Open());
          if (oldParent) {
            if (oldNext && oldNext->parentNode == oldParent) oldParent->insertBefore(node, oldNext.GetInterfacePtr());
            else oldParent->appendChild(node);
          } else if (node->parentNode) node->parentNode->removeChild(node);
          CComDispatchDriver driver(script);
          const HRESULT rebuilt = driver.Invoke0(L"RebuildImagesInfo");
          const HRESULT filled = driver.Invoke0(L"FillCoverList");
          for (const auto& cover : covers) {
            MSHTML::IHTMLDOMNodePtr select(cover.select);
            while (select->firstChild) select->removeChild(select->firstChild);
            for (const auto& option : cover.options) select->appendChild(option);
            if (cover.select->value != cover.value) cover.select->value = cover.value;
          }
          const HRESULT compensated = compensation.Close(), closed = recording.Close();
          if (FAILED(rebuilt) || FAILED(filled) || FAILED(compensated) || FAILED(closed)) return E_ABORT;
          return failure;
      } catch (...) { return E_ABORT; } // compensation itself could not finish
    } catch (const _com_error& error) { return error.Error(); }
    catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (const CAtlException& error) { return error.m_hr; }
    catch (...) { return E_UNEXPECTED; }
  }
  STDMETHOD(GetDescription)(BSTR* text) {
    if (!text) return E_POINTER;
    *text = ::SysAllocString(L"Imported image binary"); return *text ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHOD(GetUnitType)(CLSID* clsid, LONG* id) {
    if (!clsid || !id) return E_POINTER;
    *clsid = applyState ? FbeUndo::UnitClass() : CLSID_NULL; *id = 0; return S_OK;
  }
  STDMETHOD(OnNextAdd)() { return S_OK; }
};

HRESULT CommitBinaryEdit(const IDispatchPtr& script, IOleUndoManager* manager,
                         const std::function<void(bool)>& applyState) {
  if (!manager || !script) return E_POINTER;
  DWORD state = 0;
  if (manager->GetOpenParentState(&state) != S_FALSE) return E_UNEXPECTED;
  try {
    CComObject<ImportedBinaryUndoUnit>* raw = nullptr;
    CheckError(CComObject<ImportedBinaryUndoUnit>::CreateInstance(&raw));
    CComPtr<IOleUndoUnit> unit(raw); raw->script = script; raw->applyState = applyState;
    FbeUndo::HistoryCheckpoint history(manager);
    BinaryRecordingScope compensation(manager, false), recording(manager);
    HRESULT failure = S_OK;
    try {
      applyState(false);
      CheckError(FbeUndo::Inject(L"binary-api-close")); CheckError(recording.Close());
      CheckError(FbeUndo::Inject(L"binary-api-add")); CheckError(manager->Add(unit));
      return S_OK;
    } catch (const _com_error& error) { failure = error.Error(); }
    catch (const std::bad_alloc&) { failure = E_OUTOFMEMORY; }
    catch (const CAtlException& error) { failure = error.m_hr; }
    catch (...) { failure = E_UNEXPECTED; }
    try {
      if (!recording.IsOpen()) CheckError(compensation.Open());
      applyState(true);
      CheckError(compensation.Close()); CheckError(recording.Close());
      CheckError(history.Restore(manager));
    } catch (...) { return E_ABORT; }
    return failure;
  } catch (const _com_error& error) { return error.Error(); }
  catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
  catch (const CAtlException& error) { return error.m_hr; }
  catch (...) { return E_UNEXPECTED; }
}

static MSHTML::IHTMLDOMNodePtr BinaryParent(const IDispatchPtr& script) {
  MSHTML::IHTMLDocument2Ptr dom(ScriptDocument(script));
  IDispatchPtr container(dom->all->item(L"binobj"));
  if (!container) return MSHTML::IHTMLDOMNodePtr();
  // Modern Description wraps attachment rows in an inner body. The script
  // appends there, so snapshot and rollback must observe that same parent.
  CComDispatchDriver dispatch(container);
  CComVariant inner;
  if (SUCCEEDED(dispatch.GetPropertyByName(L"_descriptionBody", &inner)) && inner.vt == VT_DISPATCH && inner.pdispVal) {
    MSHTML::IHTMLDOMNodePtr node(inner.pdispVal);
    if (node) return node;
  }
  return MSHTML::IHTMLDOMNodePtr(container);
}

static IDispatchPtr FindImportedImage(MSHTML::IHTMLDOMNodePtr node, const _variant_t& id) {
  if (!node) return IDispatchPtr();
  MSHTML::IHTMLElementPtr element(node);
  if (element && CString(static_cast<LPCWSTR>(element->className)) == L"image") {
    const _variant_t href(element->getAttribute(L"href", 2));
    if (V_VT(&href) == VT_BSTR && CString(href.bstrVal) == CString(L"#") + id.bstrVal)
      return IDispatchPtr(element);
  }
  for (MSHTML::IHTMLDOMNodePtr child(node->firstChild); child; child = child->nextSibling) {
    IDispatchPtr image(FindImportedImage(child, id));
    if (image) return image;
  }
  return IDispatchPtr();
}

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
  return FbeBinary::NormalizeXmlId(name).AllocSysString();
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
  MSHTML::IHTMLDOMNodePtr parent;
  std::vector<MSHTML::IHTMLDOMNodePtr> existing;
  bool attempted = false;
  try {
    parent = BinaryParent(script);
    if (!parent) return Finish(out, E_UNEXPECTED);
    for (MSHTML::IHTMLDOMNodePtr child(parent->firstChild); child; child = child->nextSibling) existing.push_back(child);
    attempted = true;
    hr = body.InvokeN(L"apiAddBinary", args, 4, &id);
    MSHTML::IHTMLDOMNodePtr created;
    for (MSHTML::IHTMLDOMNodePtr child(parent->firstChild); child; child = child->nextSibling) {
      if (std::find(existing.begin(), existing.end(), child) == existing.end()) {
        if (created) { hr = E_UNEXPECTED; break; }
        created = child;
      }
    }
    if (SUCCEEDED(hr) && (!created || V_VT(&id) != VT_BSTR || !id.bstrVal || !*id.bstrVal)) hr = E_UNEXPECTED;
    if (SUCCEEDED(hr)) hr = body.Invoke0(L"FillCoverList");
    if (SUCCEEDED(hr) && out) out->createdBinary = created;
  } catch (const _com_error& error) { hr = error.Error(); }
  catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
  if (FAILED(hr) && parent && attempted) {
    // Identity-based rollback cannot remove a pre-existing binary with the
    // same requested ID. apiAddBinary is synchronous; no other import runs here.
    try {
      for (MSHTML::IHTMLDOMNodePtr child(parent->firstChild); child;) {
        MSHTML::IHTMLDOMNodePtr next(child->nextSibling);
        if (std::find(existing.begin(), existing.end(), child) == existing.end()) parent->removeChild(child);
        child = next;
      }
      const HRESULT rebuilt = body.Invoke0(L"RebuildImagesInfo");
      const HRESULT covers = body.Invoke0(L"FillCoverList");
      if (FAILED(rebuilt)) return Finish(out, rebuilt);
      if (FAILED(covers)) return Finish(out, covers);
    } catch (const _com_error& error) { return Finish(out, error.Error()); }
  }
  return Finish(out, hr, &id);
}
HRESULT InsertImportedImage(const IDispatchPtr &script, const BYTE *bytes,
                            size_t size, const CString &name,
                            const CString &mime, ImagePlacement placement,
                            ImageInsertionResult *out, IOleUndoManager *manager) {
  if (!manager) return Finish(out, E_POINTER);
  CComObject<ImportedBinaryUndoUnit>* raw = nullptr;
  HRESULT hr = CComObject<ImportedBinaryUndoUnit>::CreateInstance(&raw);
  if (FAILED(hr)) return Finish(out, hr);
  CComPtr<IOleUndoUnit> binaryUnit(raw);
  ImageInsertionResult result;
  HRESULT binaryClosed = S_OK;
  {
    BinaryRecordingScope recording(manager);
    hr = AddImportedBinary(script, bytes, size, name, mime, &result);
    binaryClosed = recording.Close();
  }
  if (FAILED(binaryClosed)) {
    if (result.createdBinary) {
      try {
        MSHTML::IHTMLDOMNodePtr parent(result.createdBinary->parentNode);
        if (parent) parent->removeChild(result.createdBinary);
        CComDispatchDriver body(script);
        const HRESULT rebuilt = body.Invoke0(L"RebuildImagesInfo");
        const HRESULT covers = body.Invoke0(L"FillCoverList");
        if (FAILED(rebuilt)) return Finish(out, rebuilt);
        if (FAILED(covers)) return Finish(out, covers);
      } catch (const _com_error& error) { return Finish(out, error.Error()); }
    }
    return Finish(out, binaryClosed);
  }
  if (SUCCEEDED(hr)) {
    raw->script = script; raw->node = result.createdBinary;
    raw->parent = raw->node->parentNode; raw->next = raw->node->nextSibling;
    hr = manager->Add(binaryUnit);
    if (FAILED(hr)) {
      const HRESULT rolledBack = raw->Do(nullptr);
      return Finish(out, FAILED(rolledBack) ? rolledBack : hr);
    }
    CComDispatchDriver body(script);
    _variant_t check(false);
    _variant_t inserted;
    // The editor records the affected children through its existing DOM undo
    // unit. Do not mix those snapshots with MSHTML's deferred script units.
    BinaryRecordingScope insertion(manager);
    hr = body.Invoke2(placement == ImagePlacement::Inline ? L"InsInlineImage"
                                                          : L"InsImage",
                      &check, &result.binaryId, &inserted);
    if (SUCCEEDED(hr)) {
      try {
        if (V_VT(&inserted) == VT_DISPATCH && V_DISPATCH(&inserted)) {
          MSHTML::IHTMLDOMNodePtr node(V_DISPATCH(&inserted));
          if (node && node->parentNode) result.insertedElement = FindImportedImage(node, result.binaryId);
        }
        if (!result.insertedElement) hr = E_FAIL;
      } catch (const _com_error& error) { hr = error.Error(); }
    }
    const HRESULT closed = insertion.Close();
    if (FAILED(closed)) hr = closed;
    result.result = hr;
  }
  if (out)
    *out = result;
  return hr;
}
} // namespace FbeImage
