#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <new>
#include <vector>

namespace {
int checks = 0, failures = 0, allocations = 0, releases = 0, handlerCalls = 0;
int failNext = 0, invalidReleases = 0, live = 0;
SIZE_T lastSize = 0;
void* blocks[256] = {};

void Check(bool condition, const char* name)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", name); }
}

struct HandlerScope {
    std::new_handler previous;
    explicit HandlerScope(std::new_handler handler) : previous(std::set_new_handler(handler)) {}
    ~HandlerScope() { failNext = 0; std::set_new_handler(previous); }
};

void Retry() { ++handlerCalls; }
void ThrowBadAlloc() { ++handlerCalls; throw std::bad_alloc(); }
struct HandlerError {};
void ThrowOther() { ++handlerCalls; throw HandlerError(); }
void Uninstall() { ++handlerCalls; std::set_new_handler(nullptr); }

void FailedThrowing(bool array)
{
    HandlerScope scope(nullptr);
    const int before = allocations;
    failNext = 1;
    void* block = nullptr;
    bool caught = false;
    try { block = array ? ::operator new[](24) : ::operator new(24); }
    catch (const std::bad_alloc&) { caught = true; }
    Check(caught, array ? "array allocation throws bad_alloc" : "scalar allocation throws bad_alloc");
    Check(allocations == before + 1, "failure uses the production process-heap allocator");
    if (block) ::operator delete(block);
}

void FailedNothrow(bool array, std::new_handler handler)
{
    HandlerScope scope(handler);
    handlerCalls = 0;
    failNext = 1;
    void* block = array ? ::operator new[](24, std::nothrow) : ::operator new(24, std::nothrow);
    Check(block == nullptr, array ? "array nothrow returns nullptr" : "scalar nothrow returns nullptr");
    Check(handlerCalls == (handler ? 1 : 0), "nothrow observes the installed handler");
    if (block) ::operator delete(block);
}

struct Object {
    static int constructed, destroyed;
    int value = 7;
    Object() { ++constructed; }
    ~Object() { ++destroyed; }
};
int Object::constructed = 0;
int Object::destroyed = 0;

struct ThrowingObject {
    ThrowingObject() { throw HandlerError(); }
};
struct ArrayObject {
    static int attempts, destroyed;
    ArrayObject() { if (++attempts == 3) throw HandlerError(); }
    ~ArrayObject() { ++destroyed; }
};
int ArrayObject::attempts = 0;
int ArrayObject::destroyed = 0;

struct alignas(64) AlignedObject { int value = 17; char payload[60] = {}; };
}

// Only MemoryAllocation.cpp is compiled with allocation-fault-api.h forced in.
// These wrappers use the real Win32 heap for every successful allocation/free.
LPVOID WINAPI FbeTestHeapAlloc(HANDLE heap, DWORD flags, SIZE_T bytes)
{
    ++allocations;
    lastSize = bytes;
    if (failNext > 0) { --failNext; return nullptr; }
    void* block = ::HeapAlloc(heap, flags, bytes);
    if (block) {
        for (void*& entry : blocks) if (!entry) { entry = block; ++live; return block; }
        ++invalidReleases;
    }
    return block;
}

BOOL WINAPI FbeTestHeapFree(HANDLE heap, DWORD flags, LPVOID block)
{
    ++releases;
    for (void*& entry : blocks) if (entry == block && block) {
        entry = nullptr;
        --live;
        return ::HeapFree(heap, flags, block);
    }
    ++invalidReleases;
    return FALSE;
}

int main()
{
    ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    HandlerScope noHandler(nullptr);
    const int initialLive = live;
    void* first = ::operator new(0);
    void* second = ::operator new(0);
    Check(first && second && first != second, "zero-size live allocations have non-null distinct addresses");
    Check(lastSize > 0, "zero size is normalized before HeapAlloc");
    Check(::HeapValidate(::GetProcessHeap(), 0, first) != FALSE, "scalar allocation retains the process heap");
    Check(reinterpret_cast<std::uintptr_t>(first) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == 0, "ordinary allocation meets the compiler default alignment");
    ::operator delete(first);
    ::operator delete(second);
    const int beforeNull = releases;
    ::operator delete(nullptr);
    ::operator delete[](nullptr);
    ::operator delete(nullptr, std::size_t(8));
    ::operator delete[](nullptr, std::size_t(8));
    Check(releases == beforeNull, "all ordinary delete(nullptr) forms are no-ops");
    void* emptyArray = ::operator new[](0);
    Check(emptyArray && lastSize > 0, "zero-size array allocation is non-null and normalized");
    ::operator delete[](emptyArray);

    for (int mode = 0; mode != 6; ++mode) {
        void* block = mode % 2 ? ::operator new[](32) : ::operator new(32);
        Check(block && ::HeapValidate(::GetProcessHeap(), 0, block), "ordinary allocation family shares the process heap");
        const int before = releases;
        switch (mode) {
        case 0: ::operator delete(block); break;
        case 1: ::operator delete[](block); break;
        case 2: ::operator delete(block, std::size_t(32)); break;
        case 3: ::operator delete[](block, std::size_t(32)); break;
        case 4: ::operator delete(block, std::nothrow); break;
        case 5: ::operator delete[](block, std::nothrow); break;
        }
        Check(releases == before + 1, "ordinary delete family forwards to the matching scalar heap free");
    }

    FailedThrowing(false);
    FailedThrowing(true);
    // Do not enter STL/object-construction tests when testing a regressed
    // allocator which returned nullptr instead of throwing: those clients
    // legitimately rely on the throwing-new contract and may dereference it.
    if (failures) {
        std::printf("FAIL: %d checks, %d failures (unsafe allocator; remaining clients skipped)\n", checks, failures);
        return 1;
    }
    for (bool array : {false, true}) {
        FailedNothrow(array, nullptr);
        FailedNothrow(array, ThrowBadAlloc);
        FailedNothrow(array, ThrowOther);
        void* block = array ? ::operator new[](24, std::nothrow) : ::operator new(24, std::nothrow);
        Check(block && ::HeapValidate(::GetProcessHeap(), 0, block), "successful nothrow allocation uses the process heap");
        if (array) ::operator delete[](block); else ::operator delete(block);
    }
    {
        HandlerScope scope(Retry);
        handlerCalls = 0;
        failNext = 2;
        const int before = allocations;
        void* block = ::operator new(24);
        Check(block && handlerCalls == 2 && allocations == before + 3, "returning new_handler retries allocation until success");
        ::operator delete(block);
    }
    {
        HandlerScope scope(ThrowOther);
        handlerCalls = 0;
        failNext = 1;
        bool caught = false;
        void* block = nullptr;
        try { block = ::operator new(24); } catch (const HandlerError&) { caught = true; }
        Check(caught && handlerCalls == 1, "throwing new propagates the handler exception");
        if (block) ::operator delete(block);
    }
    {
        HandlerScope scope(Uninstall);
        handlerCalls = 0;
        failNext = 2;
        bool caught = false;
        void* block = nullptr;
        try { block = ::operator new(24); } catch (const std::bad_alloc&) { caught = true; }
        Check(caught && handlerCalls == 1, "handler can uninstall itself before the next failed attempt");
        if (block) ::operator delete(block);
    }
    {
        auto existing = std::make_unique<Object>();
        Object* identity = existing.get();
        failNext = 1;
        bool caught = false;
        try { auto replacement = std::make_unique<Object>(); existing = std::move(replacement); }
        catch (const std::bad_alloc&) { caught = true; }
        Check(caught && existing.get() == identity && existing->value == 7, "failed replacement allocation preserves the previously owned object");
    }
    {
        const int before = Object::constructed;
        failNext = 1;
        bool caught = false;
        try { auto object = std::make_unique<Object>(); Check(object->value == 7, "unexpected successful object value"); }
        catch (const std::bad_alloc&) { caught = true; }
        Check(caught && Object::constructed == before, "failed object allocation never enters its constructor");
    }
    {
        failNext = 1;
        bool caught = false;
        try { std::vector<int> values(32, 9); Check(values[0] == 9, "unexpected vector value"); }
        catch (const std::bad_alloc&) { caught = true; }
        Check(caught, "STL allocation failure is catchable as bad_alloc");
    }
    {
        const int before = live;
        bool caught = false;
        try { auto* object = new ThrowingObject; delete object; }
        catch (const HandlerError&) { caught = true; }
        Check(caught && live == before, "throwing scalar constructor releases its allocation");
        caught = false;
        try { auto* objects = new ArrayObject[4]; delete[] objects; }
        catch (const HandlerError&) { caught = true; }
        Check(caught && ArrayObject::destroyed == 2 && live == before, "partial array construction destroys elements and frees the array cookie block");
    }

    // These allocations use the CRT aligned pair, not FBE's scalar heap pair.
    // Check direct sized/nothrow fallbacks as well as compiler-generated delete.
    for (int mode = 0; mode != 8; ++mode) {
        const auto alignment = std::align_val_t(64);
        const int before = live;
        void* block = mode >= 6
            ? (mode % 2 ? ::operator new[](128, alignment, std::nothrow) : ::operator new(128, alignment, std::nothrow))
            : (mode % 2 ? ::operator new[](128, alignment) : ::operator new(128, alignment));
        Check(block && reinterpret_cast<std::uintptr_t>(block) % 64 == 0, "CRT over-aligned allocation has the requested alignment");
        switch (mode) {
        case 0: ::operator delete(block, alignment); break;
        case 1: ::operator delete[](block, alignment); break;
        case 2: ::operator delete(block, std::size_t(128), alignment); break;
        case 3: ::operator delete[](block, std::size_t(128), alignment); break;
        case 4: ::operator delete(block, alignment, std::nothrow); break;
        case 5: ::operator delete[](block, alignment, std::nothrow); break;
        case 6: ::operator delete(block, alignment, std::nothrow); break;
        case 7: ::operator delete[](block, alignment, std::nothrow); break;
        }
        Check(live == before, "aligned delete does not cross into the scalar heap family");
    }
    {
        auto object = std::make_unique<AlignedObject>();
        Check(object->value == 17 && reinterpret_cast<std::uintptr_t>(object.get()) % 64 == 0, "compiler-generated over-aligned new/delete remain usable");
    }
    Check(live == initialLive, "all test allocations released");
    Check(invalidReleases == 0, "no unmatched allocation/deallocation or null HeapFree calls");
    std::printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
