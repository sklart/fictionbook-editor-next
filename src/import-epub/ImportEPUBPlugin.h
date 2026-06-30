#pragma once

// The ATL COM class CImportEPUBPlugin is intentionally defined only in
// ImportEPUBPlugin.cpp.
//
// Earlier versions kept the class definition in this header. ATL's
// BEGIN_COM_MAP macro generates an inline function containing a static
// _entries array, and PVS-Studio reports this pattern as V1096 because the
// inline function has external linkage and may cause ODR-related issues when
// included from more than one translation unit.
//
// No other source file needs the concrete class type, so keeping it local to
// ImportEPUBPlugin.cpp is cleaner and removes the warning.
