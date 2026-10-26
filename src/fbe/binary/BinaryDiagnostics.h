#pragma once
// C4 counters exist only in a separately compiled diagnostic executable.
#ifdef FBE_BINARY_STORE_DIAGNOSTICS
#include <windows.h>
#include <cstdint>
namespace FbeBinary { namespace Diagnostics {
enum class Phase { DocumentLoad, DomCreation, ScriptLoad, Discovery, Population, BehaviorAttach, Reconcile, Metadata, ImageDecode, Serialization, Save, Count };
struct Counters {
    std::uint64_t imageDecodedBitmapBytesTotal=0, maxSingleDecodedBitmapBytes=0, imageDecodeCalls=0;
    std::uint64_t facadeGets=0, facadeGetBytes=0, facadePuts=0, putCopyBytes=0, sameValuePuts=0;
    std::uint64_t attachCalls=0, behaviorAttachments=0, adoptCalls=0, assignCalls=0;
    std::uint64_t enumerationCalls=0, enumeratedEntries=0, reconcileCalls=0, metadataInvalidations=0;
    std::uint64_t phaseCalls[static_cast<unsigned>(Phase::Count)]={};
    double phaseMs[static_cast<unsigned>(Phase::Count)]={};
};
inline Counters& Get() { static Counters value; return value; }
inline void ImageBitmap(unsigned long long bytes) { auto& c=Get();c.imageDecodedBitmapBytesTotal+=bytes;++c.imageDecodeCalls;if(bytes>c.maxSingleDecodedBitmapBytes)c.maxSingleDecodedBitmapBytes=bytes; }
inline double Now() { LARGE_INTEGER n,f; ::QueryPerformanceCounter(&n); ::QueryPerformanceFrequency(&f); return 1000.0*static_cast<double>(n.QuadPart)/static_cast<double>(f.QuadPart); }
inline void Record(Phase phase,double started) { const auto i=static_cast<unsigned>(phase); ++Get().phaseCalls[i]; Get().phaseMs[i]+=Now()-started; }
struct Scope { Phase phase; double start; explicit Scope(Phase p):phase(p),start(Now()){} ~Scope(){Record(phase,start);} };
}}
#define FBE_BD_JOIN_(a,b) a##b
#define FBE_BD_JOIN(a,b) FBE_BD_JOIN_(a,b)
#define FBE_BINARY_DIAG_SCOPE(phase) FbeBinary::Diagnostics::Scope FBE_BD_JOIN(c4Scope,__LINE__)(FbeBinary::Diagnostics::Phase::phase)
#define FBE_BINARY_DIAG_ADD(name,value) (FbeBinary::Diagnostics::Get().name += (value))
#define FBE_BINARY_DIAG_BEGIN(phase) const double FBE_BD_JOIN(c4Start,phase)=FbeBinary::Diagnostics::Now()
#define FBE_BINARY_DIAG_END(phase) FbeBinary::Diagnostics::Record(FbeBinary::Diagnostics::Phase::phase,FBE_BD_JOIN(c4Start,phase))
#else
#define FBE_BINARY_DIAG_SCOPE(phase) ((void)0)
#define FBE_BINARY_DIAG_ADD(name,value) ((void)0)
#define FBE_BINARY_DIAG_BEGIN(phase) ((void)0)
#define FBE_BINARY_DIAG_END(phase) ((void)0)
#endif
