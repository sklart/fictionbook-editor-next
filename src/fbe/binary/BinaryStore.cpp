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
