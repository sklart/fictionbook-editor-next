#include "../../src/fbe/utils/WindowRectangle.h"
#include <cstdio>

int main()
{
    struct Case { LONG end, begin; int expected; };
    const Case cases[] = {
        {0, CW_USEDEFAULT, CW_USEDEFAULT}, {640, 0, 640}, {100, 20, 80},
        {20, 100, -80}, {INT_MAX, INT_MIN, -1}, {INT_MIN, INT_MAX, 1},
        {INT_MIN, 0, INT_MIN}, {INT_MAX, 0, INT_MAX}, {0, INT_MAX, -INT_MAX},
        {INT_MIN, INT_MIN, 0}, {INT_MAX, INT_MAX, 0}
    };
    for (const auto& item : cases)
        if (WindowRectangle::Span(item.end, item.begin) != item.expected) return 1;
    std::puts("Window rectangle: 11 checks, 0 failures");
    return 0;
}
