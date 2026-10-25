// Included by FBDoc.cpp. Serialization uses the legacy store-backed facade.
namespace FB {
Doc* Doc::BinaryOwner(const GUID& identity)
{
    if (identity == GUID_NULL) return nullptr;
    for (int index = 0; index < m_active_docs.GetSize(); ++index) {
        Doc* doc = m_active_docs.GetKeyAt(index);
        if (identity == doc->ClipboardIdentity()) return doc;
    }
    return nullptr;
}
void Doc::ReconcileBinaryStore()
{
    // During arbitrary script execution a script may change an expando between
    // two native calls. Reconcile conservatively even if it replaced handlers.
    if (m_binaryLegacyDepth) m_binaryStore.Invalidate();
    if (!m_binaryStore.Dirty()) return;
    const auto objects = FbeScripts::Binary::Objects(m_body.Document());
    m_binaryStore.Clear();
    m_binaryDuplicateIds = false;
    try {
        for (const auto& object : objects) {
            CheckError(m_binaryStore.payloads.AdoptLegacy(object.element));
            // Legacy DOM scripts can temporarily create duplicate IDs. The old
            // API selected the first DOM object; preserve that observable rule.
            if (m_binaryStore.Contains(object.id)) { m_binaryDuplicateIds = true; continue; }
            FbeBinary::BinaryMetadata metadata;
            metadata.id = object.id; metadata.contentType = object.mime;
            CString mime(object.mime); mime.MakeLower(); metadata.image = mime.Left(6) == L"image/";
            m_binaryStore.Register(metadata, object.element);
        }
        m_binaryStore.Reconciled(true);
    } catch (...) { m_binaryStore.Clear(); throw; }
}

CString Doc::UniqueBinaryId(const CString& preferred)
{
    ReconcileBinaryStore();
    const CString base = FbeBinary::NormalizeXmlId(preferred);
    CString id(base);
    for (unsigned suffix = 0; ; ++suffix) {
        if (!m_binaryStore.Contains(id)) return id;
        if (suffix == UINT_MAX) CheckError(E_OUTOFMEMORY);
        id.Format(L"%s_%u", base.GetString(), suffix);
    }
}

CString Doc::BinaryInfo(const CString& id)
{
    ReconcileBinaryStore();
    auto* metadata = m_binaryStore.Find(id);
    if (!metadata) CheckError(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
    if (!metadata->sizeKnown || (metadata->image && !metadata->dimensionsAttempted)) {
        FbeScripts::Binary::Object object;
        object.id = id; object.mime = metadata->contentType; object.element = m_binaryStore.Element(id);
        object.container = MSHTML::IHTMLDOMNodePtr(object.element)->parentNode;
        FbeScripts::Binary::LoadedObject payload; FbeScripts::Binary::Load(object, payload);
        if (!metadata->sizeKnown) {
            metadata->byteSize = payload.size; metadata->sizeKnown = true; m_binaryStore.SizeCalculation();
        } else m_binaryStore.SizeHit();
        if (metadata->image && !metadata->dimensionsAttempted) {
            m_binaryStore.DimensionsCalculation();
            CComPtr<IStream> stream; CheckError(::CreateStreamOnHGlobal(nullptr, TRUE, &stream));
            FbeScripts::Binary::Bytes bytes(payload.data.parray); ULONG written = 0;
            CheckError(stream->Write(bytes.data, payload.size, &written));
            if (written != payload.size) CheckError(STG_E_WRITEFAULT);
            LARGE_INTEGER start = {}; CheckError(stream->Seek(start, STREAM_SEEK_SET, nullptr));
            CImage image;
            if (payload.size && SUCCEEDED(image.Load(stream))) {
                metadata->width = image.GetWidth(); metadata->height = image.GetHeight(); metadata->dimensionsKnown = true;
            }
            metadata->dimensionsAttempted = true;
        }
    } else {
        m_binaryStore.SizeHit();
        if (metadata->image) m_binaryStore.DimensionsHit();
    }
    CString dimensions = L"\"width\":null,\"height\":null";
    if (metadata->dimensionsKnown) dimensions.Format(L"\"width\":%d,\"height\":%d", metadata->width, metadata->height);
    CString json;
    json.Format(L"{\"schemaVersion\":1,\"id\":\"%s\",\"mimeType\":\"%s\",\"size\":%llu,%s,\"referenceCount\":null}",
        FbeScripts::JsonEscape(id).GetString(), FbeScripts::JsonEscape(metadata->contentType).GetString(),
        static_cast<unsigned long long>(metadata->byteSize), dimensions.GetString());
    return json;
}

void Doc::BinaryStoreNotification(const CString& action, IDispatch* dispatch)
{
    // LoadFromDOM can participate in the same document's native XML Undo.
    // Its old nodes/leases must stay valid until history is discarded. Only
    // document destruction releases the payload state; reset rebuilds the ID
    // projection for the newly installed DOM.
    if (action == L"reset") { m_binaryStore.Clear(); m_binaryDuplicateIds = false; m_binaryStore.Reconciled(true); return; }
    if (action == L"attach") {
        if (m_binaryStore.Dirty()) m_binaryStore.Invalidate();
        CheckError(m_binaryStore.payloads.Attach(dispatch)); return;
    }
    if (action == L"invalidate") { m_binaryStore.Invalidate(); return; }
    if (m_binaryLegacyDepth) { m_binaryStore.Invalidate(true); return; }
    if (m_binaryDuplicateIds) { m_binaryStore.Invalidate(); return; }
    MSHTML::IHTMLElementPtr element(dispatch);
    if (!element) CheckError(E_INVALIDARG);
    MSHTML::IHTMLElementCollectionPtr inputs(MSHTML::IHTMLElement2Ptr(element)->getElementsByTagName(L"INPUT"));
    CString id, mime, oldId, size, dimensions;
    for (long index = 0; index < inputs->length; ++index) {
        MSHTML::IHTMLInputElementPtr field(inputs->item(index));
        MSHTML::IHTMLElementPtr input(field); const CString name(static_cast<LPCWSTR>(input->id));
        if (name == L"id") {
            id = static_cast<LPCWSTR>(field->value);
            const _variant_t old(input->getAttribute(L"oldId", 2)); if (old.vt == VT_BSTR) oldId = old.bstrVal;
        } else if (name == L"type") mime = static_cast<LPCWSTR>(field->value);
        else if (name == L"size") size = static_cast<LPCWSTR>(field->value);
        else if (name == L"dims") dimensions = static_cast<LPCWSTR>(field->value);
    }
    if (action == L"remove") { m_binaryStore.Remove(id); return; }
    if (action == L"rename" && !m_binaryStore.Dirty() && m_binaryStore.Rename(oldId, id)) return;
    if (action != L"register") { m_binaryStore.Invalidate(); return; }
    // Only committed apiAddBinary calls supply these already-computed controls.
    // Reconciliation never trusts UI controls for legacy-mutated data.
    if (m_binaryStore.CatalogDirty()) return;
    FbeBinary::BinaryMetadata metadata;
    metadata.id = id; metadata.contentType = mime; mime.MakeLower(); metadata.image = mime.Left(6) == L"image/";
    wchar_t* end = nullptr;
    metadata.byteSize = _wcstoui64(size, &end, 10); metadata.sizeKnown = !size.IsEmpty() && end && !*end;
    const int separator = dimensions.Find(L'x');
    if (separator > 0) {
        metadata.width = _wtoi(dimensions.Left(separator)); metadata.height = _wtoi(dimensions.Mid(separator + 1));
        metadata.dimensionsKnown = metadata.image && metadata.width > 0 && metadata.height > 0;
    }
    metadata.dimensionsAttempted = metadata.image;
    m_binaryStore.Register(metadata, element);
    m_binaryStore.AcceptPayloadRevision();
}

void Doc::TraceBinaryStore() const
{
    if (!StartupTrace::Enabled()) return;
    const auto& c = m_binaryStore.Statistics(); CString details;
    details.Format(L"binary_store_entries=%llu; binary_store_rebuilds=%llu; binary_store_reconciliations=%llu; binary_lookup_count=%llu; binary_lookup_misses=%llu; binary_size_calculations=%llu; binary_size_cache_hits=%llu; binary_dimensions_calculations=%llu; binary_dimensions_cache_hits=%llu; binary_legacy_invalidations=%llu",
        static_cast<unsigned long long>(m_binaryStore.Count()), c.rebuilds, c.reconciliations, c.lookups, c.misses,
        c.sizeCalculations, c.sizeHits, c.dimensionsCalculations, c.dimensionsHits, c.legacyInvalidations);
    StartupTrace::Event(L"binary", L"BS100", details);
}
}
