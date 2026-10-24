#include "../../src/fbe/source/VirtualAddressRange.h"
#include <cstdio>

int main()
{
    using std::uintptr_t;
    const uintptr_t maximum = (std::numeric_limits<uintptr_t>::max)();
    struct Case { uintptr_t base; std::size_t size; uintptr_t current; bool advances; uintptr_t next; };
    const Case cases[] = {
        {0, 65536, 0, true, 65536}, {0, 0, 0, false, 0},
        {maximum, 1, maximum, false, 0}, {maximum - 15, 16, 0, false, 0},
        {0, 4096, 4096, false, 0}, {100, 1, 100, true, 101},
        {16, 16, 20, true, 32}, {0, maximum, 0, true, maximum}
    };
    for (const auto& item : cases) {
        uintptr_t next = 123;
        const bool advances = FbeSourceDiagnostics::NextVirtualRegion(item.base, item.size, item.current, next);
        if (advances != item.advances || (advances ? next != item.next : next != 123)) return 1;
    }
    std::puts("Virtual address ranges: 8 checks, 0 failures");
    return 0;
}
