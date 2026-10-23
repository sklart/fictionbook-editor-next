#include <windows.h>
#include <new>

// Retain FBE's process-heap allocation domain. The v143 CRT forwards ordinary
// array/nothrow new and array/sized/nothrow delete to these scalar overloads.
// Over-aligned allocations retain the CRT's separate aligned allocation pair.
void* operator new(size_t amount)
{
    if (amount == 0) amount = 1;
    for (;;) {
        if (void* block = ::HeapAlloc(::GetProcessHeap(), 0, amount)) return block;
        const std::new_handler handler = std::get_new_handler();
        if (!handler) throw std::bad_alloc();
        handler();
    }
}

void operator delete(void* block) noexcept
{
    if (block) ::HeapFree(::GetProcessHeap(), 0, block);
}
