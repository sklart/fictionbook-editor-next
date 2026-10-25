#include <windows.h>
#include <cstdio>
#include <type_traits>
#include "binary/BinaryStore.h"
#include "../../src/common/DiagnosticPrivacy.h"

// An x86 by-value copy of BinaryMetadata can be only four-byte aligned.
// Keep this internal ABI reference-based; the UBSan native unit reproduces
// the original defect before this guard and runs clean with this signature.
using RegisterMetadata = void (FbeBinary::BinaryStore::*)(const FbeBinary::BinaryMetadata&, IDispatch*);
static_assert(std::is_same_v<decltype(&FbeBinary::BinaryStore::Register), RegisterMetadata>,
    "BinaryMetadata must not cross the Win32 ABI by value" );

int wmain()
{
    FbeBinary::BinaryStore store(true), other(true);
    for (int index = 0; index < 100; ++index) {
        FbeBinary::BinaryMetadata metadata;
        metadata.id.Format(L"payload%d", index); metadata.contentType = L"application/octet-stream";
        metadata.byteSize = 65536; metadata.sizeKnown = true;
        store.Register(metadata);
        if (metadata.generation != 0 || metadata.byteSize != 65536) return 12;
    }
    store.Reconciled(true);
    if (store.Count() != 100 || other.Count() || store.Dirty()) return 1;
    for (int index = 0; index < 500; ++index) {
        const auto* metadata = store.Find(L"payload42");
        if (!metadata || metadata->byteSize != 65536 || !metadata->sizeKnown) return 2;
    }
    const auto generation = store.Find(L"payload42")->generation;
    if (!store.Rename(L"payload42", L"renamed") || store.Contains(L"payload42") || store.Count() != 100) return 3;
    if (store.Find(L"renamed")->generation <= generation || store.Find(L"renamed")->byteSize != 65536) return 4;
    if (store.Rename(L"renamed", L"payload1") || store.Rename(L"missing", L"new")) return 5;
    auto metadata = *store.Find(L"renamed"); metadata.byteSize = 7;
    store.Register(metadata);
    if (store.Count() != 100 || store.Find(L"renamed")->byteSize != 7) return 6;
    store.Remove(L"renamed"); if (store.Count() != 99 || store.Contains(L"renamed")) return 7;
    store.Invalidate(true); if (!store.Dirty() || store.Statistics().legacyInvalidations != 1) return 8;
    store.Clear(); if (store.Count() || !store.Dirty() || other.Count()) return 9;
    FbeBinary::BinaryStore quiet;
    quiet.Contains(L"absent"); quiet.Invalidate(true); quiet.SizeCalculation(); quiet.DimensionsCalculation();
    if (quiet.Statistics().lookups || quiet.Statistics().legacyInvalidations || quiet.Statistics().sizeCalculations || quiet.Statistics().dimensionsCalculations) return 10;
    const auto diagnostic = FbeDiagnosticPrivacy::Details(L"binary_store_entries=100; binary_lookup_count=500; binary_store_bytes=123; binary_data=SECRET; binary_dimensions_calculations=C:\\secret");
    if (diagnostic.find(L"binary_store_entries=100") == std::wstring::npos || diagnostic.find(L"binary_lookup_count=500") == std::wstring::npos ||
        diagnostic.find(L"SECRET") != std::wstring::npos || diagnostic.find(L"secret") != std::wstring::npos || diagnostic.find(L"binary_store_bytes") != std::wstring::npos) return 11;
    puts("BinaryStore native unit: PASS (100 entries, 500 lookups, rename/conflicts/replace/remove/lifecycle/trace gating)");
    return 0;
}
