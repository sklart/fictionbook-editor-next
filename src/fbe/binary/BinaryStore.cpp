#include "BinaryStore.h"
#include <utility>

namespace FbeBinary {
void BinaryStore::Clear() { m_entries.clear(); ++m_generation; m_dirty = true; }
bool BinaryStore::Contains(const CString& id) const { return Find(id) != nullptr; }
const BinaryMetadata* BinaryStore::Find(const CString& id) const {
    if (m_diagnostics) ++m_counters.lookups;
    const auto found = m_entries.find(Key(id));
    if (found != m_entries.end()) return &found->second.metadata;
    if (m_diagnostics) ++m_counters.misses;
    return nullptr;
}
BinaryMetadata* BinaryStore::Find(const CString& id) {
    return const_cast<BinaryMetadata*>(static_cast<const BinaryStore*>(this)->Find(id));
}
IDispatch* BinaryStore::Element(const CString& id) const {
    const auto found = m_entries.find(Key(id));
    return found == m_entries.end() ? nullptr : found->second.element.p;
}
void BinaryStore::Register(const BinaryMetadata& metadata, IDispatch* element) {
    const auto key = Key(metadata.id);
    Entry entry; entry.metadata = metadata; entry.metadata.generation = ++m_generation; entry.element = element;
    m_entries.insert_or_assign(key, std::move(entry));
}
bool BinaryStore::Rename(const CString& oldId, const CString& newId) {
    if (oldId == newId) return Contains(oldId);
    if (Contains(newId)) return false;
    const auto oldKey = Key(oldId);
    const auto found = m_entries.find(oldKey);
    if (found == m_entries.end()) return false;
    Entry entry = found->second; entry.metadata.id = newId;
    entry.metadata.generation = ++m_generation;
    m_entries.emplace(Key(newId), std::move(entry));
    m_entries.erase(oldKey);
    return true;
}
void BinaryStore::Remove(const CString& id) { m_entries.erase(Key(id)); }
void BinaryStore::Invalidate(bool legacy) {
    m_dirty = true;
    if (m_diagnostics && legacy) ++m_counters.legacyInvalidations;
}
void BinaryStore::Reconciled(bool rebuilt) {
    m_dirty = false;
    m_payloadRevision = payloads.Revision();
    if (m_diagnostics) { ++m_counters.reconciliations; if (rebuilt) ++m_counters.rebuilds; }
}
void BinaryStore::SizeCalculation() { if (m_diagnostics) ++m_counters.sizeCalculations; }
void BinaryStore::SizeHit() { if (m_diagnostics) ++m_counters.sizeHits; }
void BinaryStore::DimensionsCalculation() { if (m_diagnostics) ++m_counters.dimensionsCalculations; }
void BinaryStore::DimensionsHit() { if (m_diagnostics) ++m_counters.dimensionsHits; }
}
#ifdef FBE_BINARY_STORE_DIAGNOSTICS
namespace FbeBinary {
unsigned long long BinaryStore::DiagnosticEstimatedBytes() const {
    unsigned long long bytes=sizeof(*this)+static_cast<unsigned long long>(m_entries.bucket_count())*sizeof(void*);
    for(const auto& entry:m_entries){bytes+=sizeof(entry)+2*sizeof(void*);
        bytes+=static_cast<unsigned long long>(entry.first.capacity()+1)*sizeof(wchar_t);
        bytes+=static_cast<unsigned long long>(entry.second.metadata.id.GetAllocLength()+1)*sizeof(wchar_t);
        bytes+=static_cast<unsigned long long>(entry.second.metadata.contentType.GetAllocLength()+1)*sizeof(wchar_t);
    }
    return bytes; // Estimate excludes allocator headers and COM/MSHTML allocations.
}
}
#endif
#ifdef FBE_BINARY_STORE_DIAGNOSTICS
namespace FbeBinary {
PayloadDiagnosticStats BinaryStore::DiagnosticCurrentPayloads() const {
    PayloadDiagnosticStats result;std::unordered_set<const void*> seen;
    for(const auto& entry:m_entries){const auto value=payloads.Snapshot(entry.second.element);
        if(!value||value->vt!=(VT_ARRAY|VT_UI1)||!value->parray||!value->parray->pvData)continue;
        if(seen.insert(value->parray->pvData).second){result.canonicalBytes+=DiagnosticVariantBytes(*value);if(payloads.DiagnosticHistoryReferences(value.get()))result.historyReferencedBytes+=DiagnosticVariantBytes(*value);}
    }
    result.uniquePayloadBlocks=seen.size();return result;
}
}
#endif
