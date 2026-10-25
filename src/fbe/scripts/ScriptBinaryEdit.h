#pragma once
#include "ScriptBinaryDocument.h"
#include "../image/ImageDocumentInserter.h"
#include "../undo/UndoFailureRecovery.h"
#include <exception>

namespace FbeScripts { namespace Binary {
struct Position {
    MSHTML::IHTMLDOMNodePtr node, parent, next;
    explicit Position(MSHTML::IHTMLDOMNodePtr value) : node(value), parent(value->parentNode), next(value->nextSibling) {}
    void Attach() const {
        if (!parent) CheckError(E_UNEXPECTED);
        if (node->parentNode == parent) return;
        if (next && next->parentNode == parent) parent->insertBefore(node, next.GetInterfacePtr());
        else parent->appendChild(node);
    }
    void Detach() const { if (node->parentNode == parent) parent->removeChild(node); }
};
struct Cover {
    MSHTML::IHTMLSelectElementPtr select;
    CString value;
};
inline bool Reference(const _variant_t& value, const CString& id)
{ return value.vt == VT_BSTR && (CString(value.bstrVal) == L"#" + id || CString(value.bstrVal) == L"fbw-internal:#" + id); }
inline std::vector<Cover> Covers(MSHTML::IHTMLDocument2Ptr document)
{
    std::vector<Cover> result;
    for (LPCWSTR name : {L"tiCover", L"stiCover"}) {
        MSHTML::IHTMLElementPtr container(document->all->item(name));
        if (!container) CheckError(E_UNEXPECTED);
        MSHTML::IHTMLElementCollectionPtr selects(MSHTML::IHTMLElement2Ptr(container)->getElementsByTagName(L"SELECT"));
        for (long index = 0; index < selects->length; ++index) {
            MSHTML::IHTMLSelectElementPtr select(selects->item(index));
            result.push_back({select, CString(static_cast<LPCWSTR>(select->value))});
        }
    }
    return result;
}
struct References {
    std::vector<Position> images;
    bool any = false, unknown = false, image = false;
};
inline References FindReferences(MSHTML::IHTMLDocument2Ptr document, const CString& id, const std::vector<Cover>& covers)
{
    References result;
    auto elements = document->all;
    if (!elements) CheckError(E_UNEXPECTED);
    for (long index = 0; index < elements->length; ++index) {
        MSHTML::IHTMLElementPtr element(elements->item(index));
        if (!element) continue;
        MSHTML::IHTMLSelectElementPtr select(element);
        if (select && CString(static_cast<LPCWSTR>(select->value)) == L"#" + id) {
            result.any = true;
            if (std::none_of(covers.begin(), covers.end(), [&](const auto& cover) { return cover.select == select; })) result.unknown = true;
            else result.image = true;
        }
        const bool href = Reference(element->getAttribute(L"href", 2), id), src = Reference(element->getAttribute(L"src", 2), id);
        if (!href && !src) continue;
        result.any = true;
        const CString tag(static_cast<LPCWSTR>(element->tagName)), css(static_cast<LPCWSTR>(element->className));
        if (href && css == L"image" && (tag == L"DIV" || tag == L"SPAN") && !src) {
            result.image = true;
            bool body = false;
            for (auto ancestor = element->parentElement; ancestor; ancestor = ancestor->parentElement)
                if (U::scmp(ancestor->id, L"fbw_body") == 0) body = true;
            if (!body) { result.unknown = true; continue; }
            bool safe = true;
            for (MSHTML::IHTMLDOMNodePtr child(MSHTML::IHTMLDOMNodePtr(element)->firstChild); child; child = child->nextSibling) {
                if (child->nodeType == 3) { CString text(static_cast<LPCWSTR>(_bstr_t(child->nodeValue))); text.Trim(); if (!text.IsEmpty()) safe = false; }
                else if (child->nodeType == 1) {
                    MSHTML::IHTMLElementPtr image(child);
                    if (U::scmp(image->tagName, L"IMG") || !Reference(image->getAttribute(L"src", 2), id)) safe = false;
                } else safe = false;
            }
            if (safe) result.images.emplace_back(MSHTML::IHTMLDOMNodePtr(element)); else result.unknown = true;
        } else if (src && tag == L"IMG" && !href) {
            result.image = true;
            const auto parent = element->parentElement;
            if (!parent || U::scmp(parent->className, L"image") || !Reference(parent->getAttribute(L"href", 2), id)) result.unknown = true;
        } else result.unknown = true;
    }
    // Preserve the legacy conservative reference check, including host fallbacks.
    CComDispatchDriver script(document->Script); CComVariant argument(id), referenced;
    CheckError(script.Invoke1(L"BinaryIsReferenced", &argument, &referenced));
    if (referenced.vt != VT_BOOL) CheckError(E_UNEXPECTED);
    if (referenced.boolVal && !result.any) { result.any = true; result.unknown = true; result.image = true; }
    return result;
}
inline void ReadFile(const CString& path, CString mime, LoadedObject& result)
{
    CHandle file(::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file == INVALID_HANDLE_VALUE) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
    LARGE_INTEGER size = {};
    if (!::GetFileSizeEx(file, &size)) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
    if (size.QuadPart <= 0 || size.QuadPart > 64LL * 1024 * 1024) CheckError(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
    result.size = static_cast<ULONG>(size.QuadPart);
    result.data.vt = VT_ARRAY | VT_UI1; result.data.parray = ::SafeArrayCreateVector(VT_UI1, 0, result.size);
    if (!result.data.parray) CheckError(E_OUTOFMEMORY);
    Bytes bytes(result.data.parray); DWORD read = 0;
    if (!::ReadFile(file, bytes.data, result.size, &read, nullptr)) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
    if (read != result.size) CheckError(STG_E_READFAULT);
    CString detected;
    if (result.size >= 8 && !memcmp(bytes.data, "\x89PNG\r\n\x1a\n", 8)) detected = L"image/png";
    else if (result.size >= 3 && bytes.data[0] == 0xff && bytes.data[1] == 0xd8 && bytes.data[2] == 0xff) detected = L"image/jpeg";
    else if (result.size >= 6 && (!memcmp(bytes.data, "GIF87a", 6) || !memcmp(bytes.data, "GIF89a", 6))) detected = L"image/gif";
    else if (result.size >= 2 && bytes.data[0] == 'B' && bytes.data[1] == 'M') detected = L"image/bmp";
    else if (result.size >= 4 && (!memcmp(bytes.data, "II\x2a\0", 4) || !memcmp(bytes.data, "MM\0\x2a", 4))) detected = L"image/tiff";
    else if (result.size >= 5 && !memcmp(bytes.data, "%PDF-", 5)) detected = L"application/pdf";
    mime.Trim(); mime.MakeLower();
    if (mime == L"image/jpg" || mime == L"image/pjpeg") mime = L"image/jpeg";
    if (mime.IsEmpty()) {
        if (!detected.IsEmpty()) mime = detected;
        else CheckError(E_INVALIDARG);
    }
    const int slash = mime.Find(L'/');
    if (slash <= 0 || slash == mime.GetLength()-1) CheckError(E_INVALIDARG);
    for (int index = 0; index < mime.GetLength(); ++index)
        if (!(isalnum(static_cast<unsigned char>(mime[index])) && mime[index] < 128) && !wcschr(L"/!#$&^_.+-", mime[index])) CheckError(E_INVALIDARG);
    if (mime.Left(6) == L"image/") {
        if (detected != mime) CheckError(E_INVALIDARG);
        CComPtr<IStream> stream; CheckError(::CreateStreamOnHGlobal(nullptr, TRUE, &stream)); ULONG written = 0;
        CheckError(stream->Write(bytes.data, result.size, &written)); if (written != result.size) CheckError(STG_E_WRITEFAULT);
        LARGE_INTEGER start = {}; CheckError(stream->Seek(start, STREAM_SEEK_SET, nullptr)); CImage image;
        CheckError(image.Load(stream));
        if (image.GetWidth() <= 0 || image.GetHeight() <= 0 || image.GetWidth() > 16384 || image.GetHeight() > 16384 ||
            static_cast<ULONGLONG>(image.GetWidth()) * image.GetHeight() > 32000000) CheckError(E_INVALIDARG);
        result.width = image.GetWidth(); result.height = image.GetHeight();
        result.dimensionsKnown = result.dimensionsAttempted = true;
    } else if ((!detected.IsEmpty() && detected != mime) || (mime == L"application/pdf" && detected != mime)) CheckError(E_INVALIDARG);
    result.mime = mime;
}
struct Edit {
    FB::Doc* owner;
    MSHTML::IHTMLDocument2Ptr document;
    LoadedObject before, after;
    std::shared_ptr<FbeBinary::BinaryPayloadStore::RetainedPayload> beforePayload, afterPayload;
    bool existed, remains;
    std::unique_ptr<Position> binary;
    References references;
    std::vector<Cover> covers;
    FB::Doc::UndoSavepoints savepoints;
    bool currentBefore = true, savedBefore = true, recoveryBefore = true;
    bool savedKnown, recoveryKnown;
    MSHTML::IHTMLTxtRangePtr selection;
    _bstr_t selectionBookmark;
    MSHTML::IHTMLControlRangePtr controls;
    Edit(FB::Doc* doc) : owner(doc), document(doc->m_body.Document()), existed(false), remains(true), covers(Covers(document)), savepoints(doc->CaptureUndoSavepoints()), savedKnown(savepoints.clean), recoveryKnown(savepoints.recoveryClean) {
        IDispatchPtr range(document->selection->createRange()); selection = range; controls = range;
        if (selection) { selection = selection->duplicate(); selectionBookmark = selection->getBookmark(); }
    }
    void Apply(bool undo) {
        if (FB::Doc::m_active_doc != owner || owner->m_body.Document() != document) CheckError(E_UNEXPECTED);
        // Includes OLE compensation after a partially failed edit.
        struct InvalidateOnExit {
            FB::Doc* doc;
            int exceptions = std::uncaught_exceptions();
            ~InvalidateOnExit() { if (std::uncaught_exceptions() > exceptions) doc->m_binaryStore.Invalidate(); }
        } invalidate{owner};
        owner->ReconcileBinaryStore();
        const auto live = owner->CaptureUndoSavepoints();
        // A save between edit and Undo establishes a new baseline. Remember
        // which side was saved instead of reinstating the obsolete save point.
        if (live.clean) {
            savepoints.savedVersion = live.savedVersion; savepoints.savedHtml = live.savedHtml; savepoints.hasSaved = live.hasSaved;
            savedKnown = true; savedBefore = currentBefore;
        }
        if (live.recoveryClean) {
            savepoints.recoveryVersion = live.recoveryVersion; savepoints.recoveryHtml = live.recoveryHtml; savepoints.hasRecovery = live.hasRecovery;
            recoveryKnown = true; recoveryBefore = currentBefore;
        }
        CComDispatchDriver script(document->Script);
        const bool attach = undo ? existed : remains;
        LoadedObject& target = undo ? before : after;
        const auto lease = undo ? beforePayload : afterPayload;
        const auto payload = lease ? lease->Get() : nullptr;
        if (attach && !payload) CheckError(CO_E_OBJNOTCONNECTED);
        unsigned long long payloadRevision = owner->m_binaryStore.payloads.Revision();
        if (!binary && attach) {
            FbeImage::ImageInsertionResult result;
            Bytes bytes(payload->parray);
            CheckError(FbeImage::AddImportedBinary(document->Script, bytes.data, after.size, after.id, after.mime, &result));
            // Retain the actual node before any further allocation can fail.
            after.element = result.createdBinary;
            binary = std::make_unique<Position>(MSHTML::IHTMLDOMNodePtr(after.element));
            CheckError(owner->m_binaryStore.payloads.Assign(after.element, payload));
            payloadRevision = owner->m_binaryStore.payloads.Revision();
            if (result.binaryId.vt != VT_BSTR || after.id != result.binaryId.bstrVal) CheckError(E_UNEXPECTED);
        }
        if (binary) {
            if (attach) {
                binary->Attach();
                if (existed && remains) {
                    CheckError(owner->m_binaryStore.payloads.Assign(IDispatchPtr(binary->node), payload));
                    payloadRevision = owner->m_binaryStore.payloads.Revision();
                    CComVariant values[4] = {CComVariant(target.mime), CComVariant(target.id), CComVariant(L""), CComVariant(binary->node)};
                    // Borrow only for the synchronous Invoke; the edit retains
                    // ownership of the byte array throughout Undo/Redo.
                    VARIANT args[5] = {*payload, values[0], values[1], values[2], values[3]};
                    CheckError(script.InvokeN(L"BuildBinaryControls", args, 5));
                }
            } else binary->Detach();
        } else if (after.element && !attach) {
            MSHTML::IHTMLDOMNodePtr node(after.element); if (node->parentNode) node->parentNode->removeChild(node);
        }
        if (attach) {
            FbeBinary::BinaryMetadata metadata;
            metadata.id = target.id; metadata.contentType = target.mime; metadata.byteSize = target.size; metadata.sizeKnown = true;
            CString mime(target.mime); mime.MakeLower(); metadata.image = mime.Left(6) == L"image/";
            metadata.width = target.width; metadata.height = target.height;
            metadata.dimensionsKnown = target.dimensionsKnown; metadata.dimensionsAttempted = target.dimensionsAttempted;
            owner->m_binaryStore.Register(metadata, binary ? IDispatchPtr(binary->node).GetInterfacePtr() : target.element.GetInterfacePtr());
            // Validated metadata describes this exact assigned snapshot. Keep
            // unrelated cached entries if no script callback wrote more data.
            if (owner->m_binaryStore.payloads.Revision() == payloadRevision)
                owner->m_binaryStore.AcceptPayloadRevision();
        } else owner->m_binaryStore.Remove(existed ? before.id : after.id);
        if (owner->m_binaryDuplicateIds) owner->m_binaryStore.Invalidate();
        if (!remains) {
            if (undo) { for (auto image = references.images.rbegin(); image != references.images.rend(); ++image) image->Attach(); }
            else for (const auto& image : references.images) image.Detach();
        }
        CheckError(FbeUndo::Inject(L"binary-api-update"));
        if (FAILED(FbeUndo::Inject(L"binary-api-oom"))) throw std::bad_alloc();
        CheckError(script.Invoke0(L"RebuildImagesInfo")); CheckError(script.Invoke0(L"FillCoverList"));
        for (const auto& cover : covers) cover.select->value = !undo && !remains && cover.value == L"#" + before.id ? L"" : static_cast<LPCWSTR>(cover.value);
        if (existed && remains) {
            auto elements = document->all;
            for (long index = 0; index < elements->length; ++index) {
                MSHTML::IHTMLElementPtr element(elements->item(index));
                if (U::scmp(element->tagName, L"IMG") == 0 && Reference(element->getAttribute(L"src", 2), before.id)) {
                    MSHTML::IHTMLImgElementPtr image(element); image->src = L""; image->src = _bstr_t(L"fbw-internal:#" + before.id);
                }
            }
        }
        auto targetPoints = savepoints;
        targetPoints.clean = savedKnown && savedBefore == undo;
        targetPoints.recoveryClean = recoveryKnown && recoveryBefore == undo;
        targetPoints.reconcile = undo && savepoints.reconcile;
        const long version = owner->m_body.GetVersionNumber();
        if (!targetPoints.clean && targetPoints.savedVersion == version) targetPoints.savedVersion = -1;
        if (!targetPoints.recoveryClean && targetPoints.recoveryVersion == version) targetPoints.recoveryVersion = -1;
        if (targetPoints.clean) owner->m_body.ResetFormChanged();
        if (targetPoints.recoveryClean) owner->m_body.ResetFormCP();
        owner->RestoreUndoSavepoints(targetPoints);
        currentBefore = undo;
        if (undo || remains) {
            if (selection) {
                auto restored = selection->duplicate();
                // A retained live range moves when an image before its end is
                // detached. Undo needs the original position, not that moved end.
                if (undo && selectionBookmark.length() && !restored->moveToBookmark(selectionBookmark)) CheckError(E_UNEXPECTED);
                restored->select();
            } else if (controls) controls->select();
        }
    }
};
inline void Mutate(FB::Doc* document, DISPID method, const std::vector<CComVariant>& args, CComVariant& result)
{
    document->ReconcileBinaryStore();
    if (HasDocumentUpdates()) CheckError(E_UNEXPECTED);
    CComPtr<IOleUndoManager> manager; IServiceProviderPtr services(document->m_body.Document());
    CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager)));
    DWORD state = 0; if (manager->GetOpenParentState(&state) != S_FALSE) CheckError(E_UNEXPECTED);
    auto edit = std::make_shared<Edit>(document);
    const auto objects = Objects(edit->document);
    if (method == 149) {
        ReadFile(args[0].bstrVal, args.size() == 3 ? CString(args[2].bstrVal) : CString(), edit->after);
        edit->after.id = document->UniqueBinaryId(CString(args[1].bstrVal).IsEmpty() ? CString(args[0].bstrVal) : CString(args[1].bstrVal));
    } else {
        auto found = std::find_if(objects.begin(), objects.end(), [&](const auto& object) { return object.id == args[0].bstrVal; });
        if (found == objects.end()) CheckError(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
        Load(*found, edit->before); edit->existed = true;
        if (const auto* metadata = document->m_binaryStore.Find(found->id)) {
            edit->before.width = metadata->width; edit->before.height = metadata->height;
            edit->before.dimensionsKnown = metadata->dimensionsKnown; edit->before.dimensionsAttempted = metadata->dimensionsAttempted;
        }
        edit->binary = std::make_unique<Position>(MSHTML::IHTMLDOMNodePtr(found->element));
        edit->references = FindReferences(edit->document, found->id, edit->covers);
        if (method == 151) {
            const bool remove = args.size() == 2 && args[1].boolVal != VARIANT_FALSE;
            if ((edit->references.any && !remove) || edit->references.unknown) CheckError(E_ACCESSDENIED);
            edit->remains = false;
        } else {
            ReadFile(args[1].bstrVal, args.size() == 3 ? CString(args[2].bstrVal) : CString(), edit->after);
            edit->after.id = found->id; edit->after.element = found->element;
            if (edit->references.image && edit->after.mime.Left(6) != L"image/") CheckError(E_INVALIDARG);
            if (edit->before.mime == edit->after.mime && edit->before.size == edit->after.size) {
                Bytes before(edit->before.data.parray), after(edit->after.data.parray);
                if (!memcmp(before.data, after.data, edit->before.size)) { result = true; return; }
            }
        }
    }
    // Allocate the returned ID before committing the edit. An allocation
    // failure must not turn an already committed addition into a failed call.
    if (method == 149) {
        CheckError(result.Clear()); result.vt = VT_BSTR;
        result.bstrVal = edit->after.id.AllocSysString();
        if (!result.bstrVal) CheckError(E_OUTOFMEMORY);
    } else result = true;
    if (edit->existed) {
        const auto payload = document->m_binaryStore.payloads.Snapshot(edit->before.element);
        if (!payload) CheckError(E_UNEXPECTED);
        edit->beforePayload = document->m_binaryStore.payloads.Retain(payload);
        CheckError(edit->before.data.Clear());
    }
    if (edit->remains) {
        auto payload = std::make_shared<CComVariant>();
        CheckError(payload->Attach(&edit->after.data));
        edit->afterPayload = document->m_binaryStore.payloads.Retain(payload);
    }
    CheckError(FbeImage::CommitBinaryEdit(edit->document->Script, manager, [edit](bool undo) { edit->Apply(undo); }));
    document->ReconcileBinaryStore();
}
} }
