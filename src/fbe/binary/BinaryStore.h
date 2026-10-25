#pragma once
#include <atlbase.h>
#include <atlstr.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include "BinaryPayloadStore.h"

namespace FbeBinary {
struct BinaryMetadata {
    CString id, contentType;
    std::uint64_t byteSize = 0;
    int width = 0, height = 0;
    bool sizeKnown = false, dimensionsKnown = false, image = false;
    bool dimensionsAttempted = false;
    unsigned __int64 generation = 0;
};

// Document-owned, editor-apartment catalog and canonical payload owner.
class BinaryStore {
public:
    struct Counters {
        std::uint64_t rebuilds = 0, reconciliations = 0, lookups = 0, misses = 0;
        std::uint64_t sizeCalculations = 0, sizeHits = 0, dimensionsCalculations = 0, dimensionsHits = 0;
        std::uint64_t legacyInvalidations = 0;
    };
    explicit BinaryStore(bool diagnostics = false) : m_diagnostics(diagnostics) {}
    BinaryPayloadStore payloads;
    void Clear();
    bool Contains(const CString& id) const;
    BinaryMetadata* Find(const CString& id);
    const BinaryMetadata* Find(const CString& id) const;
    IDispatch* Element(const CString& id) const;
    // Win32 ABI provides only four-byte alignment for by-value arguments.
    // Metadata contains eight-byte-aligned counters; keep the boundary indirect.
    void Register(const BinaryMetadata& metadata, IDispatch* element = nullptr);
    bool Rename(const CString& oldId, const CString& newId);
    void Remove(const CString& id);
    void Invalidate(bool legacy = false);
    bool Dirty() const { return m_dirty || m_payloadRevision != payloads.Revision(); }
    bool CatalogDirty() const { return m_dirty; }
    void AcceptPayloadRevision() { m_payloadRevision = payloads.Revision(); }
    void Reconciled(bool rebuilt = false);
    size_t Count() const { return m_entries.size(); }
    void SizeCalculation();
    void SizeHit();
    void DimensionsCalculation();
    void DimensionsHit();
    const Counters& Statistics() const { return m_counters; }
private:
    struct Entry { BinaryMetadata metadata; CComPtr<IDispatch> element; };
    static std::wstring Key(const CString& id) { return std::wstring(id.GetString(), id.GetLength()); }
    std::unordered_map<std::wstring, Entry> m_entries;
    unsigned __int64 m_generation = 0;
    unsigned long long m_payloadRevision = 0;
    bool m_dirty = true, m_diagnostics;
    mutable Counters m_counters;
};
}
