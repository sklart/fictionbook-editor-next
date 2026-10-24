#include <windows.h>
#include <objidl.h>
#include <oleauto.h>
#include <atlbase.h>
#include <atlimage.h>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
enum class Failure { None, Access, Allocate, Lock, Stream };
Failure failure = Failure::None;
int checks = 0, failed = 0, allocations = 0, frees = 0;
void Check(bool value, const char* message) { ++checks; if(!value) { ++failed; std::printf("FAIL: %s\n", message); } }
HRESULT WINAPI TestAccess(SAFEARRAY* array, void** bytes) { return failure == Failure::Access ? E_FAIL : ::SafeArrayAccessData(array, bytes); }
HGLOBAL WINAPI TestAllocate(UINT flags, SIZE_T bytes) { ++allocations; Check(flags == GMEM_MOVEABLE, "stream uses a movable HGLOBAL"); return failure == Failure::Allocate ? NULL : ::GlobalAlloc(flags, bytes); }
LPVOID WINAPI TestLock(HGLOBAL memory) { return failure == Failure::Lock ? NULL : ::GlobalLock(memory); }
HGLOBAL WINAPI TestFree(HGLOBAL memory) { ++frees; return ::GlobalFree(memory); }
HRESULT WINAPI TestStream(HGLOBAL memory, BOOL release, IStream** stream) {
    Check(release == TRUE, "stream owns its copied allocation");
    return failure == Failure::Stream ? E_OUTOFMEMORY : ::CreateStreamOnHGlobal(memory, release, stream);
}
}
#define SafeArrayAccessData TestAccess
#define GlobalAlloc TestAllocate
#define GlobalLock TestLock
#define GlobalFree TestFree
#define CreateStreamOnHGlobal TestStream
#include "../../src/fbe/ImageDataStream.h"
#undef SafeArrayAccessData
#undef GlobalAlloc
#undef GlobalLock
#undef GlobalFree
#undef CreateStreamOnHGlobal

int main() {
    Check(SUCCEEDED(::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)), "COM initialized");
    IStream* invalid = reinterpret_cast<IStream*>(1);
    Check(FbeImageData::CreateStream(NULL, &invalid) == E_INVALIDARG && invalid == NULL, "null array clears output");
    Check(FbeImageData::CreateStream(NULL, NULL) == E_POINTER, "null output rejected");
    SAFEARRAY* wrong = ::SafeArrayCreateVector(VT_I4, 0, 2);
    Check(FbeImageData::CreateStream(wrong, &invalid) == E_INVALIDARG, "non-byte array rejected");
    ::SafeArrayDestroy(wrong);
    SAFEARRAYBOUND bounds[2] = {{2,0},{2,0}};
    wrong = ::SafeArrayCreate(VT_UI1, 2, bounds);
    Check(FbeImageData::CreateStream(wrong, &invalid) == E_INVALIDARG, "multidimensional byte array rejected");
    ::SafeArrayDestroy(wrong);
    wrong = ::SafeArrayCreateVector(VT_UI1, 0, 0);
    Check(FbeImageData::CreateStream(wrong, &invalid) == E_INVALIDARG, "empty byte array rejected");
    if(wrong) ::SafeArrayDestroy(wrong);

    const BYTE source[] = {0,1,2,0x80,0xFF};
    for(LONG lower : {0L, 5L, -7L}) {
        SAFEARRAY* array = ::SafeArrayCreateVector(VT_UI1, lower, sizeof(source));
        Check(array != NULL, "byte array allocated");
        if(!array) continue;
        void* bytes = NULL; ::SafeArrayAccessData(array, &bytes); std::memcpy(bytes, source, sizeof(source)); ::SafeArrayUnaccessData(array);
        CComPtr<IStream> stream;
        Check(SUCCEEDED(FbeImageData::CreateStream(array, &stream)) && stream, "bounded stream accepts nonzero lower bounds");
        Check(array->cLocks == 0, "array unlocked after stream creation");
        ::SafeArrayDestroy(array);
        if(stream) {
            BYTE copy[sizeof(source)] = {}; ULONG read = 0;
            Check(stream->Read(copy, sizeof(copy), &read) == S_OK && read == sizeof(copy) && !std::memcmp(copy, source, sizeof(source)), "stream retains exact bytes after caller array destruction");
            CComPtr<IStream> clone;
            Check(SUCCEEDED(stream->Clone(&clone)) && clone, "owned HGLOBAL stream supports Clone on Win7 contract");
            stream.Release();
            if(clone) {
                LARGE_INTEGER start = {}; clone->Seek(start, STREAM_SEEK_SET, NULL);
                Check(clone->Read(copy, sizeof(copy), &read) == S_OK && read == sizeof(copy), "clone survives original stream release");
            }
        }
    }
    SAFEARRAY* array = ::SafeArrayCreateVector(VT_UI1, 0, 3);
    for(Failure point : {Failure::Access, Failure::Allocate, Failure::Lock, Failure::Stream}) {
        failure = point;
        const int allocBefore = allocations, freeBefore = frees;
        CComPtr<IStream> stream;
        Check(FAILED(FbeImageData::CreateStream(array, &stream)) && !stream, "injected API failure leaves no stream");
        Check(array->cLocks == 0, "failure balances SAFEARRAY access");
        Check(allocations == allocBefore + (point == Failure::Access ? 0 : 1), "failed access does not allocate");
        Check(frees == freeBefore + (point == Failure::Lock || point == Failure::Stream ? 1 : 0), "failure frees only an allocated untransferred HGLOBAL");
    }
    failure = Failure::None;
    ::SafeArrayDestroy(array);

    // A real image decoder consumes the copied stream after the SAFEARRAY dies.
    BITMAPFILEHEADER file = {}; file.bfType = 0x4D42; file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER); file.bfSize = file.bfOffBits + 24;
    BITMAPINFOHEADER info = {}; info.biSize = sizeof(info); info.biWidth = 3; info.biHeight = 2; info.biPlanes = 1; info.biBitCount = 32; info.biSizeImage = 24;
    std::vector<BYTE> bitmap(file.bfSize, 0xFF);
    std::memcpy(bitmap.data(), &file, sizeof(file)); std::memcpy(bitmap.data() + sizeof(file), &info, sizeof(info));
    array = ::SafeArrayCreateVector(VT_UI1, 0, static_cast<ULONG>(bitmap.size()));
    void* bytes = NULL; ::SafeArrayAccessData(array, &bytes); std::memcpy(bytes, bitmap.data(), bitmap.size()); ::SafeArrayUnaccessData(array);
    CComPtr<IStream> stream;
    Check(SUCCEEDED(FbeImageData::CreateStream(array, &stream)), "BMP stream created");
    ::SafeArrayDestroy(array);
    if(stream) {
        CImage image;
        Check(image.Load(stream) == S_OK && image.GetWidth() == 3 && image.GetHeight() == 2, "real CImage decoder preserves dimensions");
    }
    stream.Release();
    ::CoUninitialize();
    std::printf("Image data stream: %d checks, %d failures\n", checks, failed);
    return failed ? 1 : 0;
}
