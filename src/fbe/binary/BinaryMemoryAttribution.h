#pragma once
// Internal observation points for the isolated C4.1 executable only.
// This adds no COM surface, object fields, or normal-runtime callbacks.
#ifdef FBE_BINARY_STORE_MEMORY_ATTRIBUTION
namespace FbeBinary { namespace MemoryAttribution {
using Observer = void (*)(void*, const char*, unsigned long long);
inline Observer& Callback() { static Observer value = nullptr; return value; }
inline void*& Context() { static void* value = nullptr; return value; }
inline void Point(const char* stage, unsigned long long knownBytes = 0) {
    if (Callback()) Callback()(Context(), stage, knownBytes);
}
} }
#define FBE_C41_POINT(stage, bytes) FbeBinary::MemoryAttribution::Point(stage, bytes)
#else
#define FBE_C41_POINT(stage, bytes) ((void)0)
#endif
