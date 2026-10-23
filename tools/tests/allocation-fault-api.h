#pragma once
#include <windows.h>

// Forced into MemoryAllocation.cpp only by the native test build. Production
// has no injection flags, counters, environment switches or alternate paths.
LPVOID WINAPI FbeTestHeapAlloc(HANDLE heap, DWORD flags, SIZE_T bytes);
BOOL WINAPI FbeTestHeapFree(HANDLE heap, DWORD flags, LPVOID block);
#define HeapAlloc FbeTestHeapAlloc
#define HeapFree FbeTestHeapFree
