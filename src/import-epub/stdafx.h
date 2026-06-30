#pragma once

// ImportEPUB is a small ATL/COM in-process plugin for Fiction Book Editor.
// It implements IFBEImportPlugin from fbe/FBE.h and is loaded by FBE via
// CoCreateInstance using the CLSID written to the FBE Plugins registry key.

#ifndef UNICODE
#error This plugin requires Unicode build settings.
#endif

#define WINVER       0x0601
#define _WIN32_WINNT 0x0601

#define _ATL_APARTMENT_THREADED
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define _ATL_ALL_WARNINGS

#pragma warning(disable : 4996)

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <shldisp.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>
#include <atlpath.h>
#include <comdef.h>

// Main FBE import plugin interface definition.
//
// Do not include fbe/FBE.h here: in different checkouts it can be generated or
// placed differently. We only need IFBEImportPlugin, so a minimal binary-
// compatible declaration with the same IID is kept locally.
#include "FBEPluginInterfaces.h"
