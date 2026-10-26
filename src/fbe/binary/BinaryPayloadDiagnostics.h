#pragma once
#ifdef FBE_BINARY_STORE_DIAGNOSTICS
#include <unordered_set>
namespace FbeBinary {
struct PayloadDiagnosticStats {
    unsigned long long currentEntries=0, historyEntries=0, uniqueCurrentObjects=0, uniqueHistoryObjects=0, uniqueObjects=0;
    unsigned long long uniquePayloadBlocks=0, canonicalPayloadBlocks=0;
    unsigned long long canonicalBytes=0, historyReferencedBytes=0, historyOnlyBytes=0, uniqueBytes=0, sharedReferences=0;
};
inline unsigned long long DiagnosticVariantBytes(const CComVariant& value) {
    if(value.vt!=(VT_ARRAY|VT_UI1)||!value.parray||value.parray->cDims!=1) return 0;
    return static_cast<unsigned long long>(value.parray->rgsabound[0].cElements)*value.parray->cbElements;
}
}
#endif
