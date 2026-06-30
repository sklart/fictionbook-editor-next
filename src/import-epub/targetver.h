#pragma once

// Minimum supported Windows version for the plugin.
// FictionBook Editor itself is a classic Win32 application; Windows 7+ API level is enough here.

#ifndef WINVER
#define WINVER 0x0601
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
